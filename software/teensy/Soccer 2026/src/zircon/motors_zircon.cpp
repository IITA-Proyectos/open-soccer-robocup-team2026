#include "motors_zircon.h"
#include "config_zircon.h"
#include "kinematics.h"
#include <Arduino.h>

namespace iitasoccer {

namespace {

// Pinout indexado por motor — los `constexpr int PIN_*` de config_zircon.h se
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

    // TODO kicker: activar solenoide cuando cmd.kicker_fire == 1 (solo en ROBOT2)
    // Requiere pin del solenoide en config_zircon.h — confirmar con Enzo.
}

void motors_stop() {
    for (int i = 0; i < 3; ++i) {
        apply_pwm_to_motor(i, 0);
    }
}

void motors_set_one(int motor_idx, int pwm_signed) {
    apply_pwm_to_motor(motor_idx, pwm_signed);
}

}  // namespace iitasoccer
