#include "motors_zircon.h"
#include "config_central.h"
#include "kinematics.h"
#ifdef CENTRAL_MOTOR_KICKSTART
#include "motor_kickstart.h"   // impulso inicial anti-inercia (módulo PURO, técnica 2025)
#endif
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
// El indice i alinea: Motor_i <-> MOTOR_PINS[i]/MOTOR_INVERT[i]/MOTOR_MIN_PWM[i] (driver) <-> WHEEL_ANGLES_DEG[i] (geometria) <-> posicion fisica.
// ROBOT1 (confirmado Gustavo 2026-06-08): i=0 M1/U5 330deg delantera-IZQUIERDA · i=1 M2/U17 210deg delantera-DERECHA (INVERTIDO HW) · i=2 M3/U7 90deg trasera.
const WheelConfig WHEELS[3] = {
    { WHEEL_ANGLES_DEG[0] * PI_F / 180.0f, WHEEL_RADIUS_MM },
    { WHEEL_ANGLES_DEG[1] * PI_F / 180.0f, WHEEL_RADIUS_MM },
    { WHEEL_ANGLES_DEG[2] * PI_F / 180.0f, WHEEL_RADIUS_MM },
};

#ifdef CENTRAL_MOTOR_KICKSTART
// IMPULSO INICIAL anti-inercia (gateado, default OFF → binario idéntico sin el flag).
// Técnica del robot 2025 portada (docs/firmware/MOTION-CONTROL-HISTORICO.md §1): al
// detectar la transición parado→comando de CADA rueda, durante una ventana corta el
// PWM se multiplica (1.8× por 40 ms, cap duro 153 < ~70% que quema los motores) para
// ROMPER el rozamiento estático; pasada la ventana fluye el PWM de régimen. El cálculo
// es el módulo PURO motor_kickstart (host-testeado); acá solo vive el cronómetro.
bool     g_kick_active[3]   = { false, false, false };
uint32_t g_kick_start_ms[3] = { 0, 0, 0 };
#endif

#ifdef CENTRAL_REAR_BRAKE_LEAD
// Freno anticipado de la trasera (ver motors_zircon.h). El caller manda el timing.
bool g_rear_cut = false;
#endif

#ifdef CENTRAL_MOTOR_KICKSTART
constexpr int KICKSTART_WINDOW_MS  = 40;   // ventana del impulso (2025: 40 ms)
// IMPULSO FIJO (banco robot2 2026-06-09, decisión de Gustavo): el ×1.8 multiplicativo
// dejaba el golpe DESPAREJO (delanteras 126, trasera 42×1.8=75 → no rompía inercia).
// Ahora el arranque va DIRECTO a 130 PWM en TODAS las ruedas: factor alto (×9.9) +
// cap 130 ⇒ cualquier base llega al cap ⇒ impulso fijo = 130, conservando el signo.
// (130 queda bajo el límite de quemado ~150 de los motores 5V a 7,4V.)
constexpr int KICKSTART_FACTOR_X10 = 99;   // ×9.9: garantiza que toda rueda llegue al cap
// Impulso FIJO POR RUEDA (banco robot2 2026-06-09): la trasera "se quedaba" al arrancar
// con 130 → su golpe sube a 140 (transitorio de 40 ms, tolerable; NO pasar ~150).
constexpr int KICKSTART_PWM_CAP[3] = { 130, 130, 140 };  // {M1, M2, M3=trasera}
#endif

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
    // cinemática → baja parejo el PWM de las 3 ruedas. ⚠️ OJO: el piso por rueda MOTOR_MIN_PWM[i]
    // actúa DESPUÉS y puede LEVANTAR el PWM ya escalado (estos motores no andan lento → el piso
    // gana). DEFAULT 1.0 = SIN EFECTO → binario de competencia IDÉNTICO. Se activa solo
    // con -DCENTRAL_SLOW_MOTION (env central_robotN_slow). ⚠️ NO usar en competencia.
#ifdef CENTRAL_SLOW_MOTION
    constexpr float MOTION_SCALE = 0.7f;   // banco/observación. (0.4 quedaba bajo el stiction
                                           // del motor → NO se movía. Estos motores tienen poco
                                           // rango lento: < ~30 PWM no arrancan. Si igual no mueve,
                                           // subir a 0.85; si es muy rápido, NO bajar de ~0.6.)
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
        // Piso de PWM POR RUEDA (deadzone bajo carga): a vx/vy bajos el PWM cae en la zona
        // muerta del motor y raspa/stalled. MOTOR_MIN_PWM[i] (por rueda, config_central.h) lo
        // eleva al piso de ESA rueda — las delanteras oblicuas necesitan más que la trasera
        // (si la trasera lleva el mismo piso se adelanta y hace ROTAR el robot en el strafe).
        // DEFAULT {0,0,0} (ROBOT2) → no-op → binario neutro.
        pwm = apply_pwm_floor(pwm, MOTOR_MIN_PWM[i], MOTOR_PWM_NOISE_THRESH);
#ifdef CENTRAL_REAR_BRAKE_LEAD
        // Freno anticipado: con el cut activo la TRASERA (idx 2) corta a 0 mientras las
        // delanteras siguen. Va ANTES del kickstart: el 0 re-arma el disparador del
        // impulso → el próximo arranque de la trasera vuelve a pegar el golpe inicial.
        if (i == 2 && g_rear_cut) pwm = 0;
#endif
#ifdef CENTRAL_MOTOR_KICKSTART
        // Impulso inicial: boost 1.8× por 40 ms SOLO en la transición parado→comando
        // de esta rueda (ver bloque de estado arriba). Actúa sobre el PWM final (post
        // piso). pwm==0 re-arma el disparador para el próximo arranque.
        {
            const uint32_t kick_now = millis();
            if (pwm == 0) {
                g_kick_active[i] = false;
            } else {
                if (!g_kick_active[i]) {
                    g_kick_active[i]   = true;
                    g_kick_start_ms[i] = kick_now;
                }
                pwm = motor_kickstart_pwm(pwm,
                                          static_cast<int>(kick_now - g_kick_start_ms[i]),
                                          KICKSTART_WINDOW_MS, KICKSTART_FACTOR_X10,
                                          KICKSTART_PWM_CAP[i]);
            }
        }
#endif
        apply_pwm_to_motor(i, pwm);
    }
}

void motors_stop() {
    for (int i = 0; i < 3; ++i) {
        apply_pwm_to_motor(i, 0);
#ifdef CENTRAL_MOTOR_KICKSTART
        // Re-armar el impulso inicial: sin esto, una parada vía motors_stop() (PAUSE de los
        // diags, watchdog, stop de la FSM) dejaba g_kick_active=true y el PRÓXIMO arranque
        // de esa rueda salía SIN golpe (hallazgo workflow 2026-06-09: las delanteras solo
        // kickeaban el primer tramo; la trasera sí re-armaba porque el rear-cut la pasa
        // por apply_command con 0).
        g_kick_active[i] = false;
#endif
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
#ifdef CENTRAL_MOTOR_KICKSTART
        g_kick_active[i] = false;   // re-armar el impulso (igual que motors_stop)
#endif
    }
}

void motors_set_one(int motor_idx, int pwm_signed) {
    apply_pwm_to_motor(motor_idx, pwm_signed);
}

#ifdef CENTRAL_REAR_BRAKE_LEAD
void motors_set_rear_cut(bool cut) { g_rear_cut = cut; }
#endif

}  // namespace iitasoccer
