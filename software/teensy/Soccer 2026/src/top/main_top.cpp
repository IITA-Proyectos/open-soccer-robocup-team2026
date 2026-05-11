// main_top.cpp — Firmware del cerebro sensorial (Teensy 4.0 sobre placa TOP).
//
// Responsabilidad: percibir el mundo y entregar un WORLD_SNAPSHOT al CENTRAL.
// TOP NO toma decisiones tácticas ni controla motores — sólo percibe + fusiona.
//
// Inputs:
//   • 2 cámaras OpenMV (Serial3 + Serial5)
//   • 2 BNO055 IMU (I2C Wire + Wire1 remap 24/25)
//   • 4 ToF + 1 HC-SR04 (I2C dual + GPIO)
//   • Odometría OTOS desde ABAJO (Serial1)
//   • Comm árbitros + partner ESP-NOW (Serial4 → placa COMM)
//
// Outputs:
//   • WORLD_SNAPSHOT a CENTRAL (Serial2) a 100 Hz.
//
// Build:
//   pio run -e top

#include <Arduino.h>

#include "config_top.h"
#include "sensors_imu.h"
#include "sensors_tof.h"
#include "cameras.h"
#include "comm_down.h"      // recibe odometría OTOS desde ABAJO
#include "comm_arbiter.h"   // bridge a placa COMM
#include "comm_central.h"   // envía snapshot al CENTRAL
#include "types.h"

using namespace iitasoccer;

namespace {

elapsedMillis g_since_imu_tick;
elapsedMillis g_since_tof_tick;
elapsedMillis g_since_snapshot;
elapsedMillis g_since_debug;

uint32_t g_loop_count = 0;

// Construye el WorldSnapshot a partir de todas las fuentes percibidas.
WorldSnapshot build_snapshot() {
    WorldSnapshot s{};

    // Pose propia — por ahora solo heading del IMU dual. Pose absoluta (x, y)
    // requiere fusión cámaras + OTOS — pendiente Nivel 2 / EKF.
    s.my_x_mm = 0;
    s.my_y_mm = 0;
    s.my_heading_centideg = static_cast<int16_t>(sensors_imu_get_heading_deg() * 100.0f);
    s.my_pose_confidence = (sensors_imu_left_ready() || sensors_imu_right_ready()) ? 60 : 0;

    // Pelota — provendrá de cameras_tick() cuando se integre el parser.
    s.ball_visible = 0;
    s.ball_x_mm = 0;
    s.ball_y_mm = 0;
    s.ball_confidence = 0;

    // Arcos — idem.
    s.goal_opp_visible = 0;
    s.goal_own_visible = 0;
    s.goal_opp_angle_centideg = 0;
    s.goal_opp_distance_mm = 0;

    // Obstáculo más cercano (de ToFs + HC-SR04).
    s.min_obstacle_mm = sensors_tof_get_min_distance_mm();

    // Comando árbitro y flags.
    s.referee_cmd = static_cast<uint8_t>(comm_arbiter_get_last_command());
    uint8_t flags = 0;
    if (comm_arbiter_is_match_running())    flags |= (1 << 3);
    if (comm_arbiter_partner_is_fresh())    flags |= (1 << 1);
    // bit 0 (in_own_penalty_area) requiere pose absoluta — Nivel 2.
    // bit 2 (partner_sees_ball) requiere parseo del partner snapshot — futuro.
    s.flags = flags;

    return s;
}

}  // namespace

void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    Serial.begin(115200);
    delay(100);
    Serial.println("\n=========================================");
    Serial.println("  IITA Soccer Open — TOP firmware");
    Serial.println("  Cerebro sensorial (Teensy 4.0)");
    Serial.println("=========================================");

    sensors_imu_init();
    sensors_tof_init();
    comm_down_init();    // Serial1 ← odometría desde ABAJO
    comm_arbiter_init(); // Serial4 ↔ placa COMM
    comm_central_init(); // Serial2 → snapshot a CENTRAL

    digitalWrite(PIN_LED_STATUS, HIGH);
    Serial.println("[TOP] cerebro sensorial listo, enviando snapshots a CENTRAL");
}

void loop() {
    g_loop_count++;

    // === RX: drenar UARTs (no bloquea) ===
    comm_down_tick();      // odometría OTOS desde ABAJO
    comm_arbiter_tick();   // comm con placa COMM (árbitros + partner)
    comm_central_tick();   // comandos desde CENTRAL (reset, calib)

    // === Sensores periódicos ===
    if (g_since_imu_tick >= IMU_TICK_INTERVAL_MS) {
        g_since_imu_tick = 0;
        sensors_imu_tick();
    }
    if (g_since_tof_tick >= TOF_TICK_INTERVAL_MS) {
        g_since_tof_tick = 0;
        sensors_tof_tick();
    }

    // === Snapshot → CENTRAL ===
    if (g_since_snapshot >= 10) {  // 100 Hz
        g_since_snapshot = 0;
        WorldSnapshot snap = build_snapshot();
        comm_central_send_snapshot(snap);
    }

    // === Debug ===
    if (g_since_debug >= 500) {
        g_since_debug = 0;
        Serial.print("[TOP] loop=");
        Serial.print(g_loop_count);
        Serial.print(" hdg=");
        Serial.print(sensors_imu_get_heading_deg(), 1);
        Serial.print(" imu_L=");
        Serial.print(sensors_imu_left_ready() ? "Y" : "N");
        Serial.print(" imu_R=");
        Serial.print(sensors_imu_right_ready() ? "Y" : "N");
        Serial.print(" min_obst=");
        Serial.println(sensors_tof_get_min_distance_mm());
    }
}
