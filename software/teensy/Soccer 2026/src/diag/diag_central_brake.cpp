// diag_central_brake.cpp — ¿el Zircon FRENA o queda en COAST? (banco)
//
// Para qué sirve:
//   El freno de borde de competencia (main_central.cpp: motors_brake()) hace un
//   freno ACTIVO: pone INA=INB=HIGH (corto en el H-bridge) con PWM=0. PERO falta
//   confirmar en el Zircon Rev v15 que eso de verdad FRENA y no queda en COAST
//   (rueda libre). Este sketch lo dirime EN BANCO: acelera un motor, y a tu orden
//   aplica BRAKE (HIGH/HIGH) o COAST (LOW/LOW) para que compares cuánto tarda en
//   parar cada uno.
//
//   ⚠️ Es un sketch AGNÓSTICO (no incluye config_central.h ni el firmware vivo):
//   replica el patrón de pines de diag_central_motors.cpp. Si el cableado físico
//   difiere, ajustá la tabla MOTORS[] de abajo. NO cambia el binario de competencia.
//
// Build / flash:
//   pio run -e diag_central_brake -t upload
//   pio device monitor -b 115200
//
// Procedimiento:
//   1. SUJETÁ EL ROBOT o ruedas al aire.
//   2. Apretá el botón (pin 9) o mandá ENTER por el Serial Monitor para avanzar.
//      Cada paso: acelera el motor 1 ~2 s y luego aplica el modo del paso:
//        Paso 1 → acelera y aplica BRAKE  (INA=INB=HIGH). Cronometrá hasta parar.
//        Paso 2 → acelera y aplica COAST  (INA=INB=LOW).  Cronometrá hasta parar.
//        Paso 3 → fin.
//   3. Comparás: si BRAKE para MUCHO más rápido que COAST → el freno activo
//      funciona (el borde es confiable). Si tardan ~igual → el H-bridge NO cortocircuita
//      y el robot queda en COAST: el freno de borde NO sirve tal cual y hay que
//      revisar el patrón (probar INA=INB=LOW, o contra-PWM corto).
//
// Atribución:
//   Author: Claude Opus 4.8 (1M context) (Anthropic)
//   Requested-by: Gustavo Viollaz (@gviollaz)

#include <Arduino.h>

// ── Pinout Zircon Rev v15 (igual que diag_central_motors.cpp) ──────────────
struct MotorPins { const char* label; int ina; int inb; int pwm; };
constexpr MotorPins MOTORS[3] = {
    { "MOTOR 1 - driver U5  (INA=2, INB=5, PWM=3)",  2, 5,  3 },
    { "MOTOR 2 - driver U17 (INA=8, INB=7, PWM=6)",  8, 7,  6 },
    { "MOTOR 3 - driver U7  (INA=11, INB=12, PWM=4)", 11, 12, 4 },
};

constexpr int      TEST_MOTOR  = 0;       // motor a probar (0..2); editá si querés otro
constexpr int      PIN_BUTTON  = 9;
constexpr int      PIN_LED     = 13;
constexpr int      SPIN_PWM    = 110;     // ~43% — suficiente para que tenga inercia visible, seguro
constexpr uint32_t SPIN_MS     = 2000;    // acelera 2 s antes de frenar
constexpr uint32_t DEBOUNCE_MS = 50;
constexpr uint32_t MIN_DWELL_MS = 400;

// ── Helpers de motor ───────────────────────────────────────────────────────
void motor_drive(int idx, int pwm) {      // adelante (INA=HIGH, INB=LOW)
    const MotorPins& m = MOTORS[idx];
    digitalWrite(m.ina, HIGH); digitalWrite(m.inb, LOW); analogWrite(m.pwm, pwm);
}
void motor_brake(int idx) {               // freno activo = corto HIGH/HIGH, PWM=0 (== motors_brake())
    const MotorPins& m = MOTORS[idx];
    digitalWrite(m.ina, HIGH); digitalWrite(m.inb, HIGH); analogWrite(m.pwm, 0);
}
void motor_coast(int idx) {               // rueda libre = LOW/LOW, PWM=0
    const MotorPins& m = MOTORS[idx];
    digitalWrite(m.ina, LOW); digitalWrite(m.inb, LOW); analogWrite(m.pwm, 0);
}

enum class State { WAITING, BRAKE_TRIAL, COAST_TRIAL, DONE };
State    g_state = State::WAITING;
uint32_t g_state_ms = 0;
bool     g_applied = false;          // ya se aplicó el freno/coast de este trial
uint32_t g_apply_ms = 0;

bool     g_btn_last = false;
uint32_t g_btn_change_ms = 0;

bool button_edge() {
    static bool raw_last = false;
    const bool raw = (digitalRead(PIN_BUTTON) == LOW);
    const uint32_t now = millis();
    if (raw != raw_last) { raw_last = raw; g_btn_change_ms = now; }
    if ((now - g_btn_change_ms) >= DEBOUNCE_MS && raw != g_btn_last) {
        g_btn_last = raw;
        if (g_btn_last) return true;
    }
    return false;
}
bool serial_enter() {
    bool adv = false;
    while (Serial.available() > 0) { const char c = (char)Serial.read(); if (c=='\n'||c=='\r') adv = true; }
    return adv;
}

void enter_state(State s) {
    g_state = s; g_state_ms = millis(); g_applied = false;
    motor_coast(TEST_MOTOR);
    switch (s) {
        case State::WAITING:
            Serial.println();
            Serial.println("=====================================================");
            Serial.println(" diag_central_brake — FRENO vs COAST (Zircon Rev v15)");
            Serial.println("=====================================================");
            Serial.print  (" Motor de prueba: "); Serial.println(MOTORS[TEST_MOTOR].label);
            Serial.println(" >>> SUJETA EL ROBOT o ruedas al aire <<<");
            Serial.println(" Apreta el BOTON (pin 9) o ENTER para empezar.");
            Serial.println("   Paso 1 = acelera 2s y aplica BRAKE (HIGH/HIGH)");
            Serial.println("   Paso 2 = acelera 2s y aplica COAST (LOW/LOW)");
            Serial.println("   Cronometra/observa cuanto tarda en parar cada uno.");
            break;
        case State::BRAKE_TRIAL:
            Serial.println("\n>>> PASO 1: acelerando 2s, luego BRAKE...");
            break;
        case State::COAST_TRIAL:
            Serial.println("\n>>> PASO 2: acelerando 2s, luego COAST...");
            break;
        case State::DONE:
            motor_coast(TEST_MOTOR);
            Serial.println("\n===== FIN. Compara: BRAKE deberia parar MUCHO mas rapido que COAST.");
            Serial.println("      Si tardan ~igual -> el Zircon NO frena (queda en COAST):");
            Serial.println("      reportalo, hay que cambiar el patron de motors_brake().");
            Serial.println("      Reset del Teensy para repetir.");
            break;
    }
}

void run_trial(bool is_brake) {
    const uint32_t el = millis() - g_state_ms;
    if (el < SPIN_MS) {
        motor_drive(TEST_MOTOR, SPIN_PWM);            // fase de aceleracion
    } else if (!g_applied) {
        g_applied = true; g_apply_ms = millis();
        if (is_brake) { motor_brake(TEST_MOTOR); Serial.println("    [t=0] BRAKE aplicado (INA=INB=HIGH). Cronometra hasta parar."); }
        else          { motor_coast(TEST_MOTOR); Serial.println("    [t=0] COAST aplicado (INA=INB=LOW).  Cronometra hasta parar."); }
        Serial.println("    Apreta BOTON/ENTER cuando se detenga (o para pasar al siguiente).");
    }
}

void setup() {
    pinMode(PIN_LED, OUTPUT); digitalWrite(PIN_LED, LOW);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    for (const auto& m : MOTORS) { pinMode(m.ina, OUTPUT); pinMode(m.inb, OUTPUT); pinMode(m.pwm, OUTPUT); }
    motor_coast(TEST_MOTOR);
    Serial.begin(115200);
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* spin */ }
    delay(300);
    while (Serial.available() > 0) Serial.read();
    g_btn_last = (digitalRead(PIN_BUTTON) == LOW);
    enter_state(State::WAITING);
}

void loop() {
    digitalWrite(PIN_LED, (g_state == State::WAITING || g_state == State::DONE) ? ((millis()/250)%2) : HIGH);

    const bool dwell_ok = (millis() - g_state_ms) >= MIN_DWELL_MS;
    if (dwell_ok && (button_edge() || serial_enter())) {
        switch (g_state) {
            case State::WAITING:     enter_state(State::BRAKE_TRIAL); break;
            case State::BRAKE_TRIAL: enter_state(State::COAST_TRIAL); break;
            case State::COAST_TRIAL: enter_state(State::DONE);        break;
            case State::DONE:        break;
        }
    }

    if (g_state == State::BRAKE_TRIAL) run_trial(true);
    else if (g_state == State::COAST_TRIAL) run_trial(false);
}
