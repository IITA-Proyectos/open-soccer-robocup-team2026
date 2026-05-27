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
#include "cameras_runtime.h"   // drena Serial3+Serial5, fusiona front+back
#include "comm_down.h"         // recibe odometría OTOS desde ABAJO
#include "comm_arbiter.h"      // bridge a placa COMM
#include "comm_central.h"      // envía snapshot al CENTRAL
#include "localization_runtime.h"  // fusión TOF+IMU → pose absoluta en cancha
#include "types.h"

using namespace iitasoccer;

namespace {

elapsedMillis g_since_imu_tick;
elapsedMillis g_since_tof_tick;
elapsedMillis g_since_loc_tick;
elapsedMillis g_since_snapshot;
elapsedMillis g_since_debug;

uint32_t g_loop_count = 0;

// Construye el WorldSnapshot a partir de todas las fuentes percibidas.
WorldSnapshot build_snapshot() {
    WorldSnapshot s{};

    // Pose propia — trilateración TOF+IMU del módulo localization (Sprint 1).
    // El runtime cachea el último cómputo; si valid=false (p.ej. <2 TOFs útiles),
    // x/y caen a 0 y confidence=0 para que el CENTRAL sepa ignorar la pose.
    auto pose = iitasoccer::localization_runtime_get_pose();
    s.my_x_mm             = pose.x_mm;
    s.my_y_mm             = pose.y_mm;
    s.my_heading_centideg = pose.heading_centideg;
    s.my_pose_confidence  = pose.valid ? 70 : 0;

    // Pelota — fusión front+back desde cameras_runtime (sección 7.2 de
    // FIRMWARE-PLACA-ARRIBA.md). Coords relativas al robot en mm.
    s.ball_visible    = cameras_ball_visible() ? 1 : 0;
    s.ball_x_mm       = cameras_get_ball_x_mm();
    s.ball_y_mm       = cameras_get_ball_y_mm();
    s.ball_confidence = cameras_get_ball_confidence();

    // Arcos — mapping de colores → opp/own.
    // TODO: este mapping (yellow=opp, blue=own) está hardcoded. La polaridad
    // real depende del lado de cancha asignado por árbitro al inicio del
    // partido. Cuando se integre el comando del árbitro (referee_cmd) con
    // mensaje "play side", revisar y posiblemente invertir. Pendiente Enzo.
    s.goal_opp_visible        = cameras_goal_yellow_visible() ? 1 : 0;
    s.goal_opp_angle_centideg = cameras_get_goal_yellow_angle_centideg();
    s.goal_opp_distance_mm    = cameras_get_goal_yellow_distance_mm();
    s.goal_own_visible        = cameras_goal_blue_visible() ? 1 : 0;

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
    // OJO: el robot DEBE apuntar al arco rival (+Y) al boot — esta llamada
    // calibra bno_offset_centideg leyendo el heading actual.
    iitasoccer::localization_runtime_init();
    cameras_init();      // Serial3 + Serial5 ← OpenMV front + back
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
    cameras_tick();        // OpenMV front (Serial3) + back (Serial5)

    // === Sensores periódicos ===
    if (g_since_imu_tick >= IMU_TICK_INTERVAL_MS) {
        g_since_imu_tick = 0;
        sensors_imu_tick();
    }
    if (g_since_tof_tick >= TOF_TICK_INTERVAL_MS) {
        g_since_tof_tick = 0;
        sensors_tof_tick();
    }
    if (g_since_loc_tick >= 33) {  // ~30 Hz — matchea cadencia de los TOFs
        g_since_loc_tick = 0;
        iitasoccer::localization_runtime_tick();
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
        Serial.print(sensors_tof_get_min_distance_mm());
        Serial.print(" cam_F/B=");
        Serial.print(cameras_front_alive() ? "Y" : "N");
        Serial.print("/");
        Serial.print(cameras_back_alive() ? "Y" : "N");
        Serial.print(" ball=");
        if (cameras_ball_visible()) {
            Serial.print("(");
            Serial.print(cameras_get_ball_x_mm());
            Serial.print(",");
            Serial.print(cameras_get_ball_y_mm());
            Serial.print(")");
        } else {
            Serial.print("--");
        }
        Serial.print(" pkts_F/B=");
        Serial.print(cameras_packets_front());
        Serial.print("/");
        Serial.print(cameras_packets_back());
        Serial.print(" resync=");
        Serial.println(cameras_resyncs_total());
    }
}
