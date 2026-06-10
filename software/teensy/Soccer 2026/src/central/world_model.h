// world_model.h (CENTRAL) — Espejo del WorldSnapshot recibido del TOP.
//
// CENTRAL no fusiona sensores — recibe el WorldSnapshot armado por TOP y lo
// expone como estado del mundo para que strategy lo consulte. Si CENTRAL deja
// de recibir snapshots (TOP cuelga), `world_model_snapshot_is_fresh()` pasa a
// false (>500 ms sin snapshot) y main_central frena los motores (modo seguro).

#pragma once
#include <stdint.h>
#include "types.h"

namespace iitasoccer {

void world_model_init();

// Actualiza el world model con el último snapshot recibido del TOP.
// Llamar desde comm_top cuando llega un WORLD_SNAPSHOT.
void world_model_apply_snapshot(const WorldSnapshot& snap);

// Actualiza el mini-mundo de la línea con el último LINE_URGENT recibido de DOWN.
// Recibe el contrato v2 (LineStatusV2, 16 bytes). Ver docs/firmware/CONTRATO-DATOS-DOWN.md.
void world_model_apply_line(const LineStatusV2& line);

// Fresheness check — si no se recibe snapshot en N ms, datos son stale.
bool world_model_snapshot_is_fresh();
bool world_model_line_is_fresh();

// === Acceso a pose y entidades ===
// #16 — OJO: varios de estos accessors EXISTEN pero la FSM (strategy.cpp) NO los consume
// hoy (son superficie de contrato para Nivel 2/3 futuro): get_my_x/y, min_obstacle,
// goal_opp_distance, in_own_penalty_area, partner_alive/sees_ball, get_otos_x/y, otos_omega,
// otos_slip, otos_pose_confidence. Que tengan accessor NO implica que el robot los use
// (p.ej. NO hay evasión de obstáculos aunque min_obstacle esté expuesto). Antes de asumir
// una conducta, verificar con grep en strategy.cpp. Ver auditoría 2026-06-03 #16.
float world_model_get_my_x_mm();
float world_model_get_my_y_mm();
float world_model_get_my_heading_deg();
// Confianza (0-100) de la pose del snapshot (trilateración del TOP). Para gatear el
// uso de my_x/my_y (límites de patrulla del arquero por pose, diseño Gustavo 2026-06-09).
uint8_t world_model_get_my_pose_confidence();

bool  world_model_ball_visible();
float world_model_get_ball_x_mm();
float world_model_get_ball_y_mm();

// "Vi la pelota hace menos de window_ms" — ventana de gracia (origen: Delantero
// 2025). Devuelve true si llegó un snapshot con ball_visible dentro de los
// últimos window_ms (resta unsigned wrap-safe, mismo patrón que los _is_fresh
// de arriba). Sirve para no saltar a SEARCH al primer frame sin detección
// cuando la pelota titila entre frames.
//
// #16: ADITIVO y DORMIDO — ningún caller en strategy.cpp lo usa todavía, así que
// NO cambia el binario de competencia. world_model_ball_visible() queda EXACTO
// (instantáneo, sin ventana); este accessor es la verdad EXTENDIDA en el tiempo.
// strategy.cpp lo consumirá en una etapa futura (gated). La ventana (300-500 ms)
// se tunea en banco. Si nunca se vio la pelota (g_last_ball_seen_ms==0) → false.
bool world_model_ball_recently_seen(uint16_t window_ms);

// Velocidad de la pelota (mm/s, marco robot) del WorldSnapshot. 0 si N/A o quieta.
// Lo usa el arquero (strategy GK_INTERCEPT) para anticipar vía ball_predict.
int16_t world_model_get_ball_vx_mm_s();
int16_t world_model_get_ball_vy_mm_s();

bool  world_model_goal_opp_visible();
float world_model_get_goal_opp_angle_deg();
float world_model_get_goal_opp_distance_mm();

// Arco PROPIO (schema v3). Espejo passthrough del WorldSnapshot del TOP.
// Sentinela: MISMO criterio que goal_opp → si world_model_goal_own_visible()==false,
// angle/distance NO son válidos (no usarlos). El arquero los consume SOLO bajo esa
// compuerta (ver strategy.cpp::goalkeeper_tick / gk_own_goal_orient). Cuando no es
// visible la conducta del arquero es IDÉNTICA a la previa al schema v3.
bool  world_model_goal_own_visible();
float world_model_get_goal_own_angle_deg();       // centideg/100 (grados); válido sólo si goal_own_visible
float world_model_get_goal_own_distance_mm();     // mm; válido sólo si goal_own_visible

// heading_valid (flags bit 4, máscara 0x10): 1 = heading del BNO válido. Al boot
// (BNO sin converger) el TOP lo pone en 0 → strategy NO debe orientar con ese
// heading=0 falso. Es la compuerta de central_gate_heading_omega (abajo). Caso
// normal (heading_valid=1) → conducta IDÉNTICA a hoy.
bool  world_model_heading_valid();

uint16_t world_model_get_min_obstacle_mm();

// === Línea (de DOWN bus emergencia) ===
bool     world_model_line_detected();
float    world_model_get_line_angle_deg();
uint8_t  world_model_get_line_depth();
bool     world_model_imminent_exit();
bool     world_model_line_data_valid();   // data_valid del último frame (compuerta maestra)
uint8_t  world_model_line_event_flags();  // EV_* del último frame (diagnóstico / observabilidad)

// Distancia perpendicular FIRMADA del centro del robot a la recta de la línea,
// en mm (WP-3-DOWN / Capa 3; computada en down_model con geometría real).
// Convención de signo (ver down_model.cpp): + = línea ADELANTE del centro (+Y),
// - = ATRÁS (-Y). El arquero la usa como señal de error del PID lateral para
// mantener DISTANCIA PERPENDICULAR fija y desplazarse PARALELO a la línea
// lateral (strafe). Devuelve 0.0f cuando no es confiable (ver más abajo) —
// usar SIEMPRE junto con world_model_cross_track_valid() para distinguir
// "centrado sobre la línea" (0 mm válido) de "sin señal" (N/A).
float    world_model_get_cross_track_mm();

// ¿El cross_track del último frame es una medición real y usable?
// true sólo si data_valid==1 Y el campo no es el centinela N/A (LSV2_NA_I16).
// Es la compuerta que usa el arquero (strategy goalkeeper_tick) para elegir
// entre el control por cross_track (strafe paralelo, Capa 3) y el fallback
// por profundidad (world_model_get_line_depth). NO confundir con line_fresh:
// esto es validez del CAMPO dentro del frame; la frescura del frame es aparte.
bool     world_model_cross_track_valid();

// === Estado táctico (de WorldSnapshot.flags) ===
bool world_model_match_running();
// Override de arranque manual (F3, fail-safe de banco). Cuando es true,
// world_model_match_running() devuelve true aunque no haya START de COMM.
// Solo lo activa main_central bajo -DCENTRAL_ENABLE_MANUAL_START.
void world_model_set_force_match_running(bool on);
bool world_model_in_own_penalty_area();
bool world_model_partner_alive();
bool world_model_partner_sees_ball();

// #19: referee_cmd — RESERVADO / NO consumido por la FSM (0 callers en strategy.cpp).
// El contrato REAL del árbitro de este equipo es BINARIO: START/STOP llega como NIVEL
// GPIO (pines 5/6 del TOP) → flag bit3 MATCH_RUNNING del snapshot → world_model_match_running().
// halftime(2)/reset(3) NO se transmiten por este COMM. No construir lógica sobre este campo
// sin antes cablear su emisión en el TOP. Ver auditoría 2026-06-03 #19 + CONTRATO-DATOS-CENTRAL.
uint8_t world_model_referee_cmd();

// === OTOS directo de DOWN (Capa 1 broadcast) ===
// La pose de cancha AUTORITATIVA sigue siendo la del WorldSnapshot del TOP
// (world_model_get_my_*). El OTOS directo es SOLO para control de movimiento
// (drive-straight / patear derecho), que se cablea en Capa 2.
void world_model_apply_otos_pose(const Pose2D& pose);
void world_model_apply_otos_vel(const Velocity2D& vel);
bool world_model_otos_is_fresh();      // frescura de la POSE (0x11)
bool world_model_otos_vel_is_fresh();  // #21: frescura de la VEL (0x12), por separado de la pose

float   world_model_get_otos_x_mm();
float   world_model_get_otos_y_mm();
float   world_model_get_otos_heading_deg();
float   world_model_get_otos_vx_mm_s();
float   world_model_get_otos_vy_mm_s();
float   world_model_get_otos_omega_deg_s();
uint8_t world_model_get_otos_slip();
uint8_t world_model_otos_pose_confidence();

// ============================================================================
// Decisiones PURAS del consumo schema v3 (sin Arduino → host-testeables).
//
// Viven en el header (no en el .cpp, que depende de Arduino) para que el test
// host (test_snapshot_v3_consume) las incluya por ruta relativa, igual que
// test_link_health hace con comm_arbiter.h. run-host-tests.sh sólo agrega
// -I src/shared + el dir del test (NO -I src/central); este header sólo incluye
// <stdint.h> + types.h, así que compila host. NO agregar acá includes de
// Arduino ni de line_view.h.
// ============================================================================

// (1) heading_valid → usar/ignorar el heading para orientar ESTE tick.
// strategy computa una ω candidata con el HeadingPID hacia un setpoint de rumbo;
// esta compuerta decide si esa ω se aplica. Si heading_valid==false (ej. BNO sin
// converger al boot → heading=0 falso), devuelve 0 (NO girar con un rumbo
// inventado). Si heading_valid==true → devuelve omega_if_valid sin tocarlo →
// conducta IDÉNTICA a hoy. Es la ÚNICA regla; no hace nada más.
inline float central_gate_heading_omega(bool heading_valid, float omega_if_valid) {
    return heading_valid ? omega_if_valid : 0.0f;
}

// (2) goal_own → ajuste de orientación del ARQUERO respecto al arco PROPIO.
//
// Cuando el arco propio es visible (y el heading es válido), el arquero puede
// orientar el frente hacia la CANCHA (de espaldas a su arco) usando el ángulo al
// arco propio: el arco propio está DETRÁS, así que mirar al campo =
// my_heading + (goal_own_angle + 180), normalizado. Esto le da una referencia de
// rumbo estable en PATROL/INTERCEPT (estados que hoy NO setean rumbo: ω=0).
//
// FALLBACK EXACTO (no-regresión): si goal_own NO es visible O heading_valid==false,
// set_heading=false → el caller NO setea setpoint de rumbo ese tick → ω queda 0,
// BYTE-IDÉNTICO a la conducta previa al schema v3 (PATROL/INTERCEPT no orientaban).
// La mejora SÓLO se activa con dato nuevo presente y heading válido.
struct GkOwnGoalOrient {
    bool  set_heading;          // false => no setear rumbo este tick (fallback exacto)
    float heading_target_deg;   // rumbo absoluto objetivo (sólo si set_heading)
};

inline GkOwnGoalOrient gk_own_goal_orient(bool goal_own_visible, bool heading_valid,
                                          float my_heading_deg, float goal_own_angle_deg) {
    GkOwnGoalOrient r{};
    if (!goal_own_visible || !heading_valid) {
        r.set_heading = false;          // fallback EXACTO: sin rumbo nuevo
        r.heading_target_deg = my_heading_deg;
        return r;
    }
    // Frente a la cancha = opuesto al arco propio (que está detrás).
    float target = my_heading_deg + goal_own_angle_deg + 180.0f;
    while (target >  180.0f) target -= 360.0f;
    while (target <= -180.0f) target += 360.0f;
    r.set_heading = true;
    r.heading_target_deg = target;
    return r;
}

}  // namespace iitasoccer
