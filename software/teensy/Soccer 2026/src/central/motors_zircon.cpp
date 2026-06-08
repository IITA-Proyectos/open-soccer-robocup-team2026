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
// El indice i alinea: Motor_i <-> MOTOR_PINS[i]/MOTOR_INVERT[i] (driver) <-> WHEEL_ANGLES_DEG[i] (geometria) <-> posicion fisica.
// ROBOT1: i=0 M1/U5 +60deg delantera-derecha · i=1 M2/U17 -60deg delantera-izquierda (INVERTIDO HW) · i=2 M3/U7 180deg trasera.
const WheelConfig WHEELS[3] = {
    { WHEEL_ANGLES_DEG[0] * PI_F / 180.0f, WHEEL_RADIUS_MM },
    { WHEEL_ANGLES_DEG[1] * PI_F / 180.0f, WHEEL_RADIUS_MM },
    { WHEEL_ANGLES_DEG[2] * PI_F / 180.0f, WHEEL_RADIUS_MM },
};

void apply_pwm_to_motor(int motor_idx, int pwm_signed) {
    if (motor_idx < 0 || motor_idx >= 3) return;
    const MotorPins& p = MOTOR_PINS[motor_idx];
    // Sentido por motor: algunos drivers tienen INA/INB invertidos por hardware
    // (validado en banco — ver MOTOR_INVERT en config_central.h). Negar el PWM
    // firmado equivale a cruzar INA/INB, igual que diag_central_line_sweep::motor2().
    pwm_signed *= MOTOR_INVERT[motor_idx];
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
}

void motors_apply_command(const MotorCommand& cmd) {
    // SLOW-MO DE BANCO (gateado): escala TODO el comando (vx/vy/omega) para poder OBSERVAR
    // la conducta del arquero sin que sea brusco. Cae sobre la velocidad antes de la
    // cinemática → baja parejo el PWM de las 3 ruedas (el piso MOTOR_MIN_PWM=0 por default no
    // interfiere). DEFAULT 1.0 = SIN EFECTO → binario de competencia IDÉNTICO. Se activa solo
    // con -DCENTRAL_SLOW_MOTION (env central_robotN_slow). ⚠️ NO usar en competencia.
#ifdef CENTRAL_SLOW_MOTION
    constexpr float MOTION_SCALE = 0.4f;   // ~40% para banco/observación
#else
    constexpr float MOTION_SCALE = 1.0f;
#endif
    // Convertir centideg/s a rad/s para la cinemática
    const float omega_rad_s = static_cast<float>(cmd.omega_centideg_s)
                            * (PI_F / 18000.0f) * MOTION_SCALE;
    const float vx = static_cast<float>(cmd.vx_mm_s) * MOTION_SCALE;
    const float vy = static_cast<float>(cmd.vy_mm_s) * MOTION_SCALE;

    WheelSpeeds ws = inverse_kinematics(vx, vy, omega_rad_s, WHEELS);
    saturate_wheels(ws, MAX_SPEED_MM_S);

    for (int i = 0; i < 3; ++i) {
        int pwm = wheel_speed_to_pwm(ws.wheel[i], MAX_SPEED_MM_S, MAX_PWM);
        // Piso de PWM (deadzone compensation): a vx/vy bajos el PWM cae en la zona
        // muerta del motor y raspa/stalled. Con MOTOR_MIN_PWM>0 (banco) lo eleva al
        // piso. DEFAULT MOTOR_MIN_PWM=0 → no-op → binario de competencia idéntico.
        pwm = apply_pwm_floor(pwm, MOTOR_MIN_PWM, MOTOR_PWM_NOISE_THRESH);
        apply_pwm_to_motor(i, pwm);
    }
}

void motors_stop() {
    for (int i = 0; i < 3; ++i) {
        apply_pwm_to_motor(i, 0);
    }
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
}

void motors_set_one(int motor_idx, int pwm_signed) {
    apply_pwm_to_motor(motor_idx, pwm_signed);
}

}  // namespace iitasoccer
