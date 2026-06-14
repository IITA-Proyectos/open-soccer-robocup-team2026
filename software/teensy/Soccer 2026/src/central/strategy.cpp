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
#include "ball_trajectory.h"
#include "atk_nogyro.h"     // helpers puros del delantero sin gyro (práctica R1)
#include "pfm_heading.h"    // control PI+PFM de rumbo p/ zona muerta (banco María)

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
    WAIT_START, KICKOFF, SEARCH, POSITION, APPROACH, LINE_AVOID,
    // v2 delantero (2026-06-11, spec Gustavo = movimiento validado del robot 2025):
    // PUSH = empuje comprometido full-adelante por tiempo fijo (el "pateo" 2025);
    // PUSH_BACK = retroceso corto post-empuje (despegarse de pelota/línea, 2025).
    PUSH, PUSH_BACK
};
enum class GkState : uint8_t {
    // GOTO_LINE (índice 1, espejo de GkPhase en strategy_transitions.h): al recibir
    // START, el arquero va en DIAGONAL atrás-y-a-la-derecha (un solo movimiento) hasta
    // que los sensores de piso detectan la línea del arco; recién ahí pasa a PATROL.
    WAIT_START, GOTO_LINE, PATROL, INTERCEPT, CLEAR, LINE_AVOID
};

AtkState g_atk_state = AtkState::WAIT_START;
GkState  g_gk_state  = GkState::WAIT_START;

// Estado auxiliar del kickoff (timer) — válido solo cuando g_atk_state == KICKOFF.
uint32_t g_kickoff_started_ms = 0;
bool     g_match_was_running = false;   // detecta flanco "STOP → RUN" para KICKOFF
// Timer del empuje (PUSH/PUSH_BACK) — se setea al entrar a cada fase.
uint32_t g_atk_push_started_ms = 0;
#ifdef ATK_OTOS_NOGYRO
// Empuje sin gyro: dirección congelada (vector unitario a la pelota al entrar a
// PUSH) + yaw del OTOS capturado en ese instante (setpoint del rumbo sostenido).
NoGyroPushDir g_atk_push_dir  = {false, 0.0f, 1.0f};
float         g_atk_push_hdg0 = 0.0f;
#endif

// Timer de GOTO_LINE (arquero) — guardado al entrar; para el timeout de seguridad.
uint32_t g_goto_line_started_ms = 0;

// Anti-flapping PATROL<->LINE_AVOID (banco 2026-06-09): el arquero PISA su línea por
// diseño → imminent_exit no puede disparar la huida al primer tick.
uint32_t g_gk_imminent_since_ms  = 0;  // desde cuándo persiste imminent_exit (0 = no activo)
uint32_t g_gk_line_avoid_gate_ms = 0;  // último exit de LINE_AVOID / entrada a PATROL (cooldown)

// GOTO_LINE en 2 fases (diseño Gustavo 2026-06-09 v2): 0 = RETROCESO (recto atrás con
// gyro hasta tocar la línea) · 1 = AVANCE (despegarse hasta DEJAR DE VER la línea +
// margen, con timeout) y recién ahí PATROL.
uint8_t  g_goto_line_phase       = 0;
uint32_t g_gk_advance_started_ms = 0;
uint32_t g_gk_advance_clear_ms   = 0;  // desde cuándo la línea dejó de verse (0 = aún se ve)
uint32_t g_gk_start_seen_ms      = 0;  // 1er tick con match=GO (para el delay de arranque)

// Centro de patrulla AUTO-CAPTURADO (banco 2026-06-09: el centro fijo 910 no coincidía
// con la x real del puesto en la cancha → la ventana quedaba siempre a un costado y
// cada tramo elegía LA MISMA dirección, "dos veces al mismo lado + giro"). Ahora el
// centro = la x del robot al ENTRAR a patrullar (recién posicionado contra su línea,
// frente a su arco) → la ventana queda centrada en el puesto REAL, sin calibración.
float g_gk_patrol_x_center = -1.0f;    // <0 = sin capturar → usa GK_PATROL_X_CENTER_MM

// === PIDs ===
HeadingPID g_heading_pid;
LateralPID g_lateral_pid_gk;

// === Tuning constants ===

// Delantero — Nivel 1
#ifdef ATK_SEARCH_SPIN_ONLY
// Práctica R1 sin gyro (2026-06-12, spec Gustavo: "busque la pelota GIRANDO"):
// buscar rotando EN EL LUGAR, sin avanzar — sin lazo de rumbo el avance+giro
// derivaría hacia las paredes sin control.
constexpr float ATK_SEARCH_VY_MM_S       = 0.0f;
#else
constexpr float ATK_SEARCH_VY_MM_S       = 200.0f;
#endif
constexpr float ATK_SEARCH_OMEGA_DEG_S   = 60.0f;
// v2 (2026-06-11, spec Gustavo): acercarse LENTO (movimiento 2025) — la potencia
// va en el EMPUJE comprometido (PUSH), no en el acercamiento. 600→400.
constexpr float ATK_APPROACH_MAX_SPEED   = 400.0f;
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
constexpr float ATK_POSITION_MAX_SPEED         = 400.0f;   // v2: orbit sereno (era 500)
// EMPUJE comprometido (v2 2026-06-11 = "PATEANDO" del robot 2025, que pateaba con
// rampa a ~94% PWM 500 ms y retrocedía 200 ms): cerca + alineado al eje de ataque
// → full adelante por tiempo FIJO sin re-mirar la pelota (a esa distancia las
// cámaras la pierden contra el paragolpes; el 2025 también pateaba "a ciegas"),
// después retroceso corto para despegarse de la pelota/línea del área.
constexpr float    ATK_PUSH_SPEED_MM_S      = 700.0f;
constexpr uint32_t ATK_PUSH_MS              = 500;
constexpr float    ATK_PUSH_BACK_SPEED_MM_S = 300.0f;
constexpr uint32_t ATK_PUSH_BACK_MS         = 250;
constexpr float ATK_KICKOFF_SPEED_MM_S         = 500.0f;
constexpr uint32_t ATK_KICKOFF_DURATION_MS     = 250;      // boost inicial al frente al arrancar match

#ifdef ATK_OTOS_NOGYRO
// ── DELANTERO SIN GYRO (práctica R1 2026-06-12; env central_robot1_delantero_practica*) ──
// R1 corre sin BNO (desconectados tras el recableado) pero con 2 OTOS en DOWN.
// Tres reemplazos (helpers puros en atk_nogyro.h, host-testeados):
//   • eje de ataque: fallback extra por yaw del OTOS (0 = boot = arco rival);
//   • gatillo de empuje: GEOMÉTRICO (pelota sobre el eje), no "frente apuntando
//     al arco" (sin lazo fino de rumbo eso jamás se cumple y PUSH nunca dispara);
//   • empuje: dirección CONGELADA al vector a la pelota + rumbo sostenido por
//     el yaw del OTOS (P con clamp y bail-out).
// 🔧 TUNEAR EN BANCO (práctica):
constexpr float ATK_NOGYRO_PUSH_DIST_MM    = 150.0f; // gatillo más lejos que los 80 con gyro:
                                                     // acá la dirección la pone el VECTOR a la
                                                     // pelota (no hace falta tenerla en el paragolpes)
constexpr float ATK_NOGYRO_PUSH_TOL_DEG    = 15.0f;  // pelota dentro de ±esto del eje de ataque
constexpr float ATK_NOGYRO_OMEGA_MAX_DEGPS = 40.0f;  // mismo tope anti-bang-bang del arquero
constexpr float ATK_NOGYRO_BAILOUT_DEG     = 45.0f;  // error mayor → no corregir (¿signo invertido?)
#endif

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
// ⚠️ VELOCIDADES A RÉGIMEN FIEL (revisión matemática 2026-06-09): con los pisos de PWM
// {70,70,107}, todo comando < ~420 mm/s queda DEBAJO del piso y la dirección sale
// APLASTADA (la proporción entre ruedas se destruye; PATROL a 150 daba 1:1:1.5 en vez
// de 0.5:0.5:1.0 = errático). 420 mm/s = el PWM crudo de la rueda dominante (trasera,
// piso 107) queda SOBRE su piso → el mixer respeta la dirección pedida.
constexpr float GK_PATROL_OSCILLATE_PERIOD_MS = 2000.0f;
// 200 = RITMO DE OBSERVACIÓN (banco 2026-06-09: a 420 "salía volando a los costados").
// Con la patrulla ahora siendo strafe PURO (vy=0), comandar bajo es SEGURO: los pisos
// la clavan en la mezcla {70,70,107} — exactamente la del diag de strafe que ya se vio
// andar derecha y moderada. 🔧 Para competencia se puede subir (420 = fiel pleno).
constexpr float GK_PATROL_SPEED_MM_S          = 200.0f;
constexpr float GK_INTERCEPT_KP_VS_BALL_X     = 4.0f;
constexpr float GK_LATERAL_SETPOINT_DEPTH     = 1.0f;
constexpr float GK_LINE_RETREAT_SPEED         = 420.0f;   // era 250 (bajo piso → dirección basura)

// GOTO_LINE: reposicionamiento inicial. Al recibir START, el arquero va HACIA ATRÁS
// (-Y, hacia su arco) hasta que los sensores de piso detectan la línea.
// ⚠️ BANCO robot2 2026-06-09 (Gustavo): la versión DIAGONAL (vx=180,vy=-180) daba
// CÍRCULOS amplios — con los pisos de PWM {70,70,107} las velocidades dispares del
// diagonal (ruedas a -66/+246/-180 mm/s → PWM crudo 17/63/46) se APLASTAN al piso y
// la dirección resultante es basura. RECTO ATRÁS es simétrico (M1/M2 ±iguales, M3=0)
// → los pisos lo respetan → sale derecho. Orden de Gustavo: recto atrás. VX=0.
// DELAY DE ARRANQUE (banco 2026-06-09, pedido de Gustavo: "no me da el tiempo para
// acomodarlo"): tras el START del árbitro el arquero espera esto antes de moverse —
// da tiempo a posicionarlo/soltarlo. 🔧 Para COMPETENCIA bajar a 0 (debe salir ya).
constexpr uint32_t GK_START_DELAY_MS      = 2000;

constexpr int16_t GK_GOTO_LINE_VX_RIGHT   = 0;     // recto atrás (era 180 diagonal → círculos)
// 420 mm/s: PWM crudo delanteras ~93 > piso 70 → fiel + rápido (era 180: "MUY LENTO").
// El recto-atrás es inestable por geometría (trasera=0, cualquier asimetría guiña) →
// se compensa con GYRO HOLD (diseño de Gustavo 2026-06-09): ω = PID al rumbo 0° del boot.
constexpr int16_t GK_GOTO_LINE_VY_BACK    = 420;   // hacia atrás → se aplica como -Y
constexpr uint32_t GK_GOTO_LINE_TIMEOUT_MS = 4000; // safety: si no encuentra la línea → PATROL

// ── TRIMS del retroceso (banco 2026-06-09, 3ª pasada: "deriva suave a la DERECHA") ──
// La deriva yendo atrás tiene DOS componentes y cada una tiene su perilla:
// (a) TRASLACIONAL (desliza de costado MIRANDO al frente — el gyro NO la ve; viene de
//     asimetría de fuerza M1 vs M2): se compensa con un empuje lateral constante.
//     ⚠️ TOPE FÍSICO ±19 mm/s: más, y la componente que le toca a la TRASERA supera el
//     umbral de ruido (5 PWM) → el piso la dispara a 107 = patada lateral. Dentro de
//     ±19, el trim viaja por las delanteras (que van a ~93 PWM, sobre su piso) y la
//     trasera queda en silencio.
// (b) DE RUMBO (rota despacio y el PID, topeado a 10°/s, corrige con atraso): se
//     compensa sesgando el RUMBO objetivo unos grados al lado contrario.
// 🔧 CALIBRAR EN BANCO (deriva A LA DERECHA → ambos trims hacia la IZQUIERDA):
//     1º subí/bajá VX_TRIM de a 5 (−15 → −19 → −10…) mirando el camino;
//     2º si todavía termina chueco DE RUMBO, tocá HEADING_TRIM de a 1°.
//     Deriva a la IZQUIERDA → signos opuestos. 0 = sin compensación.
constexpr float GK_GOTO_LINE_VX_TRIM_MM_S    = 0.0f;    // − = empuje a la IZQ (máx ±19).
                                                        // ⚠️ banco 2026-06-09: VOLVIÓ a 0 para
                                                        // aislar variables (con -15 salió una J —
                                                        // sospecha = signo del heading / target de
                                                        // cámara, no el trim; re-tunear DESPUÉS de
                                                        // confirmar el signo del BNO de robot2).
constexpr float GK_GOTO_LINE_HEADING_TRIM_DEG = 0.0f;   // + = sesgar rumbo a la IZQ (CCW)

// AVANCE POST-LÍNEA (diseño de Gustavo, banco 2026-06-09 v2): al tocar la línea el
// arquero NO se queda sobre ella (quedarse pegado hacía que el PID de borde saturara
// → giraba sobre su eje a gran velocidad). En cambio AVANZA ~10 cm para despegarse y
// patrulla DELANTE de la línea. Es también la respuesta a CUALQUIER toque de línea
// durante la patrulla: la línea del arquero SIEMPRE está atrás → adelante siempre es
// seguro (reemplaza al retreat por ángulo de LINE_AVOID, que con los pisos salía en
// direcciones impredecibles).
constexpr int16_t  GK_ADVANCE_SPEED_MM_S = 300;   // suave para observar (los pisos lo
                                                  // dejan en ~70 PWM delanteras = recto)
// Banco 2026-06-09 (2ª iteración): el avance fijo de 350 ms a veces NO despegaba del
// todo (motores bajo carga rinden menos) → loop avance↔toque. Ahora avanza HASTA QUE
// LA LÍNEA DEJE DE VERSE y un margen extra; el timeout es la red de seguridad.
// v3.3 (banco 2026-06-10): margen 350→100 ms. Con 350 (~10 cm) la patrulla quedaba
// tan adelante que el anillo JAMÁS veía la línea → el rebote lateral no podía
// disparar y "no se guiaba de la línea" (Gustavo). Con ~3 cm la línea queda al
// borde trasero del anillo: visible de guía, sin pisarla de lleno.
constexpr uint32_t GK_ADVANCE_MS         = 100;   // margen EXTRA tras dejar de ver línea
constexpr uint32_t GK_ADVANCE_TIMEOUT_MS = 1500;  // tope duro de la fase de avance
// Re-ENGANCHE de línea en patrulla (v3.3): si en una parada la línea no está a la
// vista (el strafe derivó adelante), retroceder despacio hasta re-verla (la línea
// es LA referencia de la patrulla — sin verla, los rebotes no funcionan).
constexpr int16_t  GK_PATROL_REACQ_VY_MM_S = 200; // retroceso suave de re-enganche
constexpr uint32_t GK_PATROL_REACQ_MAX_MS  = 700; // tope (no insistir si no aparece)

// LÍMITES DE PATRULLA POR POSE DEL TOP (diseño de Gustavo — cubre el gap R3): la
// oscilación lateral REBOTA en [centro ± rango] usando my_x de la trilateración del
// TOP → el arquero cubre la boca del arco sin irse al lateral (no dejar el arco libre)
// y sin depender del timer. Marco cancha: +X lateral, ancho pared-a-pared 1820 mm →
// centro ≈ 910. Sin pose confiable → fallback a la oscilación por tiempo (la de hoy).
// 🔧 needs_bench: verificar el centro real que reporta la trilateración frente al arco
// y ajustar el rango a la boca del arco de la cancha de Gustavo.
constexpr float   GK_PATROL_X_CENTER_MM     = 910.0f;
constexpr float   GK_PATROL_X_HALF_RANGE_MM = 350.0f;
constexpr uint8_t GK_POSE_CONF_MIN          = 40;    // banco mostró conf=70 con 4 ToF

// Arquero — Capa 3 (WP-3-GK): strafe PARALELO a la línea lateral usando el
// cross_track REAL (distancia perpendicular firmada del centro del robot a la
// recta de la línea, en mm; computado en down_model con geometría). El PID
// lateral lleva cross_track -> setpoint (0 = pisar la línea, distancia
// perpendicular nula) mientras la oscilación de PATROL / el seguimiento de bola
// de INTERCEPT mueven al robot A LO LARGO de la línea => queda paralelo.
// ⚠️ REDISEÑO banco 2026-06-09 (Gustavo): "apenas pisando la línea", NO centrado.
// Centrado (setpoint 0) ponía 6+ sensores en blanco → disparaba imminent_exit →
// flapping violento PATROL<->LINE_AVOID (causa #1 del banco). Con la línea ~40 mm
// DETRÁS del centro (cross_track + = línea adelante ⇒ setpoint NEGATIVO) el anillo
// la toca con 2-4 sensores: patrulla el borde sin disparar la alarma de salida.
constexpr float GK_CROSS_TRACK_SETPOINT_MM    = -40.0f;  // era 0 (centrado → flapping)

// ⚠️ FIX DE EJE (banco 2026-06-09): la corrección de cross_track iba a vx — correcto
// para una línea PARALELA al robot (línea lateral de cancha), pero la línea del ÁREA
// que patrulla el arquero corre IZQUIERDA-DERECHA (a lo largo de X) → su distancia
// perpendicular se corrige con **vy** (adelante/atrás), no con vx. La oscilación de
// patrulla vive en vx (a lo largo de la línea) y la corrección en vy (mantener el
// borde). 🔧 SIGNO A CONFIRMAR EN BANCO (Fase 4): si el robot se ALEJA de la línea
// en vez de mantenerla, invertir GK_CT_VY_SIGN a -1.
constexpr float GK_CT_VY_SIGN                 = +1.0f;

// Anti-flapping PATROL<->LINE_AVOID (causa #1 del banco 2026-06-09):
// la huida solo dispara si imminent_exit PERSISTE (no un roce de patrulla) y con
// cooldown tras volver (que el PID tenga tiempo de levantar el robot al setpoint).
constexpr uint32_t GK_LINE_AVOID_DEBOUNCE_MS  = 150;  // imminent debe persistir esto
constexpr uint32_t GK_LINE_AVOID_COOLDOWN_MS  = 400;  // sin re-disparo tras volver a PATROL

// GYRO HOLD (diseño de Gustavo 2026-06-09): rumbo a mantener cuando NO se ve el arco
// propio pero el heading del BNO es válido. 0° = el frente capturado al boot (el robot
// se enciende mirando al arco rival) → "siempre mirando al frente".
constexpr float GK_GYRO_HOLD_TARGET_DEG       = 0.0f;

// ── PATRULLA v3 SEGMENTADA (banco 2026-06-09, último log: el strafe CONTINUO genera
// un yaw físico ~80°/s — sesgo del mix de pisos + reversas — que la corrección en
// movimiento (≤40°/s) no puede pelear → giro sin fin). El patrón VALIDADO del banco
// era MOVER-PARAR-MOVER (el diag de strafe). v3 = tramo corto de strafe → freno →
// si quedó chueco, RE-ORIENTAR PARADO con pulsos de rotación pura (con floor_scale
// la rotación pura SÍ funciona; en pulsos porque sale rápida) → tramo al otro lado.
// v3.1 (banco 2026-06-09 noche): los pulsos eran una TORMENTA — con los pisos la
// rotación mínima física es ~300°/s → cada pulso se pasaba (más inercia al soltar) →
// el siguiente corregía al revés → ping-pong violento (±90-180° por panel en el log).
// Fixes: pulsos más cortos + CORTE EN VIVO al acercarse al frente (no duración fija)
// + más asentamiento (la inercia termina de girar tras soltar) + umbral de entrada
// más alto (un error chico apenas inclina el lateral: cos20°=0.94, no vale el pulso).
// v3.2 (banco 2026-06-10, diseño Gustavo): la patrulla se guía por la LÍNEA BLANCA
// — mantiene el sentido tramo tras tramo hasta TOCAR la línea lateral (DOWN la ve
// directo: dato físico y rápido, no pasa por el TOP) y ahí REBOTA al otro lado. La
// pose del TOP queda como límite secundario (visión sin recalibrar = confianza
// floja) y un tope de tramos de fail-safe. Pulsos DOMESTICADOS para el heading
// LENTO real: el snapshot del TOP llega a ~4 Hz (no 100 Hz — loop del TOP frenado
// por I²C de ToF, ver journal 2026-06-10) → con dato de hasta 250 ms de atraso un
// pulso chico es lotería: umbral de entrada más alto, pulsos más cortos, menos
// pulsos por parada y asentamiento que cubra ≥2 muestras frescas a 4 Hz.
constexpr uint32_t GK_PATROL_SEG_MS        = 1200; // duración del tramo de strafe
constexpr uint32_t GK_PATROL_STOP_MS       = 300;  // freno entre tramos (mide rumbo quieto)
constexpr uint32_t GK_PATROL_BOUNCE_COOLDOWN_MS = 800; // 1 rebote por toque de línea
constexpr uint8_t  GK_PATROL_MAX_SEGS_SAME_DIR  = 3;   // fail-safe sin línea ni pose
constexpr float    GK_REORIENT_ENTER_DEG   = 35.0f;// error que dispara re-orientación
constexpr float    GK_REORIENT_EXIT_DEG    = 25.0f;// CORTE EN VIVO del pulso (anticipa la inercia)
constexpr float    GK_REORIENT_MS_PER_DEG  = 2.0f; // tope de duración ∝ error (rotación real ~300°/s)
constexpr uint32_t GK_REORIENT_PULSE_MIN_MS = 40;
constexpr uint32_t GK_REORIENT_PULSE_MAX_MS = 80;
constexpr uint32_t GK_REORIENT_SETTLE_MS   = 700;  // quieto tras el pulso (inercia + ≥2 snapshots a 4 Hz)
constexpr uint8_t  GK_REORIENT_MAX_PULSES  = 2;    // tope de pulsos por parada
// ORIENTACIÓN POR CÁMARA — DESHABILITADA hasta validar (banco 2026-06-09): el signo
// del gyro de robot2 se CONFIRMÓ correcto (izquierda → hdg sube), por lo tanto la J/U
// del retroceso la causaba la rama (1) de la cascada: el target derivado del ángulo
// del ARCO PROPIO visto por la cámara TRASERA — ángulo jamás validado en banco (la
// homografía se calibró para DISTANCIAS de la frontal). Hasta validar goal_own_angle:
// el arquero se orienta SOLO por gyro (probado confiable). Re-habilitar con true
// cuando el banco confirme el ángulo de la trasera (la referencia absoluta corrige
// el drift del gyro en partidos largos — vale la pena recuperarla después).
constexpr bool GK_CAMERA_ORIENT_ENABLED      = false;

// TOPE de ω del arquero — depende del RÉGIMEN DE PISO del build:
//   • Clamp por-rueda (viejo): ω > ~11°/s disparaba la trasera de 0 a 107 (bang-bang)
//     → tope 10°/s (corrige 90° en ~9 s = "gira a paso de tortuga", banco 2026-06-09:
//     el arquero quedaba chueco eternamente entre avances).
//   • ESCALADO UNIFORME (-DCENTRAL_FLOOR_SCALE): las proporciones se conservan y el
//     cero-o-fiel mata el bang-bang → se puede dar autoridad REAL: 40°/s corrige 90°
//     en ~2.3 s, suficiente para enderezarse entre movimientos sin marear el strafe.
#ifdef CENTRAL_FLOOR_SCALE
constexpr float GK_ORIENT_OMEGA_MAX_DEGPS     = 40.0f;
#else
constexpr float GK_ORIENT_OMEGA_MAX_DEGPS     = 10.0f;
#endif

// Arquero — Nivel 2
constexpr float GK_CLEAR_TRIGGER_MM           = 250.0f;    // pelota más cerca que esto → CLEAR
constexpr float GK_CLEAR_RELEASE_MM           = 400.0f;    // histéresis: vuelve a PATROL al alejarse
constexpr float GK_CLEAR_SPEED_MM_S           = 500.0f;

// Arquero — clasificación de trayectoria (bt_classify, src/shared/ball_trajectory).
//
// El arquero ANTICIPA la X de la pelota vía ball_predict en INTERCEPT y, además,
// distingue si el tiro va al ARCO PROPIO (amenaza real) o si la pelota sale hacia
// el arco rival / cruza de costado. bt_classify clasifica la trayectoria a partir
// de la velocidad de la pelota (marco robot) y la geometría de los arcos, y el
// arquero usa el resultado para REFORZAR la respuesta en INTERCEPT:
//
//   kind                       conducta INTERCEPT
//   ------------------------   ---------------------------------------------------
//   BT_TO_OWN_GOAL (amenaza)   X PREDICHA con MÁS lead (×LEAD_FACTOR) + KP ×KP_FACTOR
//   BT_OTHER / BT_TO_OPP_GOAL  X PREDICHA con lead/KP base (idéntico a hoy)
//   BT_STILL / sin geometría   X PREDICHA con lead/KP base (idéntico a hoy)
//
// ⚠️ FALLBACK EXACTO: si la velocidad es N/A o la pelota está quieta (vx=vy=0),
// bt_classify devuelve BT_STILL y, además, ball_predict no agrega lead (px=bx);
// si el arco rival NO es visible deshabilitamos la clasificación de arcos
// (toward_tol=0) → no hay amenaza → lead_factor=kp_factor=1.0 → la conducta es
// BYTE-IDÉNTICA a la de hoy en todos esos casos. SOLO la rama BT_TO_OWN_GOAL
// escala lead y KP (ver GK_BT_THREAT_* y gk_threat_response). Tunear esos factores
// en banco. El espejo host-testeable es bt_decide_intercept (test_central_trajectory).
constexpr int16_t GK_BT_SPEED_MIN_MM_S        = 80;        // |v| < esto → BT_STILL (pelota quieta / ruido)
constexpr int16_t GK_BT_TOWARD_TOL_CENTIDEG   = 4500;      // ±45°: cono "va hacia el arco" (0 deshabilita)

// === Arquero — RESPUESTA A AMENAZA (bt_classify threat ACTIVADO) ===
//
// Cuando bt_classify marca BT_TO_OWN_GOAL (la pelota VA al arco PROPIO = amenaza
// real), el arquero refuerza la respuesta de dos maneras INDEPENDIENTES y aditivas:
//
//   1) MÁS LEAD: re-predice la X con ball_predict usando lookahead/max_lead
//      escalados por GK_BT_THREAT_LEAD_FACTOR (anticipa más el tiro entrante).
//   2) MÁS GANANCIA: el KP del eje X de INTERCEPT se multiplica por
//      GK_BT_THREAT_KP_FACTOR (reacción más agresiva a llegar a esa X).
//
// ⚠️ FALLBACK EXACTO (no-regresión): si threat==false (BT_STILL/OPP/OTHER, o
// velocidad N/A, o sin arco rival visible), AMBOS factores valen 1.0 → la X
// predicha es BYTE-IDÉNTICA a la de hoy (ball_predict con default params) y el KP
// queda en GK_INTERCEPT_KP_VS_BALL_X. La amenaza solo MULTIPLICA, nunca cambia el
// signo ni el camino del fallback.
//
// 🔧 needs_bench: estos dos factores CAMBIAN la conducta del arquero ante un tiro
// al arco propio → tunear en banco (un factor muy alto puede sobre-anticipar y
// dejar hueco; muy bajo no aporta sobre el comportamiento actual). Valores
// iniciales conservadores: +50% de lead, +50% de ganancia.
constexpr float GK_BT_THREAT_LEAD_FACTOR      = 1.5f;      // ×lookahead y ×max_lead cuando amenaza (1.0 = sin cambio)
constexpr float GK_BT_THREAT_KP_FACTOR        = 1.5f;      // ×GK_INTERCEPT_KP_VS_BALL_X cuando amenaza (1.0 = sin cambio)

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

// Orientación del arquero — CASCADA (diseño de Gustavo, banco 2026-06-09):
//   1) ARCO PROPIO visible + heading válido → de espaldas al arco (referencia
//      ABSOLUTA por cámara: además corrige el drift acumulado del gyro).
//   2) Solo heading válido (cámara no ve el arco) → **GYRO HOLD**: mantener el
//      rumbo 0° capturado al boot (el robot se enciende mirando al arco rival) →
//      "siempre mirando al frente" aunque la visión no ayude. [NUEVO 2026-06-09 —
//      antes acá ω=0 y el arquero quedaba sin orientación sin cámara.]
//   3) heading inválido → ω=0 (no girar a ciegas; fail-safe de siempre).
// Bridge entre la decisión PURA gk_own_goal_orient (world_model.h, host-testeada)
// y el HeadingPID (Arduino). Devuelve ω en centideg/s para cmd.omega_centideg_s.
// GYRO HOLD PURO para el RETROCESO (banco 2026-06-09, 4ª pasada: con la cascada el
// retroceso hizo una J/U — sospecha: o el target del arco propio por la cámara TRASERA
// (ángulo nunca validado) o el signo del heading de robot2 invertido). Reglas:
//   • SOLO gyro (rama 2 de la cascada): jamás perseguir a la cámara en una reversa ciega.
//   • BAIL-OUT: si |error de rumbo| > 45°, ω=0 — un error que CRECE con el PID activo
//     significa signo invertido o heading basura: mejor reversa abierta (recta-ish por
//     simetría) que enroscarse en una U persiguiendo la corrección equivocada.
inline int16_t gk_gyro_hold_omega(uint32_t now_ms, float target_trim_deg) {
    if (!world_model_heading_valid()) return 0;
    const float heading = world_model_get_my_heading_deg();
    const float target  = GK_GYRO_HOLD_TARGET_DEG + target_trim_deg;
    float err = target - heading;
    while (err > 180.0f)  err -= 360.0f;
    while (err < -180.0f) err += 360.0f;
    if (err > 45.0f || err < -45.0f) return 0;   // bail-out: no perseguir errores grandes
    heading_pid_set_target(g_heading_pid, target);
    float omega = heading_pid_tick(g_heading_pid, heading, now_ms);
    if (omega >  GK_ORIENT_OMEGA_MAX_DEGPS) omega =  GK_ORIENT_OMEGA_MAX_DEGPS;
    if (omega < -GK_ORIENT_OMEGA_MAX_DEGPS) omega = -GK_ORIENT_OMEGA_MAX_DEGPS;
    return omega_degps_to_centideg(omega);
}

// Pose del TOP utilizable para límites de patrulla: snapshot fresco + confianza
// mínima de la trilateración (diseño Gustavo 2026-06-09: no alejarse del arco).
inline bool gk_pose_ok() {
    return world_model_snapshot_is_fresh()
        && world_model_get_my_pose_confidence() >= GK_POSE_CONF_MIN;
}

// target_trim_deg: sesgo opcional del rumbo objetivo (perilla de banco — hoy lo usa
// el retroceso de GOTO_LINE para compensar deriva de rumbo; + = CCW/izquierda).
inline int16_t gk_orient_omega(uint32_t now_ms, float target_trim_deg = 0.0f) {
    const float heading = world_model_get_my_heading_deg();
    const GkOwnGoalOrient o = gk_own_goal_orient(
        world_model_goal_own_visible(),
        world_model_heading_valid(),
        heading,
        world_model_get_goal_own_angle_deg());
    float omega = 0.0f;
    if (GK_CAMERA_ORIENT_ENABLED && o.set_heading) {
        // (1) referencia absoluta: de espaldas al arco propio. ⚠️ Gateada OFF
        // (banco 2026-06-09): el ángulo de la cámara trasera causó la J/U del
        // retroceso; re-habilitar tras validar goal_own_angle en banco.
        heading_pid_set_target(g_heading_pid, o.heading_target_deg + target_trim_deg);
        omega = heading_pid_tick(g_heading_pid, heading, now_ms);
    } else if (world_model_heading_valid()) {
        // (2) GYRO HOLD: rumbo del boot (0° = mirando al frente).
        heading_pid_set_target(g_heading_pid, GK_GYRO_HOLD_TARGET_DEG + target_trim_deg);
        omega = heading_pid_tick(g_heading_pid, heading, now_ms);
    } else {
        return 0;   // (3) sin heading válido → ω=0 (fail-safe de siempre).
    }
    // Tope anti-bang-bang con los pisos (ver GK_ORIENT_OMEGA_MAX_DEGPS).
    if (omega >  GK_ORIENT_OMEGA_MAX_DEGPS) omega =  GK_ORIENT_OMEGA_MAX_DEGPS;
    if (omega < -GK_ORIENT_OMEGA_MAX_DEGPS) omega = -GK_ORIENT_OMEGA_MAX_DEGPS;
    return omega_degps_to_centideg(omega);  // satura int16 (anti sign-flip, #9)
}

// Clasifica la trayectoria de la pelota para el arquero y decide qué X (marco
// robot) debe perseguir el INTERCEPT. PURA (sin world_model / Arduino) para poder
// caracterizarla host-side en test_central_trajectory (espejo bt_decide_intercept).
//
// Entradas:
//   bx, by               : posición actual de la pelota (mm, marco robot).
//   bx_pred              : X PREDICHA por ball_predict (mm) — lo que hoy persigue.
//   vx, vy               : velocidad de la pelota (mm/s, marco robot); 0 si N/A/quieta.
//   goal_opp_visible     : ¿se ve el arco rival? (gobierna la geometría de arcos).
//   goal_opp_angle_deg   : ángulo al arco rival (deg, 0 = frente del robot).
//   dist_mm              : distancia robot↔pelota (mm).
//
// Salida: la X (marco robot) que el INTERCEPT debe usar + el multiplicador de KP.
//
// Convención de arcos (la MISMA que bt_classify / behind_ball): heading en marco
// robot, 0 cd = frente (+Y), +90° = derecha (+X). El arco PROPIO del arquero está
// detrás (lado contrario al rival): goal_own ≈ goal_opp + 180°. Sin arco rival
// visible no hay geometría confiable → deshabilitamos la clasificación de arcos
// (toward_tol=0) y la pelota cae en BT_OTHER/BT_STILL → conducta IDÉNTICA a hoy.
//
// RESPUESTA A AMENAZA: si la clasificación da BT_TO_OWN_GOAL (amenaza), la X se
// re-predice con MÁS lead (×GK_BT_THREAT_LEAD_FACTOR) y el KP se escala
// (×GK_BT_THREAT_KP_FACTOR). En cualquier otra rama (incluido velocidad N/A,
// pelota quieta o sin arco rival visible) los factores valen 1.0 → la X coincide
// con bx_pred (lo que el arquero ya perseguía) y kp_scale=1.0 ⇒ FALLBACK EXACTO,
// byte-idéntico a hoy. La decisión threat→(lead,kp) está extraída a la función
// pura gk_threat_response (host-testeada vía su espejo en test_central_trajectory).
struct GkInterceptDecision {
    BallTrajKind kind;
    float        target_x_mm;   // X que el INTERCEPT debe perseguir (marco robot).
    bool         threat;        // true = tiro clasificado al arco PROPIO (amenaza).
    float        kp_scale;      // multiplicador del KP del eje X (1.0 = sin amenaza).
};

// Decisión PURA de la respuesta a amenaza: dado el flag `threat`, la posición y
// velocidad de la pelota (marco robot) y los parámetros base de predicción/KP,
// devuelve (a) la X objetivo y (b) el multiplicador de KP.
//
// FALLBACK EXACTO: con threat==false los factores son 1.0 → target_x es
// ball_predict(base_params).px (idéntico a hoy) y kp_scale=1.0. Con threat==true
// re-predice con lookahead/max_lead escalados por lead_factor y devuelve kp_factor.
// Host-testeable: solo depende de ball_predict (src/shared) y aritmética.
struct GkThreatResponse { float target_x_mm; float kp_scale; };

inline GkThreatResponse gk_threat_response(bool threat,
                                           int16_t bx, int16_t by,
                                           int16_t vx, int16_t vy,
                                           const BallPredictParams& base,
                                           float lead_factor, float kp_factor) {
    const float lf = threat ? lead_factor : 1.0f;
    BallPredictParams p{ base.lookahead_s * lf, base.max_lead_mm * lf };
    const BallPredictOut pred = ball_predict(bx, by, vx, vy, p);
    GkThreatResponse r{};
    r.target_x_mm = pred.px_mm;
    r.kp_scale    = threat ? kp_factor : 1.0f;
    return r;
}

inline GkInterceptDecision gk_classify_intercept(float bx, float by, float bx_pred,
                                                 int16_t vx, int16_t vy,
                                                 bool goal_opp_visible,
                                                 float goal_opp_angle_deg,
                                                 float dist_mm) {
    BallTrajIn in{};
    in.ball_vx_mm_s        = vx;
    in.ball_vy_mm_s        = vy;
    in.ball_speed_min_mm_s = GK_BT_SPEED_MIN_MM_S;
    in.ball_dist_mm        = static_cast<int16_t>(dist_mm > 32767.0f ? 32767.0f : dist_mm);
    in.reach_mm            = 0;   // el arquero no usa in_reach; CLEAR ya gobierna la cercanía.
    if (goal_opp_visible) {
        // Arco rival visible → geometría confiable. Arco propio = opuesto al rival.
        // Normalizar a (-180,180] ANTES de pasar a centideg: si goal_opp ~ +180,
        // own=+360 → *100 desbordaría int16 (bt_classify espera centideg en rango).
        float own_deg = goal_opp_angle_deg + 180.0f;
        while (own_deg >  180.0f) own_deg -= 360.0f;
        while (own_deg <= -180.0f) own_deg += 360.0f;
        in.goal_opp_angle_centideg = static_cast<int16_t>(lroundf(goal_opp_angle_deg * 100.0f));
        in.goal_own_angle_centideg = static_cast<int16_t>(lroundf(own_deg * 100.0f));
        in.toward_tol_centideg     = GK_BT_TOWARD_TOL_CENTIDEG;
    } else {
        // Sin geometría: toward_tol=0 deshabilita la clasificación de arcos.
        // Resultado: BT_STILL (sin velocidad) o BT_OTHER → fallback exacto.
        in.toward_tol_centideg = 0;
    }
    const BallTraj t = bt_classify(in);
    GkInterceptDecision d{};
    d.kind   = t.kind;
    d.threat = (t.kind == BT_TO_OWN_GOAL);
    // Respuesta a amenaza ACTIVADA: si threat → más lead (X re-predicha con
    // lookahead/max_lead escalados) y más KP. Si NO-threat → lead_factor y
    // kp_factor valen 1.0 ⇒ X == ball_predict(default) == bx_pred (idéntico a hoy)
    // y kp_scale=1.0. Por eso el path de no-amenaza es BYTE-IDÉNTICO al previo.
    const GkThreatResponse tr = gk_threat_response(
        d.threat,
        static_cast<int16_t>(lroundf(bx)), static_cast<int16_t>(lroundf(by)),
        vx, vy, ball_predict_default_params(),
        GK_BT_THREAT_LEAD_FACTOR, GK_BT_THREAT_KP_FACTOR);
    d.target_x_mm = tr.target_x_mm;
    d.kp_scale    = tr.kp_scale;
    // bx_pred queda como referencia del cálculo del caller (mismo default params);
    // en no-amenaza d.target_x_mm == bx_pred por construcción. No se usa más acá.
    (void)bx_pred;
    return d;
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

static inline float atk_norm180(float a) {
    while (a > 180.0f)  a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}

// EJE DE ATAQUE (v2 2026-06-11, spec Gustavo = robot 2025): la dirección "hacia el
// arco rival" RELATIVA al robot sale de la CÁMARA si ve el arco; si no, FALLBACK
// al 0 del BNO (por protocolo el robot se enciende mirando al arco rival → heading
// absoluto 0 ≈ eje de ataque). Antes, sin arco visible el delantero NO orbitaba
// (empujaba la pelota para cualquier lado); ahora siempre tiene un eje.
struct AttackAxis { bool valid; float rel_deg; };
static AttackAxis atk_attack_axis(float heading) {
    if (world_model_goal_opp_visible()) {
        return {true, world_model_get_goal_opp_angle_deg()};
    }
    if (world_model_heading_valid()) {
        return {true, atk_norm180(-heading)};   // relativo: apuntar al 0 absoluto
    }
#ifdef ATK_OTOS_NOGYRO
    // Práctica R1 sin gyro: el yaw del OTOS de DOWN reemplaza al BNO como
    // fallback del eje (0 del OTOS = rumbo del boot = arco rival, misma
    // convención). Sin el flag este branch no existe → R2/competencia intactos.
    if (world_model_otos_is_fresh()) {
        return {true, atk_norm180(-world_model_get_otos_heading_deg())};
    }
#endif
    return {false, 0.0f};
}

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
                // v2: el eje de ataque sale de la CÁMARA o del 0 del BNO (spec
                // 2025) — con eje disponible, pelota desalineada → POSITION
                // (orbit). Solo sin NINGÚN eje (ni arco ni heading) APPROACH directo.
                const float bx = world_model_get_ball_x_mm();
                const float by = world_model_get_ball_y_mm();
                const AttackAxis ax = atk_attack_axis(heading);
                if (ax.valid) {
                    const bool aligned = ball_is_in_attack_line(bx, by, ax.rel_deg,
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
            const AttackAxis ax = atk_attack_axis(heading);
            if (!ax.valid) {
                // Sin NINGÚN eje (ni arco por cámara ni heading del BNO):
                // degradar a APPROACH directo (Nivel 1).
                transition_atk(AtkState::APPROACH);
                return cmd;
            }

            const float bx = world_model_get_ball_x_mm();
            const float by = world_model_get_ball_y_mm();
            const float goal_angle = ax.rel_deg;   // cámara o 0-del-BNO (v2, spec 2025)

            const BehindBallTarget tgt = compute_behind_ball_target(
                bx, by, goal_angle, ATK_BEHIND_BALL_GAP_MM);

            // Heading: siempre mirar al arco rival.
            // heading_valid gate (#schema v3): si el BNO aún no convergió
            // (heading=0 falso al boot) NO orientar con ese rumbo este tick (ω=0).
            // Si heading_valid (caso normal) → ω idéntica a hoy. Igual seteamos el
            // target para que el PID no arranque frío cuando el heading vuelva.
            heading_pid_set_target(g_heading_pid, heading + goal_angle);
            const float omega = central_gate_heading_omega(
                world_model_heading_valid(),
                heading_pid_tick(g_heading_pid, heading, now_ms));

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

            // Si la pelota dejó de estar alineada con el eje de ataque (la
            // perdimos al costado), volver a POSITION para rodearla otra vez.
            // v2: eje = cámara O 0-del-BNO (antes solo cámara).
            const AttackAxis ax = atk_attack_axis(heading);
            if (ax.valid) {
                if (!ball_is_in_attack_line(bx, by, ax.rel_deg,
                                             ATK_ATTACK_LINE_TOL_DEG + 10.0f)) {
                    // Histéresis +10° para no oscilar entre APPROACH↔POSITION.
                    transition_atk(AtkState::POSITION);
                    return cmd;
                }
                // EMPUJE COMPROMETIDO (v2 = "pateo" del robot 2025): pelota cerca
                // + robot apuntando al eje de ataque → PUSH (full adelante por
                // tiempo fijo). Antes este chequeo era solo documental y el robot
                // llegaba a la pelota al MÍNIMO de velocidad (~200 mm/s): empuje
                // débil. Ahora el gol se busca de verdad.
#ifdef ATK_OTOS_NOGYRO
                // SIN GYRO el robot no rota para apuntar → |ax.rel_deg| chico
                // jamás se cumple y el PUSH clásico no dispararía nunca. Gatillo
                // GEOMÉTRICO en su lugar: pelota CERCA y SOBRE el eje de ataque.
                // La dirección del empuje se CONGELA al vector a la pelota (≈
                // dirección al arco por la condición de alineación) y el rumbo
                // se sostiene con el yaw del OTOS durante el empuje.
                if (dist < ATK_NOGYRO_PUSH_DIST_MM &&
                    ball_is_in_attack_line(bx, by, ax.rel_deg,
                                           ATK_NOGYRO_PUSH_TOL_DEG)) {
                    g_atk_push_dir  = nogyro_push_dir(bx, by);
                    g_atk_push_hdg0 = world_model_get_otos_heading_deg();
                    g_atk_push_started_ms = now_ms;
                    transition_atk(AtkState::PUSH);
                    return cmd;
                }
#else
                if (dist < ATK_KICK_DIST_MM &&
                    std::fabs(ax.rel_deg) <= ATK_KICK_ANGLE_DEG) {
                    g_atk_push_started_ms = now_ms;
                    transition_atk(AtkState::PUSH);
                    return cmd;
                }
#endif
            }

            // Heading target: orientar el frente hacia la pelota.
            // ball_x relativo al robot: 0 = frente. Convertir a absoluto.
            const float ball_angle_rel = std::atan2(bx, by) * (180.0f / M_PI);
            const float ball_angle_abs = heading + ball_angle_rel;
            // heading_valid gate (#schema v3): no orientar con heading=0 falso al
            // boot. heading_valid → ω idéntica a hoy.
            heading_pid_set_target(g_heading_pid, ball_angle_abs);
            const float omega = central_gate_heading_omega(
                world_model_heading_valid(),
                heading_pid_tick(g_heading_pid, heading, now_ms));

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

        case AtkState::PUSH: {
            g_state_name = "ATK_PUSH";
#ifdef ATK_OTOS_NOGYRO
            // EMPUJE SIN GYRO (práctica R1): dirección CONGELADA al vector a la
            // pelota del momento de comprometerse (contra el paragolpes las
            // cámaras la pierden — igual que el empuje a ciegas del 2025) +
            // rumbo sostenido con el yaw del OTOS (P puro con clamp y bail-out:
            // ante señal dudosa mejor no corregir que perseguir un signo
            // invertido). El omni empuja en diagonal SIN rotar — no hace falta
            // que el frente apunte al arco.
            cmd.vx_mm_s = static_cast<int16_t>(g_atk_push_dir.ux * ATK_PUSH_SPEED_MM_S);
            cmd.vy_mm_s = static_cast<int16_t>(g_atk_push_dir.uy * ATK_PUSH_SPEED_MM_S);
            cmd.omega_centideg_s = omega_degps_to_centideg(
                nogyro_yaw_hold_omega_degps(world_model_otos_is_fresh(),
                                            g_atk_push_hdg0,
                                            world_model_get_otos_heading_deg(),
                                            DS_KP_HEADING,
                                            ATK_NOGYRO_OMEGA_MAX_DEGPS,
                                            ATK_NOGYRO_BAILOUT_DEG));
#else
            // EMPUJE comprometido (v2 2026-06-11 = "PATEANDO_adelante" del 2025):
            // full adelante por tiempo FIJO, SIN re-mirar la pelota (contra el
            // paragolpes las cámaras la pierden; el 2025 también pateaba a ciegas).
            // El rumbo se sostiene hacia el eje de ataque con el gyro: el empuje
            // sale DERECHO al arco. El kickstart de motores le da el golpe inicial.
            const AttackAxis ax = atk_attack_axis(heading);
            if (ax.valid) {
                heading_pid_set_target(g_heading_pid, heading + ax.rel_deg);
                const float omega = central_gate_heading_omega(
                    world_model_heading_valid(),
                    heading_pid_tick(g_heading_pid, heading, now_ms));
                cmd.omega_centideg_s = omega_degps_to_centideg(omega);
            }
            cmd.vy_mm_s = static_cast<int16_t>(ATK_PUSH_SPEED_MM_S);
#endif
            if (now_ms - g_atk_push_started_ms >= ATK_PUSH_MS) {
                g_atk_push_started_ms = now_ms;     // re-armar timer para PUSH_BACK
                transition_atk(AtkState::PUSH_BACK);
            }
            return cmd;
        }

        case AtkState::PUSH_BACK: {
            g_state_name = "ATK_PUSH_BACK";
            // Retroceso corto post-empuje ("PATEANDO_atras" del 2025): despegarse
            // de la pelota y de la línea del área antes de volver a buscar (evita
            // quedar pegado al borde con el freno de emergencia encima).
#ifdef ATK_OTOS_NOGYRO
            // Sin gyro el empuje pudo ser diagonal → retroceder POR EL MISMO EJE
            // (deshacer el camino), no ciegamente hacia -Y.
            cmd.vx_mm_s = static_cast<int16_t>(-g_atk_push_dir.ux * ATK_PUSH_BACK_SPEED_MM_S);
            cmd.vy_mm_s = static_cast<int16_t>(-g_atk_push_dir.uy * ATK_PUSH_BACK_SPEED_MM_S);
#else
            cmd.vy_mm_s = static_cast<int16_t>(-ATK_PUSH_BACK_SPEED_MM_S);
#endif
            if (now_ms - g_atk_push_started_ms >= ATK_PUSH_BACK_MS) {
                transition_atk(AtkState::SEARCH);
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

#ifdef GK_SIMPLE_STRAFE
    // === ARQUERO SIMPLE — STRAFE LATERAL MANTENIENDO EL FRENTE (pedido María,
    //     práctica 2026-06-12) ===
    // Conducta mínima y directa, distinta de la patrulla v3.3:
    //   • Al arrancar (pedido Gustavo 2026-06-14): RETROCEDE recto al arco hasta tocar
    //     su línea de fondo y AVANZA ~10 cm para despegarse (fases GOTO_BACK→ADVANCE,
    //     reusan la lógica de GOTO_LINE). Recién ahí entra al strafe.
    //   • Strafe lateral PURO a GK_PATROL_SPEED_MM_S (200 mm/s) der↔izq con rebote en
    //     los laterales (SIN pulsos, SIN REACQ, SIN pelota).
    //   • omega=0 (strafe puro, SIN corregir el frente durante el movimiento). Se
    //     PROBÓ omega continuo con el BNO (banco María 2026-06-12) y DESCONTROLÓ el
    //     robot (giro 0→-150° en 3 s) — la degeneración del mix giro+strafe ya
    //     documentada en la v3.3 (con estos pisos {70,70,107} las delanteras van
    //     chicas en el strafe y cualquier ω las desborda). El frente se sostiene
    //     porque el strafe es simétrico; si deriva, agregar enderezado en pausas.
    //   • REBOTE por línea (misma lógica v3.2, validada): si la placa DOWN ve línea
    //     a un costado (NO la de atrás), invertir el sentido. Convención de signo
    //     del line_angle (CONTRATO-DATOS-DOWN): + = línea a la DERECHA → ir IZQUIERDA
    //     (direction -1); - = línea a la IZQUIERDA → ir DERECHA (+1). Atrás (~±180)
    //     = la línea del propio arco → se ignora.
    {
        // v6 (banco María 2026-06-12, diseño según skills control-pid-zona-muerta
        // + dinamica-omni-3-ruedas): MOVIMIENTO CONTINUO con control PI+PFM.
        // Historia de la planta (medida hoy): a 200 mm/s el robot está en régimen
        // CUANTIZADO — el giro es casi todo-o-nada por los pisos. PID continuo
        // clásico falló por los dos modos: capado a 40°/s PERDÍA contra la deriva
        // parásita (~80°/s); capado a 120°/s OSCILABA ±140° (overshoot
        // cuantizado). Solución industrial: PFM — la corrección deseada se
        // entrega como PULSOS de magnitud fija (que el robot SÍ sabe hacer)
        // durante una fracción de cada ventana de 160 ms → el promedio temporal
        // es la corrección fina que el motor no puede hacer continua. El
        // integrador del PI aprende solo la deriva parásita (auto-calibración).
        //   • Línea detectada → ESCAPE ~12 cm al lado contrario SIN leer
        //     sensores (validado hoy) → retoma el movimiento continuo.
        //   • RED DE SEGURIDAD: error >45° (lazo perdido: patinada, choque) →
        //     parar, escuadrarse en el lugar, asentar, retomar.
        // 🔧 Ajuste (titración, ver skill): tiembla → subir deadband (5→8°);
        //    deriva lenta sin corregir → subir ki (0.4→0.8) o kp (2→3).
        constexpr float    GKS_RESQUARE_DEG    = 45.0f;  // dispara la red de seguridad
        constexpr float    GKS_RESQUARE_EXIT   = 12.0f;  // escuadre: corte en vivo
        constexpr uint32_t GKS_RESQUARE_MAX_MS = 900;    // tope del escuadre parado
        constexpr uint32_t GKS_SETTLE_MS       = 300;    // quieto post-escuadre
        constexpr uint32_t GKS_ESCAPE_MS       = 1300;   // huida post-línea (banco 2026-06-14:
                                                         // 600→900→1300, "le falta distancia de
                                                         // escape". 420 mm/s · 1300 ms ≈ 55 cm.
                                                         // 🔧 más perilla: GKS_ESCAPE_SPEED_MM_S.)
        // ESCAPE más RÁPIDO que el strafe normal (GK_PATROL_SPEED=200) para despegarse de
        // la línea DECIDIDO y salir antes de volver a leer la línea (pedido María
        // 2026-06-14: "se quedaba trabado en la línea blanca"). 420 = "fiel pleno" → la
        // rueda trasera queda SOBRE su piso de PWM (no aplastada) = strafe limpio y veloz.
        // 420 mm/s · 600 ms ≈ 25 cm. 🔧 banco: si aún no despega, subir a ~500 o el
        // ESCAPE_MS; si se va de más, bajar la velocidad.
        constexpr float    GKS_ESCAPE_SPEED_MM_S = 420.0f;

#ifdef GK_SIMPLE_BALL
        // CONDUCTA DE PELOTA (cámara) sobre el strafe — pedido María 2026-06-14.
        // CERCA → despeje hacia adelante; LEJOS → seguir de costado a la pelota para quedar
        // enfrente (las líneas la acotan, NO se abre); sin pelota → patrulla de siempre.
        // Gateado: el env sin -DGK_SIMPLE_BALL (strafe_bb) queda BYTE-IDÉNTICO.
        constexpr float GK_BALL_NEAR_MM     = 250.0f;  // dist < esto = "cerca" → despeje
        constexpr float GK_BALL_PUSH_SPEED  = 500.0f;  // impulso de despeje hacia +Y (adelante)
        constexpr float GK_BALL_TRACK_DB_MM = 40.0f;   // deadband lateral (anti-jitter al seguir)
        constexpr int   GK_BALL_TRACK_SIGN  = -1;      // signo del seguimiento. Banco 2026-06-14:
                                                       // iba al REVÉS (no seguía la pelota) → -1.
                                                       // Si AÚN va al revés, poné +1 (eje X cámara↔strafe).
#endif

        static int             dir_simple  = +1;   // arranca hacia la derecha
        static uint32_t        bounce_gate = 0;
        static uint8_t         phase       = 5;    // 5=GOTO_BACK 6=ADVANCE 0=MOVE 2=RESQUARE 3=SETTLE 4=ESCAPE
        static uint32_t        phase_t0    = 0;
        static uint32_t        goto_clear_ms = 0;  // fase ADVANCE: desde cuándo la línea dejó de verse
        static PfmHeadingState pfm{};               // estado del PI+PFM (integ + ventana)

        if (!world_model_match_running()) {
            g_state_name = "GK_SIMPLE_WAIT";
            dir_simple = +1; bounce_gate = 0;   // reset (sin statics colgados)
            phase = 5; phase_t0 = now_ms; goto_clear_ms = 0;   // arranca por el retroceso al arco (#1)
            pfm_heading_reset(pfm);
            return cmd;                          // ceros → quieto
        }
        if (phase_t0 == 0) phase_t0 = now_ms;

        // Error de rumbo hacia el ARCO RIVAL (= 0° del boot: por protocolo el
        // robot se enciende mirándolo). Frente permanente, pedido María.
        const bool hv = world_model_heading_valid();
        const float heading_now = world_model_get_my_heading_deg();
        float hdg_err = 0.0f;
        if (hv) {
            hdg_err = GK_GYRO_HOLD_TARGET_DEG - heading_now;
            while (hdg_err > 180.0f)  hdg_err -= 360.0f;
            while (hdg_err < -180.0f) hdg_err += 360.0f;
        }
        const float aerr = (hdg_err < 0.0f) ? -hdg_err : hdg_err;

        // PI+PFM: corrección fina por pulsos repartidos (ver pfm_heading.h).
        // Tick de strategy = 10 ms (100 Hz). Devuelve centideg/s del comando.
        auto pfm_omega = [&]() -> int16_t {
            const float w = pfm_heading_tick(pfm, pfm_heading_default_cfg(),
                                             hdg_err, hv, now_ms, 0.01f);
            return omega_degps_to_centideg(w);
        };

        switch (phase) {
            case 5: {   // GOTO_BACK — retroceso recto al arco hasta tocar la línea de fondo (#1)
                // Reusa GOTO_LINE fase 0 (banco Gustavo 2026-06-09 v2): -Y a GK_GOTO_LINE_VY_BACK
                // con gyro-hold de rumbo, hasta que la DOWN ve su línea (la de atrás) o timeout.
                g_state_name = "GK_SIMPLE_GOTO_BACK";
                cmd.vx_mm_s = clamp_velocity_mm_s(
                    static_cast<float>(GK_GOTO_LINE_VX_RIGHT) + GK_GOTO_LINE_VX_TRIM_MM_S);
                cmd.vy_mm_s = clamp_velocity_mm_s(-static_cast<float>(GK_GOTO_LINE_VY_BACK));  // -Y = atrás
                cmd.omega_centideg_s = gk_gyro_hold_omega(now_ms, GK_GOTO_LINE_HEADING_TRIM_DEG);
                const bool line_here = world_model_line_detected() && world_model_line_data_valid();
                const bool to = (now_ms - phase_t0) >= GK_GOTO_LINE_TIMEOUT_MS;
                if (line_here || to) { phase = 6; phase_t0 = now_ms; goto_clear_ms = 0; }
                break;
            }
            case 6: {   // ADVANCE — despegarse de la línea hasta dejar de verla + margen (#1)
                // Reusa GOTO_LINE fase 1: +Y a GK_ADVANCE_SPEED_MM_S con gyro-hold, hasta que la
                // línea DEJE de verse GK_ADVANCE_MS seguidos (o timeout). Recién ahí: strafe lateral.
                g_state_name = "GK_SIMPLE_ADVANCE";
                cmd.vx_mm_s = 0;
                cmd.vy_mm_s = clamp_velocity_mm_s(static_cast<float>(GK_ADVANCE_SPEED_MM_S));  // +Y = adelante
                cmd.omega_centideg_s = gk_gyro_hold_omega(now_ms, 0.0f);
                if (!world_model_line_detected()) {
                    if (goto_clear_ms == 0) goto_clear_ms = now_ms;
                } else {
                    goto_clear_ms = 0;   // la sigue viendo → reiniciar el margen
                }
                const bool cleared = (goto_clear_ms != 0) && (now_ms - goto_clear_ms) >= GK_ADVANCE_MS;
                const bool to = (now_ms - phase_t0) >= GK_ADVANCE_TIMEOUT_MS;
                if (cleared || to) { phase = 0; phase_t0 = now_ms; }   // → MOVE (strafe lateral der↔izq)
                break;
            }
            case 0: {   // MOVE — strafe CONTINUO + PID de rumbo al arco
                g_state_name = "GK_SIMPLE_MOVE";
                // Red de seguridad: el PID está perdiendo → escuadrarse parado.
                if (hv && aerr > GKS_RESQUARE_DEG) {
                    phase = 2; phase_t0 = now_ms;
                    break;
                }
                // Rebote por línea lateral (cooldown anti-rebote-múltiple).
                if (line_data_fresh() && world_model_line_detected() &&
                    (now_ms - bounce_gate) >= GK_PATROL_BOUNCE_COOLDOWN_MS) {
                    const float la = world_model_get_line_angle_deg();
                    const bool  la_behind = (la > 135.0f || la < -135.0f);
                    if (!la_behind) {
                        // OPUESTO al sentido de ENTRADA (pedido María 2026-06-14): invertir
                        // el strafe en curso. Robusto al rumbo — NO depende de leer bien el
                        // ángulo de la línea (que se lee mal cuando el robot está rotado, y
                        // por eso antes a veces escapaba HACIA la línea metiéndose más).
                        dir_simple  = -dir_simple;
                        bounce_gate = now_ms;
                        phase = 4; phase_t0 = now_ms;   // PRIMERO escapar de la línea
                        break;
                    }
                }
#ifdef GK_SIMPLE_BALL
                // CÁMARA: si ve la pelota, pre-empta la patrulla a ciegas. (El rebote por
                // línea de arriba YA corrió → seguir a la pelota queda acotado por las líneas.)
                if (world_model_ball_visible()) {
                    const float bx = world_model_get_ball_x_mm();   // +x = derecha (marco robot)
                    const float by = world_model_get_ball_y_mm();   // +y = adelante
                    const int   xdir = (bx >  GK_BALL_TRACK_DB_MM) ?  GK_BALL_TRACK_SIGN
                                     : (bx < -GK_BALL_TRACK_DB_MM) ? -GK_BALL_TRACK_SIGN : 0;
                    if (bx * bx + by * by < GK_BALL_NEAR_MM * GK_BALL_NEAR_MM) {
                        // CERCA → DESPEJE: empuja hacia ADELANTE (+Y, lejos de su arco) y se
                        // centra en la pelota. Rápido. (Confirmado María: hacia adelante.)
                        g_state_name = "GK_SIMPLE_PUSH";
                        cmd.vx_mm_s = clamp_velocity_mm_s(xdir * GK_BALL_PUSH_SPEED);
                        cmd.vy_mm_s = clamp_velocity_mm_s(GK_BALL_PUSH_SPEED);
                    } else {
                        // LEJOS → SEGUIR de costado para quedar SIEMPRE enfrente de la pelota.
                        // NO se abre (la red de rebote por línea lo acota); NO avanza (vy=0);
                        // si ya está alineado (|bx|<deadband) se queda quieto.
                        g_state_name = "GK_SIMPLE_TRACK";
                        if (xdir != 0) dir_simple = xdir;
                        cmd.vx_mm_s = clamp_velocity_mm_s(xdir * GK_PATROL_SPEED_MM_S);
                        cmd.vy_mm_s = 0;
                    }
                    cmd.omega_centideg_s = pfm_omega();   // siempre de frente al arco
                    break;
                }
#endif
                cmd.vx_mm_s = clamp_velocity_mm_s(dir_simple * GK_PATROL_SPEED_MM_S);
                cmd.vy_mm_s = 0;
                cmd.omega_centideg_s = pfm_omega();   // frente al arco, continuo
                break;
            }
            case 4: {   // ESCAPE — despegarse de la línea ANTES de decidir (fix María)
                g_state_name = "GK_SIMPLE_ESCAPE";
                // Strafe al lado contrario SIN mirar la línea (sobre la línea las
                // lecturas confunden). El PID de rumbo SIGUE activo: el escape
                // también sale derecho. Sale RÁPIDO (GKS_ESCAPE_SPEED) y retoma el MOVE.
                cmd.vx_mm_s = clamp_velocity_mm_s(dir_simple * GKS_ESCAPE_SPEED_MM_S);
                cmd.vy_mm_s = 0;
                cmd.omega_centideg_s = pfm_omega();
                if ((now_ms - phase_t0) >= GKS_ESCAPE_MS) { phase = 0; phase_t0 = now_ms; }
                break;
            }
            case 2: {   // RESQUARE — parado, girar hacia el arco a autoridad plena
                g_state_name = "GK_SIMPLE_RESQUARE";
                // Sin strafe que pelear, el giro parado es el movimiento más
                // confiable del robot. Corte en vivo a 12° (la inercia termina).
                cmd.omega_centideg_s = pfm_omega();
                if (aerr <= GKS_RESQUARE_EXIT || (now_ms - phase_t0) >= GKS_RESQUARE_MAX_MS) {
                    phase = 3; phase_t0 = now_ms;
                }
                break;
            }
            case 3: {   // SETTLE — quieto un instante y retomar el continuo
                g_state_name = "GK_SIMPLE_SETTLE";
                if ((now_ms - phase_t0) >= GKS_SETTLE_MS) { phase = 0; phase_t0 = now_ms; }
                break;
            }
        }
        return cmd;
    }
#endif  // GK_SIMPLE_STRAFE

    if (!world_model_match_running()) {
        transition_gk(GkState::WAIT_START);
    } else if (world_model_imminent_exit() && line_data_fresh()) {
        // ANTI-FLAPPING (banco 2026-06-09, causa #1): el arquero PISA su línea por
        // diseño → un roce de patrulla NO puede disparar la huida. LINE_AVOID solo
        // entra si imminent_exit PERSISTE (debounce) y pasó el cooldown desde la
        // última vuelta a PATROL (que el PID tenga tiempo de levantar el robot al
        // setpoint "apenas pisando"). En LINE_AVOID no se re-dispara a sí mismo.
        if (g_gk_imminent_since_ms == 0) g_gk_imminent_since_ms = now_ms;
        const bool persisted = (now_ms - g_gk_imminent_since_ms) >= GK_LINE_AVOID_DEBOUNCE_MS;
        const bool cooled    = (now_ms - g_gk_line_avoid_gate_ms) >= GK_LINE_AVOID_COOLDOWN_MS;
        if (persisted && cooled && g_gk_state != GkState::GOTO_LINE) {
            // Diseño Gustavo v2 (banco 2026-06-09): la línea del arquero SIEMPRE está
            // ATRÁS → la respuesta a tocarla es AVANZAR ~10 cm (fase 1 de GOTO_LINE),
            // NO el retreat por ángulo de LINE_AVOID (con los pisos salía en
            // direcciones basura y el robot giraba sobre su eje a gran velocidad).
            // v3.2 (banco 2026-06-10): PERO si la línea está al COSTADO durante la
            // patrulla (la lateral del área/cancha), avanzar NO la saca de ahí — eso
            // lo resuelve el REBOTE del MOVE (invierte el sentido del strafe). Solo
            // se avanza si la línea está realmente ATRÁS.
            const float la_imm    = world_model_get_line_angle_deg();
            const bool  la_behind = (la_imm > 135.0f || la_imm < -135.0f);
            if (la_behind || g_gk_state != GkState::PATROL) {
                g_goto_line_phase       = 1;
                g_gk_advance_started_ms = now_ms;
                transition_gk(GkState::GOTO_LINE);
            }
        }
    } else {
        g_gk_imminent_since_ms = 0;   // imminent se apagó → re-armar el debounce
    }

    switch (g_gk_state) {
        case GkState::WAIT_START: {
            g_state_name = "GK_WAIT_START";
            if (world_model_match_running()) {
                // DELAY DE ARRANQUE (banco): tiempo para acomodar el robot tras el GO.
                if (g_gk_start_seen_ms == 0) g_gk_start_seen_ms = now_ms;
                if ((now_ms - g_gk_start_seen_ms) >= GK_START_DELAY_MS) {
                    // NO va directo a PATROL: primero se reposiciona contra su línea.
                    transition_gk(GkState::GOTO_LINE);
                    g_goto_line_started_ms = now_ms;
                    g_goto_line_phase      = 0;   // fase RETROCESO
                }
            } else {
                g_gk_start_seen_ms = 0;   // STOP re-arma el delay para el próximo GO
            }
            return cmd;
        }

        case GkState::GOTO_LINE: {
            if (g_goto_line_phase == 0) {
                // ── Fase 0: RETROCESO recto con GYRO HOLD hasta tocar la línea ──
                g_state_name = "GK_GOTO_LINE";
                // vx = trim anti-deriva traslacional (banco: derivaba suave a la
                // derecha mirando al frente — eso el gyro NO lo ve; ver el bloque
                // de TRIMS arriba; tope físico ±19 mm/s por el piso de la trasera).
                cmd.vx_mm_s = static_cast<int16_t>(
                    static_cast<float>(GK_GOTO_LINE_VX_RIGHT) + GK_GOTO_LINE_VX_TRIM_MM_S);
                cmd.vy_mm_s = static_cast<int16_t>(-GK_GOTO_LINE_VY_BACK);  // -Y = atrás
                // GYRO PURO (+ trim de rumbo opcional): en reversa ciega NUNCA usar
                // el target de cámara (la J del banco) — solo el gyro, con bail-out.
                cmd.omega_centideg_s = gk_gyro_hold_omega(now_ms, GK_GOTO_LINE_HEADING_TRIM_DEG);

                const bool line_here = world_model_line_detected()
                                    && world_model_line_data_valid();
                const bool timed_out =
                    (now_ms - g_goto_line_started_ms) >= GK_GOTO_LINE_TIMEOUT_MS;
                if (line_here || timed_out) {
                    // Diseño Gustavo v2: tocó la línea → NO patrullar sobre ella
                    // (pegado a la línea el PID de borde saturaba y el robot giraba
                    // sobre su eje). Despegarse AVANZANDO ~10 cm primero.
                    g_goto_line_phase       = 1;
                    g_gk_advance_started_ms = now_ms;
                }
            } else {
                // ── Fase 1: AVANCE hasta DESPEGARSE de la línea (+margen, c/timeout) ──
                g_state_name = "GK_ADVANCE";
                cmd.vx_mm_s = 0;
                cmd.vy_mm_s = GK_ADVANCE_SPEED_MM_S;          // +Y = adelante
                cmd.omega_centideg_s = gk_gyro_hold_omega(now_ms, 0.0f);  // gyro puro (sin cámara)

                // Despegue REAL: la línea tiene que DEJAR de verse y mantenerse así
                // GK_ADVANCE_MS (margen). El avance fijo de antes a veces quedaba
                // corto (motores bajo carga) → loop avance↔toque (banco 2026-06-09).
                if (!world_model_line_detected()) {
                    if (g_gk_advance_clear_ms == 0) g_gk_advance_clear_ms = now_ms;
                } else {
                    g_gk_advance_clear_ms = 0;    // la sigue viendo → reiniciar margen
                }
                const bool cleared = (g_gk_advance_clear_ms != 0)
                    && (now_ms - g_gk_advance_clear_ms) >= GK_ADVANCE_MS;
                const bool adv_timeout =
                    (now_ms - g_gk_advance_started_ms) >= GK_ADVANCE_TIMEOUT_MS;
                if (cleared || adv_timeout) {
                    g_gk_advance_clear_ms   = 0;
                    g_gk_line_avoid_gate_ms = now_ms;   // gracia anti re-disparo
                    // AUTO-CAPTURA del centro de patrulla: acá el robot está EN su
                    // puesto (recién despegado de su línea, frente a su arco).
                    if (gk_pose_ok()) {
                        g_gk_patrol_x_center = world_model_get_my_x_mm();
                    }
                    transition_gk(GkState::PATROL);
                }
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
                g_gk_line_avoid_gate_ms = now_ms;   // cooldown: sin re-disparo inmediato
                transition_gk(GkState::PATROL);
            }
            return cmd;
        }

        case GkState::PATROL: {
            // ── PATRULLA v3 SEGMENTADA (mover-parar-corregir-mover) ──
            // El strafe continuo acumulaba yaw físico (~80°/s) imposible de pelear
            // en movimiento. Ahora: tramo corto → freno → si quedó chueco, pulsos
            // de ROTACIÓN PURA parado (lo que el banco validó que funciona) → tramo
            // al otro lado. Límites por pose deciden la dirección de cada tramo.
            static int      direction   = 1;
            static uint8_t  pphase      = 0;   // 0=MOVE 1=STOP 2=PULSO 3=ASENTAR
            static uint32_t pphase_t0   = 0;
            static uint32_t pulse_ms    = 0;
            static uint8_t  pulse_count = 0;
            static uint8_t  same_dir_segs  = 0;   // tramos seguidos en el mismo sentido (v3.2)
            static uint32_t bounce_gate_ms = 0;   // cooldown del rebote por línea (v3.2)
            static uint8_t  reacq_dry      = 0;   // re-enganches SEGUIDOS sin hallar línea (guard)
            if (pphase_t0 == 0) pphase_t0 = now_ms;

            // El panel muestra la SUB-FASE (diagnóstico banco 2026-06-10): con solo
            // "GK_PATROL" no se distingue si el desastre pasa en el tramo o en el pulso.
            static const char* kPatrolPhaseName[5] =
                {"GK_PATROL_MOVE", "GK_PATROL_STOP", "GK_PATROL_PULSE",
                 "GK_PATROL_SETTLE", "GK_PATROL_REACQ"};
            g_state_name = kPatrolPhaseName[(pphase < 5) ? pphase : 0];

            // Centro de la ventana: el AUTO-CAPTURADO al llegar al puesto (fallback
            // a la constante si la pose no estaba disponible en ese momento).
            const float xc = (g_gk_patrol_x_center >= 0.0f)
                           ? g_gk_patrol_x_center : GK_PATROL_X_CENTER_MM;

            // Error de rumbo (envuelto a ±180) — solo con heading válido.
            const bool hv = world_model_heading_valid();
            float hdg_err = 0.0f;
            if (hv) {
                hdg_err = GK_GYRO_HOLD_TARGET_DEG - world_model_get_my_heading_deg();
                while (hdg_err > 180.0f)  hdg_err -= 360.0f;
                while (hdg_err < -180.0f) hdg_err += 360.0f;
            }

            switch (pphase) {
                case 0: {   // MOVE: tramo de strafe PURO (ω=0)
                    // REBOTE POR LÍNEA (v3.2, diseño Gustavo): el límite lateral REAL
                    // de la patrulla es la línea blanca que ve DOWN. Línea al COSTADO
                    // (o adelante) → rebotar al otro lado. Línea ATRÁS (~±180°) NO
                    // rebota: esa es la del área y la maneja el router de arriba
                    // (avanzar a despegarse).
                    if (line_data_fresh() && world_model_line_detected() &&
                        (now_ms - bounce_gate_ms) >= GK_PATROL_BOUNCE_COOLDOWN_MS) {
                        const float la = world_model_get_line_angle_deg();
                        const bool  la_behind = (la > 135.0f || la < -135.0f);
                        if (!la_behind) {
                            direction      = (la >= 0.0f) ? -1 : +1;  // alejarse de la línea
                            bounce_gate_ms = now_ms;
                            same_dir_segs  = 0;
                            pphase = 1; pphase_t0 = now_ms;  // frenar y arrancar al otro lado
                            break;
                        }
                    }
                    cmd.vx_mm_s = clamp_velocity_mm_s(direction * GK_PATROL_SPEED_MM_S);
                    cmd.vy_mm_s = 0;
                    // ω=0 A PROPÓSITO (v3.1): mezclar corrección de giro con el strafe
                    // DEGENERA en estos motores — en el strafe las delanteras van chicas
                    // (25 PWM crudo) y cualquier ω las desborda; el escalado + cap deja
                    // la trasera bajo su piso → se apaga → bandazo a full de las
                    // delanteras (gran parte del "giro continuo" de los logs). El strafe
                    // PURO cae en la mezcla validada {70,70,107} que va derecha. El rumbo
                    // se corrige PARADO (pulsos) — única forma fiel con estos pisos.
                    cmd.omega_centideg_s = 0;
                    bool seg_end = (now_ms - pphase_t0) >= GK_PATROL_SEG_MS;
                    // Límite por pose: corta el tramo al llegar al borde del arco.
                    if (gk_pose_ok()) {
                        const float x = world_model_get_my_x_mm();
                        if ((x > xc + GK_PATROL_X_HALF_RANGE_MM && direction > 0) ||
                            (x < xc - GK_PATROL_X_HALF_RANGE_MM && direction < 0)) {
                            seg_end = true;
                        }
                    }
                    if (seg_end) { pphase = 1; pphase_t0 = now_ms; }
                    break;
                }
                case 1: {   // STOP: frenar, medir rumbo quieto, decidir
                    // cmd queda en 0 → el mixer re-arma el impulso para el próximo tramo.
                    if ((now_ms - pphase_t0) >= GK_PATROL_STOP_MS) {
                        const float aerr = (hdg_err < 0.0f) ? -hdg_err : hdg_err;
                        if (hv && aerr > GK_REORIENT_ENTER_DEG &&
                            pulse_count < GK_REORIENT_MAX_PULSES) {
                            // Quedó chueco → pulso de rotación pura hacia el frente.
                            float ms = aerr * GK_REORIENT_MS_PER_DEG;
                            if (ms < GK_REORIENT_PULSE_MIN_MS) ms = GK_REORIENT_PULSE_MIN_MS;
                            if (ms > GK_REORIENT_PULSE_MAX_MS) ms = GK_REORIENT_PULSE_MAX_MS;
                            pulse_ms  = static_cast<uint32_t>(ms);
                            pphase    = 2; pphase_t0 = now_ms;
                        } else {
                            // Derecho (o tope de pulsos) → próximo tramo. v3.2: el sentido
                            // se MANTIENE tramo tras tramo hasta tocar la línea (rebote en
                            // MOVE) — la patrulla recorre el arco de línea a línea, como
                            // pidió el banco. La pose (si es confiable) sigue acotando, y
                            // el tope de tramos evita strafe infinito si línea y pose
                            // fallan a la vez.
                            pulse_count = 0;
                            bool flipped = false;
                            if (gk_pose_ok()) {
                                const float x = world_model_get_my_x_mm();
                                if (x > xc + GK_PATROL_X_HALF_RANGE_MM)      { direction = -1; flipped = true; }
                                else if (x < xc - GK_PATROL_X_HALF_RANGE_MM) { direction = +1; flipped = true; }
                            }
                            if (flipped) {
                                same_dir_segs = 0;
                            } else if (++same_dir_segs >= GK_PATROL_MAX_SEGS_SAME_DIR) {
                                same_dir_segs = 0;
                                direction = -direction;   // fail-safe: sin referencia, oscilar
                            }
                            // v3.3: antes de largar el tramo, RE-ENGANCHAR la línea si no
                            // está a la vista — la patrulla vive pegada a la línea, no a
                            // 10 cm (sin línea en el anillo el rebote no puede guiar).
                            // GUARD anti-"caminar al arco" (banco 2026-06-10): si el
                            // re-enganche venció 2 veces SEGUIDAS sin encontrar línea
                            // (sensado caído: batería floja/calibración → valid=0), NO
                            // seguir retrocediendo a ciegas — el requisito duro es no
                            // meterse al arco. Patrulla donde está hasta re-ver línea.
                            const bool line_in_view = line_data_fresh() &&
                                                      world_model_line_detected();
                            if (line_in_view) reacq_dry = 0;
                            pphase = (line_in_view || reacq_dry >= 2) ? 0 : 4;
                            pphase_t0 = now_ms;
                        }
                    }
                    break;
                }
                case 2: {   // PULSO: rotación pura breve hacia el frente, con CORTE EN VIVO
                    cmd.omega_centideg_s = static_cast<int16_t>(
                        (hdg_err >= 0.0f ? +1 : -1) * GK_ORIENT_OMEGA_MAX_DEGPS * 100.0f);
                    // Corte EN VIVO (v3.1): la rotación real es ~300°/s (mínimo físico
                    // con pisos) → no esperar la duración completa: soltar apenas el
                    // error entra en la zona (la inercia recorre el resto). Anti
                    // ping-pong de pulsos que se pasan de largo.
                    const float aerr = (hdg_err < 0.0f) ? -hdg_err : hdg_err;
                    const bool  close_enough = hv && (aerr <= GK_REORIENT_EXIT_DEG);
                    if (close_enough || (now_ms - pphase_t0) >= pulse_ms) {
                        ++pulse_count;
                        pphase = 3; pphase_t0 = now_ms;
                    }
                    break;
                }
                case 3: {   // ASENTAR: quieto, dejar que el rumbo se lea fresco
                    if ((now_ms - pphase_t0) >= GK_REORIENT_SETTLE_MS) {
                        pphase = 1; pphase_t0 = now_ms;   // re-evaluar en STOP
                    }
                    break;
                }
                case 4: {   // RE-ENGANCHE (v3.3): atrás despacio hasta re-VER la línea
                    cmd.vx_mm_s = 0;
                    cmd.vy_mm_s = static_cast<int16_t>(-GK_PATROL_REACQ_VY_MM_S);  // -Y = atrás
                    cmd.omega_centideg_s = gk_gyro_hold_omega(now_ms, 0.0f);
                    const bool touched = line_data_fresh() && world_model_line_detected();
                    if (touched) {
                        reacq_dry = 0;
                        pphase = 0; pphase_t0 = now_ms;   // línea al borde → tramo lateral
                    } else if ((now_ms - pphase_t0) >= GK_PATROL_REACQ_MAX_MS) {
                        ++reacq_dry;                       // venció sin línea (guard arriba)
                        pphase = 0; pphase_t0 = now_ms;
                    }
                    break;
                }
            }

#ifndef GK_IGNORE_BALL
            // (Banco 2026-06-09: con -DGK_IGNORE_BALL la patrulla NO sale a la pelota
            //  — env central_robot2_arquero_patrol. La pelota visible secuestraba el
            //  test: PATROL→CLEAR→INTERCEPT en loop y el rumbo quedaba a ±90°.)
            if (world_model_ball_visible()) {
                transition_gk(GkState::INTERCEPT);
            }
#endif
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

            // CLASIFICACIÓN DE TRAYECTORIA (bt_classify): decide qué X perseguir y
            // cuánto reforzar según si el tiro va al arco propio (amenaza), al rival,
            // cruza o está quieto. RESPUESTA A AMENAZA ACTIVADA: si BT_TO_OWN_GOAL,
            // bt.target_x trae MÁS lead y bt.kp_scale escala el KP (ver
            // GK_BT_THREAT_*). FALLBACK EXACTO: en cualquier otra rama (o velocidad
            // N/A / sin arco rival visible) target_x == bx_pred y kp_scale==1.0 → el
            // comando es IDÉNTICO al previo. Ver gk_classify_intercept / gk_threat_response.
            const GkInterceptDecision bt = gk_classify_intercept(
                bx, by, bx_pred,
                world_model_get_ball_vx_mm_s(),
                world_model_get_ball_vy_mm_s(),
                world_model_goal_opp_visible(),
                world_model_goal_opp_visible() ? world_model_get_goal_opp_angle_deg() : 0.0f,
                dist);
            (void)bt.kind;      // expuesto para observabilidad/tuning.

            // RESPUESTA A AMENAZA ACTIVADA: target_x ya trae más lead si bt.threat;
            // y el KP se escala por bt.kp_scale (1.0 si no es amenaza → idéntico a
            // hoy). Ambos refuerzos colapsan a la conducta previa cuando threat=false.
            const float kp_intercept = GK_INTERCEPT_KP_VS_BALL_X * bt.kp_scale;
            const float vx_intercept = bt.target_x_mm * kp_intercept;
            // clamp_velocity_mm_s: defensivo (target_x_mm * kp puede crecer si el
            // wire dejara pasar una X fuera de rango). Byte-idéntico a (int16_t)
            // en el rango normal; sólo satura/anula NaN en el path de falla.
            cmd.vx_mm_s = clamp_velocity_mm_s(vx_intercept);
            cmd.vy_mm_s = 0;   // v2: sin PID de borde; la línea quedó atrás
            // LÍMITES POR POSE (diseño Gustavo): no perseguir la X de la pelota más
            // allá del rango del arco — al llegar al límite, no empujar más afuera.
            if (gk_pose_ok()) {
                const float x  = world_model_get_my_x_mm();
                const float xc = (g_gk_patrol_x_center >= 0.0f)
                               ? g_gk_patrol_x_center : GK_PATROL_X_CENTER_MM;
                if ((x > xc + GK_PATROL_X_HALF_RANGE_MM && cmd.vx_mm_s > 0) ||
                    (x < xc - GK_PATROL_X_HALF_RANGE_MM && cmd.vx_mm_s < 0)) {
                    cmd.vx_mm_s = 0;
                }
            }

            // Orientación: cascada arco-propio → GYRO HOLD → ω=0 (2026-06-09).
            cmd.omega_centideg_s = gk_orient_omega(now_ms);

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
            // heading_valid gate (#schema v3): no orientar con heading=0 falso al
            // boot. heading_valid → ω idéntica a hoy.
            const float ball_angle_rel = std::atan2(bx, by) * (180.0f / M_PI);
            heading_pid_set_target(g_heading_pid,
                                   world_model_get_my_heading_deg() + ball_angle_rel);
            float omega = central_gate_heading_omega(
                world_model_heading_valid(),
                heading_pid_tick(g_heading_pid,
                                 world_model_get_my_heading_deg(),
                                 now_ms));
            // Tope de giro del CLEAR (banco 2026-06-09): sin tope (clamp 327°/s) el
            // despeje encaraba la pelota con trompos violentos. Mismo límite que el
            // resto del arquero; el empuje sale igual (vx/vy van directo a la pelota).
            if (omega >  GK_ORIENT_OMEGA_MAX_DEGPS) omega =  GK_ORIENT_OMEGA_MAX_DEGPS;
            if (omega < -GK_ORIENT_OMEGA_MAX_DEGPS) omega = -GK_ORIENT_OMEGA_MAX_DEGPS;
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
    g_goto_line_started_ms = 0;
    heading_pid_reset(g_heading_pid);
    lateral_pid_reset(g_lateral_pid_gk);
}

MotorCommand strategy_tick() {
    MotorCommand cmd = (g_role == RobotRole::ATTACKER) ? attacker_tick()
                                                       : goalkeeper_tick();
#ifdef ATK_OBSTACLE_STOP_MM
    // FRENO ANTI-CHOQUE (práctica, 2ª instancia — env *_obst): si hay obstáculo
    // más cerca que el umbral, cortar el componente de AVANCE del comando.
    // min_obstacle = min(4 ToF, HC-SR04 frontal) del snapshot del TOP; 65535 =
    // sin lectura → no frena. SOLO delantero: el arquero patrulla con la pared
    // del arco atrás y el min (sin dirección) lo dejaría mudo contra esa pared.
    if (g_role == RobotRole::ATTACKER) {
        cmd.vy_mm_s = nogyro_obstacle_gate_vy(
            cmd.vy_mm_s,
            world_model_snapshot_is_fresh(),
            world_model_get_min_obstacle_mm(),
            static_cast<uint16_t>(ATK_OBSTACLE_STOP_MM));
    }
#endif
    return cmd;
}

void        strategy_set_role(RobotRole role)         { g_role = role; }
RobotRole   strategy_get_role()                       { return g_role; }
void        strategy_set_attack_color(AttackColor c)  { g_attack_color = c; }
AttackColor strategy_get_attack_color()               { return g_attack_color; }
const char* strategy_get_state_name()                 { return g_state_name; }

}  // namespace iitasoccer
