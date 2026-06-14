// =============================================================================
// strategy.cpp — Estrategia del robot: Delantero y Arquero
// =============================================================================
//
// Este archivo contiene la lógica táctica completa para los dos roles del robot:
//
//   DELANTERO (attacker_tick):
//     Busca la pelota, se posiciona detrás de ella mirando al arco rival,
//     se acerca y empuja. Los estados son:
//       WAIT_START → KICKOFF → SEARCH → POSITION → APPROACH → PUSH → PUSH_BACK
//     con LINE_AVOID como interrupción de emergencia cuando pisa un borde.
//
//   ARQUERO (goalkeeper_tick):
//     Se posiciona contra su línea de área, patrulla lateralmente y se
//     interpone entre la pelota y el arco. Los estados son:
//       WAIT_START → GOTO_LINE → PATROL → INTERCEPT → CLEAR
//     con re-posicionamiento automático si se aleja demasiado de la línea.
//
// Arquitectura general:
//   - El punto de entrada público es strategy_tick(), llamado desde main_central.cpp
//     cada 10 ms (~100 Hz). Internamente llama a attacker_tick() o goalkeeper_tick()
//     según el rol.
//   - Los outputs de estas funciones son (vx, vy, omega): velocidad lateral,
//     velocidad de avance y velocidad de giro. La conversión a PWM de cada motor
//     la hace el módulo motors_zircon con la cinemática inversa — ese módulo es
//     independiente de este, así se puede ajustar cada uno por separado.
//   - Los datos del mundo (posición de la pelota, ángulo del robot, sensores de
//     línea, etc.) vienen todos de world_model.h. Este archivo solo LEE de ahí,
//     nunca escribe.
//   - Niveles de juego implementados:
//       Nivel 1 (mínimo para Incheon): búsqueda + acercamiento + patrulla básica.
//       Nivel 2 (este archivo): kickoff, posicionamiento behind-the-ball, empuje
//         comprometido, despeje del arquero.
//
// =============================================================================

#include "strategy.h"
#include "world_model.h"       // datos del mundo: pelota, heading, líneas, pose
#include "pids.h"              // controladores PID de rumbo y posición lateral
#include "behind_ball.h"       // calcula el punto detrás de la pelota mirando al arco
#include "drive_straight.h"    // cancela la deriva lateral usando el OTOS
#include "ball_predict.h"      // predice dónde va a estar la pelota con su velocidad actual
#include "ball_trajectory.h"   // clasifica la trayectoria: ¿va al arco propio, rival, quieta?
#include "atk_nogyro.h"        // helpers para el delantero sin giroscopio (práctica R1)
#include "pfm_heading.h"       // control PI+PFM de rumbo para el arquero simple

#include <Arduino.h>
#include <cmath>

namespace iitasoccer {

// Todo lo que sigue es privado de este archivo (no accesible desde otros .cpp).
namespace {

// =============================================================================
// CONFIGURACIÓN TÁCTICA (rol y color de ataque)
// =============================================================================

// Rol del robot: ATTACKER o GOALKEEPER. Se setea una vez desde main_central.cpp
// llamando a strategy_set_role(), que lee los DIP switches al encender.
RobotRole   g_role = RobotRole::ATTACKER;

// Color del arco que atacamos (MAGENTA o CYAN, según qué lado de la cancha
// nos toca). Guardado para uso futuro; aún no se usa en la lógica principal.
AttackColor g_attack_color = AttackColor::MAGENTA;

// Nombre del estado actual en texto, solo para debug serial (imprimido
// por main_central.cpp cada 500 ms).
const char* g_state_name = "INIT";


// =============================================================================
// ESTADOS DE LAS MÁQUINAS DE ESTADO (FSMs)
// =============================================================================
//
// Cada robot tiene su propia FSM. Solo importa la del rol activo, pero ambas
// existen en memoria.
//
// El tipo uint8_t (entero de 8 bits) ahorra memoria respecto al int de 32 bits
// que usaría un enum normal. El "enum class" (en lugar de "enum") evita que se
// compare accidentalmente un AtkState con un GkState.

enum class AtkState : uint8_t {
    WAIT_START,   // esperando la señal GO del árbitro
    KICKOFF,      // boost recto al frente al arrancar el partido
    SEARCH,       // buscando la pelota: avanza + gira
    POSITION,     // se mueve al punto detrás de la pelota (behind-the-ball)
    APPROACH,     // avanza hacia la pelota ya alineado con el arco
    LINE_AVOID,   // emergencia: huyendo de un borde detectado
    PUSH,         // empuje comprometido: full adelante por tiempo fijo
    PUSH_BACK     // retroceso corto post-empuje para despegarse
};

enum class GkState : uint8_t {
    WAIT_START,   // esperando GO del árbitro
    GOTO_LINE,    // reposicionamiento inicial: retroceder hasta la línea de área y despegarse
    PATROL,       // patrulla lateral cubriendo el arco
    INTERCEPT,    // se mueve lateralmente para interceptar la pelota
    CLEAR,        // va directo a la pelota a despejarla (cuando está muy cerca)
    LINE_AVOID    // emergencia: huyendo de un borde detectado
};

// Estado actual de cada FSM.
AtkState g_atk_state = AtkState::WAIT_START;
GkState  g_gk_state  = GkState::WAIT_START;


// =============================================================================
// VARIABLES DE ESTADO (delantero)
// =============================================================================

// Timer del kickoff: se guarda el momento en que arrancó para saber cuándo terminan los 250 ms.
uint32_t g_kickoff_started_ms = 0;

// Para detectar el flanco PARADO→CORRIENDO y entrar a KICKOFF solo una vez.
bool g_match_was_running = false;

// Timer del empuje (PUSH y PUSH_BACK): se re-usa para ambos, reseteado al entrar a cada fase.
uint32_t g_atk_push_started_ms = 0;

#ifdef ATK_OTOS_NOGYRO
// Solo para práctica R1 (sin giroscopio BNO):
// Dirección del empuje congelada al momento de comprometerse (vector unitario hacia la pelota).
NoGyroPushDir g_atk_push_dir  = {false, 0.0f, 1.0f};
// Rumbo del OTOS capturado al inicio del empuje, usado como setpoint para mantener la dirección.
float         g_atk_push_hdg0 = 0.0f;
#endif


// =============================================================================
// VARIABLES DE ESTADO (arquero)
// =============================================================================

// Timer de GOTO_LINE: se guarda al entrar para el timeout de seguridad (4 s máximo).
uint32_t g_goto_line_started_ms = 0;

// Anti-flapping PATROL ↔ LINE_AVOID:
// El arquero patrulla rozando su propia línea, entonces un roce no es una "salida".
// Solo se activa LINE_AVOID si imminent_exit persiste varios ticks seguidos (debounce)
// y además pasó un tiempo mínimo desde la última vez que se volvió a PATROL (cooldown).
uint32_t g_gk_imminent_since_ms  = 0;  // desde cuándo está activo imminent_exit (0 = no activo)
uint32_t g_gk_line_avoid_gate_ms = 0;  // cuándo fue la última vez que se volvió a PATROL

// GOTO_LINE tiene dos fases internas:
//   Fase 0: retrocede recto hasta tocar la línea de área.
//   Fase 1: avanza despacio hasta dejar de ver la línea + un margen de ~3 cm.
uint8_t  g_goto_line_phase       = 0;
uint32_t g_gk_advance_started_ms = 0;
uint32_t g_gk_advance_clear_ms   = 0;  // desde cuándo la línea dejó de verse (0 = todavía se ve)

// Delay de arranque: se guarda el primer tick con GO para medir los 2 s de espera.
uint32_t g_gk_start_seen_ms = 0;

// Centro de la ventana de patrulla, capturado automáticamente al llegar al puesto.
// El robot se posiciona contra su línea al arrancar, y en ese momento guarda su X
// como centro de la ventana. Así la patrulla queda centrada en su puesto real.
// Valor -1 = todavía no se capturó (se usa la constante GK_PATROL_X_CENTER_MM como fallback).
float g_gk_patrol_x_center = -1.0f;


// =============================================================================
// PIDs (controladores de rumbo y posición lateral)
// =============================================================================
//
// Un PID (Proporcional-Integral-Derivativo) calcula una corrección basándose en:
//   - el error actual (cuánto me alejo del objetivo),
//   - el error acumulado en el tiempo (para eliminar errores pequeños persistentes),
//   - qué tan rápido cambia el error (para amortiguar oscilaciones).
//
// Son objetos globales porque el PID necesita recordar su historial entre ticks.
// En particular el término integral acumula el error histórico y necesita persistir.

HeadingPID g_heading_pid;       // corrige el rumbo del robot (entrada: error en grados; salida: grados/s)
LateralPID g_lateral_pid_gk;    // corrige la posición lateral del arquero respecto a su línea


// =============================================================================
// CONSTANTES DE TUNING — DELANTERO
// =============================================================================

// Velocidad y giro durante la búsqueda.
// En práctica R1 sin giroscopio, VY = 0: solo gira en el lugar, porque sin lazo
// de rumbo el avance+giro derivaría hacia las paredes sin control.
#ifdef ATK_SEARCH_SPIN_ONLY
constexpr float ATK_SEARCH_VY_MM_S     = 0.0f;
#else
constexpr float ATK_SEARCH_VY_MM_S     = 200.0f;
#endif
constexpr float ATK_SEARCH_OMEGA_DEG_S = 60.0f;

// Perfil de velocidad al acercarse a la pelota (interpolación lineal entre estos límites):
//   distancia >= 500 mm → 400 mm/s
//   distancia <= 50 mm  → 200 mm/s
//   entre esos valores  → velocidad proporcional a la distancia
constexpr float ATK_APPROACH_MAX_SPEED = 400.0f;
constexpr float ATK_APPROACH_MIN_SPEED = 200.0f;
constexpr float ATK_APPROACH_CLOSE_MM  = 50.0f;
constexpr float ATK_APPROACH_FAR_MM    = 500.0f;

// Velocidad de huida al detectar un borde (LINE_AVOID).
constexpr float ATK_LINE_RETREAT_SPEED = 400.0f;

// Separación entre el robot y la pelota cuando está en POSITION (behind-the-ball).
// El robot se mueve a un punto 120 mm detrás de la pelota, en la línea pelota→arco.
constexpr float ATK_BEHIND_BALL_GAP_MM = 120.0f;

// El eje de ataque (dirección hacia el arco rival) tiene esta tolerancia angular.
// Si la pelota está dentro de ±30° del eje, se considera "ya alineada".
constexpr float ATK_ATTACK_LINE_TOL_DEG = 30.0f;

// En POSITION, se considera que el robot "llegó al target" cuando está a menos de 80 mm.
constexpr float ATK_POSITION_REACHED_MM = 80.0f;

// Velocidad máxima en POSITION (el acercamiento al target detrás de la pelota).
constexpr float ATK_POSITION_MAX_SPEED  = 400.0f;

// Condiciones para disparar el empuje (PUSH):
//   - pelota a menos de 80 mm del robot, Y
//   - el robot apunta al arco con menos de ±12° de error
constexpr float ATK_KICK_DIST_MM  = 80.0f;
constexpr float ATK_KICK_ANGLE_DEG = 12.0f;

// Empuje comprometido: full adelante 500 ms, luego retroceso corto 250 ms.
// El robot NO mira la pelota durante el empuje (a esa distancia la cámara
// la pierde contra el paragolpes), solo mantiene el rumbo con el giroscopio.
constexpr float    ATK_PUSH_SPEED_MM_S      = 700.0f;
constexpr uint32_t ATK_PUSH_MS              = 500;
constexpr float    ATK_PUSH_BACK_SPEED_MM_S = 300.0f;
constexpr uint32_t ATK_PUSH_BACK_MS         = 250;

// Duración del boost inicial del kickoff (más corto y moderado que el empuje de gol).
constexpr float    ATK_KICKOFF_SPEED_MM_S   = 500.0f;
constexpr uint32_t ATK_KICKOFF_DURATION_MS  = 250;

#ifdef ATK_OTOS_NOGYRO
// ── Parámetros extra para práctica R1 sin giroscopio ──
// Sin BNO, el robot no puede girar para apuntar al arco, así que el gatillo del
// empuje es diferente: distancia + alineación geométrica (no ángulo de rumbo).
// La dirección del empuje se congela al vector a la pelota en ese momento.
// 🔧 Ajustar en banco.
constexpr float ATK_NOGYRO_PUSH_DIST_MM    = 150.0f;  // gatillo más lejos (no hace falta tener la pelota en el paragolpes)
constexpr float ATK_NOGYRO_PUSH_TOL_DEG    = 15.0f;   // la pelota debe estar dentro de ±esto del eje de ataque
constexpr float ATK_NOGYRO_OMEGA_MAX_DEGPS = 40.0f;   // tope de corrección de rumbo por el OTOS
constexpr float ATK_NOGYRO_BAILOUT_DEG     = 45.0f;   // si el error es mayor, no corregir (puede ser signo invertido)
#endif

// Ganancias del módulo drive_straight (corrección de deriva lateral con OTOS):
//   El módulo llama "vx/fwd" al eje de avance y "vy" a la corrección lateral.
//   En nuestro robot, el avance físico es +Y y el lateral es +X, así que hay un
//   bind de ejes: out.vx → cmd.vy y out.vy → cmd.vx.
constexpr float DS_KP_HEADING = 3.0f;   // grados/s por grado de error de rumbo
constexpr float DS_KP_LATERAL = 0.5f;   // mm/s de corrección por mm de deriva lateral


// =============================================================================
// CONSTANTES DE TUNING — ARQUERO
// =============================================================================

// Velocidad de la patrulla lateral.
// 200 mm/s parece lento pero es intencional: a velocidades mayores (~420 mm/s)
// la mezcla de PWM de los motores se degrada y el robot "sale volando a los costados".
// Con 200 mm/s los motores caen en la mezcla {70, 70, 107} que va derecho y moderado.
// Para competencia se puede subir a 420 mm/s (régimen pleno).
constexpr float GK_PATROL_SPEED_MM_S = 200.0f;

// Parámetros de la oscilación del arquero (no usados en la patrulla v3, solo como referencia).
constexpr float GK_PATROL_OSCILLATE_PERIOD_MS = 2000.0f;

// Ganancia proporcional del seguimiento lateral en INTERCEPT.
// El arquero mueve vx = X_pelota × KP. No tiene integral ni derivada porque
// el movimiento lateral no necesita precisión de posición, solo "estar por ahí".
constexpr float GK_INTERCEPT_KP_VS_BALL_X = 4.0f;

// Setpoint del control de profundidad de línea (fallback si cross_track no está disponible).
constexpr float GK_LATERAL_SETPOINT_DEPTH = 1.0f;

// Velocidad de huida en LINE_AVOID (arquero).
// 420 mm/s: el PWM crudo de las delanteras queda sobre su piso mínimo, así la
// dirección del movimiento es fiel. Antes era 250 mm/s (por debajo del piso → dirección basura).
constexpr float GK_LINE_RETREAT_SPEED = 420.0f;

// Delay entre el GO del árbitro y que el arquero empiece a moverse.
// Da tiempo para posicionar y soltar el robot manualmente.
// 🔧 Para competencia bajar a 0.
constexpr uint32_t GK_START_DELAY_MS = 2000;

// Velocidades del reposicionamiento inicial (GOTO_LINE):
//   Fase 0 (retroceso): vx = 0 (recto, sin componente lateral), vy = -420 mm/s (atrás).
//   El retroceso recto es más estable que el diagonal: la mezcla de PWM es simétrica
//   entre las ruedas delanteras, así que los pisos no distorsionan la dirección.
constexpr int16_t  GK_GOTO_LINE_VX_RIGHT    = 0;
constexpr int16_t  GK_GOTO_LINE_VY_BACK     = 420;   // se aplica como -Y (hacia atrás)
constexpr uint32_t GK_GOTO_LINE_TIMEOUT_MS  = 4000;  // si no encuentra la línea en 4 s → PATROL igual

// Trims de calibración del retroceso (compensan derivas mecánicas del robot):
//   VX_TRIM: empuje lateral constante para compensar deriva traslacional (el gyro no la ve).
//   HEADING_TRIM: sesgo del rumbo objetivo para compensar una rotación lenta residual.
// 🔧 Calibrar en banco: deriva a la derecha → ambos trims negativos (hacia izquierda).
//   Tope físico del VX_TRIM: ±19 mm/s (más de eso activa la rueda trasera y da un bandazo).
constexpr float GK_GOTO_LINE_VX_TRIM_MM_S    = 0.0f;  // sin compensación por ahora
constexpr float GK_GOTO_LINE_HEADING_TRIM_DEG = 0.0f;

// Fase 1 del reposicionamiento: avanzar hasta DEJAR DE VER la línea + 100 ms de margen.
// Con 100 ms (~3 cm) la línea queda al borde trasero del anillo: visible como referencia
// de patrulla, pero sin pisarla de lleno (lo que antes causaba que el PID de borde
// saturara y el robot girara sobre su eje a gran velocidad).
constexpr int16_t  GK_ADVANCE_SPEED_MM_S  = 300;
constexpr uint32_t GK_ADVANCE_MS          = 100;   // margen extra tras dejar de ver la línea
constexpr uint32_t GK_ADVANCE_TIMEOUT_MS  = 1500;  // tope duro de la fase de avance

// Re-enganche de línea en patrulla:
// Si al parar entre tramos la línea no está visible (el strafe derivó), el arquero
// retrocede despacio hasta re-verla. Sin línea en el anillo el rebote lateral no puede guiar.
constexpr int16_t  GK_PATROL_REACQ_VY_MM_S = 200;  // retroceso suave
constexpr uint32_t GK_PATROL_REACQ_MAX_MS  = 700;  // tope (si no aparece, patrullar donde está)

// Límites de la ventana de patrulla en X (ancho de la boca del arco).
// Si la pose del TOP está disponible (trilateración), el arquero no se va más allá de
// [centro ± rango]. Si no, solo usa el timer como límite.
// 🔧 Verificar el centro real con la trilateración frente al arco de la cancha.
constexpr float   GK_PATROL_X_CENTER_MM     = 910.0f;  // centro de la cancha (fallback si no hay pose)
constexpr float   GK_PATROL_X_HALF_RANGE_MM = 350.0f;  // medio ancho de la ventana
constexpr uint8_t GK_POSE_CONF_MIN          = 40;       // confianza mínima de la trilateración (0–100)

// Setpoint del control de distancia perpendicular a la línea (cross_track):
//   -40 mm significa "la línea debe quedar 40 mm detrás del centro del robot".
//   Era 0 mm (centrado sobre la línea) pero ponía 6+ sensores en blanco y disparaba
//   inminent_exit constantemente, causando el flapping PATROL↔LINE_AVOID del banco.
constexpr float GK_CROSS_TRACK_SETPOINT_MM = -40.0f;

// Signo de la corrección de cross_track:
//   La línea de área corre de izquierda a derecha (+X), entonces su distancia
//   perpendicular se corrige con vy (adelante/atrás), no con vx.
//   🔧 Si el robot se aleja de la línea en vez de mantenerse, invertir a -1.
constexpr float GK_CT_VY_SIGN = +1.0f;

// Anti-flapping PATROL ↔ LINE_AVOID:
//   La alarma de borde debe persistir 150 ms antes de actuar (debounce).
//   Después de volver a PATROL hay 400 ms de gracia antes de que pueda volver a dispararse.
constexpr uint32_t GK_LINE_AVOID_DEBOUNCE_MS = 150;
constexpr uint32_t GK_LINE_AVOID_COOLDOWN_MS = 400;

// Rumbo objetivo del arquero: 0° = mirando al frente (hacia el arco rival).
// El robot se enciende mirando al arco rival, así que 0° = boot heading = frente.
constexpr float GK_GYRO_HOLD_TARGET_DEG = 0.0f;

// Patrulla v3 segmentada: parámetros de los tramos y las pausas de corrección.
//
// Por qué segmentada: el strafe continuo acumulaba un giro físico de ~80°/s que
// el PID en movimiento no podía compensar. La solución validada en banco fue
// hacer tramos cortos de strafe puro → parar → corregir el rumbo parado con
// pulsos breves → tramo al otro lado.
constexpr uint32_t GK_PATROL_SEG_MS           = 1200;  // duración de un tramo de strafe
constexpr uint32_t GK_PATROL_STOP_MS          = 300;   // tiempo parado entre tramos (mide el rumbo quieto)
constexpr uint32_t GK_PATROL_BOUNCE_COOLDOWN_MS = 800; // tiempo mínimo entre dos rebotes por línea
constexpr uint8_t  GK_PATROL_MAX_SEGS_SAME_DIR  = 3;   // fail-safe: después de 3 tramos iguales, invertir

// Corrección de rumbo parado (pulsos de rotación pura):
//   Si al parar el error de rumbo supera 35°, se aplica un pulso de rotación pura.
//   El pulso dura error × 2 ms (mínimo 40 ms, máximo 80 ms), pero se corta en vivo
//   si el error baja de 25° (para anticipar la inercia del robot).
//   Después 700 ms quieto para que la inercia termine y lleguen datos frescos del TOP.
//   Máximo 2 pulsos por parada (si sigue chueco, aceptarlo y seguir).
constexpr float    GK_REORIENT_ENTER_DEG    = 35.0f;
constexpr float    GK_REORIENT_EXIT_DEG     = 25.0f;
constexpr float    GK_REORIENT_MS_PER_DEG   = 2.0f;
constexpr uint32_t GK_REORIENT_PULSE_MIN_MS = 40;
constexpr uint32_t GK_REORIENT_PULSE_MAX_MS = 80;
constexpr uint32_t GK_REORIENT_SETTLE_MS    = 700;
constexpr uint8_t  GK_REORIENT_MAX_PULSES   = 2;

// Orientación por cámara: deshabilitada hasta validar el ángulo de la cámara trasera
// en banco. Por ahora el arquero se orienta solo por giroscopio.
// Re-habilitar con true cuando se confirme el ángulo de la cámara trasera
// (la referencia absoluta corrige el drift del gyro en partidos largos).
constexpr bool GK_CAMERA_ORIENT_ENABLED = false;

// Tope de velocidad angular del arquero.
//   Con el escalado uniforme de pisos (CENTRAL_FLOOR_SCALE): 40°/s.
//   Sin escalado (pisos por rueda): 10°/s (más de eso hace bang-bang en la rueda trasera).
#ifdef CENTRAL_FLOOR_SCALE
constexpr float GK_ORIENT_OMEGA_MAX_DEGPS = 40.0f;
#else
constexpr float GK_ORIENT_OMEGA_MAX_DEGPS = 10.0f;
#endif

// Histéresis del despeje (CLEAR):
//   Entra a CLEAR cuando la pelota está a menos de 250 mm del arquero.
//   Vuelve a INTERCEPT cuando se aleja más de 400 mm.
//   Los 150 mm de diferencia evitan que el arquero entre y salga repetidamente
//   si la pelota ronda en el límite.
constexpr float GK_CLEAR_TRIGGER_MM = 250.0f;
constexpr float GK_CLEAR_RELEASE_MM = 400.0f;
constexpr float GK_CLEAR_SPEED_MM_S = 500.0f;

// Clasificación de trayectoria de la pelota (para INTERCEPT):
//   Velocidad mínima para considerar que la pelota se mueve (si es menor → quieta).
constexpr int16_t GK_BT_SPEED_MIN_MM_S      = 80;
//   Cono de "la pelota va hacia el arco": ±45° alrededor del eje del arco propio.
//   Valor 0 desactiva la clasificación de arcos (fallback al comportamiento sin geometría).
constexpr int16_t GK_BT_TOWARD_TOL_CENTIDEG = 4500;

// Respuesta a amenaza (cuando bt_classify detecta BT_TO_OWN_GOAL):
//   Cuando la pelota viene directo al arco propio, el arquero refuerza la respuesta:
//   - MÁS LEAD: re-predice la X de la pelota con 50% más anticipación.
//   - MÁS GANANCIA: el KP de INTERCEPT se multiplica por 1.5x.
//   Si NO hay amenaza, ambos factores valen 1.0 → comportamiento idéntico al anterior.
//   🔧 Afinar en banco: muy alto = sobre-anticipa y deja hueco; muy bajo = no aporta.
constexpr float GK_BT_THREAT_LEAD_FACTOR = 1.5f;
constexpr float GK_BT_THREAT_KP_FACTOR   = 1.5f;


// =============================================================================
// HELPERS INTERNOS
// =============================================================================

// Verifica si los datos de la línea que llegaron del DOWN son recientes.
inline bool line_data_fresh() {
    return world_model_line_is_fresh();
}

// Salida del PID lateral del arquero (mm/s en el eje +X del robot).
//
// Controla qué tan pegado está el arquero a su línea de área.
// Usa cross_track si está disponible (distancia perpendicular firmada a la línea,
// en mm; positivo = línea adelante). Si no, usa la profundidad del blanco como fallback.
// Si los datos de línea no son frescos, devuelve 0 (sin corrección).
inline float gk_lateral_pid_output(uint32_t now_ms) {
    if (!line_data_fresh()) {
        return 0.0f;
    }
    if (world_model_cross_track_valid()) {
        const float cross_track = world_model_get_cross_track_mm();
        lateral_pid_set_target(g_lateral_pid_gk, GK_CROSS_TRACK_SETPOINT_MM);
        return lateral_pid_tick(g_lateral_pid_gk, cross_track, now_ms);
    }
    // Fallback: control por profundidad del sensor de línea.
    const float depth = static_cast<float>(world_model_get_line_depth());
    lateral_pid_set_target(g_lateral_pid_gk, GK_LATERAL_SETPOINT_DEPTH);
    return lateral_pid_tick(g_lateral_pid_gk, depth, now_ms);
}

// Devuelve true si la pose del robot (posición XY) es confiable para usarla como
// límite de patrulla. Requiere snapshot fresco del TOP con confianza mínima.
inline bool gk_pose_ok() {
    return world_model_snapshot_is_fresh()
        && world_model_get_my_pose_confidence() >= GK_POSE_CONF_MIN;
}

// Corrección de rumbo usando solo el giroscopio (GYRO HOLD puro).
// Úsalo cuando NO queremos que la cámara intervenga (por ejemplo, en el retroceso
// a ciegas de GOTO_LINE, donde el ángulo de la cámara trasera nunca fue validado).
//
// target_trim_deg: sesgo opcional del rumbo objetivo (perilla de banco; + = girar CCW/izquierda).
//
// Bail-out de ±45°: si el error de rumbo es muy grande, probablemente el sensor
// esté dando datos basura o el signo esté invertido. En ese caso devuelve 0
// (no corregir a ciegas es mejor que empeorar las cosas).
inline int16_t gk_gyro_hold_omega(uint32_t now_ms, float target_trim_deg) {
    if (!world_model_heading_valid()) return 0;
    const float heading = world_model_get_my_heading_deg();
    const float target  = GK_GYRO_HOLD_TARGET_DEG + target_trim_deg;
    float err = target - heading;
    // Normalizar a [-180, +180] para que el error dé la vuelta correcta.
    while (err > 180.0f)  err -= 360.0f;
    while (err < -180.0f) err += 360.0f;
    if (err > 45.0f || err < -45.0f) return 0;  // bail-out: error grande → no corregir
    heading_pid_set_target(g_heading_pid, target);
    float omega = heading_pid_tick(g_heading_pid, heading, now_ms);
    if (omega >  GK_ORIENT_OMEGA_MAX_DEGPS) omega =  GK_ORIENT_OMEGA_MAX_DEGPS;
    if (omega < -GK_ORIENT_OMEGA_MAX_DEGPS) omega = -GK_ORIENT_OMEGA_MAX_DEGPS;
    return omega_degps_to_centideg(omega);
}

// Corrección de rumbo completa para el arquero: cascada de tres niveles.
//
//   1) Cámara ve el arco propio + heading válido → orientarse de espaldas al arco
//      (referencia absoluta que además corrige el drift acumulado del giroscopio).
//      ⚠️ Deshabilitada por ahora (GK_CAMERA_ORIENT_ENABLED = false) hasta validar
//      el ángulo de la cámara trasera en banco.
//
//   2) Solo heading válido → GYRO HOLD: mantener el rumbo del boot (0° = mirando al frente).
//
//   3) Sin heading válido → no girar (fail-safe).
//
// target_trim_deg: sesgo opcional (perilla de banco; + = CCW/izquierda).
inline int16_t gk_orient_omega(uint32_t now_ms, float target_trim_deg = 0.0f) {
    const float heading = world_model_get_my_heading_deg();
    const GkOwnGoalOrient o = gk_own_goal_orient(
        world_model_goal_own_visible(),
        world_model_heading_valid(),
        heading,
        world_model_get_goal_own_angle_deg());
    float omega = 0.0f;
    if (GK_CAMERA_ORIENT_ENABLED && o.set_heading) {
        // (1) Referencia absoluta por cámara — deshabilitada hasta validar.
        heading_pid_set_target(g_heading_pid, o.heading_target_deg + target_trim_deg);
        omega = heading_pid_tick(g_heading_pid, heading, now_ms);
    } else if (world_model_heading_valid()) {
        // (2) GYRO HOLD: mantener 0° (el frente del boot).
        heading_pid_set_target(g_heading_pid, GK_GYRO_HOLD_TARGET_DEG + target_trim_deg);
        omega = heading_pid_tick(g_heading_pid, heading, now_ms);
    } else {
        return 0;  // (3) Sin heading → no girar.
    }
    if (omega >  GK_ORIENT_OMEGA_MAX_DEGPS) omega =  GK_ORIENT_OMEGA_MAX_DEGPS;
    if (omega < -GK_ORIENT_OMEGA_MAX_DEGPS) omega = -GK_ORIENT_OMEGA_MAX_DEGPS;
    return omega_degps_to_centideg(omega);
}


// =============================================================================
// CLASIFICACIÓN DE TRAYECTORIA DE LA PELOTA (arquero, para INTERCEPT)
// =============================================================================
//
// Analiza hacia dónde va la pelota y decide qué X debe perseguir el arquero.
// Si la pelota viene directo al arco propio (amenaza real), refuerza la respuesta:
//   más anticipación (lead) + más ganancia (KP).
// En cualquier otro caso (pelota quieta, yendo al arco rival, sin geometría
// disponible), el comportamiento es idéntico al que había antes.

struct GkInterceptDecision {
    BallTrajKind kind;         // clasificación de la trayectoria
    float        target_x_mm; // X que el arquero debe perseguir (en mm, marco del robot)
    bool         threat;       // true = la pelota va al arco propio
    float        kp_scale;     // multiplicador de KP (1.0 = sin cambio)
};

// Calcula la X objetivo y el multiplicador de KP según si hay amenaza o no.
// Si threat = false, los factores valen 1.0 y el resultado es idéntico a ball_predict sin amenaza.
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

// Wrapper completo: recibe los datos del world_model, llama a bt_classify,
// y decide la X objetivo + KP.
// Si el arco rival no es visible, desactiva la clasificación de arcos (toward_tol = 0)
// y el resultado es idéntico al comportamiento anterior.
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
    in.reach_mm            = 0;  // el arquero no usa reach; CLEAR gobierna la cercanía
    if (goal_opp_visible) {
        // Arco rival visible → geometría confiable.
        // Normalizar own_deg a (-180, 180] antes de convertir a centideg (evita desborde de int16).
        float own_deg = goal_opp_angle_deg + 180.0f;
        while (own_deg >  180.0f) own_deg -= 360.0f;
        while (own_deg <= -180.0f) own_deg += 360.0f;
        in.goal_opp_angle_centideg = static_cast<int16_t>(lroundf(goal_opp_angle_deg * 100.0f));
        in.goal_own_angle_centideg = static_cast<int16_t>(lroundf(own_deg * 100.0f));
        in.toward_tol_centideg     = GK_BT_TOWARD_TOL_CENTIDEG;
    } else {
        // Sin geometría: desactivar clasificación de arcos.
        in.toward_tol_centideg = 0;
    }
    const BallTraj t = bt_classify(in);
    GkInterceptDecision d{};
    d.kind   = t.kind;
    d.threat = (t.kind == BT_TO_OWN_GOAL);
    const GkThreatResponse tr = gk_threat_response(
        d.threat,
        static_cast<int16_t>(lroundf(bx)), static_cast<int16_t>(lroundf(by)),
        vx, vy, ball_predict_default_params(),
        GK_BT_THREAT_LEAD_FACTOR, GK_BT_THREAT_KP_FACTOR);
    d.target_x_mm = tr.target_x_mm;
    d.kp_scale    = tr.kp_scale;
    (void)bx_pred;  // solo es referencia externa; en no-amenaza d.target_x_mm == bx_pred por construcción
    return d;
}


// =============================================================================
// FUNCIONES DE TRANSICIÓN DE ESTADO
// =============================================================================

// Cambia el estado del delantero y resetea el PID de rumbo.
// El reset evita que el PID arrastre integral acumulada del estado anterior.
void transition_atk(AtkState new_state) {
    if (new_state == g_atk_state) return;
    g_atk_state = new_state;
    heading_pid_reset(g_heading_pid);
}

// Cambia el estado del arquero y resetea los PIDs relevantes.
// El PID lateral solo se resetea si el estado nuevo NO es PATROL ni INTERCEPT
// (esos dos estados comparten el contexto lateral y no deben interrumpirlo).
void transition_gk(GkState new_state) {
    if (new_state == g_gk_state) return;
    g_gk_state = new_state;
    heading_pid_reset(g_heading_pid);
    if (new_state != GkState::PATROL && new_state != GkState::INTERCEPT) {
        lateral_pid_reset(g_lateral_pid_gk);
    }
}


// =============================================================================
// FSM DELANTERO — attacker_tick()
// =============================================================================

// Normaliza un ángulo al rango [-180, +180] grados.
static inline float atk_norm180(float a) {
    while (a > 180.0f)  a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}

// Determina la dirección "hacia el arco rival" relativa al robot.
// Cascada de fuentes, de mejor a peor:
//   1) Cámara ve el arco rival → ángulo directo y confiable.
//   2) Heading del giroscopio válido → heading absoluto = eje de ataque (el robot
//      se enciende mirando al arco rival, así que 0° absoluto ≈ eje de ataque).
//   3) Práctica R1 sin gyro → yaw del OTOS como fallback.
//   4) Sin nada → axis no válido (delantero degrada a APPROACH directo).
struct AttackAxis { bool valid; float rel_deg; };
static AttackAxis atk_attack_axis(float heading) {
    if (world_model_goal_opp_visible()) {
        return {true, world_model_get_goal_opp_angle_deg()};
    }
    if (world_model_heading_valid()) {
        return {true, atk_norm180(-heading)};  // relativo: error hacia el rumbo 0° absoluto
    }
#ifdef ATK_OTOS_NOGYRO
    if (world_model_otos_is_fresh()) {
        return {true, atk_norm180(-world_model_get_otos_heading_deg())};
    }
#endif
    return {false, 0.0f};
}

// ─────────────────────────────────────────────────────────────────────────────
// attacker_tick(): se llama cada 10 ms y devuelve el comando de movimiento.
// ─────────────────────────────────────────────────────────────────────────────
MotorCommand attacker_tick() {
    MotorCommand cmd{};  // empieza en cero: si un estado no escribe nada, el robot queda quieto
    const uint32_t now_ms = millis();
    const float heading = world_model_get_my_heading_deg();

    // Detección del flanco PARADO→CORRIENDO (para entrar a KICKOFF solo una vez al arrancar).
    const bool match_running = world_model_match_running();
    const bool kickoff_edge  = match_running && !g_match_was_running;
    g_match_was_running = match_running;

    // ─── Transiciones globales prioritarias (se evalúan antes del switch) ───
    // Si varias se cumplen al mismo tiempo, gana la más arriba en esta lista.
    if (!match_running) {
        transition_atk(AtkState::WAIT_START);
    } else if (world_model_imminent_exit() && line_data_fresh()) {
        transition_atk(AtkState::LINE_AVOID);
    } else if (kickoff_edge) {
        transition_atk(AtkState::KICKOFF);
        g_kickoff_started_ms = now_ms;
    }

    switch (g_atk_state) {

        // ─── WAIT_START ───────────────────────────────────────────────────────
        // Esperando el GO del árbitro. El robot no hace nada.
        // La salida de este estado la maneja la transición global de arriba (kickoff_edge).
        case AtkState::WAIT_START: {
            g_state_name = "ATK_WAIT_START";
            return cmd;
        }

        // ─── KICKOFF ──────────────────────────────────────────────────────────
        // Al arrancar el partido, un boost recto al frente durante 250 ms.
        // Si hay OTOS disponible, usa drive_straight para cancelar la deriva lateral
        // y salir perfectamente recto. Si no, avanza sin corrección.
        // Después de 250 ms → SEARCH.
        case AtkState::KICKOFF: {
            g_state_name = "ATK_KICKOFF";
            const KickoffVelocity kv = compute_kickoff_velocity(ATK_KICKOFF_SPEED_MM_S);
            heading_pid_set_target(g_heading_pid, heading);  // mantener el ángulo actual, no girar

            if (world_model_otos_is_fresh()) {
                // Con OTOS: corrección de deriva lateral para salir recto.
                // Nota: los ejes del módulo drive_straight están rotados 90° respecto al robot.
                //   out.vx → cmd.vy (avance del robot = +Y)
                //   out.vy → cmd.vx (lateral del robot = +X)
                DriveStraightIn ds_in;
                ds_in.target_heading_deg = world_model_get_otos_heading_deg();
                ds_in.cur_heading_deg    = world_model_get_otos_heading_deg();
                ds_in.otos_vy_mm_s       = world_model_otos_vel_is_fresh()
                                               ? world_model_get_otos_vx_mm_s()
                                               : 0.0f;
                ds_in.fwd_speed_mm_s     = kv.vy_mm_s;
                const DriveStraightCfg ds_cfg{DS_KP_HEADING, DS_KP_LATERAL};
                const DriveStraightCmd ds = drive_straight_compute(ds_in, ds_cfg);
                cmd.vy_mm_s          = static_cast<int16_t>(ds.vx_mm_s);
                cmd.vx_mm_s          = static_cast<int16_t>(ds.vy_mm_s);
                cmd.omega_centideg_s = omega_degps_to_centideg(ds.omega_deg_s);
            } else {
                // Sin OTOS: avance directo sin corrección.
                cmd.vx_mm_s          = static_cast<int16_t>(kv.vx_mm_s);
                cmd.vy_mm_s          = static_cast<int16_t>(kv.vy_mm_s);
                cmd.omega_centideg_s = 0;
            }

            if (now_ms - g_kickoff_started_ms >= ATK_KICKOFF_DURATION_MS) {
                transition_atk(AtkState::SEARCH);
            }
            return cmd;
        }

        // ─── LINE_AVOID ───────────────────────────────────────────────────────
        // Emergencia: el robot está pisando un borde.
        // Se aleja en dirección opuesta a la línea detectada.
        // Cuando imminent_exit se apaga → SEARCH.
        case AtkState::LINE_AVOID: {
            g_state_name = "ATK_LINE_AVOID";
            const float line_angle = world_model_get_line_angle_deg();
            const float retreat    = line_angle + 180.0f;  // dirección opuesta a la línea
            const float rad        = retreat * (M_PI / 180.0f);
            // Convertir ángulo de escape a componentes vx/vy.
            // Nota: en la convención del robot, +Y es adelante y +X es a la derecha.
            // Entonces para un ángulo θ: vx = sin(θ), vy = cos(θ).
            cmd.vx_mm_s = static_cast<int16_t>(std::sin(rad) * ATK_LINE_RETREAT_SPEED);
            cmd.vy_mm_s = static_cast<int16_t>(std::cos(rad) * ATK_LINE_RETREAT_SPEED);
            if (!world_model_imminent_exit()) {
                transition_atk(AtkState::SEARCH);
            }
            return cmd;
        }

        // ─── SEARCH ───────────────────────────────────────────────────────────
        // Busca la pelota: avanza a 200 mm/s girando a 60°/s para barrer el campo.
        // Cuando aparece la pelota, decide si ir directo (APPROACH) o rodearla (POSITION).
        case AtkState::SEARCH: {
            g_state_name = "ATK_SEARCH";
            cmd.vy_mm_s          = static_cast<int16_t>(ATK_SEARCH_VY_MM_S);
            cmd.omega_centideg_s = omega_degps_to_centideg(ATK_SEARCH_OMEGA_DEG_S);
            if (world_model_ball_visible()) {
                const float bx = world_model_get_ball_x_mm();
                const float by = world_model_get_ball_y_mm();
                const AttackAxis ax = atk_attack_axis(heading);
                if (ax.valid) {
                    // Si la pelota ya está alineada con el eje de ataque → ir directo.
                    // Si no → rodearla (behind-the-ball).
                    const bool aligned = ball_is_in_attack_line(bx, by, ax.rel_deg,
                                                                ATK_ATTACK_LINE_TOL_DEG);
                    transition_atk(aligned ? AtkState::APPROACH : AtkState::POSITION);
                } else {
                    // Sin eje de ataque → degradar a APPROACH directo (sin alineación).
                    transition_atk(AtkState::APPROACH);
                }
            }
            return cmd;
        }

        // ─── POSITION ─────────────────────────────────────────────────────────
        // Se mueve al punto 120 mm detrás de la pelota en la línea pelota→arco.
        // Mientras se mueve, también va girando para quedar mirando al arco rival.
        // Cuando llega al target Y está alineado → APPROACH.
        case AtkState::POSITION: {
            g_state_name = "ATK_POSITION";
            if (!world_model_ball_visible()) {
                transition_atk(AtkState::SEARCH);
                return cmd;
            }
            const AttackAxis ax = atk_attack_axis(heading);
            if (!ax.valid) {
                // Sin eje de ataque: no tiene sentido posicionarse → ir directo.
                transition_atk(AtkState::APPROACH);
                return cmd;
            }

            const float bx = world_model_get_ball_x_mm();
            const float by = world_model_get_ball_y_mm();
            const float goal_angle = ax.rel_deg;

            // Punto target: 120 mm detrás de la pelota en la línea pelota→arco.
            const BehindBallTarget tgt = compute_behind_ball_target(
                bx, by, goal_angle, ATK_BEHIND_BALL_GAP_MM);

            // Corrección de rumbo: girar para mirar al arco.
            heading_pid_set_target(g_heading_pid, heading + goal_angle);
            const float omega = central_gate_heading_omega(
                world_model_heading_valid(),
                heading_pid_tick(g_heading_pid, heading, now_ms));

            // Moverse hacia el target con velocidad proporcional a la distancia.
            const float tx    = tgt.target_x_mm;
            const float ty    = tgt.target_y_mm;
            const float tdist = std::sqrt(tx * tx + ty * ty);
            const float speed = approach_velocity(tdist,
                                                  ATK_POSITION_REACHED_MM,
                                                  ATK_APPROACH_FAR_MM,
                                                  ATK_POSITION_MAX_SPEED,
                                                  ATK_APPROACH_MIN_SPEED);
            if (tdist > 1.0f) {
                // tx/tdist y ty/tdist son el vector unitario hacia el target.
                // Multiplicado por speed da las componentes de velocidad en cada eje.
                cmd.vx_mm_s = static_cast<int16_t>(tx / tdist * speed);
                cmd.vy_mm_s = static_cast<int16_t>(ty / tdist * speed);
            }
            cmd.omega_centideg_s = omega_degps_to_centideg(omega);

            // Transición: llegué al target Y estoy alineado con el arco → APPROACH.
            const bool reached = (tdist < ATK_POSITION_REACHED_MM);
            const bool aligned = ball_is_in_attack_line(bx, by, goal_angle,
                                                         ATK_ATTACK_LINE_TOL_DEG);
            if (reached && aligned) {
                transition_atk(AtkState::APPROACH);
            }
            return cmd;
        }

        // ─── APPROACH ─────────────────────────────────────────────────────────
        // Avanza hacia la pelota mirando al arco, con velocidad proporcional a la distancia.
        // Si la pelota se desvía más de 40° del eje → volver a POSITION.
        // Si llega a menos de 80 mm Y está alineado a ±12° → PUSH.
        case AtkState::APPROACH: {
            g_state_name = "ATK_APPROACH";
            const float bx   = world_model_get_ball_x_mm();
            const float by   = world_model_get_ball_y_mm();
            const float dist = std::sqrt(bx * bx + by * by);

            if (!world_model_ball_visible() || dist < 1.0f) {
                transition_atk(AtkState::SEARCH);
                return cmd;
            }

            const AttackAxis ax = atk_attack_axis(heading);
            if (ax.valid) {
                // Si la pelota se desvió más de 40° del eje (30° + 10° de histéresis), rodearla.
                // Los 10° extra evitan oscilar entre APPROACH y POSITION si el ángulo ronda exactamente en el límite.
                if (!ball_is_in_attack_line(bx, by, ax.rel_deg,
                                             ATK_ATTACK_LINE_TOL_DEG + 10.0f)) {
                    transition_atk(AtkState::POSITION);
                    return cmd;
                }

                // Gatillo del empuje: pelota cerca + robot apuntando al arco.
#ifdef ATK_OTOS_NOGYRO
                // Sin gyro: gatillo geométrico (distancia + alineación de la pelota sobre el eje).
                if (dist < ATK_NOGYRO_PUSH_DIST_MM &&
                    ball_is_in_attack_line(bx, by, ax.rel_deg, ATK_NOGYRO_PUSH_TOL_DEG)) {
                    g_atk_push_dir        = nogyro_push_dir(bx, by);
                    g_atk_push_hdg0       = world_model_get_otos_heading_deg();
                    g_atk_push_started_ms = now_ms;
                    transition_atk(AtkState::PUSH);
                    return cmd;
                }
#else
                // Con gyro: gatillo clásico (distancia + ángulo de rumbo).
                if (dist < ATK_KICK_DIST_MM &&
                    std::fabs(ax.rel_deg) <= ATK_KICK_ANGLE_DEG) {
                    g_atk_push_started_ms = now_ms;
                    transition_atk(AtkState::PUSH);
                    return cmd;
                }
#endif
            }

            // Orientar el frente hacia la pelota.
            // atan2(bx, by) y no atan2(by, bx): en nuestro robot +Y es adelante y +X es derecha,
            // lo inverso a la convención matemática estándar.
            const float ball_angle_rel = std::atan2(bx, by) * (180.0f / M_PI);
            const float ball_angle_abs = heading + ball_angle_rel;
            heading_pid_set_target(g_heading_pid, ball_angle_abs);
            const float omega = central_gate_heading_omega(
                world_model_heading_valid(),
                heading_pid_tick(g_heading_pid, heading, now_ms));

            // Velocidad proporcional a la distancia (más rápido lejos, más lento cerca).
            const float speed = approach_velocity(dist,
                                                  ATK_APPROACH_CLOSE_MM,
                                                  ATK_APPROACH_FAR_MM,
                                                  ATK_APPROACH_MAX_SPEED,
                                                  ATK_APPROACH_MIN_SPEED);
            if (dist > 1.0f) {
                cmd.vx_mm_s = static_cast<int16_t>(bx / dist * speed);
                cmd.vy_mm_s = static_cast<int16_t>(by / dist * speed);
            }
            cmd.omega_centideg_s = omega_degps_to_centideg(omega);

            // Corrección adicional de deriva lateral con OTOS (si está disponible).
            // No toca vy ni omega; solo suma una pequeña corrección en vx para salir más recto.
            if (world_model_otos_is_fresh()) {
                DriveStraightIn ds_in;
                ds_in.target_heading_deg = 0.0f;   // omega lo controla el HeadingPID arriba
                ds_in.cur_heading_deg    = 0.0f;
                ds_in.otos_vy_mm_s       = world_model_otos_vel_is_fresh()
                                               ? world_model_get_otos_vx_mm_s()
                                               : 0.0f;
                ds_in.fwd_speed_mm_s     = 0.0f;
                const DriveStraightCfg ds_cfg{DS_KP_HEADING, DS_KP_LATERAL};
                const DriveStraightCmd ds = drive_straight_compute(ds_in, ds_cfg);
                cmd.vx_mm_s = static_cast<int16_t>(
                    static_cast<float>(cmd.vx_mm_s) + ds.vy_mm_s);
            }
            return cmd;
        }

        // ─── PUSH ─────────────────────────────────────────────────────────────
        // Empuje comprometido: full adelante 500 ms sin mirar la pelota.
        // A esa distancia la cámara la pierde contra el paragolpes, así que el robot
        // solo mantiene el rumbo hacia el arco con el giroscopio.
        // Después de 500 ms → PUSH_BACK.
        case AtkState::PUSH: {
            g_state_name = "ATK_PUSH";
#ifdef ATK_OTOS_NOGYRO
            // Sin gyro: usar la dirección congelada al comprometerse + mantener rumbo con OTOS.
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
            // Con gyro: full adelante manteniendo el rumbo hacia el arco.
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
                g_atk_push_started_ms = now_ms;  // re-armar el timer para PUSH_BACK
                transition_atk(AtkState::PUSH_BACK);
            }
            return cmd;
        }

        // ─── PUSH_BACK ────────────────────────────────────────────────────────
        // Retroceso corto post-empuje: despegarse de la pelota y de la línea del área
        // antes de volver a buscar. Evita quedar pegado al borde.
        // Después de 250 ms → SEARCH.
        case AtkState::PUSH_BACK: {
            g_state_name = "ATK_PUSH_BACK";
#ifdef ATK_OTOS_NOGYRO
            // Sin gyro: retroceder por el mismo eje del empuje (deshacer el camino).
            cmd.vx_mm_s = static_cast<int16_t>(-g_atk_push_dir.ux * ATK_PUSH_BACK_SPEED_MM_S);
            cmd.vy_mm_s = static_cast<int16_t>(-g_atk_push_dir.uy * ATK_PUSH_BACK_SPEED_MM_S);
#else
            // Con gyro: retroceder recto hacia atrás (-Y).
            cmd.vy_mm_s = static_cast<int16_t>(-ATK_PUSH_BACK_SPEED_MM_S);
#endif
            if (now_ms - g_atk_push_started_ms >= ATK_PUSH_BACK_MS) {
                transition_atk(AtkState::SEARCH);
            }
            return cmd;
        }

    }  // switch (g_atk_state)

    return cmd;
}


// =============================================================================
// FSM ARQUERO — goalkeeper_tick()
// =============================================================================

MotorCommand goalkeeper_tick() {
    MotorCommand cmd{};
    const uint32_t now_ms = millis();

#ifdef GK_SIMPLE_STRAFE
    // ─────────────────────────────────────────────────────────────────────────
    // ARQUERO SIMPLE (pedido María, práctica 2026-06-12)
    // ─────────────────────────────────────────────────────────────────────────
    // Versión más directa del arquero, sin la patrulla segmentada v3.3.
    //
    // Flujo:
    //   1. Al arrancar: retrocede recto hasta tocar su línea de área (fase GOTO_BACK).
    //   2. Avanza ~10 cm para despegarse (fase ADVANCE).
    //   3. Strafe lateral continuo de lado a lado (fase MOVE), rebotando en las líneas.
    //      El frente se mantiene con control PI+PFM (control por pulsos) que funciona
    //      bien en movimiento continuo sin descontrolar el strafe.
    //
    // Por qué PI+PFM y no PID clásico:
    //   A 200 mm/s el robot está en "régimen cuantizado" por los pisos de PWM.
    //   El PID clásico fallaba: capado a 40°/s perdía contra la deriva parásita (~80°/s);
    //   capado a 120°/s oscilaba ±140°. El PFM entrega la corrección como pulsos de
    //   magnitud fija repartidos en el tiempo → el promedio temporal da la corrección
    //   fina que los motores no pueden hacer de forma continua.
    {
        constexpr float    GKS_RESQUARE_DEG    = 45.0f;   // error que dispara la red de seguridad (re-escuadre parado)
        constexpr float    GKS_RESQUARE_EXIT   = 12.0f;   // corte en vivo del re-escuadre
        constexpr uint32_t GKS_RESQUARE_MAX_MS = 900;     // tope del re-escuadre parado
        constexpr uint32_t GKS_SETTLE_MS       = 300;     // quieto post-escuadre
        constexpr uint32_t GKS_ESCAPE_MS       = 600;     // huida post-línea (~12 cm a 200 mm/s)

        // Variables persistentes entre ticks (static = sobreviven entre llamadas).
        static int             dir_simple    = +1;  // dirección del strafe: +1=derecha, -1=izquierda
        static uint32_t        bounce_gate   = 0;   // cooldown anti-rebote-múltiple por toque de línea
        static uint8_t         phase         = 5;   // fase actual (5=GOTO_BACK, 6=ADVANCE, 0=MOVE, 2=RESQUARE, 3=SETTLE, 4=ESCAPE)
        static uint32_t        phase_t0      = 0;   // timestamp de entrada a la fase actual
        static uint32_t        goto_clear_ms = 0;   // en ADVANCE: desde cuándo la línea dejó de verse
        static PfmHeadingState pfm{};               // estado interno del PI+PFM (integrador + ventana de pulsos)

        if (!world_model_match_running()) {
            g_state_name    = "GK_SIMPLE_WAIT";
            dir_simple      = +1;
            bounce_gate     = 0;
            phase           = 5;
            phase_t0        = now_ms;
            goto_clear_ms   = 0;
            pfm_heading_reset(pfm);
            return cmd;  // ceros → quieto
        }
        if (phase_t0 == 0) phase_t0 = now_ms;

        // Error de rumbo respecto al arco rival (= 0° del boot).
        const bool  hv          = world_model_heading_valid();
        const float heading_now = world_model_get_my_heading_deg();
        float hdg_err = 0.0f;
        if (hv) {
            hdg_err = GK_GYRO_HOLD_TARGET_DEG - heading_now;
            while (hdg_err > 180.0f)  hdg_err -= 360.0f;
            while (hdg_err < -180.0f) hdg_err += 360.0f;
        }
        const float aerr = (hdg_err < 0.0f) ? -hdg_err : hdg_err;

        // Función lambda: calcula el omega del PI+PFM para este tick.
        auto pfm_omega = [&]() -> int16_t {
            const float w = pfm_heading_tick(pfm, pfm_heading_default_cfg(),
                                             hdg_err, hv, now_ms, 0.01f);
            return omega_degps_to_centideg(w);
        };

        switch (phase) {
            case 5: {
                // ─── GOTO_BACK: retrocede recto hasta tocar la línea de área ───
                g_state_name    = "GK_SIMPLE_GOTO_BACK";
                cmd.vx_mm_s     = clamp_velocity_mm_s(
                    static_cast<float>(GK_GOTO_LINE_VX_RIGHT) + GK_GOTO_LINE_VX_TRIM_MM_S);
                cmd.vy_mm_s     = clamp_velocity_mm_s(-static_cast<float>(GK_GOTO_LINE_VY_BACK));
                cmd.omega_centideg_s = gk_gyro_hold_omega(now_ms, GK_GOTO_LINE_HEADING_TRIM_DEG);
                const bool line_here = world_model_line_detected() && world_model_line_data_valid();
                const bool to        = (now_ms - phase_t0) >= GK_GOTO_LINE_TIMEOUT_MS;
                if (line_here || to) { phase = 6; phase_t0 = now_ms; goto_clear_ms = 0; }
                break;
            }
            case 6: {
                // ─── ADVANCE: avanza hasta dejar de ver la línea + margen ───
                g_state_name         = "GK_SIMPLE_ADVANCE";
                cmd.vx_mm_s          = 0;
                cmd.vy_mm_s          = clamp_velocity_mm_s(static_cast<float>(GK_ADVANCE_SPEED_MM_S));
                cmd.omega_centideg_s = gk_gyro_hold_omega(now_ms, 0.0f);
                if (!world_model_line_detected()) {
                    if (goto_clear_ms == 0) goto_clear_ms = now_ms;
                } else {
                    goto_clear_ms = 0;  // sigue viéndola → reiniciar el margen
                }
                const bool cleared = (goto_clear_ms != 0) && (now_ms - goto_clear_ms) >= GK_ADVANCE_MS;
                const bool to      = (now_ms - phase_t0) >= GK_ADVANCE_TIMEOUT_MS;
                if (cleared || to) { phase = 0; phase_t0 = now_ms; }
                break;
            }
            case 0: {
                // ─── MOVE: strafe lateral continuo con PI+PFM ───
                g_state_name = "GK_SIMPLE_MOVE";
                // Red de seguridad: si el error de rumbo creció demasiado → parar y re-escuadrarse.
                if (hv && aerr > GKS_RESQUARE_DEG) {
                    phase = 2; phase_t0 = now_ms;
                    break;
                }
                // Rebote por línea lateral.
                // Convención: línea a la derecha (+) → ir a la izquierda (-1); al revés (+1).
                if (line_data_fresh() && world_model_line_detected() &&
                    (now_ms - bounce_gate) >= GK_PATROL_BOUNCE_COOLDOWN_MS) {
                    const float la       = world_model_get_line_angle_deg();
                    const bool  la_behind = (la > 135.0f || la < -135.0f);
                    if (!la_behind) {
                        dir_simple  = (la >= 0.0f) ? -1 : +1;
                        bounce_gate = now_ms;
                        phase = 4; phase_t0 = now_ms;  // primero escapar de la línea, luego continuar
                        break;
                    }
                }
                cmd.vx_mm_s          = clamp_velocity_mm_s(dir_simple * GK_PATROL_SPEED_MM_S);
                cmd.vy_mm_s          = 0;
                cmd.omega_centideg_s = pfm_omega();
                break;
            }
            case 4: {
                // ─── ESCAPE: alejarse de la línea antes de retomar el strafe ───
                // El robot no lee sensores de línea mientras escapa (sobre la línea las
                // lecturas confunden). El PI+PFM sigue activo para no girar durante la huida.
                g_state_name         = "GK_SIMPLE_ESCAPE";
                cmd.vx_mm_s          = clamp_velocity_mm_s(dir_simple * GK_PATROL_SPEED_MM_S);
                cmd.vy_mm_s          = 0;
                cmd.omega_centideg_s = pfm_omega();
                if ((now_ms - phase_t0) >= GKS_ESCAPE_MS) { phase = 0; phase_t0 = now_ms; }
                break;
            }
            case 2: {
                // ─── RESQUARE: parado, girando para quedar de frente ───
                // Sin strafe que pelear, el giro parado es el movimiento más confiable.
                // Corte en vivo a 12° para anticipar la inercia.
                g_state_name         = "GK_SIMPLE_RESQUARE";
                cmd.omega_centideg_s = pfm_omega();
                if (aerr <= GKS_RESQUARE_EXIT || (now_ms - phase_t0) >= GKS_RESQUARE_MAX_MS) {
                    phase = 3; phase_t0 = now_ms;
                }
                break;
            }
            case 3: {
                // ─── SETTLE: quieto un instante y retomar el strafe ───
                g_state_name = "GK_SIMPLE_SETTLE";
                if ((now_ms - phase_t0) >= GKS_SETTLE_MS) { phase = 0; phase_t0 = now_ms; }
                break;
            }
        }
        return cmd;
    }
#endif  // GK_SIMPLE_STRAFE

    // ─── Transiciones globales prioritarias (arquero completo) ───────────────

    if (!world_model_match_running()) {
        transition_gk(GkState::WAIT_START);
    } else if (world_model_imminent_exit() && line_data_fresh()) {
        // Anti-flapping: el arquero roza su propia línea por diseño, así que un
        // roce no dispara la huida. Solo se activa LINE_AVOID si imminent_exit
        // persiste al menos GK_LINE_AVOID_DEBOUNCE_MS milisegundos Y pasó el
        // cooldown desde la última vuelta a PATROL.
        if (g_gk_imminent_since_ms == 0) g_gk_imminent_since_ms = now_ms;
        const bool persisted = (now_ms - g_gk_imminent_since_ms) >= GK_LINE_AVOID_DEBOUNCE_MS;
        const bool cooled    = (now_ms - g_gk_line_avoid_gate_ms) >= GK_LINE_AVOID_COOLDOWN_MS;
        if (persisted && cooled && g_gk_state != GkState::GOTO_LINE) {
            // La línea del arquero siempre está atrás, así que la respuesta es avanzar
            // (~10 cm, fase 1 de GOTO_LINE), no el retreat por ángulo de LINE_AVOID
            // (con los pisos de PWM ese retreat salía en direcciones impredecibles).
            // Excepción: si la línea está al costado durante la patrulla, el rebote
            // del MOVE ya la maneja. Solo avanzar si la línea está realmente atrás.
            const float la_imm   = world_model_get_line_angle_deg();
            const bool  la_behind = (la_imm > 135.0f || la_imm < -135.0f);
            if (la_behind || g_gk_state != GkState::PATROL) {
                g_goto_line_phase       = 1;
                g_gk_advance_started_ms = now_ms;
                transition_gk(GkState::GOTO_LINE);
            }
        }
    } else {
        g_gk_imminent_since_ms = 0;  // imminent_exit se apagó → re-armar el debounce
    }

    switch (g_gk_state) {

        // ─── WAIT_START ───────────────────────────────────────────────────────
        // Esperando el GO del árbitro.
        // Cuando llega, espera 2 s antes de moverse para dar tiempo a posicionar el robot.
        // Después de 2 s → GOTO_LINE (NO va directo a PATROL; primero se reposiciona).
        case GkState::WAIT_START: {
            g_state_name = "GK_WAIT_START";
            if (world_model_match_running()) {
                if (g_gk_start_seen_ms == 0) g_gk_start_seen_ms = now_ms;
                if ((now_ms - g_gk_start_seen_ms) >= GK_START_DELAY_MS) {
                    transition_gk(GkState::GOTO_LINE);
                    g_goto_line_started_ms = now_ms;
                    g_goto_line_phase      = 0;  // arrancar con la fase de retroceso
                }
            } else {
                g_gk_start_seen_ms = 0;  // si el árbitro para, resetear el delay
            }
            return cmd;
        }

        // ─── GOTO_LINE ────────────────────────────────────────────────────────
        // Reposicionamiento en dos fases:
        //
        //   Fase 0 (retroceso): va hacia atrás a 420 mm/s con el giroscopio manteniéndolo
        //     recto. Sale cuando los sensores ven la línea de área o después de 4 s.
        //
        //   Fase 1 (despegue): avanza a 300 mm/s hasta que la línea deja de verse durante
        //     100 ms seguidos. Sale a PATROL y captura la X actual como centro de patrulla.
        //
        // El despegue es necesario porque patrullar SOBRE la línea satura el PID de borde
        // y el robot gira sobre su eje sin control.
        case GkState::GOTO_LINE: {
            if (g_goto_line_phase == 0) {
                // ─── Fase 0: retroceso recto con GYRO HOLD ───
                g_state_name = "GK_GOTO_LINE";
                cmd.vx_mm_s  = static_cast<int16_t>(
                    static_cast<float>(GK_GOTO_LINE_VX_RIGHT) + GK_GOTO_LINE_VX_TRIM_MM_S);
                cmd.vy_mm_s  = static_cast<int16_t>(-GK_GOTO_LINE_VY_BACK);  // -Y = atrás
                // Solo giroscopio (no cámara) en la reversa: el ángulo de la cámara trasera
                // nunca fue validado y causó trayectorias en J durante el banco.
                cmd.omega_centideg_s = gk_gyro_hold_omega(now_ms, GK_GOTO_LINE_HEADING_TRIM_DEG);

                const bool line_here = world_model_line_detected() && world_model_line_data_valid();
                const bool timed_out = (now_ms - g_goto_line_started_ms) >= GK_GOTO_LINE_TIMEOUT_MS;
                if (line_here || timed_out) {
                    g_goto_line_phase       = 1;
                    g_gk_advance_started_ms = now_ms;
                }
            } else {
                // ─── Fase 1: avance hasta despegarse de la línea ───
                g_state_name         = "GK_ADVANCE";
                cmd.vx_mm_s          = 0;
                cmd.vy_mm_s          = GK_ADVANCE_SPEED_MM_S;   // +Y = adelante
                cmd.omega_centideg_s = gk_gyro_hold_omega(now_ms, 0.0f);

                // Medir cuándo la línea deja de verse de forma continua.
                // g_gk_advance_clear_ms se resetea si la línea vuelve a aparecer.
                if (!world_model_line_detected()) {
                    if (g_gk_advance_clear_ms == 0) g_gk_advance_clear_ms = now_ms;
                } else {
                    g_gk_advance_clear_ms = 0;  // todavía la ve → reiniciar el margen
                }
                const bool cleared     = (g_gk_advance_clear_ms != 0)
                                         && (now_ms - g_gk_advance_clear_ms) >= GK_ADVANCE_MS;
                const bool adv_timeout = (now_ms - g_gk_advance_started_ms) >= GK_ADVANCE_TIMEOUT_MS;

                if (cleared || adv_timeout) {
                    g_gk_advance_clear_ms   = 0;
                    g_gk_line_avoid_gate_ms = now_ms;  // gracia: sin re-disparar LINE_AVOID inmediatamente
                    // Capturar el centro de patrulla: el robot está en su puesto,
                    // recién despegado de la línea y frente al arco.
                    if (gk_pose_ok()) {
                        g_gk_patrol_x_center = world_model_get_my_x_mm();
                    }
                    transition_gk(GkState::PATROL);
                }
            }
            return cmd;
        }

        // ─── LINE_AVOID ───────────────────────────────────────────────────────
        // Solo se usa si la línea está al costado (no atrás).
        // Se aleja en dirección opuesta a la línea detectada y vuelve a PATROL.
        case GkState::LINE_AVOID: {
            g_state_name = "GK_LINE_AVOID";
            const float line_angle = world_model_get_line_angle_deg();
            const float retreat    = line_angle + 180.0f;
            const float rad        = retreat * (M_PI / 180.0f);
            cmd.vx_mm_s = static_cast<int16_t>(std::sin(rad) * GK_LINE_RETREAT_SPEED);
            cmd.vy_mm_s = static_cast<int16_t>(std::cos(rad) * GK_LINE_RETREAT_SPEED);
            if (!world_model_imminent_exit()) {
                g_gk_line_avoid_gate_ms = now_ms;  // cooldown: no re-dispararse inmediatamente
                transition_gk(GkState::PATROL);
            }
            return cmd;
        }

        // ─── PATROL ───────────────────────────────────────────────────────────
        // Patrulla lateral v3 segmentada (mover → parar → corregir rumbo → mover).
        //
        // Sub-fases internas:
        //   0 MOVE:    tramo de strafe puro (ω=0) hasta tocar la línea lateral o cumplir el timer.
        //   1 STOP:    frenar 300 ms y medir el rumbo quieto.
        //   2 PULSO:   rotación pura breve si quedó chueco (>35°).
        //   3 ASENTAR: quieto 700 ms post-pulso para que la inercia termine.
        //   4 REACQ:   retroceder despacio hasta re-ver la línea (si se perdió entre tramos).
        //
        // Por qué ω=0 en el tramo:
        //   Mezclar corrección de giro con strafe degrada la dirección con estos motores.
        //   Las ruedas delanteras van con PWM bajo (~25 crudo) y cualquier ω las desborda.
        //   El strafe puro cae en la mezcla validada {70, 70, 107} que va derecho.
        //   El rumbo se corrige solo cuando el robot está PARADO.
        case GkState::PATROL: {
            // Variables persistentes entre ticks.
            static int      direction      = 1;   // +1=derecha, -1=izquierda
            static uint8_t  pphase         = 0;   // sub-fase actual
            static uint32_t pphase_t0      = 0;   // timestamp de entrada a la sub-fase
            static uint32_t pulse_ms       = 0;   // duración calculada del pulso de corrección
            static uint8_t  pulse_count    = 0;   // pulsos aplicados en esta parada
            static uint8_t  same_dir_segs  = 0;   // tramos consecutivos en el mismo sentido (fail-safe)
            static uint32_t bounce_gate_ms = 0;   // cooldown del rebote por línea
            static uint8_t  reacq_dry      = 0;   // veces seguidas que el re-enganche no encontró línea

            if (pphase_t0 == 0) pphase_t0 = now_ms;

            // Nombre de sub-fase para el debug serial.
            static const char* kPatrolPhaseName[5] =
                {"GK_PATROL_MOVE", "GK_PATROL_STOP", "GK_PATROL_PULSE",
                 "GK_PATROL_SETTLE", "GK_PATROL_REACQ"};
            g_state_name = kPatrolPhaseName[(pphase < 5) ? pphase : 0];

            // Centro de la ventana de patrulla.
            const float xc = (g_gk_patrol_x_center >= 0.0f)
                           ? g_gk_patrol_x_center : GK_PATROL_X_CENTER_MM;

            // Error de rumbo actual (normalizado a ±180°).
            const bool hv = world_model_heading_valid();
            float hdg_err = 0.0f;
            if (hv) {
                hdg_err = GK_GYRO_HOLD_TARGET_DEG - world_model_get_my_heading_deg();
                while (hdg_err > 180.0f)  hdg_err -= 360.0f;
                while (hdg_err < -180.0f) hdg_err += 360.0f;
            }

            switch (pphase) {
                case 0: {
                    // ─── MOVE: strafe puro (ω=0) ───
                    // El tramo continúa hasta tocar la línea lateral O cumplir el timer O llegar al límite de pose.
                    // Línea atrás (~±180°) es la del área: NO rebota aquí (la maneja el router global).
                    if (line_data_fresh() && world_model_line_detected() &&
                        (now_ms - bounce_gate_ms) >= GK_PATROL_BOUNCE_COOLDOWN_MS) {
                        const float la       = world_model_get_line_angle_deg();
                        const bool  la_behind = (la > 135.0f || la < -135.0f);
                        if (!la_behind) {
                            // Línea lateral: rebotar al lado opuesto.
                            direction      = (la >= 0.0f) ? -1 : +1;
                            bounce_gate_ms = now_ms;
                            same_dir_segs  = 0;
                            pphase = 1; pphase_t0 = now_ms;
                            break;
                        }
                    }
                    cmd.vx_mm_s          = clamp_velocity_mm_s(direction * GK_PATROL_SPEED_MM_S);
                    cmd.vy_mm_s          = 0;
                    cmd.omega_centideg_s = 0;  // NO corregir el rumbo en movimiento (ver arriba)
                    bool seg_end = (now_ms - pphase_t0) >= GK_PATROL_SEG_MS;
                    // Límite por pose: si tenemos posición confiable, no pasar del borde del arco.
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
                case 1: {
                    // ─── STOP: frenar y medir el rumbo quieto ───
                    // cmd = 0 → quieto. Después de 300 ms decidir:
                    //   - Si quedó chueco (>35°) → aplicar un pulso de corrección.
                    //   - Si está derecho → arrancar el próximo tramo.
                    if ((now_ms - pphase_t0) >= GK_PATROL_STOP_MS) {
                        const float aerr = (hdg_err < 0.0f) ? -hdg_err : hdg_err;
                        if (hv && aerr > GK_REORIENT_ENTER_DEG &&
                            pulse_count < GK_REORIENT_MAX_PULSES) {
                            // Calcular duración del pulso proporcional al error (mínimo/máximo acotados).
                            float ms = aerr * GK_REORIENT_MS_PER_DEG;
                            if (ms < GK_REORIENT_PULSE_MIN_MS) ms = GK_REORIENT_PULSE_MIN_MS;
                            if (ms > GK_REORIENT_PULSE_MAX_MS) ms = GK_REORIENT_PULSE_MAX_MS;
                            pulse_ms = static_cast<uint32_t>(ms);
                            pphase = 2; pphase_t0 = now_ms;
                        } else {
                            // Derecho (o agotó los pulsos) → próximo tramo.
                            pulse_count = 0;
                            bool flipped = false;
                            // Dirección del próximo tramo: seguir en la misma dirección hasta
                            // tocar la línea. Solo cambiar si la pose dice que llegamos al borde.
                            if (gk_pose_ok()) {
                                const float x = world_model_get_my_x_mm();
                                if (x > xc + GK_PATROL_X_HALF_RANGE_MM)      { direction = -1; flipped = true; }
                                else if (x < xc - GK_PATROL_X_HALF_RANGE_MM) { direction = +1; flipped = true; }
                            }
                            if (flipped) {
                                same_dir_segs = 0;
                            } else if (++same_dir_segs >= GK_PATROL_MAX_SEGS_SAME_DIR) {
                                // Fail-safe: sin línea ni pose después de 3 tramos, invertir igualmente.
                                same_dir_segs = 0;
                                direction = -direction;
                            }
                            // Antes de arrancar el tramo, verificar que la línea está a la vista.
                            // Sin línea visible el rebote no puede guiar la patrulla.
                            // Guard: si el re-enganche falló 2 veces seguidas (sensor caído), no insistir.
                            const bool line_in_view = line_data_fresh() && world_model_line_detected();
                            if (line_in_view) reacq_dry = 0;
                            pphase    = (line_in_view || reacq_dry >= 2) ? 0 : 4;
                            pphase_t0 = now_ms;
                        }
                    }
                    break;
                }
                case 2: {
                    // ─── PULSO: rotación pura breve hacia el frente ───
                    // La rotación real del robot con estos pisos es ~300°/s (mínimo físico).
                    // Por eso los pulsos son muy cortos y se cortan EN VIVO cuando el error
                    // baja de 25°, para anticipar la inercia que sigue después de soltar.
                    cmd.omega_centideg_s = static_cast<int16_t>(
                        (hdg_err >= 0.0f ? +1 : -1) * GK_ORIENT_OMEGA_MAX_DEGPS * 100.0f);
                    const float aerr        = (hdg_err < 0.0f) ? -hdg_err : hdg_err;
                    const bool  close_enough = hv && (aerr <= GK_REORIENT_EXIT_DEG);
                    if (close_enough || (now_ms - pphase_t0) >= pulse_ms) {
                        ++pulse_count;
                        pphase = 3; pphase_t0 = now_ms;
                    }
                    break;
                }
                case 3: {
                    // ─── ASENTAR: quieto post-pulso ───
                    // 700 ms para que la inercia termine Y lleguen datos frescos del TOP
                    // (el TOP llega a ~4 Hz, es decir un dato cada 250 ms).
                    if ((now_ms - pphase_t0) >= GK_REORIENT_SETTLE_MS) {
                        pphase = 1; pphase_t0 = now_ms;  // volver a STOP y re-evaluar
                    }
                    break;
                }
                case 4: {
                    // ─── REACQ: retroceder despacio hasta re-ver la línea ───
                    // La patrulla vive pegada a la línea; sin verla, el rebote lateral no funciona.
                    cmd.vx_mm_s          = 0;
                    cmd.vy_mm_s          = static_cast<int16_t>(-GK_PATROL_REACQ_VY_MM_S);  // -Y = atrás
                    cmd.omega_centideg_s = gk_gyro_hold_omega(now_ms, 0.0f);
                    const bool touched = line_data_fresh() && world_model_line_detected();
                    if (touched) {
                        reacq_dry = 0;
                        pphase = 0; pphase_t0 = now_ms;  // línea al borde → arrancar tramo lateral
                    } else if ((now_ms - pphase_t0) >= GK_PATROL_REACQ_MAX_MS) {
                        ++reacq_dry;   // venció el timeout sin hallar línea
                        pphase = 0; pphase_t0 = now_ms;
                    }
                    break;
                }
            }  // switch (pphase)

#ifndef GK_IGNORE_BALL
            // Si la pelota aparece → salir a interceptarla.
            // (Con -DGK_IGNORE_BALL este bloque se omite, para testear la patrulla sola.)
            if (world_model_ball_visible()) {
                transition_gk(GkState::INTERCEPT);
            }
#endif
            return cmd;
        }

        // ─── INTERCEPT ────────────────────────────────────────────────────────
        // El arquero se mueve lateralmente para interponerse entre la pelota y el arco.
        //
        // En vez de seguir la X actual de la pelota, predice dónde va a estar
        // (ball_predict) y se mueve hacia esa X predicha.
        //
        // Si la pelota viene directo al arco propio (clasificado como BT_TO_OWN_GOAL),
        // refuerza la respuesta: más anticipación + más ganancia.
        //
        // Límites: si tiene pose confiable, no se va más allá del ancho del arco.
        //
        // Transiciones:
        //   pelota a < 250 mm → CLEAR (ir a despejarla)
        //   pelota desapareció → PATROL
        case GkState::INTERCEPT: {
            g_state_name = "GK_INTERCEPT";
            const float bx   = world_model_get_ball_x_mm();
            const float by   = world_model_get_ball_y_mm();
            const float dist = std::sqrt(bx * bx + by * by);

            // X predicha: dónde estará la pelota con su velocidad actual.
            const float bx_pred = ball_predict(world_model_get_ball_x_mm(),
                                               world_model_get_ball_y_mm(),
                                               world_model_get_ball_vx_mm_s(),
                                               world_model_get_ball_vy_mm_s(),
                                               ball_predict_default_params()).px_mm;

            // Clasificación de trayectoria y decisión de X objetivo + KP.
            const GkInterceptDecision bt = gk_classify_intercept(
                bx, by, bx_pred,
                world_model_get_ball_vx_mm_s(),
                world_model_get_ball_vy_mm_s(),
                world_model_goal_opp_visible(),
                world_model_goal_opp_visible() ? world_model_get_goal_opp_angle_deg() : 0.0f,
                dist);
            (void)bt.kind;  // disponible para observabilidad/debug

            // Control proporcional: vx = X_objetivo × KP.
            const float kp_intercept = GK_INTERCEPT_KP_VS_BALL_X * bt.kp_scale;
            const float vx_intercept = bt.target_x_mm * kp_intercept;
            cmd.vx_mm_s = clamp_velocity_mm_s(vx_intercept);
            cmd.vy_mm_s = 0;  // sin avance/retroceso en INTERCEPT

            // Límite por pose: no ir más allá del borde del arco.
            if (gk_pose_ok()) {
                const float x  = world_model_get_my_x_mm();
                const float xc = (g_gk_patrol_x_center >= 0.0f)
                               ? g_gk_patrol_x_center : GK_PATROL_X_CENTER_MM;
                if ((x > xc + GK_PATROL_X_HALF_RANGE_MM && cmd.vx_mm_s > 0) ||
                    (x < xc - GK_PATROL_X_HALF_RANGE_MM && cmd.vx_mm_s < 0)) {
                    cmd.vx_mm_s = 0;
                }
            }

            cmd.omega_centideg_s = gk_orient_omega(now_ms);

            if (dist < GK_CLEAR_TRIGGER_MM) {
                transition_gk(GkState::CLEAR);
            } else if (!world_model_ball_visible()) {
                transition_gk(GkState::PATROL);
            }
            return cmd;
        }

        // ─── CLEAR ────────────────────────────────────────────────────────────
        // La pelota está muy cerca (<250 mm): ir directo a ella a 500 mm/s.
        // El control es puramente en velocidad hacia la pelota (vector unitario × velocidad).
        // El robot no tiene kicker físico, así que empuja la pelota por inercia.
        //
        // Histéresis de 400 mm > 250 mm: evita entrar y salir repetidamente si la
        // pelota ronda en el límite.
        //
        // Transiciones:
        //   pelota se alejó (>400 mm) → INTERCEPT
        //   pelota desapareció → PATROL
        case GkState::CLEAR: {
            g_state_name = "GK_CLEAR";
            if (!world_model_ball_visible()) {
                transition_gk(GkState::PATROL);
                return cmd;
            }
            const float bx   = world_model_get_ball_x_mm();
            const float by   = world_model_get_ball_y_mm();
            const float dist = std::sqrt(bx * bx + by * by);

            if (dist > GK_CLEAR_RELEASE_MM) {
                transition_gk(GkState::INTERCEPT);
                return cmd;
            }

            if (dist > 1.0f) {
                // Vector unitario hacia la pelota × velocidad.
                cmd.vx_mm_s = static_cast<int16_t>(bx / dist * GK_CLEAR_SPEED_MM_S);
                cmd.vy_mm_s = static_cast<int16_t>(by / dist * GK_CLEAR_SPEED_MM_S);
            }

            // Orientar el frente hacia la pelota para que el despeje salga con el paragolpes.
            const float ball_angle_rel = std::atan2(bx, by) * (180.0f / M_PI);
            heading_pid_set_target(g_heading_pid,
                                   world_model_get_my_heading_deg() + ball_angle_rel);
            float omega = central_gate_heading_omega(
                world_model_heading_valid(),
                heading_pid_tick(g_heading_pid,
                                 world_model_get_my_heading_deg(),
                                 now_ms));
            // Acotar el giro para no hacer trompos mientras se va hacia la pelota.
            if (omega >  GK_ORIENT_OMEGA_MAX_DEGPS) omega =  GK_ORIENT_OMEGA_MAX_DEGPS;
            if (omega < -GK_ORIENT_OMEGA_MAX_DEGPS) omega = -GK_ORIENT_OMEGA_MAX_DEGPS;
            cmd.omega_centideg_s = omega_degps_to_centideg(omega);
            return cmd;
        }

    }  // switch (g_gk_state)

    return cmd;
}

}  // namespace (anónimo)


// =============================================================================
// FUNCIONES PÚBLICAS
// =============================================================================

void strategy_init() {
    g_state_name         = "INIT";
    g_atk_state          = AtkState::WAIT_START;
    g_gk_state           = GkState::WAIT_START;
    g_match_was_running  = false;
    g_kickoff_started_ms = 0;
    g_goto_line_started_ms = 0;
    heading_pid_reset(g_heading_pid);
    lateral_pid_reset(g_lateral_pid_gk);
}

// Punto de entrada principal: se llama desde main_central.cpp cada 10 ms.
// Llama a attacker_tick() o goalkeeper_tick() según el rol configurado.
MotorCommand strategy_tick() {
    MotorCommand cmd = (g_role == RobotRole::ATTACKER) ? attacker_tick()
                                                       : goalkeeper_tick();
#ifdef ATK_OBSTACLE_STOP_MM
    // Freno anti-choque: si hay un obstáculo muy cerca, eliminar el componente de avance.
    // Solo para el delantero: el arquero patrulla con la pared del arco atrás y el
    // mínimo de distancia (sin dirección) lo dejaría mudo contra esa pared.
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

// Setters y getters del rol y color de ataque (llamados desde main_central.cpp al arrancar).
void        strategy_set_role(RobotRole role)         { g_role = role; }
RobotRole   strategy_get_role()                       { return g_role; }
void        strategy_set_attack_color(AttackColor c)  { g_attack_color = c; }
AttackColor strategy_get_attack_color()               { return g_attack_color; }
const char* strategy_get_state_name()                 { return g_state_name; }

}  // namespace iitasoccer
