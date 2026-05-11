#include "comm_arbiter.h"
#include "config_top.h"
#include "proto.h"

#include <Arduino.h>
#include <string.h>

namespace iitasoccer {

namespace {

FrameDecoder g_decoder;
uint8_t      g_send_seq = 0;

RefereeCommand g_last_cmd = RefereeCommand::UNKNOWN;
uint32_t       g_last_cmd_ms = 0;

PartnerSnapshot g_partner{};
uint32_t        g_partner_last_rx_ms = 0;

// El "match running" se mantiene true desde un START hasta un STOP/HALFTIME/RESET.
bool g_match_running = false;

void apply_referee_command(RefereeCommand cmd) {
    g_last_cmd = cmd;
    g_last_cmd_ms = millis();
    switch (cmd) {
        case RefereeCommand::START:    g_match_running = true; break;
        case RefereeCommand::STOP:
        case RefereeCommand::HALFTIME:
        case RefereeCommand::RESET:    g_match_running = false; break;
        case RefereeCommand::UNKNOWN:  break;
    }
}

void handle_frame(const Frame& f) {
    switch (f.type) {
        case MsgType::COMM_REFEREE_CMD:
            if (f.payload_len >= 1) {
                const uint8_t code = f.payload[0];
                apply_referee_command(static_cast<RefereeCommand>(code));
            }
            break;
        case MsgType::COMM_STATUS_REQ:
            // El caller responderá con comm_arbiter_send_status();
            // este módulo no tiene la info de estado interna.
            // (TODO: pasar puntero a callback en init para ser autónomo.)
            break;
        case MsgType::COMM_PARTNER_DATA:
            if (f.payload_len == sizeof(PartnerSnapshot)) {
                memcpy(&g_partner, f.payload, sizeof(PartnerSnapshot));
                g_partner_last_rx_ms = millis();
            }
            break;
        default:
            break;
    }
}

}  // namespace

void comm_arbiter_init() {
    Serial4.begin(UART_TO_COMM_BAUD);
}

int comm_arbiter_tick() {
    int processed = 0;
    while (Serial4.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(Serial4.read());
        if (g_decoder.feed(b)) {
            handle_frame(g_decoder.get_frame());
            processed++;
        }
    }
    return processed;
}

RefereeCommand comm_arbiter_get_last_command()      { return g_last_cmd; }
uint32_t       comm_arbiter_get_last_command_ms()   { return g_last_cmd_ms; }
bool           comm_arbiter_is_match_running()      { return g_match_running; }

void comm_arbiter_send_status(uint8_t role, uint8_t error_flags, uint16_t battery_mv) {
    struct StatusReply {
        uint8_t  role;
        uint8_t  error_flags;
        uint16_t battery_mv;
        uint8_t  match_running;
    } __attribute__((packed));

    StatusReply reply{};
    reply.role = role;
    reply.error_flags = error_flags;
    reply.battery_mv = battery_mv;
    reply.match_running = g_match_running ? 1 : 0;

    Frame f{};
    f.type = MsgType::TOP_STATUS_REPLY;
    f.seq = g_send_seq++;
    f.payload_len = sizeof(StatusReply);
    memcpy(f.payload, &reply, sizeof(StatusReply));

    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(f, buf, sizeof(buf));
    if (n > 0) Serial4.write(buf, n);
}

bool comm_arbiter_partner_is_fresh() {
    return (millis() - g_partner_last_rx_ms) < 500;  // 500 ms heartbeat partner
}

const PartnerSnapshot& comm_arbiter_get_partner() { return g_partner; }

void comm_arbiter_send_partner(const PartnerSnapshot& my_snapshot) {
    Frame f{};
    f.type = MsgType::TOP_PARTNER_DATA;
    f.seq = g_send_seq++;
    f.payload_len = sizeof(PartnerSnapshot);
    memcpy(f.payload, &my_snapshot, sizeof(PartnerSnapshot));

    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(f, buf, sizeof(buf));
    if (n > 0) Serial4.write(buf, n);
}

}  // namespace iitasoccer
