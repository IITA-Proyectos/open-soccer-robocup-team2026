// strategy.cpp — FSM dual delantero/arquero con PIDs reales (Niveles 1 + 2).
//
// Niveles spec FIRMWARE-PLACA-CENTRAL §18:
//
// Nivel 1 (Incheon mínimo):
//   • DELANTERO: WAIT_START → SEARCH → APPROACH → SEARCH (sin behind-the-ball).
//   • ARQUERO:   WAIT_START → PATROL → INTERCEPT → PATROL.
//
// Nivel 2 (este archivo agrega):
//   • DELANTERO: KICKOFF (set play inicial) + POSITION (behind-the-ball) +
//                empuje cuando alineado al arco rival (sin kicker físico:
//                el robot empuja la pelota por inercia, no dispara nada).
//   • ARQUERO:   CLEAR (despeje cuando pelota llega cerca).
//
// Modo EMERGENCY_LINE bypassa la FSM (se maneja en main_central.cpp directo,
// antes de llamar a strategy_tick).
//
// Si LINE_FLAG_LIFTED set, world_model_imminent_exit() retorna false (filtrado
// en line_ring DOWN antes de enviar), así no entramos a LINE_AVOID con datos
// basura.
//
// La cinemática inversa NO vive acá — los outputs (vx, vy, omega) viajan al
// motors_zircon que aplica la kinematics. Eso permite tunear strategy y
// kinematics por separado.

#include "strategy.h"
#include "world_model.h"
#include "pids.h"
#include "behind_ball.h"
#include "drive_straight.h"
#include "ball_predict.h"

#include <Arduino.h>
#include <cmath>

namespace iitasoccer {

namespace {

// === Configuración táctica ===

RobotRole   g_role = RobotRole::ATTACKER;
AttackColor g_attack_color = AttackColor::MAGENTA;

const char* g_state_name = "INIT";

// Estados internos enumerados (no expuestos en el header — debug por nombre).
//
// Nivel 1 (Incheon mínimo):
//   ATK: WAIT_START / SEARCH / APPROACH / LINE_AVOID
//   GK:  WAIT_START / PATROL / INTERCEPT / LINE_AVOID
//
// Nivel 2 (este batch) agrega:
//   ATK: KICKOFF (set play inicial), POSITION (behind-the-ball orbit)
//   GK:  CLEAR (despeje cuando la pelota está cerca)
enum class AtkState : uint8_t {
    WAIT_START, KICKOFF, SEARCH, POSITION, APPROACH, LINE_AVOID
};
enum class GkState : uint8_t {
    WAIT_START, PATROL, INTERCEPT, CLEAR, LINE_AVOID
};

AtkState g_atk_state = AtkState::WAIT_START;
GkState  g_gk_state  = GkState::WAIT_START;

// Estado auxiliar del kickoff (timer) — válido solo cuando g_atk_state == KICKOFF.
uint32_t g_kickoff_started_ms = 0;
bool     g_match_was_running = false;   // detecta flanco "STOP → RUN" para KICKOFF

// === PIDs ===
HeadingPID g_heading_pid;
LateralPID g_lateral_pid_gk;

// === Tuning constants ===

// Delantero — Nivel 1
constexpr float ATK_SEARCH_VY_MM_S       = 200.0f;
constexpr float ATK_SEARCH_OMEGA_DEG_S   = 60.0f;
constexpr float ATK_APPROACH_MAX_SPEED   = 600.0f;
constexpr float ATK_APPROACH_MIN_SPEED   = 200.0f;
constexpr float ATK_APPROACH_CLOSE_MM    = 50.0f;
constexpr float ATK_APPROACH_FAR_MM      = 500.0f;
constexpr float ATK_LINE_RETREAT_SPEED   = 400.0f;

// Delantero — Nivel 2
constexpr float ATK_BEHIND_BALL_GAP_MM         = 120.0f;   // separación robot–pelota cuando POSITION
constexpr float ATK_ATTACK_LINE_TOL_DEG        = 30.0f;    // si ángulo ball ↔ goal < este → ya alineado
constexpr float ATK_POSITION_REACHED_MM        = 80.0f;    // distancia target POSITION para pasar a APPROACH
constexpr float ATK_KICK_DIST_MM               = 80.0f;    // pelota más cerca que esto + alineado → comprometer EMPUJE (sin kicker físico)
constexpr float ATK_KICK_ANGLE_DEG             = 12.0f;    // tolerancia angular para considerarse alineado al arco (empuje)
constexpr float ATK_POSITION_MAX_SPEED         = 500.0f;
constexpr float ATK_KICKOFF_SPEED_MM_S         = 500.0f;
constexpr uint32_t ATK_KICKOFF_DURATION_MS     = 250;      // boost inicial al frente al arrancar match

// Drive-straight (WP-2A, Capa 2): refinamiento OPCIONAL del avance recto usando
// la odometría OTOS directa de DOWN (baja latencia, sin round-trip por el TOP).
// Solo se aplica si world_model_otos_is_fresh(); si no, fallback EXACTO al
// comportamiento sin OTOS. Convención robot: +Y=frente, +X=derecha, omega CCW+.
// El módulo drive_straight (src/shared) llama "vx/fwd_speed" al avance y "vy" a
// la corrección lateral; acá los bindeamos a los ejes FÍSICOS del robot:
//   • avance del robot  = +Y  -> drive_straight.in.fwd_speed ; out.vx_mm_s -> cmd.vy_mm_s
//   • lateral del robot = +X  -> drive_straight.in.otos_vy = OTOS vx ; out.vy_mm_s -> cmd.vx_mm_s
constexpr float DS_KP_HEADING = 3.0f;   // grados/s por grado (alineado con HeadingPID.kp)
constexpr float DS_KP_LATERAL = 0.5f;   // amortigua ~media deriva lateral OTOS por tick

// Arquero — Nivel 1
constexpr float GK_PATROL_OSCILLATE_PERIOD_MS = 2000.0f;
constexpr float GK_PATROL_SPEED_MM_S          = 150.0f;
constexpr float GK_INTERCEPT_KP_VS_BALL_X     = 4.0f;
constexpr float GK_LATERAL_SETPOINT_DEPTH     = 1.0f;
constexpr float GK_LINE_RETREAT_SPEED         = 250.0f;

// Arquero — Capa 3 (WP-3-GK): strafe PARALELO a la línea lateral usando el
// cross_track REAL (distancia perpendicular firmada del centro del robot a la
// recta de la línea, en mm; computado en down_model con geometría). El PID
// lateral lleva cross_track -> setpoint (0 = pisar la línea, distancia
// perpendicular nula) mientras la oscilación de PATROL / el seguimiento de bola
// de INTERCEPT mueven al robot A LO LARGO de la línea => queda paralelo.
// Setpoint 0 mm: el arquero se mantiene CENTRADO sobre la línea. (Si en banco se
// quiere "flotar" a una distancia fija de la línea lateral, subir este valor.)
constexpr float GK_CROSS_TRACK_SETPOINT_MM    = 0.0f;

// Arquero — Nivel 2
constexpr float GK_CLEAR_TRIGGER_MM           = 250.0f;    // pelota más cerca que esto → CLEAR
constexpr float GK_CLEAR_RELEASE_MM           = 400.0f;    // histéresis: vuelve a PATROL al alejarse
constexpr float GK_CLEAR_SPEED_MM_S           = 500.0f;

// === Helpers ===

inline bool line_data_fresh() {
    return world_model_line_is_fresh();
}

// Salida del PID lateral del arquero (mm/s sobre el eje +X del robot).
//
// WP-3-GK (Capa 3): si el frame de línea está fresco Y trae un cross_track REAL
// (world_model_cross_track_valid()), el error del PID es la distancia
// perpendicular firmada a la línea (cross_track) con setpoint
// GK_CROSS_TRACK_SETPOINT_MM. Así el arquero mantiene distancia perpendicular
// fija y, combinado con la oscilación de PATROL / el seguimiento de bola de
// INTERCEPT (que mueven el eje X), se desplaza PARALELO a la línea lateral.
//
// FALLBACK EXACTO: si el cross_track es N/A (anillo parcial, sin blancos
// validados, data_valid==0, o DOWN aún sin Capa 3), se usa la señal previa
// basada en profundidad (depth) con setpoint GK_LATERAL_SETPOINT_DEPTH —
// idéntico al comportamiento anterior a este WP. Si la línea no está fresca,
// devuelve 0 (igual que antes: sin corrección lateral).
//
// Nota de signos: cross_track + = línea adelante (+Y); el PID lo mapea a una
// velocidad lateral (+X) que anula el offset. depth es no-firmado (magnitud);
// por eso son setpoints/escalas distintas y NO se mezclan. El caller pondera
// esta salida igual que antes (×0.5 en PATROL, ×0.3 en INTERCEPT).
inline float gk_lateral_pid_output(uint32_t now_ms) {
    if (!line_data_fresh()) {
        return 0.0f;
    }
    if (world_model_cross_track_valid()) {
        // Capa 3: error = cross_track (mm perpendicular firmado) -> setpoint.
        const float cross_track = world_model_get_cross_track_mm();
        lateral_pid_set_target(g_lateral_pid_gk, GK_CROSS_TRACK_SETPOINT_MM);
        return lateral_pid_tick(g_lateral_pid_gk, cross_track, now_ms);
    }
    // Fallback EXACTO al comportamiento previo (control por profundidad).
    const float depth = static_cast<float>(world_model_get_line_depth());
    lateral_pid_set_target(g_lateral_pid_gk, GK_LATERAL_SETPOINT_DEPTH);
    return lateral_pid_tick(g_lateral_pid_gk, depth, now_ms);
}

void transition_atk(AtkState new_state) {
    if (new_state == g_atk_state) return;
    g_atk_state = new_state;
    // Reset PID en cada transición — evita arrastrar windup viejo.
    heading_pid_reset(g_heading_pid);
}

void transition_gk(GkState new_state) {
    if (new_state == g_gk_state) return;
    g_gk_state = new_state;
    heading_pid_reset(g_heading_pid);
    if (new_state != GkState::PATROL && new_state != GkState::INTERCEPT) {
        lateral_pid_reset(g_lateral_pid_gk);
    }
}

// === FSM Delantero ===

MotorCommand attacker_tick() {
    MotorCommand cmd{};
    const uint32_t now_ms = millis();
    const float heading = world_model_get_my_heading_deg();

    // Detección de flanco STOP→RUN para entrar a KICKOFF.
    const bool match_running = world_model_match_running();
    const bool kickoff_edge = match_running && !g_match_was_running;
    g_match_was_running = match_running;

    // Transiciones globales prioritarias.
    if (!match_running) {
        transition_atk(AtkState::WAIT_START);
    } else if (world_model_imminent_exit() && line_data_fresh()) {
        // imminent_exit en DOWN ya respeta lifted (no se setea si lifted=true).
        transition_atk(AtkState::LINE_AVOID);
    } else if (kickoff_edge) {
        // Flanco STOP→RUN: arrancar el set play KICKOFF.
        transition_atk(AtkState::KICKOFF);
        g_kickoff_started_ms = now_ms;
    }

    switch (g_atk_state) {
        case AtkState::WAIT_START: {
            g_state_name = "ATK_WAIT_START";
            // El flanco STOP→RUN lo maneja la sección global de arriba.
            return cmd;
        }

        case AtkState::KICKOFF: {
            g_state_name = "ATK_KICKOFF";
            // Set play: boost recto al frente con heading hacia 0° absoluto
            // durante ATK_KICKOFF_DURATION_MS. Después transitions a SEARCH.
            const KickoffVelocity kv = compute_kickoff_velocity(ATK_KICKOFF_SPEED_MM_S);
            // Mantener heading actual — no queremos rotar durante el boost.
            heading_pid_set_target(g_heading_pid, heading);

            if (world_model_otos_is_fresh()) {
                // Refinamiento WP-2A: patear/manejar DERECHO con OTOS directo.
                // target = heading actual (mantener rumbo); avance = boost al
                // frente (kv.vy); deriva lateral a cancelar = OTOS vx (eje +X).
                DriveStraightIn ds_in;
                ds_in.target_heading_deg = world_model_get_otos_heading_deg();
                ds_in.cur_heading_deg    = world_model_get_otos_heading_deg();
                // #21: solo cancelar deriva si la VEL del OTOS está fresca (frame 0x12 propio).
                // Si la vel se perdió (CRC) aunque la pose siga fresca, usar 0 (no corregir con dato viejo).
                ds_in.otos_vy_mm_s       = world_model_otos_vel_is_fresh()
                                               ? world_model_get_otos_vx_mm_s()   // lateral robot = +X
                                               : 0.0f;
                ds_in.fwd_speed_mm_s     = kv.vy_mm_s;                      // boost al frente
                const DriveStraightCfg ds_cfg{DS_KP_HEADING, DS_KP_LATERAL};
                const DriveStraightCmd ds = drive_straight_compute(ds_in, ds_cfg);
                // Bind a ejes del robot: out.vx->frente(+Y)=cmd.vy ; out.vy->lateral(+X)=cmd.vx.
                cmd.vy_mm_s = static_cast<int16_t>(ds.vx_mm_s);
                cmd.vx_mm_s = static_cast<int16_t>(ds.vy_mm_s);
                cmd.omega_centideg_s = omega_degps_to_centideg(ds.omega_deg_s);  // #9
            } else {
                // Fallback EXACTO al comportamiento previo (sin OTOS).
                cmd.vx_mm_s = static_cast<int16_t>(kv.vx_mm_s);
                cmd.vy_mm_s = static_cast<int16_t>(kv.vy_mm_s);
                cmd.omega_centideg_s = 0;
            }

            if (now_ms - g_kickoff_started_ms >= ATK_KICKOFF_DURATION_MS) {
                transition_atk(AtkState::SEARCH);
            }
            return cmd;
        }

        case AtkState::LINE_AVOID: {
            g_state_name = "ATK_LINE_AVOID";
            // Retroceder en dirección opuesta a la línea detectada.
            const float line_angle = world_model_get_line_angle_deg();
            const float retreat = line_angle + 180.0f;
            const float rad = retreat * (M_PI / 180.0f);
            cmd.vx_mm_s = static_cast<int16_t>(std::sin(rad) * ATK_LINE_RETREAT_SPEED);
            cmd.vy_mm_s = static_cast<int16_t>(std::cos(rad) * ATK_LINE_RETREAT_SPEED);
            if (!world_model_imminent_exit()) {
                transition_atk(AtkState::SEARCH);
            }
            return cmd;
        }

        case AtkState::SEARCH: {
            g_state_name = "ATK_SEARCH";
            // Recorrer cancha con avance lento + rotación.
            cmd.vy_mm_s = static_cast<int16_t>(ATK_SEARCH_VY_MM_S);
            cmd.omega_centideg_s = omega_degps_to_centideg(ATK_SEARCH_OMEGA_DEG_S);  // #9
            if (world_model_ball_visible()) {
                // Si vemos arco rival y la pelota NO está alineada, primero
                // POSITION (orbit). Si está alineada o no vemos arco, APPROACH
                // directo (fallback).
                const float bx = world_model_get_ball_x_mm();
                const float by = world_model_get_ball_y_mm();
                if (world_model_goal_opp_visible()) {
                    const float goal_angle = world_model_get_goal_opp_angle_deg();
                    const bool aligned = ball_is_in_attack_line(bx, by, goal_angle,
                                                                ATK_ATTACK_LINE_TOL_DEG);
                    transition_atk(aligned ? AtkState::APPROACH : AtkState::POSITION);
                } else {
                    transition_atk(AtkState::APPROACH);
                }
            }
            return cmd;
        }

        case AtkState::POSITION: {
            g_state_name = "ATK_POSITION";
            // Behind-the-ball: ir al target que queda DETRÁS de la pelota
            // mirando al arco. Cuando llego al target Y estoy mirando al arco,
            // transitions a APPROACH para empujar/patear.
            if (!world_model_ball_visible()) {
                transition_atk(AtkState::SEARCH);
                return cmd;
            }
            if (!world_model_goal_opp_visible()) {
                // Perdí el arco: degradar a APPROACH directo (Nivel 1).
                transition_atk(AtkState::APPROACH);
                return cmd;
            }

            const float bx = world_model_get_ball_x_mm();
            const float by = world_model_get_ball_y_mm();
            const float goal_angle = world_model_get_goal_opp_angle_deg();

            const BehindBallTarget tgt = compute_behind_ball_target(
                bx, by, goal_angle, ATK_BEHIND_BALL_GAP_MM);

            // Heading: siempre mirar al arco rival.
            heading_pid_set_target(g_heading_pid, heading + goal_angle);
            const float omega = heading_pid_tick(g_heading_pid, heading, now_ms);

            // Vector al target relativo al robot. Velocidad proporcional con
            // perfil suave (mismo approach_velocity reusado).
            const float tx = tgt.target_x_mm;
            const float ty = tgt.target_y_mm;
            const float tdist = std::sqrt(tx * tx + ty * ty);
            const float speed = approach_velocity(tdist,
                                                  ATK_POSITION_REACHED_MM,
                                                  ATK_APPROACH_FAR_MM,
                                                  ATK_POSITION_MAX_SPEED,
                                                  ATK_APPROACH_MIN_SPEED);
            if (tdist > 1.0f) {
                cmd.vx_mm_s = static_cast<int16_t>(tx / tdist * speed);
                cmd.vy_mm_s = static_cast<int16_t>(ty / tdist * speed);
            }
            cmd.omega_centideg_s = omega_degps_to_centideg(omega);  // satura int16 (anti sign-flip, #9)

            // Transición a APPROACH: llegué al target Y estoy alineado a la
            // línea pelota–arco.
            const bool reached = (tdist < ATK_POSITION_REACHED_MM);
            const bool aligned = ball_is_in_attack_line(bx, by, goal_angle,
                                                         ATK_ATTACK_LINE_TOL_DEG);
            if (reached && aligned) {
                transition_atk(AtkState::APPROACH);
            }
            return cmd;
        }

        case AtkState::APPROACH: {
            g_state_name = "ATK_APPROACH";
            const float bx = world_model_get_ball_x_mm();
            const float by = world_model_get_ball_y_mm();
            const float dist = std::sqrt(bx * bx + by * by);

            if (!world_model_ball_visible() || dist < 1.0f) {
                transition_atk(AtkState::SEARCH);
                return cmd;
            }

            // Si la pelota dejó de estar alineada con el arco (la perdimos al
            // costado), volver a POSITION para rodearla otra vez.
            if (world_model_goal_opp_visible()) {
                const float goal_angle = world_model_get_goal_opp_angle_deg();
                if (!ball_is_in_attack_line(bx, by, goal_angle,
                                             ATK_ATTACK_LINE_TOL_DEG + 10.0f)) {
                    // Histéresis +10° para no oscilar entre APPROACH↔POSITION.
                    transition_atk(AtkState::POSITION);
                    return cmd;
                }
                // Geometría de alineación para empuje (sin kicker físico): aquí
                // el robot ya está cerca y apuntando al arco. No dispara nada —
                // sigue avanzando hacia la pelota (abajo) para empujarla por
                // inercia. Conservamos el chequeo para documentar el punto de
                // empuje alineado por si en el futuro se cuelga una conducta.
                (void)is_aligned_to_push(bx, by, goal_angle,
                                          ATK_KICK_DIST_MM, ATK_KICK_ANGLE_DEG);
            }

            // Heading target: orientar el frente hacia la pelota.
            // ball_x relativo al robot: 0 = frente. Convertir a absoluto.
            const float ball_angle_rel = std::atan2(bx, by) * (180.0f / M_PI);
            const float ball_angle_abs = heading + ball_angle_rel;
            heading_pid_set_target(g_heading_pid, ball_angle_abs);
            const float omega = heading_pid_tick(g_heading_pid, heading, now_ms);

            // Velocidad de avance con perfil suave.
            const float speed = approach_velocity(dist,
                                                  ATK_APPROACH_CLOSE_MM,
                                                  ATK_APPROACH_FAR_MM,
                                                  ATK_APPROACH_MAX_SPEED,
                                                  ATK_APPROACH_MIN_SPEED);

            if (dist > 1.0f) {
                cmd.vx_mm_s = static_cast<int16_t>(bx / dist * speed);
                cmd.vy_mm_s = static_cast<int16_t>(by / dist * speed);
            }
            cmd.omega_centideg_s = omega_degps_to_centideg(omega);  // satura int16 (anti sign-flip, #9)

            // Refinamiento WP-2A: cancelar la deriva lateral (eje +X) medida por
            // OTOS para que el empuje/pateo salga DERECHO. Aditivo y opcional —
            // NO toca el heading (lo maneja el HeadingPID hacia la pelota) ni el
            // avance hacia la pelota; solo suma una corrección lateral si el OTOS
            // está fresco. Si no, el comando queda EXACTAMENTE como antes.
            if (world_model_otos_is_fresh()) {
                DriveStraightIn ds_in;
                ds_in.target_heading_deg = 0.0f;   // omega lo controla el HeadingPID
                ds_in.cur_heading_deg    = 0.0f;   // -> error 0 -> ds.omega = 0 (no se usa)
                // #21: solo cancelar deriva si la VEL del OTOS está fresca (frame 0x12 propio).
                // Si la vel se perdió (CRC) aunque la pose siga fresca, usar 0 (no corregir con dato viejo).
                ds_in.otos_vy_mm_s       = world_model_otos_vel_is_fresh()
                                               ? world_model_get_otos_vx_mm_s()   // lateral robot = +X
                                               : 0.0f;
                ds_in.fwd_speed_mm_s     = 0.0f;
                const DriveStraightCfg ds_cfg{DS_KP_HEADING, DS_KP_LATERAL};
                const DriveStraightCmd ds = drive_straight_compute(ds_in, ds_cfg);
                // ds.vy_mm_s = corrección lateral; se suma al eje +X del robot.
                cmd.vx_mm_s = static_cast<int16_t>(static_cast<float>(cmd.vx_mm_s) + ds.vy_mm_s);
            }
            return cmd;
        }
    }

    return cmd;
}

// === FSM Arquero ===

MotorCommand goalkeeper_tick() {
    MotorCommand cmd{};
    const uint32_t now_ms = millis();

    if (!world_model_match_running()) {
        transition_gk(GkState::WAIT_START);
    } else if (world_model_imminent_exit() && line_data_fresh()) {
        transition_gk(GkState::LINE_AVOID);
    }

    switch (g_gk_state) {
        case GkState::WAIT_START: {
            g_state_name = "GK_WAIT_START";
            if (world_model_match_running()) {
                transition_gk(GkState::PATROL);
            }
            return cmd;
        }

        case GkState::LINE_AVOID: {
            g_state_name = "GK_LINE_AVOID";
            const float line_angle = world_model_get_line_angle_deg();
            const float retreat = line_angle + 180.0f;
            const float rad = retreat * (M_PI / 180.0f);
            cmd.vx_mm_s = static_cast<int16_t>(std::sin(rad) * GK_LINE_RETREAT_SPEED);
            cmd.vy_mm_s = static_cast<int16_t>(std::cos(rad) * GK_LINE_RETREAT_SPEED);
            if (!world_model_imminent_exit()) {
                transition_gk(GkState::PATROL);
            }
            return cmd;
        }

        case GkState::PATROL: {
            g_state_name = "GK_PATROL";
            // PID lateral: WP-3-GK lo lleva por cross_track (distancia
            // perpendicular a la línea) para que el arquero se mantenga PARALELO
            // a la línea lateral; fallback EXACTO a profundidad si cross_track es
            // N/A (ver gk_lateral_pid_output).
            const float vx_lateral_pid = gk_lateral_pid_output(now_ms);

            // Oscilación lateral encima del PID — patrulla el área chica.
            static int direction = 1;
            static uint32_t last_change = 0;
            if (now_ms - last_change > static_cast<uint32_t>(GK_PATROL_OSCILLATE_PERIOD_MS)) {
                direction = -direction;
                last_change = now_ms;
            }
            const float vx_patrol = direction * GK_PATROL_SPEED_MM_S;
            cmd.vx_mm_s = static_cast<int16_t>(vx_patrol + vx_lateral_pid * 0.5f);

            if (world_model_ball_visible()) {
                transition_gk(GkState::INTERCEPT);
            }
            return cmd;
        }

        case GkState::INTERCEPT: {
            g_state_name = "GK_INTERCEPT";
            const float bx = world_model_get_ball_x_mm();
            const float by = world_model_get_ball_y_mm();
            const float dist = std::sqrt(bx * bx + by * by);

            // ANTICIPACIÓN: en vez de seguir la X ACTUAL de la pelota, apuntar a
            // la X PREDICHA dentro de lookahead_s (ball_predict). Con velocidad
            // N/A o pelota quieta (vx=vy=0) el lead es 0 → px=bx = conducta
            // IDÉNTICA a hoy (fallback automático, sin gating extra).
            // ⚠️ A TUNEAR EN BANCO: lookahead_s / max_lead_mm.
            const float bx_pred = ball_predict(world_model_get_ball_x_mm(),
                                               world_model_get_ball_y_mm(),
                                               world_model_get_ball_vx_mm_s(),
                                               world_model_get_ball_vy_mm_s(),
                                               ball_predict_default_params()).px_mm;

            // PID lateral por cross_track (paralelo a la línea), con fallback
            // EXACTO a profundidad si es N/A — mismo helper que PATROL.
            const float vx_lateral_pid = gk_lateral_pid_output(now_ms);

            const float vx_intercept = bx_pred * GK_INTERCEPT_KP_VS_BALL_X;
            cmd.vx_mm_s = static_cast<int16_t>(vx_intercept + vx_lateral_pid * 0.3f);

            // Transición a CLEAR: la pelota llegó cerca → salir a despejar
            // en lugar de seguir solo el eje X.
            if (dist < GK_CLEAR_TRIGGER_MM) {
                transition_gk(GkState::CLEAR);
            } else if (!world_model_ball_visible()) {
                transition_gk(GkState::PATROL);
            }
            return cmd;
        }

        case GkState::CLEAR: {
            g_state_name = "GK_CLEAR";
            // Despeje: ir DERECHO a la pelota a velocidad alta (no PID lateral
            // — ya no estamos defendiendo el arco, estamos atacando la pelota).
            // El arquero NO tiene kicker en ROBOT1 → empuja la pelota fuera
            // por inercia mecánica.
            if (!world_model_ball_visible()) {
                transition_gk(GkState::PATROL);
                return cmd;
            }
            const float bx = world_model_get_ball_x_mm();
            const float by = world_model_get_ball_y_mm();
            const float dist = std::sqrt(bx * bx + by * by);

            if (dist > GK_CLEAR_RELEASE_MM) {
                // Histéresis: si la pelota se alejó, volver a defender.
                transition_gk(GkState::INTERCEPT);
                return cmd;
            }

            if (dist > 1.0f) {
                cmd.vx_mm_s = static_cast<int16_t>(bx / dist * GK_CLEAR_SPEED_MM_S);
                cmd.vy_mm_s = static_cast<int16_t>(by / dist * GK_CLEAR_SPEED_MM_S);
            }
            // Heading hacia la pelota para que el despeje sea con el frente.
            const float ball_angle_rel = std::atan2(bx, by) * (180.0f / M_PI);
            heading_pid_set_target(g_heading_pid,
                                   world_model_get_my_heading_deg() + ball_angle_rel);
            const float omega = heading_pid_tick(g_heading_pid,
                                                  world_model_get_my_heading_deg(),
                                                  now_ms);
            cmd.omega_centideg_s = omega_degps_to_centideg(omega);  // satura int16 (anti sign-flip, #9)
            return cmd;
        }
    }

    return cmd;
}

}  // namespace

void strategy_init() {
    g_state_name = "INIT";
    g_atk_state = AtkState::WAIT_START;
    g_gk_state = GkState::WAIT_START;
    g_match_was_running = false;
    g_kickoff_started_ms = 0;
    heading_pid_reset(g_heading_pid);
    lateral_pid_reset(g_lateral_pid_gk);
}

MotorCommand strategy_tick() {
    return (g_role == RobotRole::ATTACKER) ? attacker_tick() : goalkeeper_tick();
}

void        strategy_set_role(RobotRole role)         { g_role = role; }
RobotRole   strategy_get_role()                       { return g_role; }
void        strategy_set_attack_color(AttackColor c)  { g_attack_color = c; }
AttackColor strategy_get_attack_color()               { return g_attack_color; }
const char* strategy_get_state_name()                 { return g_state_name; }

}  // namespace iitasoccer
