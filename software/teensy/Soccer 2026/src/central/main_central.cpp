// main_central.cpp — Firmware master del robot (Teensy 4.1 sobre Zircon Rev v15).
//
// Responsabilidad: decidir qué hacer y ejecutarlo.
//   • Recibe WORLD_SNAPSHOT desde ARRIBA (Serial7, pin 28) a 100 Hz.
//   • Recibe LINE_URGENT desde ABAJO (Serial1, pin 0) a 100-200 Hz.
//   • Corre la FSM principal (strategy).
//   • Corre todos los PIDs (heading + lateral arquero + approach).
//   • Aplica cinemática inversa omni-3 y PWM directo a los 3 motores del Zircon.
//   • Lee BNO055 local del Zircon como respaldo si ARRIBA cae.
//
// Watchdog:
//   • Si ARRIBA timeout 500 ms → motors_stop() + LED parpadea (no hay mundo).
//   • Si ABAJO timeout 500 ms → strategy ignora línea (modo ciego de borde).
//
// Build:
//   pio run -e central_robot1   (arquero)
//   pio run -e central_robot2   (delantero)

#include <Arduino.h>

#include "config_central.h"
#include "world_model.h"
#include "strategy.h"
#include "motors_zircon.h"
#include "imu_zircon.h"
#include "comm_top.h"
#include "comm_down.h"

using namespace iitasoccer;

namespace {

elapsedMillis g_since_strategy_tick;
elapsedMillis g_since_debug;

uint32_t g_loop_count = 0;

void apply_role_from_dipswitch() {
#if defined(ROBOT1)
    strategy_set_role(RobotRole::GOALKEEPER);
    Serial.println("[CENTRAL] Role: GOALKEEPER (ROBOT1)");
#elif defined(ROBOT2)
    strategy_set_role(RobotRole::ATTACKER);
    Serial.println("[CENTRAL] Role: ATTACKER (ROBOT2)");
#else
    #error "Define ROBOT1 (arquero) o ROBOT2 (delantero) en build_flags"
#endif
}

}  // namespace

void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

#ifdef CENTRAL_ENABLE_MANUAL_START
    pinMode(PIN_MANUAL_START_BUTTON, INPUT_PULLUP);  // F3 fail-safe (solo banco)
#endif

    Serial.begin(115200);
    delay(100);
    Serial.println("\n=========================================");
    Serial.println("  IITA Soccer Open — CENTRAL firmware");
    Serial.println("  Zircon Rev v15 + Teensy 4.1, master");
    Serial.println("=========================================");

    apply_role_from_dipswitch();

    motors_init();
    Serial.println("[CENTRAL] motors init OK");

    // BNO055 local: la CENTRAL ya NO lleva BNO (2026-05-31). Los 2 BNO están en el
    // TOP; el heading llega por WORLD_SNAPSHOT de ARRIBA. El módulo imu_zircon queda
    // como compat — solo se inicializa con -DCENTRAL_HAS_LOCAL_BNO. Sin el flag no se
    // toca el bus I2C ni se pierden ~3 s buscando un sensor ausente.
#ifdef CENTRAL_HAS_LOCAL_BNO
    const bool imu_ok = imu_init();
    Serial.print("[CENTRAL] BNO055 local: ");
    Serial.println(imu_ok ? "OK (respaldo del heading de ARRIBA)"
                          : "FAIL (sigue con el heading de ARRIBA por snapshot)");
#else
    Serial.println("[CENTRAL] BNO055 local: N/A (no instalado; heading viene de ARRIBA)");
#endif

    world_model_init();
    strategy_init();

    comm_top_init();    // recibe WORLD_SNAPSHOT (Serial7, pin 28)
    comm_down_init();   // recibe LINE_URGENT (Serial1, pin 0)
    Serial.println("[CENTRAL] UARTs ARRIBA y ABAJO OK");

    digitalWrite(PIN_LED_STATUS, HIGH);
    Serial.println("[CENTRAL] master listo. Esperando snapshots.");
}

void loop() {
    g_loop_count++;

    // === RX: drenar ambos UARTs (no bloquea) ===
    comm_top_tick();    // aplica WorldSnapshot al world_model
    comm_down_tick();   // aplica LineStatusV2 al world_model

#ifdef CENTRAL_ENABLE_MANUAL_START
    // === F3 — Arranque manual fail-safe (SOLO banco, gateado) ===
    // Si la placa COMM no manda START, forzar match_running con el pulsador
    // (pin 9) o un ENTER por USB. Latch: una vez disparado, queda corriendo.
    // Pre-start los motores estan detenidos -> sin ruido de motor en el pin.
    {
        static bool g_manual_started = false;
        if (!g_manual_started) {
            const bool btn = (digitalRead(PIN_MANUAL_START_BUTTON) == LOW);
            bool ser = false;
            while (Serial.available()) { if (Serial.read() == '\n') ser = true; }
            if (btn || ser) {
                g_manual_started = true;
                world_model_set_force_match_running(true);
                Serial.println("[CENTRAL] *** ARRANQUE MANUAL forzado (CENTRAL_ENABLE_MANUAL_START) ***");
            }
        }
    }
#endif

    // === EMERGENCY_LINE — bypass de FSM ===
    // Si ABAJO reporta línea inminente Y los datos son frescos, frenar AHORA.
    // Latencia objetivo: <15 ms desde detección en DOWN hasta freno activo.
    // Se chequea cada iteración del loop (no espera al tick de 100 Hz de strategy).
    if (world_model_imminent_exit() && world_model_line_is_fresh()) {
        motors_brake();  // freno activo (corto en H-bridge), no solo PWM=0.
        // Encender LED fijo como alerta visual.
        digitalWrite(PIN_LED_STATUS, HIGH);
        // No salimos del loop — seguimos drenando UARTs para detectar el reset
        // (cuando imminent_exit baja, recuperamos control en el próximo tick).
        return;
    }

    // === Strategy + motores ===
    if (g_since_strategy_tick >= 10) {  // 100 Hz
        g_since_strategy_tick = 0;

        if (!world_model_snapshot_is_fresh()) {
            // ARRIBA caído > 500 ms → SAFE_NO_TOP. Parar motores, parpadear LED.
            motors_stop();
            digitalWrite(PIN_LED_STATUS, (millis() / 200) % 2);
        } else {
            MotorCommand cmd = strategy_tick();
            motors_apply_command(cmd);
            digitalWrite(PIN_LED_STATUS, HIGH);  // OK
        }
    }

    // === Debug print cada 500 ms ===
    if (g_since_debug >= 500) {
        g_since_debug = 0;
        Serial.print("[CENTRAL] loop=");
        Serial.print(g_loop_count);
        Serial.print(" role=");
        Serial.print(strategy_get_role() == RobotRole::ATTACKER ? "ATK" : "GK");
        Serial.print(" state=");
        Serial.print(strategy_get_state_name());
        Serial.print(" snap_fresh=");
        Serial.print(world_model_snapshot_is_fresh() ? "Y" : "N");
        Serial.print(" line_fresh=");
        Serial.print(world_model_line_is_fresh() ? "Y" : "N");
        // Telemetria del link DOWN->CENTRAL: salud del enlace + estado del dato.
        Serial.print(" down[rx=");
        Serial.print(comm_down_get_frames_received());
        Serial.print(" crc=");
        Serial.print(comm_down_get_crc_errors());
        Serial.print(" lost=");
        Serial.print(comm_down_get_frames_lost());
        Serial.print(" valid=");
        Serial.print(world_model_line_data_valid() ? "Y" : "N");
        Serial.print(" ev=0x");
        Serial.print(world_model_line_event_flags(), HEX);
        Serial.print("]");
        Serial.print(" match=");
        Serial.print(world_model_match_running() ? "RUN" : "STOP");
        Serial.print(" hdg=");
        Serial.println(world_model_get_my_heading_deg(), 1);
    }
}
