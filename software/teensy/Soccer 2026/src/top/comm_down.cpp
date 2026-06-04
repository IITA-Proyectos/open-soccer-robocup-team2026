#include "comm_down.h"
#include "comm_arbiter.h"   // LinkSeqTracker / link_seq_track (P1-SEQ-LINK-HEALTH)
#include "config_top.h"
#include "proto.h"
#include "line_view.h"

#include <Arduino.h>
#include <string.h>

namespace iitasoccer {

namespace {

FrameDecoder g_decoder;
uint32_t g_frames_received = 0;
uint8_t  g_send_seq = 0;
LinkSeqTracker g_seq{};   // gap de SEQ del enlace DOWN→TOP (P1-SEQ-LINK-HEALTH)

// Estado más reciente.
LineStatusV2 g_line{};
Pose2D      g_pose{};
Velocity2D  g_vel{};

uint32_t g_line_last_rx_ms = 0;
uint32_t g_pose_last_rx_ms = 0;
uint32_t g_vel_last_rx_ms  = 0;

bool fresh(uint32_t last_ms) {
    // last_ms == 0 → todavía no llegó ningún frame de ese tipo. Sin este guard,
    // millis()-0 da "fresco" los primeros DOWN_HEARTBEAT_TIMEOUT_MS post-boot y
    // expone el struct cero-inicializado como dato real. (Mismo patrón que
    // central/world_model.cpp.)
    return last_ms != 0 && (millis() - last_ms) < DOWN_HEARTBEAT_TIMEOUT_MS;
}

void handle_frame(const Frame& f) {
    g_frames_received++;
    switch (f.type) {
        // En la arquitectura nueva, LINE_URGENT (antes DOWN_LINE_STATUS) NO
        // viene a ARRIBA — viaja por bus de emergencia directo DOWN → CENTRAL.
        // Lo procesamos acá solo si llegó por error (no romper si por ahora
        // DOWN sigue mandándolo en la transición).
        case MsgType::LINE_URGENT: {
            LineStatusV2 ls{};
            if (lsv2_from_frame(f, ls)) {     // valida tipo + tamaño (16) + schema
                g_line = ls;
                g_line_last_rx_ms = millis();
            }
            break;
        }
        case MsgType::DOWN_OTOS_POSE:
            if (f.payload_len == sizeof(Pose2D)) {
                memcpy(&g_pose, f.payload, sizeof(Pose2D));
                g_pose_last_rx_ms = millis();
            }
            break;
        case MsgType::DOWN_OTOS_VEL:
            if (f.payload_len == sizeof(Velocity2D)) {
                memcpy(&g_vel, f.payload, sizeof(Velocity2D));
                g_vel_last_rx_ms = millis();
            }
            break;
        default:
            // Frames no esperados desde DOWN — ignorar.
            break;
    }
}

}  // namespace

// Buffer RX extra para el enlace DOWN→TOP (P0-LOOP-HCSR04 parcial, Arduino-only).
//
// El ring RX por defecto de HardwareSerial (Teensy 4.x) es de 64 bytes. A 230400
// baud entran ~23 B/ms, así que 64 B se llenan en ~2.8 ms: si el loop se bloquea
// (p.ej. el pulseIn del HC-SR04 ~12 ms), el ring se desborda en SILENCIO y se
// pierde odometría OTOS sin disparar LOST. addMemoryForRead() amplía ese ring con
// un buffer estático nuestro (512 B ≈ ~22 ms de aire @230400) → absorbe el bloqueo
// sin perder bytes. Es ADITIVO (no toca el path de decodificación) y no cambia el
// contrato de wire. La causa raíz (pulseIn bloqueante) la trata TASK-014 por otro
// lado; esto es el colchón de bajo riesgo recomendado en paralelo.
//
// El tamaño debe ser múltiplo del tamaño interno de Teensy (8 B); 512 lo es.
// Guardado por macro para no asumir la API en builds host (este .cpp no compila
// host hoy, pero el guard documenta la dependencia de plataforma).
#ifndef DOWN_RX_EXTRA_BYTES
#define DOWN_RX_EXTRA_BYTES 512
#endif
static uint8_t g_down_rx_extra[DOWN_RX_EXTRA_BYTES];

void comm_down_init() {
    Serial1.begin(UART_FROM_DOWN_BAUD);
#if defined(__IMXRT1062__) || defined(CORE_TEENSY)
    // Teensy 4.x: agranda el ring RX con nuestro buffer estático (Arduino-only).
    Serial1.addMemoryForRead(g_down_rx_extra, sizeof(g_down_rx_extra));
#else
    (void)g_down_rx_extra;
#endif
}

int comm_down_tick() {
    int processed = 0;
    while (Serial1.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(Serial1.read());
        if (g_decoder.feed(b)) {
            const Frame& f = g_decoder.get_frame();
            link_seq_track(g_seq, f.seq);   // salud del enlace DOWN→TOP por SEQ
            handle_frame(f);
            processed++;
        }
    }
    return processed;
}

void comm_down_send_reset_otos() {
    Frame f{};
    f.type = MsgType::CENTRAL_RESET_OTOS;
    f.seq = g_send_seq++;
    f.payload_len = 1;
    f.payload[0] = 1;
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(f, buf, sizeof(buf));
    if (n > 0) Serial1.write(buf, n);
}

void comm_down_send_calib_line(bool white) {
    Frame f{};
    f.type = MsgType::CENTRAL_CALIB_LINE;
    f.seq = g_send_seq++;
    f.payload_len = 1;
    f.payload[0] = white ? 1 : 0;
    uint8_t buf[PROTO_MAX_FRAME];
    size_t n = proto_encode(f, buf, sizeof(buf));
    if (n > 0) Serial1.write(buf, n);
}

bool                comm_down_is_line_fresh() { return fresh(g_line_last_rx_ms); }
const LineStatusV2& comm_down_get_line_status() { return g_line; }

bool              comm_down_is_pose_fresh() { return fresh(g_pose_last_rx_ms); }
const Pose2D&     comm_down_get_pose() { return g_pose; }

bool              comm_down_is_vel_fresh() { return fresh(g_vel_last_rx_ms); }
const Velocity2D& comm_down_get_velocity() { return g_vel; }

uint32_t comm_down_get_frames_received() { return g_frames_received; }
uint32_t comm_down_get_crc_errors()      { return g_decoder.crc_errors(); }
uint32_t comm_down_get_frames_lost()     { return g_seq.lost; }

}  // namespace iitasoccer
