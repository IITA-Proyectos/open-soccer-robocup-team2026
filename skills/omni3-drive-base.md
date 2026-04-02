# 🔄 OmniDriveBase — API de Alto Nivel para Robot 3 Ruedas Omni

## Skill: DriveBase estilo Pybricks pero para 3 ruedas omnidireccionales
## Traslación y rotación independientes, field-centric, PID con gyro

---

## 1. CONCEPTO

Esta clase permite controlar un robot omnidireccional de 3 ruedas con instrucciones de alto nivel:

```cpp
OmniDriveBase base(motor1, motor2, motor3, imu);

// Moverse 300mm hacia la derecha manteniendo heading
base.move(90, 300);          // dirección 90°, distancia 300mm

// Girar 45° en el lugar
base.turn(45);

// Ir a un punto del campo
base.go_to(500, 800, 0);     // x=500mm, y=800mm, heading=0°

// Moverse continuamente (velocidad) mirando a 45°
base.drive(200, 0, 45);      // vel=200mm/s, dir=0°(adelante), head=45°

// Moverse en field-centric: ir hacia el norte del campo sin importar rotación
base.drive_field(0, 200, 0); // vx=0, vy=200, mantener heading=0

base.stop();
```

---

## 2. GEOMETRÍA DEL ROBOT

```
3 ruedas omni a 120°, montadas a distancia R del centro.

Vista superior (rueda 0 = frente):

           [W0] 0° (frente)
           / \
          /   \
         /     \
     [W1]       [W2]
     240°       120°

Parámetros físicos a medir:
  WHEEL_RADIUS:  radio de la rueda omni (mm)
  ROBOT_RADIUS:  distancia del centro del robot al centro de la rueda (mm)
  ENCODER_CPR:   pulsos del encoder por revolución completa de la rueda
  GEAR_RATIO:    relación de reducción (motor/rueda)
```

---

## 3. CLASE COMPLETA C++

```cpp
// ============================================================================
// OmniDriveBase — API de alto nivel para 3 ruedas omnidireccionales
// ============================================================================
//
// Características:
//   - Traslación y rotación independientes
//   - Field-centric (referenciado al gyro)
//   - Mover por distancia (mm) y girar por grados
//   - PID de heading, PID de posición, PID de velocidad de motores
//   - Odometría integrada (encoders + gyro)
//   - Opcional: posicionamiento ToF/EKF
//
// ============================================================================

#include <Arduino.h>
#include <math.h>

// Forward declarations
class PID_Controller;
class IMU_Interface;
class Motor_Interface;

// ============= CONFIGURACIÓN =============

struct OmniConfig {
    float wheel_radius_mm = 30.0f;     // Radio de la rueda (mm)
    float robot_radius_mm = 85.0f;     // Centro del robot a centro de rueda (mm)
    float encoder_cpr = 360.0f;        // Pulsos por revolución
    float gear_ratio = 1.0f;           // Reducción (>1 = motor gira más que rueda)
    float max_speed_mmps = 1500.0f;    // Velocidad máxima (mm/s)
    float max_accel_mmps2 = 3000.0f;   // Aceleración máxima (mm/s²)
    float max_omega_dps = 360.0f;      // Velocidad angular máxima (°/s)

    // Ángulos de montaje de las ruedas (en radianes)
    float wheel_angles[3] = {
        0.0f,            // Rueda 0: frente (0°)
        2.0944f,         // Rueda 1: 120°
        4.1888f          // Rueda 2: 240°
    };

    // PID de heading
    float heading_kp = 3.0f;
    float heading_ki = 0.1f;
    float heading_kd = 0.5f;

    // PID de posición (para go_to)
    float position_kp = 2.0f;
    float position_ki = 0.0f;
    float position_kd = 1.0f;

    // Tolerancias
    float position_tolerance_mm = 15.0f;   // Llegaste si estás a <15mm
    float heading_tolerance_deg = 2.0f;    // Heading correcto si <2°
    float settle_time_ms = 100;            // Tiempo de estabilización
};

// ============= CLASE PRINCIPAL =============

class OmniDriveBase {
public:
    OmniConfig config;

    // Estado
    float x = 0, y = 0;          // Posición estimada (mm)
    float heading = 0;            // Heading actual (°) del gyro
    float vx = 0, vy = 0;        // Velocidad actual (mm/s)
    float omega = 0;              // Velocidad angular actual (°/s)

    // ====== INICIALIZACIÓN ======

    void begin() {
        // Configurar motores, encoders, IMU
        // (implementación depende del hardware)
        reset();
    }

    void reset() {
        x = y = 0;
        heading_offset = read_imu_heading_raw();
        odo_x = odo_y = 0;
    }

    // Setear posición conocida (ej: desde ToF al inicio)
    void set_position(float new_x, float new_y, float new_heading) {
        x = new_x;
        y = new_y;
        heading_offset = read_imu_heading_raw() - new_heading;
    }

    // ====== COMANDOS DE VELOCIDAD (continuos) ======

    // Mover con velocidad y rotación INDEPENDIENTES (robot-centric)
    // speed_mmps: velocidad de traslación (mm/s)
    // direction_deg: dirección de movimiento relativa al robot (0=frente, 90=derecha)
    // target_heading_deg: heading deseado (field-centric, referenciado al gyro)
    void drive(float speed_mmps, float direction_deg, float target_heading_deg) {
        float dir_rad = direction_deg * DEG2RAD;
        float vx_r = speed_mmps * sinf(dir_rad);
        float vy_r = speed_mmps * cosf(dir_rad);

        // PID de heading
        float h_error = normalize_angle(target_heading_deg - get_heading());
        float omega_cmd = config.heading_kp * h_error;  // P simple para velocidad
        omega_cmd = constrain_f(omega_cmd, -config.max_omega_dps, config.max_omega_dps);

        apply_velocities(vx_r, vy_r, omega_cmd * DEG2RAD);
    }

    // Mover en coordenadas del CAMPO (field-centric)
    // vx_field, vy_field: velocidades en mm/s en coordenadas del campo
    // target_heading: heading deseado (°)
    void drive_field(float vx_field, float vy_field, float target_heading) {
        float h_rad = get_heading() * DEG2RAD;

        // Rotar al frame del robot
        float vx_r =  cosf(h_rad) * vx_field + sinf(h_rad) * vy_field;
        float vy_r = -sinf(h_rad) * vx_field + cosf(h_rad) * vy_field;

        // PID de heading
        float h_error = normalize_angle(target_heading - get_heading());
        float omega_cmd = config.heading_kp * h_error;
        omega_cmd = constrain_f(omega_cmd, -config.max_omega_dps, config.max_omega_dps);

        apply_velocities(vx_r, vy_r, omega_cmd * DEG2RAD);
    }

    void stop() {
        set_motor_speeds(0, 0, 0);
    }

    // ====== COMANDOS DE MOVIMIENTO (bloqueantes, con PID) ======

    // Mover distancia en una dirección (field-centric), manteniendo heading
    // direction_deg: dirección en grados (0=adelante en el campo, 90=derecha)
    // distance_mm: distancia a recorrer
    // speed_mmps: velocidad (default: 500mm/s)
    // heading_deg: heading a mantener durante el movimiento
    void move(float direction_deg, float distance_mm,
              float speed_mmps = 500, float heading_deg = NAN) {

        if (isnan(heading_deg)) heading_deg = get_heading();  // Mantener actual

        float start_x = x, start_y = y;
        float dir_rad = direction_deg * DEG2RAD;
        float target_x = x + distance_mm * sinf(dir_rad);
        float target_y = y + distance_mm * cosf(dir_rad);

        unsigned long t0 = millis();
        unsigned long settled_since = 0;

        while (true) {
            update_odometry();

            float dx = target_x - x;
            float dy = target_y - y;
            float dist_remaining = sqrtf(dx*dx + dy*dy);

            if (dist_remaining < config.position_tolerance_mm) {
                if (settled_since == 0) settled_since = millis();
                if (millis() - settled_since > config.settle_time_ms) break;
            } else {
                settled_since = 0;
            }

            // Timeout safety
            if (millis() - t0 > 5000) break;  // 5s max

            // Velocidad proporcional a distancia (desacelerar al llegar)
            float spd = min(speed_mmps, dist_remaining * config.position_kp);
            spd = max(spd, 50.0f);  // Mínimo 50mm/s para no quedarse

            drive_field(dx / dist_remaining * spd,
                        dy / dist_remaining * spd,
                        heading_deg);

            delay(10);  // 100 Hz
        }
        stop();
    }

    // Girar en el lugar a un heading absoluto
    void turn(float target_heading_deg) {
        unsigned long t0 = millis();
        unsigned long settled_since = 0;

        while (true) {
            update_odometry();

            float error = normalize_angle(target_heading_deg - get_heading());

            if (fabsf(error) < config.heading_tolerance_deg) {
                if (settled_since == 0) settled_since = millis();
                if (millis() - settled_since > config.settle_time_ms) break;
            } else {
                settled_since = 0;
            }

            if (millis() - t0 > 3000) break;  // 3s max

            float omega_cmd = config.heading_kp * error +
                              config.heading_kd * (error - prev_heading_error) * 100;
            prev_heading_error = error;
            omega_cmd = constrain_f(omega_cmd, -config.max_omega_dps, config.max_omega_dps);

            apply_velocities(0, 0, omega_cmd * DEG2RAD);
            delay(10);
        }
        stop();
    }

    // Girar un ángulo RELATIVO (positivo = horario)
    void turn_relative(float degrees) {
        turn(get_heading() + degrees);
    }

    // Ir a un punto del campo con heading final
    void go_to(float target_x, float target_y, float target_heading = NAN) {
        if (isnan(target_heading)) target_heading = get_heading();

        unsigned long t0 = millis();
        unsigned long settled_since = 0;

        while (true) {
            update_odometry();

            float dx = target_x - x;
            float dy = target_y - y;
            float dist = sqrtf(dx*dx + dy*dy);
            float h_error = normalize_angle(target_heading - get_heading());

            bool pos_ok = dist < config.position_tolerance_mm;
            bool head_ok = fabsf(h_error) < config.heading_tolerance_deg;

            if (pos_ok && head_ok) {
                if (settled_since == 0) settled_since = millis();
                if (millis() - settled_since > config.settle_time_ms) break;
            } else {
                settled_since = 0;
            }

            if (millis() - t0 > 10000) break;  // 10s max

            // Velocidad proporcional a distancia
            float spd = min(config.max_speed_mmps, dist * config.position_kp);
            spd = max(spd, 0.0f);

            float vx_f = (dist > 10) ? dx / dist * spd : 0;
            float vy_f = (dist > 10) ? dy / dist * spd : 0;

            drive_field(vx_f, vy_f, target_heading);
            delay(10);
        }
        stop();
    }

    // ====== ESTADO ======

    float get_heading() const {
        return normalize_angle(read_imu_heading_raw() - heading_offset);
    }

    float get_x() const { return x; }
    float get_y() const { return y; }
    float get_speed() const { return sqrtf(vx*vx + vy*vy); }
    bool is_moving() const { return get_speed() > 20.0f; }

    // Distancia recorrida desde último reset
    float distance() const {
        return sqrtf(odo_x*odo_x + odo_y*odo_y);
    }

    // ====== ODOMETRÍA ======

    void update_odometry() {
        float dt = 0.01f;  // Asumir 100Hz

        // Leer velocidades de encoders (rad/s de cada rueda)
        float w0 = get_encoder_speed(0);
        float w1 = get_encoder_speed(1);
        float w2 = get_encoder_speed(2);

        // Cinemática directa: velocidades del robot (robot-frame)
        float vx_r = (-w1 + w2) * 0.5774f * config.wheel_radius_mm;
        float vy_r = (2*w0 - w1 - w2) / 3.0f * config.wheel_radius_mm;
        // omega la tomamos del gyro (más preciso que encoders)

        // Heading del gyro
        float h_rad = get_heading() * DEG2RAD;

        // Rotar al frame del campo
        vx =  cosf(h_rad) * vx_r - sinf(h_rad) * vy_r;
        vy =  sinf(h_rad) * vx_r + cosf(h_rad) * vy_r;

        // Integrar posición
        x += vx * dt;
        y += vy * dt;
        odo_x += vx * dt;
        odo_y += vy * dt;
    }

    // Corregir posición con fuente externa (ToF, EKF, etc.)
    void correct_position(float measured_x, float measured_y, float weight = 0.3f) {
        x = x * (1 - weight) + measured_x * weight;
        y = y * (1 - weight) + measured_y * weight;
    }

private:
    float heading_offset = 0;
    float odo_x = 0, odo_y = 0;
    float prev_heading_error = 0;
    static constexpr float DEG2RAD = 0.017453f;
    static constexpr float RAD2DEG = 57.2958f;

    // ====== CINEMÁTICA INVERSA 3 RUEDAS ======

    void apply_velocities(float vx_robot, float vy_robot, float omega_rad) {
        float R = config.robot_radius_mm;
        float r = config.wheel_radius_mm;

        float w[3];
        for (int i = 0; i < 3; i++) {
            float a = config.wheel_angles[i];
            w[i] = (-sinf(a) * vx_robot + cosf(a) * vy_robot + R * omega_rad) / r;
        }

        // Normalizar si alguna rueda excede el máximo
        float max_w = 0;
        float max_motor_speed = config.max_speed_mmps / r;
        for (int i = 0; i < 3; i++) {
            if (fabsf(w[i]) > max_w) max_w = fabsf(w[i]);
        }
        if (max_w > max_motor_speed) {
            float scale = max_motor_speed / max_w;
            for (int i = 0; i < 3; i++) w[i] *= scale;
        }

        set_motor_speeds(w[0], w[1], w[2]);
    }

    static float normalize_angle(float a) {
        while (a > 180) a -= 360;
        while (a < -180) a += 360;
        return a;
    }

    static float constrain_f(float v, float lo, float hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    // ====== STUBS DE HARDWARE (implementar según placa) ======

    // Leer heading raw del IMU (grados, 0-360)
    float read_imu_heading_raw() const {
        // return bno.getHeading();
        return 0;  // IMPLEMENTAR
    }

    // Leer velocidad angular del encoder (rad/s)
    float get_encoder_speed(int motor_idx) const {
        // return encoder[motor_idx].getSpeed() * 2*PI / config.encoder_cpr / config.gear_ratio;
        return 0;  // IMPLEMENTAR
    }

    // Setear velocidades de motores (rad/s de rueda)
    void set_motor_speeds(float w0, float w1, float w2) {
        // Convertir rad/s a PWM o a setpoint del PID de velocidad
        // motor[0].setSpeed(w0);
        // IMPLEMENTAR
    }
};
```

---

## 4. EJEMPLO DE USO COMPLETO

```cpp
#include "OmniDriveBase.h"
#include "BNO055_Competition.h"

BNO055_Competition imu;
OmniDriveBase base;

void setup() {
    imu.begin(MODE_IMUPLUS);
    while (!imu.is_gyro_calibrated()) delay(100);

    base.config.wheel_radius_mm = 30;
    base.config.robot_radius_mm = 85;
    base.config.encoder_cpr = 360;
    base.begin();

    // Posición inicial conocida (zona de inicio)
    base.set_position(200, 200, 0);
}

void loop() {
    // Ejemplo: secuencia de movimientos

    // 1. Avanzar 400mm hacia el arco rival (0° en el campo)
    base.move(0, 400);

    // 2. Moverse 200mm a la derecha sin cambiar heading
    base.move(90, 200);

    // 3. Girar para mirar a la derecha (90°)
    base.turn(90);

    // 4. Ir a punto específico del campo mirando hacia adelante
    base.go_to(500, 1000, 0);

    // 5. Velocidad continua: moverse al frente mirando a 45°
    base.drive(300, 0, 45);  // 300mm/s, dirección=frente, head=45°
    delay(2000);  // 2 segundos
    base.stop();

    // 6. Velocidad field-centric: ir hacia el norte a 200mm/s
    base.drive_field(0, 200, 0);  // vx=0, vy=200, heading=0
    delay(1000);
    base.stop();

    while (true);  // Fin
}
```

---

## 5. INTEGRACIÓN CON ToF PARA POSICIÓN ABSOLUTA

```cpp
// En el loop principal (no dentro de move/go_to):
void update_position_from_tof() {
    float tof_x, tof_y;
    if (tof_array.compute_position(base.get_heading(), tof_x, tof_y)) {
        // Corregir odometría con posición absoluta (30% peso ToF)
        base.correct_position(tof_x, tof_y, 0.3f);
    }
}
```

---

## 6. API RESUMIDA

| Método | Tipo | Descripción |
|--------|------|------------|
| `drive(speed, direction, heading)` | Continuo | Moverse a velocidad constante con heading independiente |
| `drive_field(vx, vy, heading)` | Continuo | Velocidad en coordenadas del campo |
| `move(direction, distance)` | Bloqueante | Mover distancia exacta en una dirección |
| `turn(heading)` | Bloqueante | Girar a heading absoluto |
| `turn_relative(degrees)` | Bloqueante | Girar ángulo relativo |
| `go_to(x, y, heading)` | Bloqueante | Ir a punto del campo |
| `stop()` | Instantáneo | Parar motores |
| `get_heading()` | Lectura | Heading actual (gyro) |
| `get_x(), get_y()` | Lectura | Posición estimada |
| `distance()` | Lectura | Distancia total recorrida |
| `correct_position(x, y)` | Corrección | Actualizar posición con fuente externa |
| `set_position(x, y, h)` | Reset | Setear posición conocida |

---

## 7. CONFIGURACIÓN TÍPICA POR COMPETENCIA

| Parámetro | RoboCup Soccer | WRO RoboSports |
|-----------|:-:|:-:|
| wheel_radius | 25-35 mm | 30-40 mm |
| robot_radius | 70-90 mm | 80-100 mm |
| max_speed | 1000-2000 mm/s | 500-1500 mm/s |
| heading_kp | 2.0-4.0 | 2.0-4.0 |
| position_kp | 1.5-3.0 | 1.5-3.0 |
| tolerance | 10-20 mm | 15-25 mm |

---

## 8. PARÁMETROS DE RUEDAS NO ESTÁNDAR

Si las ruedas NO están a 0°/120°/240° (ej: 60°/180°/300°):

```cpp
// Cambiar los ángulos en config:
base.config.wheel_angles[0] = 60 * DEG2RAD;   // 60°
base.config.wheel_angles[1] = 180 * DEG2RAD;  // 180°
base.config.wheel_angles[2] = 300 * DEG2RAD;  // 300°
```

La cinemática inversa es genérica y funciona con CUALQUIER disposición angular.

---

## 9. REGLAS DE ORO

| Regla | Por qué |
|-------|--------|
| **Medir wheel_radius con precisión** | 1mm de error = 3% de error en distancia |
| **Medir robot_radius con precisión** | Afecta los giros |
| **Calibrar PID de heading primero** | Sin heading estable, nada funciona |
| **Usar gyro para heading, encoders para distancia** | Gyro no driftea en distancia corta |
| **Corregir con ToF cuando sea posible** | Odometría driftea con el tiempo |
| **move() es bloqueante: no usar en loop de competencia** | Usar drive_field() con FSM |
| **Normalizar ángulos SIEMPRE** | 350° - 10° = -20°, no 340° |

---

## FUENTES

- Pybricks DriveBase API (inspiración para la interfaz)
- Oliveira et al.: Dynamical Models for Omni-directional Robots (2008)
- Modern Robotics (Northwestern) Ch.13.2
- manav20/3-wheel-omni (GitHub): PID speed control + vectoring
- CMU Minnow/Hammerhead: competition omni robots
- Springer: 3-servo-wheel omnidirectional platform with OPS-9 positioning
- IITA legacy RoboCup 2025 kinematics
- Ver skill genérico: `wro-2026-robosport-nacional-iita-salta/skills/02-movement/omnidirectional-kinematics.md`
