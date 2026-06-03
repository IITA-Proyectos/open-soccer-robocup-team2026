// diag_central_arbitro_strafe.cpp — Patrulla lateral del ARQUERO gatillada por el ÁRBITRO.
//
// Qué hace (lo pedido):
//   - Espera el START del árbitro.
//   - Al recibir START: mueve ~30 cm a la IZQUIERDA -> para 3 s -> ~30 cm a la
//     DERECHA -> para 3 s -> y así sucesivamente (patrulla lateral del arquero).
//   - Al recibir STOP: FRENA y queda esperando.
//   - Al recibir START otra vez: retoma la patrulla desde el principio (IZQ).
//
// De dónde sale el START/STOP del árbitro (importante):
//   La CENTRAL NO recibe el árbitro directo. El árbitro entra como NIVEL GPIO en
//   los pines 5/6 del TOP; el TOP arma `match_running` y lo manda en el flag
//   MATCH_RUNNING del WorldSnapshot. La CENTRAL lo lee por Serial7 (pin 28) vía
//   comm_top -> world_model -> world_model_match_running().
//   => CADENA NECESARIA:  COMM --(GPIO 5/6)--> TOP --(WorldSnapshot, Serial7)--> CENTRAL.
//   Si el TOP no está mandando snapshots frescos, NO hay árbitro (fail-safe = STOP).
//
// OVERRIDE MANUAL DE BANCO (para probar el movimiento SIN COMM/TOP):
//   Por el Serial Monitor:  's' = START forzado   |   'x' (o 'p') = STOP forzado.
//   El robot corre si (árbitro real = RUN)  O  (override manual = START).
//   Así se prueba el motor en banco aunque la cadena del árbitro no esté lista,
//   y también se prueba la cadena real (dejar el manual en STOP y mandar START
//   desde el árbitro/COMM).
//
// ⚠️ OPEN-LOOP, sin realimentación de heading (igual que diag_central_strafe):
//   La CENTRAL no tiene BNO. Se comanda omega=0: para un omni con ruedas parejas
//   y bien calibradas, eso es traslación pura sin rotar. Cualquier rotación
//   residual es DERIVA y no se corrige acá. La distancia es OPEN-LOOP por TIEMPO
//   (a S mm/s, 30 cm tardan 300/S s) — los "30 cm" son nominales, calibrar.
//
// ⚠️⚠️ CAVEAT DE CINEMÁTICA (mismo que diag_central_strafe — leer):
//   Mueve los motores vía motors_apply_command() -> inverse_kinematics() con
//   WHEEL_ANGLES_DEG={60,-60,180} (TENTATIVO, config_central.h:72, sin medir).
//   El banco del 2026-06-01 (María) encontró que la cinemática genérica da
//   CÍRCULOS y que en ese robot el M2 tiene la polaridad INA/INB invertida por HW
//   (y motors_zircon.cpp no tiene inversión por motor). => el "lateral" puede
//   salir en diagonal / rotando por la cinemática, no solo por la deriva. El
//   arquero que SÍ anduvo en banco (diag_central_line_sweep) usa control directo
//   de motores. Reconciliar el substrato de movimiento (TASK-101) antes de creer
//   los 30 cm. Ver journal/2026-06-01-arquero-seguidor-linea-y-calibracion.md.
//
// Convención (kinematics.h): +X = derecha, +Y = frente. Lateral = vx (vy=0).
//   IZQUIERDA = -vx, DERECHA = +vx.  (Invertible con -DDIAG_ARB_INVERT_LR.)
//
// Seguridad: 400 mm/s default (subido de 150, que iba muy lento). SUJETAR el robot
//   o ruedas al aire en el primer run. Batería cargada (los H-bridges NO van por USB).
//
// Build:
//   pio run -e diag_central_arbitro_strafe_robot1 -t upload   (arquero)
//   pio run -e diag_central_arbitro_strafe_robot2 -t upload   (delantero)
//   pio device monitor -b 115200
//
// Flags:
//   -DDIAG_ARB_SPEED_MM_S=600     velocidad lateral (default 400; subí/bajá para tunear potencia)
//   -DDIAG_ARB_DISTANCE_MM=400    distancia por tramo (default 300 = 30 cm)
//   -DDIAG_ARB_PAUSE_MS=3000      pausa entre tramos (default 3000 = 3 s)
//   -DDIAG_ARB_INVERT_LR          invierte izquierda/derecha
//
// Atribución:
//   Author: Claude Opus 4.8 (Anthropic)
//   Requested-by: Gustavo Viollaz (@gviollaz)

#include <Arduino.h>

#include "config_central.h"
#include "types.h"
#include "motors_zircon.h"
#include "world_model.h"
#include "comm_top.h"

using namespace iitasoccer;

namespace {

constexpr int PIN_LED = 13;   // LED_BUILTIN

#ifndef DIAG_ARB_SPEED_MM_S
  constexpr float STRAFE_SPEED_MM_S = 400.0f;  // subido de 150 (2026-06-03): a 150 iba muy lento (~13% PWM en las ruedas activas); 400 ≈ 35%.
#else
  constexpr float STRAFE_SPEED_MM_S = DIAG_ARB_SPEED_MM_S;
#endif

#ifndef DIAG_ARB_DISTANCE_MM
  constexpr float STRAFE_DISTANCE_MM = 300.0f;   // 30 cm
#else
  constexpr float STRAFE_DISTANCE_MM = DIAG_ARB_DISTANCE_MM;
#endif

#ifndef DIAG_ARB_PAUSE_MS
  constexpr uint32_t PAUSE_MS = 3000;            // 3 s (lo pedido)
#else
  constexpr uint32_t PAUSE_MS = DIAG_ARB_PAUSE_MS;
#endif

// Duración por tramo (OPEN-LOOP): distancia / velocidad.
constexpr uint32_t STRAFE_DURATION_MS =
    static_cast<uint32_t>((STRAFE_DISTANCE_MM / STRAFE_SPEED_MM_S) * 1000.0f);

constexpr uint32_t PRINT_PERIOD_MS = 250;

// IZQUIERDA = -vx (convención +X = derecha). Invertible si el banco lo muestra al revés.
#ifdef DIAG_ARB_INVERT_LR
  constexpr float LEFT_SIGN = +1.0f;
#else
  constexpr float LEFT_SIGN = -1.0f;
#endif

// Patrulla: arranca SIEMPRE por la izquierda en cada START.
enum class Phase : uint8_t { STRAFE, PAUSE };
Phase    g_phase          = Phase::STRAFE;
uint32_t g_phase_start_ms = 0;
float    g_dir            = LEFT_SIGN;

bool     g_running_prev = false;   // para detectar el flanco STOP->START
bool     g_manual_run   = false;   // override de banco (Serial 's'/'x')

elapsedMillis g_since_print;

// ---- Árbitro: real (snapshot del TOP) U override manual de banco ----
// real = match_running del snapshot Y snapshot fresco (fail-safe: TOP caído = STOP).
bool referee_running() {
    const bool real = world_model_match_running() && world_model_snapshot_is_fresh();
    return real || g_manual_run;
}

// 's' = START manual, 'x'/'p' = STOP manual. Los demás bytes se drenan.
void poll_serial_override() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == 's' || c == 'S') {
            g_manual_run = true;
            Serial.println(F(">>> START MANUAL forzado (Serial 's'). 'x' para STOP."));
        } else if (c == 'x' || c == 'X' || c == 'p' || c == 'P') {
            g_manual_run = false;
            Serial.println(F(">>> STOP MANUAL (Serial 'x'). Vuelve a depender del arbitro real."));
        }
    }
}

// Comando lateral OPEN-LOOP: vx según dirección, vy=0, omega=0 (sin heading feedback).
void apply_strafe(float dir_sign) {
    MotorCommand cmd{};
    cmd.vx_mm_s          = static_cast<int16_t>(dir_sign * STRAFE_SPEED_MM_S);
    cmd.vy_mm_s          = 0;
    cmd.omega_centideg_s = 0;
    cmd.dribbler_pwm     = 0;   // (el robot no tiene kicker; kicker_fire fue removido de MotorCommand)
    motors_apply_command(cmd);
}

const char* phase_name() {
    if (g_phase == Phase::PAUSE) return "PAUSA";
    return (g_dir < 0) ? "STRAFE_IZQUIERDA" : "STRAFE_DERECHA";
}

void start_patrol() {
    g_dir            = LEFT_SIGN;   // IZQUIERDA primero
    g_phase          = Phase::STRAFE;
    g_phase_start_ms = millis();
    Serial.println(F(">>> ARBITRO = START -> arranca patrulla (IZQUIERDA primero)."));
}

void print_status() {
    const bool real = world_model_match_running() && world_model_snapshot_is_fresh();
    Serial.print(F("[t=")); Serial.print(millis()); Serial.print(F("] "));
    if (!referee_running()) {
        Serial.print(F("ESPERANDO START"));
    } else {
        Serial.print(phase_name());
    }
    Serial.print(F(" | arbitro="));
    if (real)            Serial.print(F("RUN(arbitro real)"));
    else if (g_manual_run) Serial.print(F("RUN(manual 's')"));
    else                 Serial.print(F("STOP"));
    Serial.print(F(" | snap_fresh="));
    Serial.print(world_model_snapshot_is_fresh() ? F("Y") : F("N"));
    Serial.print(F(" match_flag="));
    Serial.print(world_model_match_running() ? F("RUN") : F("STOP"));
    Serial.print(F(" | top[fr="));
    Serial.print(comm_top_get_frames_received());
    Serial.print(F(" crc="));
    Serial.print(comm_top_get_crc_errors());
    Serial.println(F("]"));
}

}  // namespace

void setup() {
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    motors_init();
    world_model_init();
    comm_top_init();   // recibe WorldSnapshot del TOP (Serial7, pin 28)

    Serial.begin(115200);
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* spin */ }

    // Drenar basura inicial del USB.
    delay(200);
    while (Serial.available() > 0) Serial.read();

    Serial.println();
    Serial.println(F("=================================================="));
    Serial.println(F("  diag_central_arbitro_strafe — patrulla del arquero"));
    Serial.println(F("  gatillada por el ARBITRO (START/STOP)"));
    Serial.println(F("=================================================="));
#if defined(ROBOT1)
    Serial.println(F(" Build: ROBOT1 (arquero)"));
#elif defined(ROBOT2)
    Serial.println(F(" Build: ROBOT2 (delantero)"));
#endif
    Serial.print(F(" Velocidad: ")); Serial.print(STRAFE_SPEED_MM_S, 0);
    Serial.print(F(" mm/s | tramo ~")); Serial.print(STRAFE_DISTANCE_MM, 0);
    Serial.print(F(" mm (")); Serial.print(STRAFE_DURATION_MS); Serial.print(F(" ms, open-loop) | pausa "));
    Serial.print(PAUSE_MS); Serial.println(F(" ms"));
    Serial.println(F(" START del ARBITRO: por GPIO 5/6 -> TOP -> WorldSnapshot (Serial7)."));
    Serial.println(F(" OVERRIDE BANCO: 's'=START manual, 'x'=STOP manual (para probar sin COMM/TOP)."));
    Serial.println(F(" omega=0 (open-loop, sin heading). SUJETA EL ROBOT o ruedas al aire."));
    Serial.println(F(" Esperando START..."));
    Serial.println();

    g_phase_start_ms = millis();
}

void loop() {
    // RX: drenar el snapshot del TOP -> world_model (no bloquea).
    comm_top_tick();
    poll_serial_override();

    const bool running = referee_running();
    const uint32_t now = millis();

    // --- Gate del árbitro: si NO corre, FRENAR y esperar START (chequeo cada loop:
    //     un STOP en medio de un tramo frena AHORA, no espera al fin del tramo). ---
    if (!running) {
        motors_stop();
        if (g_running_prev) {
            Serial.println(F(">>> ARBITRO = STOP -> FRENO. Esperando START."));
        }
        g_running_prev = false;
        // LED: parpadeo lento = esperando START.
        digitalWrite(PIN_LED, (millis() / 400) % 2);
        if (g_since_print >= PRINT_PERIOD_MS) { g_since_print = 0; print_status(); }
        return;
    }

    // --- Flanco STOP -> START: (re)arrancar la patrulla desde la izquierda. ---
    if (!g_running_prev) {
        start_patrol();
    }
    g_running_prev = true;

    // --- Patrulla lateral: STRAFE (tramo) -> PAUSA 3 s -> alternar lado -> ... ---
    switch (g_phase) {
        case Phase::STRAFE:
            apply_strafe(g_dir);
            digitalWrite(PIN_LED, HIGH);   // fijo = moviéndose
            if (now - g_phase_start_ms > STRAFE_DURATION_MS) {
                g_phase = Phase::PAUSE;
                g_phase_start_ms = now;
                Serial.println(F(">>> PAUSA 3 s"));
            }
            break;

        case Phase::PAUSE:
            motors_stop();
            digitalWrite(PIN_LED, (millis() / 150) % 2);  // parpadeo rápido = pausa
            if (now - g_phase_start_ms > PAUSE_MS) {
                g_dir = -g_dir;                 // alternar IZQUIERDA <-> DERECHA
                g_phase = Phase::STRAFE;
                g_phase_start_ms = now;
                Serial.print(F(">>> ")); Serial.println(g_dir < 0 ? F("IZQUIERDA") : F("DERECHA"));
            }
            break;
    }

    if (g_since_print >= PRINT_PERIOD_MS) { g_since_print = 0; print_status(); }
}
