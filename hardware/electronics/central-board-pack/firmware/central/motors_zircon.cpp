// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
#include "motors_zircon.h"
#include "config_central.h"
#include "kinematics.h"
#include <Arduino.h>

namespace iitasoccer {

namespace {

// Pinout indexado por motor — los `constexpr int PIN_*` de config_central.h se
// agrupan acá para usar índice [0..2] en lugar de hardcodear el motor.
struct MotorPins { int ina; int inb; int pwm; };
const MotorPins MOTOR_PINS[3] = {
    { PIN_INA1, PIN_INB1, PIN_PWM1 },
    { PIN_INA2, PIN_INB2, PIN_PWM2 },
    { PIN_INA3, PIN_INB3, PIN_PWM3 },
};

// Configuración de las 3 ruedas omni del robot.
const WheelConfig WHEELS[3] = {
    { WHEEL_ANGLES_DEG[0] * PI_F / 180.0f, WHEEL_RADIUS_MM },
    { WHEEL_ANGLES_DEG[1] * PI_F / 180.0f, WHEEL_RADIUS_MM },
    { WHEEL_ANGLES_DEG[2] * PI_F / 180.0f, WHEEL_RADIUS_MM },
};

// === Kicker (solenoide) — solo se compila si ROBOT2 ===
#if defined(ROBOT2)
// Estado interno del kicker. Lo manejamos como pulso one-shot con cooldown:
//   • cmd.kicker_fire=1 + cooldown vencido + no hay pulso activo → arrancar pulso.
//   • Mientras el pulso está activo, el pin GPIO queda HIGH.
//   • Cuando pasa KICKER_PULSE_MS desde el arranque del pulso, lo bajamos
//     a LOW y arrancamos cooldown.
bool     g_kicker_pulse_active = false;
uint32_t g_kicker_pulse_start_ms = 0;
uint32_t g_kicker_last_fire_ms = 0;

void kicker_init() {
    pinMode(PIN_KICKER_SOL, OUTPUT);
    digitalWrite(PIN_KICKER_SOL, LOW);
    g_kicker_pulse_active = false;
    g_kicker_pulse_start_ms = 0;
    g_kicker_last_fire_ms = 0;
}

// Llamar en cada motors_apply_command — orquesta el ciclo pulso/cooldown.
// cmd.kicker_fire=1 solo arranca uno nuevo si nada está activo Y el cooldown
// venció. Una vez arrancado, el pulso corre independiente del flag.
void kicker_update(uint8_t kicker_fire_flag) {
    const uint32_t now_ms = millis();
    if (g_kicker_pulse_active) {
        if (now_ms - g_kicker_pulse_start_ms >= KICKER_PULSE_MS) {
            // Pulso terminó — bajar pin y arrancar cooldown.
            digitalWrite(PIN_KICKER_SOL, LOW);
            g_kicker_pulse_active = false;
            g_kicker_last_fire_ms = now_ms;
        }
        return;   // mientras el pulso corre, ignoramos nuevos flags
    }
    // Sin pulso activo — evaluar si arrancamos uno nuevo.
    if (kicker_fire_flag != 0) {
        const bool cooldown_ok = (g_kicker_last_fire_ms == 0)
                                 || (now_ms - g_kicker_last_fire_ms >= KICKER_COOLDOWN_MS);
        if (cooldown_ok) {
            digitalWrite(PIN_KICKER_SOL, HIGH);
            g_kicker_pulse_active = true;
            g_kicker_pulse_start_ms = now_ms;
        }
    }
}

// Apaga el kicker de inmediato (sin esperar fin de pulso). Usar en motors_stop
// y motors_brake — si entramos a emergencia, NO queremos que el solenoide
// quede pegado HIGH consumiendo corriente.
void kicker_force_off() {
    digitalWrite(PIN_KICKER_SOL, LOW);
    g_kicker_pulse_active = false;
    // No tocamos last_fire_ms — el cooldown sigue corriendo igual.
}
#endif

void apply_pwm_to_motor(int motor_idx, int pwm_signed) {
    if (motor_idx < 0 || motor_idx >= 3) return;
    const MotorPins& p = MOTOR_PINS[motor_idx];
    if (pwm_signed > 0) {
        digitalWrite(p.ina, 1);
        digitalWrite(p.inb, 0);
        analogWrite(p.pwm, pwm_signed);
    } else if (pwm_signed < 0) {
        digitalWrite(p.ina, 0);
        digitalWrite(p.inb, 1);
        analogWrite(p.pwm, -pwm_signed);
    } else {
        digitalWrite(p.ina, 0);
        digitalWrite(p.inb, 0);
        analogWrite(p.pwm, 0);
    }
}

}  // namespace

void motors_init() {
    for (int i = 0; i < 3; ++i) {
        pinMode(MOTOR_PINS[i].ina, OUTPUT);
        pinMode(MOTOR_PINS[i].inb, OUTPUT);
        pinMode(MOTOR_PINS[i].pwm, OUTPUT);
    }
    motors_stop();
#if defined(ROBOT2)
    kicker_init();
#endif
}

void motors_apply_command(const MotorCommand& cmd) {
    // Convertir centideg/s a rad/s para la cinemática
    const float omega_rad_s = static_cast<float>(cmd.omega_centideg_s)
                            * (PI_F / 18000.0f);
    const float vx = static_cast<float>(cmd.vx_mm_s);
    const float vy = static_cast<float>(cmd.vy_mm_s);

    WheelSpeeds ws = inverse_kinematics(vx, vy, omega_rad_s, WHEELS);
    saturate_wheels(ws, MAX_SPEED_MM_S);

    for (int i = 0; i < 3; ++i) {
        int pwm = wheel_speed_to_pwm(ws.wheel[i], MAX_SPEED_MM_S, MAX_PWM);
        apply_pwm_to_motor(i, pwm);
    }

#if defined(ROBOT2)
    // Delantero: pulso one-shot al solenoide cuando strategy lo pide y el
    // cooldown está OK. Manejado adentro de kicker_update — el flag solo
    // arranca un pulso, no lo sostiene.
    kicker_update(cmd.kicker_fire);
#else
    // Arquero no tiene kicker — el flag se ignora silenciosamente para evitar
    // que se cuele un pulso por error si strategy lo setea.
    (void)cmd.kicker_fire;
#endif
}

void motors_stop() {
    for (int i = 0; i < 3; ++i) {
        apply_pwm_to_motor(i, 0);
    }
#if defined(ROBOT2)
    // Apagar el solenoide si hubo un pulso activo — no queremos que un
    // watchdog deje el pin HIGH.
    kicker_force_off();
#endif
}

void motors_brake() {
    // Freno activo: INA = INB = 1 con PWM = 0 → corto interno en el H-bridge.
    // Detiene el motor más rápido que motors_stop() pero genera corriente
    // de freno alta — solo usar para emergencias.
    for (int i = 0; i < 3; ++i) {
        const MotorPins& p = MOTOR_PINS[i];
        digitalWrite(p.ina, HIGH);
        digitalWrite(p.inb, HIGH);
        analogWrite(p.pwm, 0);
    }
#if defined(ROBOT2)
    kicker_force_off();
#endif
}

void motors_set_one(int motor_idx, int pwm_signed) {
    apply_pwm_to_motor(motor_idx, pwm_signed);
}

}  // namespace iitasoccer
