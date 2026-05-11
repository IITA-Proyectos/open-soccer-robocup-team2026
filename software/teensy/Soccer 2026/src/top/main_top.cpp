// main_top.cpp — Loop principal de la placa TOP (Teensy 4.0 master).
//
// Orquesta TODOS los subsistemas:
//   • sensores: IMU dual, ToF + HC-SR04, cámaras OpenMV ×2
//   • comm: DOWN (slave sensores), COMM (árbitros + partner), Zircon (motores)
//   • lógica: world_model + strategy → MotorCommand
//
// Frecuencias:
//   • main loop:          ~100-200 Hz (un tick es muy rápido)
//   • sensors_imu_tick:   100 Hz (10 ms)
//   • sensors_tof_tick:   ~30 Hz (30 ms — los ToF I2C son lentos)
//   • strategy_tick:      100 Hz
//   • motors_send_command: 100 Hz
//   • cameras / comm RX:  every loop, no bloquea
//
// Build:
//   pio run -e top
//   pio run -e top -t upload

#include <Arduino.h>

#include "config_top.h"
#include "sensors_imu.h"
#include "sensors_tof.h"
#include "cameras.h"
#include "comm_down.h"
#include "comm_arbiter.h"
#include "motors.h"
#include "world_model.h"
#include "strategy.h"

using namespace iitasoccer;

namespace {

elapsedMillis g_since_imu_tick;
elapsedMillis g_since_tof_tick;
elapsedMillis g_since_strategy_tick;
elapsedMillis g_since_motors_send;
elapsedMillis g_since_debug_print;

uint32_t g_loop_count = 0;

void apply_role_from_dipswitch() {
    pinMode(PIN_ROLE_DIPSWITCH, INPUT_PULLUP);
    delay(10);
    const int v = digitalRead(PIN_ROLE_DIPSWITCH);
    if (v == LOW) {
        strategy_set_role(RobotRole::GOALKEEPER);
        Serial.println("[TOP] Role: GOALKEEPER (dipswitch LOW)");
    } else {
        strategy_set_role(RobotRole::ATTACKER);
        Serial.println("[TOP] Role: ATTACKER (dipswitch HIGH)");
    }
}

}  // namespace

void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    Serial.begin(115200);
    delay(100);
    Serial.println("\n========================================");
    Serial.println("  IITA Soccer Open — TOP firmware");
    Serial.println("  Roboliga 2026, Incheon target");
    Serial.println("========================================\n");

    apply_role_from_dipswitch();

    // Inicializar subsistemas en orden.
    Serial.println("[TOP] Init IMU dual...");
    sensors_imu_init();

    Serial.println("[TOP] Init ToF + HC-SR04...");
    sensors_tof_init();

    Serial.println("[TOP] Init UART desde DOWN...");
    comm_down_init();

    Serial.println("[TOP] Init UART hacia Zircon...");
    motors_init();

    Serial.println("[TOP] Init UART hacia COMM...");
    comm_arbiter_init();

    Serial.println("[TOP] Init world_model + strategy...");
    world_model_init();
    strategy_init();

    digitalWrite(PIN_LED_STATUS, HIGH);
    Serial.println("[TOP] Setup completo. Esperando match start del COMM.");
}

void loop() {
    g_loop_count++;

    // === RX: drenar todas las UARTs (no bloquea) ===
    comm_down_tick();
    comm_arbiter_tick();
    motors_tick_rx();
    // cameras_tick();  // TODO: integrar parser de cameras.h cuando esté conectado a Serial3/Serial5

    // === Sensores periódicos ===
    if (g_since_imu_tick >= IMU_TICK_INTERVAL_MS) {
        g_since_imu_tick = 0;
        sensors_imu_tick();
    }
    if (g_since_tof_tick >= TOF_TICK_INTERVAL_MS) {
        g_since_tof_tick = 0;
        sensors_tof_tick();
    }

    // === Estrategia + motores ===
    if (g_since_strategy_tick >= STRATEGY_TICK_INTERVAL_MS) {
        g_since_strategy_tick = 0;
        world_model_update();
        const MotorCommand cmd = strategy_tick();

        // Enviar comando al Zircon (a 100 Hz).
        if (g_since_motors_send >= MOTORS_SEND_INTERVAL_MS) {
            g_since_motors_send = 0;
            motors_send_command(cmd.vx_mm_s, cmd.vy_mm_s,
                                cmd.omega_centideg_s, cmd.kicker_fire != 0);
        }
    }

    // === Debug print cada 500 ms ===
    if (g_since_debug_print >= 500) {
        g_since_debug_print = 0;
        Serial.print("[TOP] loop=");
        Serial.print(g_loop_count);
        Serial.print(" role=");
        Serial.print(strategy_get_role() == RobotRole::ATTACKER ? "ATK" : "GK");
        Serial.print(" state=");
        Serial.print(strategy_get_state_name());
        Serial.print(" match=");
        Serial.print(world_model_match_running() ? "RUN" : "STOP");
        Serial.print(" hdg=");
        Serial.print(world_model_get_my_heading_deg(), 1);
        Serial.print(" down_line_fresh=");
        Serial.print(comm_down_is_line_fresh() ? "Y" : "N");
        Serial.print(" zircon_fresh=");
        Serial.println(motors_zircon_is_fresh() ? "Y" : "N");
    }
}
