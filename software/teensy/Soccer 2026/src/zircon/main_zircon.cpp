// main_zircon.cpp — Firmware "Motor Server" del Zircon Rev v15 (Teensy 4.1)
//
// Responsabilidad ÚNICA: recibir MotorCommand del TOP por Serial1, aplicar
// cinemática inversa omni-3, y ejecutar PWM en los 3 motores. Sin estrategia,
// sin visión, sin sensores de línea — esos viven en TOP y DOWN.
//
// Watchdog: si no llega un MotorCommand en COMMAND_TIMEOUT_MS, los motores se
// detienen como fail-safe. Si el TOP se cuelga o se desconecta el cable, el
// robot no queda corriendo a velocidad fija.
//
// LED:
//   - apagado:  esperando primer comando del TOP.
//   - parpadea: timeout activo, motores detenidos por watchdog.
//   - encendido: recibiendo comandos OK.
//
// Build:
//   pio run -e zircon_robot1   → arquero
//   pio run -e zircon_robot2   → delantero
//
// Upload:
//   pio run -e zircon_robot1 -t upload
//
// Selección del robot por #define en platformio.ini build_flags.

#include <Arduino.h>
#include <string.h>

#include "config_zircon.h"
#include "motors_zircon.h"
#include "imu_zircon.h"
#include "proto.h"
#include "types.h"

using namespace iitasoccer;

namespace {

FrameDecoder g_decoder;
uint32_t g_last_command_ms = 0;
uint32_t g_commands_received = 0;
uint8_t  g_status_seq = 0;

void send_zircon_status() {
    ZirconStatus st{};
    st.motors_ok = 0b111;  // TODO: verificar drivers reales con sense pins (futuro)
    st.kicker_ready = 0;
    st.battery_mv = 7400;  // TODO: leer batería real con divisor de voltaje (futuro)

    Frame f{};
    f.type = MsgType::ZIRCON_STATUS;
    f.seq = g_status_seq++;
    f.payload_len = sizeof(ZirconStatus);
    memcpy(f.payload, &st, sizeof(ZirconStatus));

    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(f, buf, sizeof(buf));
    Serial1.write(buf, n);
}

void handle_frame(const Frame& f) {
    switch (f.type) {
        case MsgType::MOTOR_COMMAND: {
            if (f.payload_len != sizeof(MotorCommand)) break;
            MotorCommand cmd;
            memcpy(&cmd, f.payload, sizeof(MotorCommand));
            motors_apply_command(cmd);
            g_last_command_ms = millis();
            g_commands_received++;
            digitalWrite(PIN_LED_STATUS, HIGH);
            break;
        }
        case MsgType::MOTOR_STATUS_REQ:
            send_zircon_status();
            break;
        default:
            // ignorar mensajes que no nos corresponden
            break;
    }
}

void check_watchdog() {
    if (g_commands_received == 0) return;  // nunca recibimos nada — no timeout aún
    if (millis() - g_last_command_ms > COMMAND_TIMEOUT_MS) {
        motors_stop();
        // LED parpadea para indicar watchdog activo
        digitalWrite(PIN_LED_STATUS, (millis() / 100) % 2);
    }
}

}  // namespace

void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    Serial.begin(115200);   // USB debug
    Serial1.begin(UART_TOP_BAUD);  // UART hacia TOP

    motors_init();

    // BNO055 init — degradación elegante si falla (no bloquea).
    const bool imu_ok = imu_init();
    Serial.print("[Zircon] BNO055: ");
    Serial.println(imu_ok ? "OK" : "FAIL (continuando sin gyro)");

    Serial.print("[Zircon] Robot: ");
#if defined(ROBOT1)
    Serial.println("ROBOT1 (arquero)");
#elif defined(ROBOT2)
    Serial.println("ROBOT2 (delantero)");
#endif

    Serial.println("[Zircon] Motor server listo. Esperando comandos del TOP...");
}

void loop() {
    // 1. Drenar UART desde TOP — procesar todos los bytes pendientes.
    while (Serial1.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(Serial1.read());
        if (g_decoder.feed(b)) {
            handle_frame(g_decoder.get_frame());
        }
    }

    // 2. Watchdog: detener motores si no hay comando reciente.
    check_watchdog();

    // 3. (Futuro) Heartbeat de estado al TOP cada cierto tiempo.
    //    Hoy solo se envía cuando el TOP pide explícitamente (MOTOR_STATUS_REQ).
}
