// types.h — structs compartidas entre placas TOP y DOWN
//
// Convención: todos los valores con unidad explícita en el nombre del campo.
// Posiciones en mm, ángulos en centidegrees (grados × 100) para precisión sin float.
//
// Estos structs viajan como payload del protocolo UART (ver proto.h).
//
// NOTA HARDWARE (TOP board):
//   Wire1 (I2C bus 1) está físicamente remapeado a pines 24 (SCL1) y 25 (SDA1)
//   en la placa TOP. El firmware DEBE llamar antes de Wire1.begin():
//     Wire1.setSCL(24);
//     Wire1.setSDA(25);
//   Serial4 (RX4/TX4) queda en pines default 16/17.
//   Ver hardware/electronics/mapa-pines-placas-nuevas.md sección Q3.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Pose 2D del robot en cancha (relativo al inicio).
struct Pose2D {
    int16_t x_mm;                 // posición X en cancha (mm)
    int16_t y_mm;                 // posición Y en cancha (mm)
    int16_t heading_centideg;     // heading (-18000 a +18000 = -180° a +180°)
    uint8_t confidence;           // 0-100 (calidad de la estimación)
} __attribute__((packed));

// Velocidades del robot.
struct Velocity2D {
    int16_t vx_mm_s;              // velocidad lineal X (mm/s)
    int16_t vy_mm_s;              // velocidad lineal Y (mm/s)
    int16_t omega_centideg_s;     // velocidad angular (centideg/s)
} __attribute__((packed));

// Estado del anillo de 32 sensores de línea (computado por DOWN).
struct LineStatus {
    int16_t angle_centideg;       // ángulo de la línea (0 = frente, ±18000)
    uint8_t depth_mm;             // qué tan dentro de la línea (mm)
    uint8_t imminent_exit_flag;   // 0 o 1: ≥ N sensores en blanco simultáneo
} __attribute__((packed));

// Comando de motor desde TOP al Zircon (motor server).
struct MotorCommand {
    int16_t vx_mm_s;              // velocidad lineal X deseada
    int16_t vy_mm_s;              // velocidad lineal Y deseada
    int16_t omega_centideg_s;     // velocidad angular deseada
    uint8_t kicker_fire;          // 0 = no, 1 = patear (solo delantero)
    uint8_t dribbler_pwm;         // 0-255 (futuro)
} __attribute__((packed));

// Estado reportado por el Zircon al TOP.
struct ZirconStatus {
    uint8_t motors_ok;            // bitmask: bit 0=motor1, 1=motor2, 2=motor3
    uint8_t kicker_ready;         // 0/1
    uint16_t battery_mv;          // mV
} __attribute__((packed));

// Observación de pelota desde cámara.
struct BallObservation {
    int16_t x_mm;                 // posición relativa al robot (mm)
    int16_t y_mm;
    uint8_t confidence;           // 0-100
    uint8_t visible;              // 0 o 1
} __attribute__((packed));

}  // namespace iitasoccer
