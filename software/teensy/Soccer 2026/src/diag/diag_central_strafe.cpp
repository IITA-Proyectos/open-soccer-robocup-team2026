// diag_central_strafe.cpp — Patrulla lateral del ARQUERO (mitad inferior, SIN TOP)
//
// Para qué sirve:
//   Test de banco de la MITAD INFERIOR: motores + placa CENTRAL + control de
//   movimiento. El robot se mueve LATERAL (perpendicular a donde mira la cámara
//   frontal): ~30 cm a la IZQUIERDA, para, ~30 cm a la DERECHA, en loop,
//   MANTENIENDO el frente fijo (heading-hold). Es la base del ARQUERO.
//
//   Cadena ejercitada:
//     BNO055 local (imu_zircon) → HeadingPID → cinemática inversa omni-3
//       → motors_zircon (PWM a los 3 H-bridges del Zircon Rev v15).
//
//   NO necesita el TOP: el heading sale del BNO055 LOCAL del CENTRAL, no del
//   WorldSnapshot. Por eso es un test puro de mitad inferior.
//
// Convención (kinematics.h): +X = derecha, +Y = frente, +omega = CCW (de arriba).
//   Lateral = comando vx (vy=0). IZQUIERDA = -vx, DERECHA = +vx.
//   Frente fijo = omega del HeadingPID que mantiene el heading inicial.
//
// ⚠️ DISTANCIA OPEN-LOOP (aproximada):
//   CENTRAL NO recibe odometría (los OTOS van DOWN→TOP, no a CENTRAL). La
//   distancia se hace por TIEMPO: a velocidad S mm/s, 30 cm = 300 mm tarda
//   300/S s. Los "30 cm" son NOMINALES — medir con regla y ajustar
//   -DDIAG_STRAFE_SPEED_MM_S / -DDIAG_STRAFE_DISTANCE_MM. No hay realimentación
//   de posición (eso vendría de los OTOS, que hoy no llegan al CENTRAL).
//
// ⚠️ HEADING-HOLD sin validar en hardware:
//   El signo de omega depende de la convención física (ver TASK-036 / análisis
//   2026-05-31: +omega podría ser HORARIO físico, no CCW). Si al activar el
//   heading-hold el robot GIRA en vez de MANTENER el frente:
//     - Hay protección ANTI-RUNAWAY: si el heading se desvía > MAX_HEADING_ERR
//       (30°) del setpoint, FRENA y avisa.
//     - Recompilar con -DDIAG_STRAFE_HEADING_REVERSE para invertir el signo.
//   RECOMENDADO el 1er run con -DSTRAFE_NO_HEADING_HOLD (strafe puro, omega=0)
//   para validar el movimiento lateral + la dirección; después activar el hold.
//
// Operativa (botón pin 9, o ENTER por Serial Monitor):
//   WAITING → (botón) captura "frente" + arranca la patrulla:
//     IZQUIERDA (~30cm) → PAUSA → DERECHA (~30cm) → PAUSA → IZQUIERDA → ... (loop)
//   Botón durante la patrulla = STOP.
//
// Seguridad:
//   - Velocidad lenta (default 150 mm/s).
//   - omega clampeado a ±150 °/s (gentil + evita overflow int16 al empaquetar).
//   - Anti-runaway de heading (frena si se desvía > 30°).
//   - SUJETAR el robot o ruedas al aire en el primer run (puede salir disparado).
//
// Build:
//   pio run -e diag_central_strafe_robot1 -t upload   (arquero)
//   pio run -e diag_central_strafe_robot2 -t upload   (delantero)
//   pio device monitor -b 115200
//
// Flags:
//   -DDIAG_STRAFE_SPEED_MM_S=200     velocidad lateral (default 150)
//   -DDIAG_STRAFE_DISTANCE_MM=400    distancia por tramo (default 300 = 30cm)
//   -DDIAG_STRAFE_HEADING_REVERSE    invierte el signo del heading-hold (anti-runaway)
//   -DDIAG_STRAFE_INVERT_LR          invierte izquierda/derecha (si el banco lo muestra al revés)
//   -DSTRAFE_NO_HEADING_HOLD         desactiva el heading-hold (strafe puro, omega=0)
//
// Atribución:
//   Author: Claude Opus 4.8 (Anthropic)
//   Requested-by: Gustavo Viollaz (@gviollaz)

#include <Arduino.h>
#include <math.h>

#include "config_central.h"
#include "types.h"
#include "imu_zircon.h"
#include "motors_zircon.h"
#include "pids.h"

using namespace iitasoccer;

namespace {

constexpr int PIN_BUTTON = 9;     // Botón 1 del Zircon (pullup interno)
constexpr int PIN_LED    = 13;    // LED_BUILTIN

#ifndef DIAG_STRAFE_SPEED_MM_S
  constexpr float STRAFE_SPEED_MM_S = 150.0f;   // lento
#else
  constexpr float STRAFE_SPEED_MM_S = DIAG_STRAFE_SPEED_MM_S;
#endif

#ifndef DIAG_STRAFE_DISTANCE_MM
  constexpr float STRAFE_DISTANCE_MM = 300.0f;  // 30 cm
#else
  constexpr float STRAFE_DISTANCE_MM = DIAG_STRAFE_DISTANCE_MM;
#endif

// Duración por tramo (OPEN-LOOP): distancia / velocidad.
constexpr uint32_t STRAFE_DURATION_MS =
    static_cast<uint32_t>((STRAFE_DISTANCE_MM / STRAFE_SPEED_MM_S) * 1000.0f);

constexpr uint32_t PAUSE_MS          = 800;     // pausa entre tramos
constexpr float    MAX_HEADING_ERR   = 30.0f;   // ° — anti-runaway
constexpr float    OMEGA_CLAMP_DEG_S = 150.0f;  // gentil + evita overflow int16 (×100 < 32767)
constexpr uint32_t DEBOUNCE_MS       = 50;      // antirebote estable (anti-ruido de motor)
constexpr uint32_t PRINT_PERIOD_MS   = 250;

#ifdef DIAG_STRAFE_HEADING_REVERSE
  constexpr float HEADING_OMEGA_SIGN = -1.0f;
#else
  constexpr float HEADING_OMEGA_SIGN = +1.0f;
#endif

// IZQUIERDA = -vx (convención +X = derecha). Invertible si el banco lo muestra al revés.
#ifdef DIAG_STRAFE_INVERT_LR
  constexpr float LEFT_SIGN = +1.0f;
#else
  constexpr float LEFT_SIGN = -1.0f;
#endif

#ifdef STRAFE_NO_HEADING_HOLD
  constexpr bool HEADING_HOLD = false;
#else
  constexpr bool HEADING_HOLD = true;
#endif

enum class State : uint8_t { WAITING, STRAFE, PAUSE, STOPPED };

State    g_state          = State::WAITING;
uint32_t g_state_start_ms = 0;
float    g_dir            = LEFT_SIGN;   // arranca IZQUIERDA
float    g_setpoint       = 0.0f;        // "frente" capturado al arrancar
bool     g_imu_ready      = false;
float    g_last_omega     = 0.0f;        // último omega comandado (telemetría)

HeadingPID g_pid;
bool     g_button_last      = false;
uint32_t g_button_change_ms = 0;
elapsedMillis g_since_print;

// ---- Botón (antirebote por estabilidad, anti-ruido de motor — igual que diag_central_motors) ----
bool button_pressed_edge() {
    static bool raw_last = false;
    const bool raw = (digitalRead(PIN_BUTTON) == LOW);
    const uint32_t now = millis();
    if (raw != raw_last) { raw_last = raw; g_button_change_ms = now; }
    if ((now - g_button_change_ms) >= DEBOUNCE_MS && raw != g_button_last) {
        g_button_last = raw;
        if (g_button_last) return true;   // flanco estable suelto -> apretado
    }
    return false;
}

// Avanza sólo con ENTER (los demás bytes se drenan) — no dispara con basura del USB.
bool serial_advance_request() {
    bool adv = false;
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') adv = true;
    }
    return adv;
}

float current_heading() {
    return g_imu_ready ? imu_get_heading() : 0.0f;
}

const char* state_name() {
    switch (g_state) {
        case State::WAITING: return "WAITING";
        case State::STRAFE:  return (g_dir < 0 ? "STRAFE_IZQUIERDA" : "STRAFE_DERECHA");
        case State::PAUSE:   return "PAUSE";
        case State::STOPPED: return "STOPPED";
    }
    return "?";
}

// Comando lateral + heading-hold. Guarda el omega en g_last_omega.
void apply_strafe(float dir_sign, uint32_t now_ms) {
    float omega_deg_s = 0.0f;
    if (HEADING_HOLD && g_imu_ready) {
        omega_deg_s = HEADING_OMEGA_SIGN * heading_pid_tick(g_pid, imu_get_heading(), now_ms);
        if (omega_deg_s >  OMEGA_CLAMP_DEG_S) omega_deg_s =  OMEGA_CLAMP_DEG_S;
        if (omega_deg_s < -OMEGA_CLAMP_DEG_S) omega_deg_s = -OMEGA_CLAMP_DEG_S;
    }
    g_last_omega = omega_deg_s;

    MotorCommand cmd{};
    cmd.vx_mm_s          = static_cast<int16_t>(dir_sign * STRAFE_SPEED_MM_S);
    cmd.vy_mm_s          = 0;
    cmd.omega_centideg_s = static_cast<int16_t>(omega_deg_s * 100.0f);
    cmd.kicker_fire      = 0;
    cmd.dribbler_pwm     = 0;
    motors_apply_command(cmd);
}

// Anti-runaway: ¿el heading se fue > MAX_HEADING_ERR del setpoint?
bool heading_runaway() {
    if (!(HEADING_HOLD && g_imu_ready)) return false;
    return fabsf(wrap_diff_deg(g_setpoint, imu_get_heading())) > MAX_HEADING_ERR;
}

void print_status() {
    Serial.print(F("[t="));   Serial.print(millis());
    Serial.print(F("] "));    Serial.print(state_name());
    Serial.print(F(" hold=")); Serial.print((HEADING_HOLD && g_imu_ready) ? "Y" : "N");
    Serial.print(F(" hdg="));  Serial.print(current_heading(), 1);
    Serial.print(F(" sp="));   Serial.print(g_setpoint, 1);
    Serial.print(F(" err="));
    Serial.print(g_imu_ready ? wrap_diff_deg(g_setpoint, imu_get_heading()) : 0.0f, 1);
    Serial.print(F(" omega=")); Serial.print(g_last_omega, 1);
    Serial.println();
}

void enter_state(State s) {
    g_state = s;
    g_state_start_ms = millis();
    switch (s) {
        case State::WAITING:
            motors_stop();
            Serial.println();
            Serial.println(F("=================================================="));
            Serial.println(F("  diag_central_strafe — patrulla lateral (arquero)"));
            Serial.println(F("=================================================="));
        #if defined(ROBOT1)
            Serial.println(F(" Build: ROBOT1 (arquero)"));
        #elif defined(ROBOT2)
            Serial.println(F(" Build: ROBOT2 (delantero)"));
        #endif
            Serial.print(F(" Velocidad: "));  Serial.print(STRAFE_SPEED_MM_S, 0);
            Serial.print(F(" mm/s | tramo ~")); Serial.print(STRAFE_DISTANCE_MM, 0);
            Serial.print(F(" mm (")); Serial.print(STRAFE_DURATION_MS); Serial.println(F(" ms, open-loop)"));
            Serial.print(F(" Heading-hold: "));
            Serial.println(HEADING_HOLD ? (g_imu_ready ? "ON (BNO055 local OK)"
                                                        : "ON pero BNO055 NO responde -> omega=0")
                                        : "OFF (-DSTRAFE_NO_HEADING_HOLD)");
            Serial.println(F(" SUJETA EL ROBOT o ruedas al aire."));
            Serial.println(F(" Boton (pin 9) o ENTER: arranca la patrulla IZQ<->DER."));
            Serial.println(F(" (Boton durante la patrulla = STOP.)"));
            Serial.println();
            break;
        case State::STRAFE:
            heading_pid_reset(g_pid);
            heading_pid_set_target(g_pid, g_setpoint);
            Serial.print(F(">>> ")); Serial.print(g_dir < 0 ? "IZQUIERDA" : "DERECHA");
            Serial.print(F("  (vx=")); Serial.print(g_dir * STRAFE_SPEED_MM_S, 0);
            Serial.print(F(" mm/s, "));  Serial.print(STRAFE_DURATION_MS); Serial.println(F(" ms)"));
            break;
        case State::PAUSE:
            motors_stop();
            Serial.println(F(">>> PAUSA"));
            break;
        case State::STOPPED:
            motors_stop();
            Serial.println(F(">>> STOP. Boton/ENTER para reiniciar la patrulla."));
            break;
    }
}

}  // namespace

void setup() {
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    motors_init();

    Serial.begin(115200);
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* spin */ }

    Serial.println(F("[strafe] init BNO055 local (puede tardar ~6 s)..."));
    g_imu_ready = imu_init();   // BNO055 local del CENTRAL — fuente de heading
    Serial.print(F("[strafe] BNO055: "));
    Serial.println(g_imu_ready ? "OK" : "NO responde (heading-hold deshabilitado)");

    heading_pid_reset(g_pid);

    // Anti-arranque-fantasma: drenar el buffer serie + leer el botón real.
    delay(200);
    while (Serial.available() > 0) Serial.read();
    g_button_last = (digitalRead(PIN_BUTTON) == LOW);

    enter_state(State::WAITING);
}

void loop() {
    const bool moving = (g_state == State::STRAFE);
    digitalWrite(PIN_LED, moving ? HIGH : ((millis() / 250) % 2));

    const bool advance = button_pressed_edge() || serial_advance_request();
    const uint32_t now = millis();

    switch (g_state) {
        case State::WAITING:
            motors_stop();
            if (advance) {
                g_setpoint = current_heading();   // "frente" = heading actual
                g_dir = LEFT_SIGN;                // arranca IZQUIERDA
                enter_state(State::STRAFE);
            }
            break;

        case State::STRAFE:
            if (advance) { enter_state(State::STOPPED); break; }
            if (heading_runaway()) {
                Serial.println(F("!!! HEADING RUNAWAY (>30 deg del frente). FRENO."));
                Serial.println(F("    Recompilar con -DDIAG_STRAFE_HEADING_REVERSE (signo de omega invertido)."));
                enter_state(State::STOPPED);
                break;
            }
            apply_strafe(g_dir, now);
            if (now - g_state_start_ms > STRAFE_DURATION_MS) enter_state(State::PAUSE);
            break;

        case State::PAUSE:
            motors_stop();
            if (advance) { enter_state(State::STOPPED); break; }
            if (now - g_state_start_ms > PAUSE_MS) {
                g_dir = -g_dir;                  // alternar IZQ <-> DER
                enter_state(State::STRAFE);
            }
            break;

        case State::STOPPED:
            motors_stop();
            if (advance) enter_state(State::WAITING);
            break;
    }

    if (g_since_print >= PRINT_PERIOD_MS) {
        g_since_print = 0;
        print_status();
    }
}
