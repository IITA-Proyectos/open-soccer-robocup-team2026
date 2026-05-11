#include "motors.h"
#include "config_top.h"
#include "proto.h"

#include <Arduino.h>
#include <string.h>

namespace iitasoccer {

namespace {

FrameDecoder g_rx_decoder;
uint32_t g_commands_sent = 0;
uint32_t g_status_received = 0;
uint8_t  g_send_seq = 0;

ZirconStatus g_zircon_status{};
uint32_t     g_zircon_last_rx_ms = 0;

void handle_frame(const Frame& f) {
    if (f.type == MsgType::ZIRCON_STATUS && f.payload_len == sizeof(ZirconStatus)) {
        memcpy(&g_zircon_status, f.payload, sizeof(ZirconStatus));
        g_zircon_last_rx_ms = millis();
        g_status_received++;
    }
}

}  // namespace

void motors_init() {
    Serial2.begin(UART_TO_ZIRCON_BAUD);
}

void motors_send_command(int16_t vx_mm_s, int16_t vy_mm_s,
                          int16_t omega_centideg_s, bool kicker_fire) {
    MotorCommand cmd{};
    cmd.vx_mm_s = vx_mm_s;
    cmd.vy_mm_s = vy_mm_s;
    cmd.omega_centideg_s = omega_centideg_s;
    cmd.kicker_fire = kicker_fire ? 1 : 0;
    cmd.dribbler_pwm = 0;  // futuro

    Frame f{};
    f.type = MsgType::MOTOR_COMMAND;
    f.seq = g_send_seq++;
    f.payload_len = sizeof(MotorCommand);
    memcpy(f.payload, &cmd, sizeof(MotorCommand));

    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(f, buf, sizeof(buf));
    if (n > 0) {
        Serial2.write(buf, n);
        g_commands_sent++;
    }
}

void motors_send_stop() {
    motors_send_command(0, 0, 0, false);
}

int motors_tick_rx() {
    int processed = 0;
    while (Serial2.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(Serial2.read());
        if (g_rx_decoder.feed(b)) {
            handle_frame(g_rx_decoder.get_frame());
            processed++;
        }
    }
    return processed;
}

bool motors_zircon_is_fresh() {
    return (millis() - g_zircon_last_rx_ms) < ZIRCON_HEARTBEAT_TIMEOUT_MS
        && g_status_received > 0;
}

const ZirconStatus& motors_zircon_get_status() { return g_zircon_status; }

void motors_request_status() {
    Frame f{};
    f.type = MsgType::MOTOR_STATUS_REQ;
    f.seq = g_send_seq++;
    f.payload_len = 0;
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(f, buf, sizeof(buf));
    if (n > 0) Serial2.write(buf, n);
}

uint32_t motors_get_commands_sent()   { return g_commands_sent; }
uint32_t motors_get_status_received() { return g_status_received; }

}  // namespace iitasoccer
