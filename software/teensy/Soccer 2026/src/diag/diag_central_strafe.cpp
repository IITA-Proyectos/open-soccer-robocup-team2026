// diag_central_strafe.cpp — Patrulla lateral del ARQUERO (OPEN-LOOP, sin heading)
//
// Para qué sirve:
//   Test de banco de la MITAD INFERIOR: motores + placa CENTRAL + cinemática.
//   El robot se mueve LATERAL (perpendicular a donde mira la cámara frontal):
//   ~30 cm a la IZQUIERDA → pausa → ~30 cm a la DERECHA → pausa → loop. Base del
//   ARQUERO.
//
//   Cadena ejercitada:
//     vx lateral → cinemática inversa omni-3 → motors_zircon (PWM al Zircon).
//
// ⚠️ SIN realimentación de heading (open-loop). Por qué:
//   - El CENTRAL **NO tiene BNO055** (el heading viene del TOP, que acá NO se usa).
//   Entonces este test comanda **omega = 0**: para un omni-3 con ruedas parejas y
//   bien calibradas, eso es **traslación pura SIN rotar** → el robot "mira al
//   frente" mientras se mueve de costado. Cualquier rotación residual es DERIVA
//   y NO se corrige acá.
//   [v2 = heading-hold real. YA VIABLE Y LOCAL EN CENTRAL: el OTOS de la base
//    llega a CENTRAL por broadcast simétrico de DOWN (2026-06-01, Serial1) y está
//    en world_model (world_model_get_otos_heading_deg / _otos_is_fresh). v2 = sumar
//    un HeadingPID sobre ese heading. NO requiere mensaje nuevo ni placa TOP.]
//
// ✅ CINEMÁTICA YA CALIBRADA (2026-06-08, Gustavo en banco) — reemplaza el caveat viejo:
//   Este sketch comanda vx vía motors_apply_command() → inverse_kinematics() con
//   WHEEL_ANGLES_DEG={330,210,90} (config_central.h), la DISPOSICIÓN FÍSICA REAL:
//   M1=delantera-IZQUIERDA, M2=delantera-DERECHA (INVERTIDA por HW vía MOTOR_INVERT),
//   M3=TRASERA. El viejo {60,-60,180} daba CÍRCULOS (estaba en eje +Y y la fórmula usa
//   +X) → RESUELTO. En un strafe puro: M1/M2 giran al MISMO lado (+0.5·vx) y la TRASERA
//   (M3) es la que más empuja (−vx). El piso de PWM es POR RUEDA (MOTOR_MIN_PWM[3]=
//   {70,70,42}): las delanteras oblicuas necesitan más piso que la trasera paralela
//   (el PWM no es proporcional a la velocidad, distinto por rueda).
//   PENDIENTE de banco: tuneo fino del lateral (que NO rote) + confirmar el SENTIDO.
//
// ⚠️ DISTANCIA OPEN-LOOP (aproximada):
//   CENTRAL no recibe odometría. La distancia se hace por TIEMPO: a S mm/s, 30 cm
//   = 300 mm tarda 300/S s. Los "30 cm" son NOMINALES — medir con regla y ajustar
//   -DDIAG_STRAFE_SPEED_MM_S / -DDIAG_STRAFE_DISTANCE_MM.
//
// Convención (kinematics.h): +X = derecha, +Y = frente. Lateral = vx (vy=0).
//   IZQUIERDA = -vx, DERECHA = +vx.  (Invertible con -DDIAG_STRAFE_INVERT_LR.)
//
// Operativa (botón pin 9, o ENTER por Serial Monitor):
//   WAITING → (botón) arranca la patrulla IZQUIERDA → PAUSA → DERECHA → PAUSA → …
//   Botón durante la patrulla = STOP.
//
// Seguridad:
//   - Velocidad lenta (default 150 mm/s).
//   - SUJETAR el robot o ruedas al aire en el primer run (puede salir disparado).
//   - Batería cargada (los H-bridges NO van por USB).
//
// Build:
//   pio run -e diag_central_strafe_robot1 -t upload   (arquero)
//   pio run -e diag_central_strafe_robot2 -t upload   (delantero)
//   pio device monitor -b 115200
//
// Flags:
//   -DDIAG_STRAFE_SPEED_MM_S=200     velocidad lateral (default 150)
//   -DDIAG_STRAFE_DISTANCE_MM=400    distancia por tramo (default 300 = 30cm)
//   -DDIAG_STRAFE_INVERT_LR          invierte izquierda/derecha (si el banco lo muestra al revés)
//
// Atribución:
//   Author: Claude Opus 4.8 (Anthropic)
//   Requested-by: Gustavo Viollaz (@gviollaz)

#include <Arduino.h>

#include "config_central.h"
#include "types.h"
#include "motors_zircon.h"

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

constexpr uint32_t PAUSE_MS        = 800;   // pausa entre tramos
constexpr uint32_t DEBOUNCE_MS     = 50;    // antirebote estable (anti-ruido de motor)
constexpr uint32_t PRINT_PERIOD_MS = 250;

// IZQUIERDA = -vx (convención +X = derecha). Invertible si el banco lo muestra al revés.
#ifdef DIAG_STRAFE_INVERT_LR
  constexpr float LEFT_SIGN = +1.0f;
#else
  constexpr float LEFT_SIGN = -1.0f;
#endif

enum class State : uint8_t { WAITING, STRAFE, PAUSE, STOPPED };

State    g_state          = State::WAITING;
uint32_t g_state_start_ms = 0;
float    g_dir            = LEFT_SIGN;   // arranca IZQUIERDA

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

const char* state_name() {
    switch (g_state) {
        case State::WAITING: return "WAITING";
        case State::STRAFE:  return (g_dir < 0 ? "STRAFE_IZQUIERDA" : "STRAFE_DERECHA");
        case State::PAUSE:   return "PAUSE";
        case State::STOPPED: return "STOPPED";
    }
    return "?";
}

// Comando lateral OPEN-LOOP: vx según dirección, vy=0, omega=0 (sin heading feedback).
void apply_strafe(float dir_sign) {
    MotorCommand cmd{};
    cmd.vx_mm_s          = static_cast<int16_t>(dir_sign * STRAFE_SPEED_MM_S);
    cmd.vy_mm_s          = 0;
    cmd.omega_centideg_s = 0;   // sin corrección de heading (no hay sensor en CENTRAL)
    cmd.dribbler_pwm     = 0;
    motors_apply_command(cmd);
}

void print_status() {
    Serial.print(F("[t="));   Serial.print(millis());
    Serial.print(F("] "));    Serial.print(state_name());
    Serial.print(F(" vx="));
    Serial.print((g_state == State::STRAFE) ? (g_dir * STRAFE_SPEED_MM_S) : 0.0f, 0);
    Serial.println(F(" mm/s  (omega=0, open-loop)"));
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
            Serial.println(F("  OPEN-LOOP (sin BNO/heading: omega=0)"));
            Serial.println(F("=================================================="));
        #if defined(ROBOT1)
            Serial.println(F(" Build: ROBOT1 (arquero)"));
        #elif defined(ROBOT2)
            Serial.println(F(" Build: ROBOT2 (delantero)"));
        #endif
            Serial.print(F(" Velocidad: "));  Serial.print(STRAFE_SPEED_MM_S, 0);
            Serial.print(F(" mm/s | tramo ~")); Serial.print(STRAFE_DISTANCE_MM, 0);
            Serial.print(F(" mm (")); Serial.print(STRAFE_DURATION_MS); Serial.println(F(" ms, open-loop)"));
            Serial.println(F(" Sin heading feedback: omega=0 (omni traslada sin rotar; deriva no se corrige)."));
            Serial.println(F(" SUJETA EL ROBOT o ruedas al aire."));
            Serial.println(F(" Boton (pin 9) o ENTER: arranca la patrulla IZQ<->DER."));
            Serial.println(F(" (Boton durante la patrulla = STOP.)"));
            Serial.println();
            break;
        case State::STRAFE:
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
            if (advance) { g_dir = LEFT_SIGN; enter_state(State::STRAFE); }   // IZQUIERDA primero
            break;

        case State::STRAFE:
            if (advance) { enter_state(State::STOPPED); break; }
            apply_strafe(g_dir);
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
