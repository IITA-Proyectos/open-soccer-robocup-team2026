#include "comm_arbiter.h"
#include "config_top.h"
#include "proto.h"

#include <Arduino.h>
#include <string.h>

namespace iitasoccer {

namespace {

FrameDecoder g_decoder;
uint8_t      g_send_seq = 0;
uint32_t     g_frames_received = 0;   // frames validos decodificados desde COMM (salud del enlace)

RefereeCommand g_last_cmd = RefereeCommand::UNKNOWN;
uint32_t       g_last_cmd_ms = 0;

PartnerSnapshot g_partner{};
uint32_t        g_partner_last_rx_ms = 0;

// El "match running" refleja el NIVEL GPIO del arbitro (ver read_referee_gpio).
bool g_match_running = false;

// === Arbitro = NIVEL GPIO (no UART). CONFIRMADO EN BANCO 2026-06-02 (TASK-039). ===
// El modulo COMM oficial RCJ entrega el estado de juego como NIVEL LOGICO en 2 pines del
// TOP: pin 5 = OUT1 y pin 6 = OUT2. 0 = PARADO, 1 = EN CURSO (3.3V).
// match_running = (pin5 O pin6) en alto  ->  *** OR ***  (probado en banco 2026-06-02,
// Gustavo): en PLAY sube SOLO UNO de los dos pines (el otro queda en 0), asi que con AND
// nunca daba GO. Con OR, si CUALQUIERA dice PLAY, el robot juega; en STOP los dos quedan
// en 0 -> STOP. INPUT_PULLDOWN: si el cable del COMM se desconecta, ambos leen 0 ->
// match_running=false (FAIL-SAFE). El debug del TOP imprime p5/p6 por separado. El UART
// (Serial2) queda solo para partner ESP-NOW/status.
constexpr uint8_t PIN_REFEREE_A = 5;   // OUT1
constexpr uint8_t PIN_REFEREE_B = 6;   // OUT2

void read_referee_gpio() {
    const bool go = (digitalRead(PIN_REFEREE_A) == HIGH) ||
                    (digitalRead(PIN_REFEREE_B) == HIGH);
    const RefereeCommand cmd = go ? RefereeCommand::START : RefereeCommand::STOP;
    if (cmd != g_last_cmd) {        // sella el instante del ultimo CAMBIO de estado
        g_last_cmd    = cmd;
        g_last_cmd_ms = millis();
    }
    g_match_running = go;
}

void handle_frame(const Frame& f) {
    switch (f.type) {
        // NOTA (TASK-039): el arbitro (START/STOP) NO viene por UART — es NIVEL GPIO en
        // los pines 5/6 (ver read_referee_gpio). El viejo COMM_REFEREE_CMD por UART quedo
        // obsoleto; ya no se procesa aca.
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
    Serial2.begin(UART_TO_COMM_BAUD);           // UART = solo partner ESP-NOW / status
    pinMode(PIN_REFEREE_A, INPUT_PULLDOWN);     // arbitro = nivel GPIO (TASK-039)
    pinMode(PIN_REFEREE_B, INPUT_PULLDOWN);
    read_referee_gpio();                         // estado inicial
}

int comm_arbiter_tick() {
    int processed = 0;
    while (Serial2.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(Serial2.read());
        if (g_decoder.feed(b)) {
            handle_frame(g_decoder.get_frame());
            g_frames_received++;
            processed++;
        }
    }
    read_referee_gpio();   // arbitro por GPIO (autoritativo, se lee cada tick)
    return processed;
}

RefereeCommand comm_arbiter_get_last_command()      { return g_last_cmd; }
uint32_t       comm_arbiter_get_last_command_ms()   { return g_last_cmd_ms; }
bool           comm_arbiter_is_match_running()      { return g_match_running; }
uint32_t       comm_arbiter_get_frames_received()   { return g_frames_received; }

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
    if (n > 0) Serial2.write(buf, n);
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
    if (n > 0) Serial2.write(buf, n);
}

}  // namespace iitasoccer
