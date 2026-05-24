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
#include <climits>  // INT16_MIN

namespace iitasoccer {

// Pose 2D del robot en cancha (relativo al inicio).
struct Pose2D {
    int16_t x_mm;                 // posición X en cancha (mm)
    int16_t y_mm;                 // posición Y en cancha (mm)
    int16_t heading_centideg;     // heading (-18000 a +18000 = -180° a +180°)
    uint8_t confidence;           // 0-100 (calidad de la estimación)
} __attribute__((packed));

// Velocidades del robot.
// `slip_estimate` es específico del OTOS dual de la placa DOWN:
//   estima la magnitud del slip entre los 2 OTOS (mm/s observados de diferencia
//   más allá de lo esperable por la rotación). Útil para detectar patinazo al
//   patear / chocar. 0 = sin slip; valores altos (>50) son anomalías.
struct Velocity2D {
    int16_t vx_mm_s;              // velocidad lineal X (mm/s)
    int16_t vy_mm_s;              // velocidad lineal Y (mm/s)
    int16_t omega_centideg_s;     // velocidad angular (centideg/s)
    uint8_t slip_estimate;        // 0-255 (DOWN OTOS dual; 0 si N/A)
} __attribute__((packed));

// Estado del anillo de 32 sensores de línea (computado por DOWN).
struct LineStatus {
    int16_t angle_centideg;       // ángulo de la línea (0 = frente, ±18000)
    uint8_t depth_mm;             // qué tan dentro de la línea (mm, o # sensores)
    uint8_t imminent_exit_flag;   // 0 o 1: ≥ N sensores en blanco simultáneo
    uint8_t flags;                // bit 0 = lifted (robot en aire — ignorar datos)
                                  // bit 1 = mux_X_dead (algún mux roto)
                                  // bit 2 = calibration_invalid
                                  // bits 3-7 = reservados
} __attribute__((packed));

// Bit masks para LineStatus.flags
constexpr uint8_t LINE_FLAG_LIFTED              = 0x01;
constexpr uint8_t LINE_FLAG_MUX_DEAD            = 0x02;
constexpr uint8_t LINE_FLAG_CALIBRATION_INVALID = 0x04;

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

// WorldSnapshot — payload que la placa ARRIBA envía al CENTRAL a 100 Hz.
//
// Es el "single source of truth" para la FSM/strategy del CENTRAL: contiene
// todo lo que ARRIBA percibió en este tick (pose propia fusionada, pelota,
// arcos, partner, comando árbitro, flags útiles).
//
// CENTRAL NO fusiona — recibe este snapshot y decide. La fusión sensorial
// (IMU + OTOS + cámaras + ToF) corre en ARRIBA.
//
// Tamaño: 27 bytes (contrato v2: +ball_vx/vy). Cabe en proto (32).
struct WorldSnapshot {
    // Pose propia fusionada en cancha
    int16_t my_x_mm;
    int16_t my_y_mm;
    int16_t my_heading_centideg;
    uint8_t my_pose_confidence;     // 0-100

    // Pelota detectada (relativa al robot)
    int16_t ball_x_mm;
    int16_t ball_y_mm;
    uint8_t ball_visible;           // 0/1
    uint8_t ball_confidence;        // 0-100
    int16_t ball_vx_mm_s;           // velocidad pelota X (mm/s, marco robot); 0 si N/A
    int16_t ball_vy_mm_s;           // velocidad pelota Y (mm/s, marco robot); 0 si N/A

    // Arco rival (ángulo + distancia estimada)
    int16_t goal_opp_angle_centideg;
    int16_t goal_opp_distance_mm;
    uint8_t goal_opp_visible;
    uint8_t goal_own_visible;

    // Obstáculo más cercano (min de ToFs + HC-SR04)
    uint16_t min_obstacle_mm;

    // Comando árbitro vigente
    uint8_t referee_cmd;            // 0=stop, 1=start, 2=halftime, 3=reset

    // Flags útiles para strategy
    uint8_t flags;                  // bit 0 = in_own_penalty_area
                                    // bit 1 = partner_alive
                                    // bit 2 = partner_sees_ball
                                    // bit 3 = match_running
                                    // bits 4-7 = reservados
} __attribute__((packed));
static_assert(sizeof(WorldSnapshot) == 27, "WorldSnapshot contrato v2 = 27 bytes");

// Contrato DOWN→CENTRAL v2 — ver docs/firmware/CONTRATO-DATOS-DOWN.md
struct LineStatusV2 {
    uint8_t  schema_version;          // = LSV2_SCHEMA
    uint8_t  data_valid;              // 0/1 (compuerta maestra)
    int16_t  line_angle_centideg;     // N/A = LSV2_NA_I16
    int16_t  escape_angle_centideg;   // N/A = LSV2_NA_I16
    uint16_t penetration_mm;          // N/A = LSV2_NA_U16
    int16_t  cross_track_mm;          // N/A = LSV2_NA_I16
    uint8_t  line_present;            // 0/1 (con histéresis)
    uint8_t  sensors_on_line;         // 0..32
    uint8_t  event_flags;             // EV_* OR-eados
    uint8_t  quality;                 // 0..100
    uint8_t  sample_age_ms;           // 0..255
    uint8_t  reserved;                // = 0
} __attribute__((packed));
static_assert(sizeof(LineStatusV2) == 16, "LineStatusV2 debe ser 16 bytes (contrato)");

constexpr uint8_t  LSV2_SCHEMA = 2;
constexpr int16_t  LSV2_NA_I16 = INT16_MIN;   // -32768
constexpr uint16_t LSV2_NA_U16 = 0xFFFF;

constexpr uint8_t EV_IMMINENT_EXIT     = 0x01;
constexpr uint8_t EV_CORNER            = 0x02;
constexpr uint8_t EV_LINE_END          = 0x04;
constexpr uint8_t EV_LIFTED            = 0x08;
constexpr uint8_t EV_CALIB_SUSPECT     = 0x10;
constexpr uint8_t EV_MUX_DEAD          = 0x20;
constexpr uint8_t EV_DEGRADED_GEOMETRY = 0x40;

}  // namespace iitasoccer
