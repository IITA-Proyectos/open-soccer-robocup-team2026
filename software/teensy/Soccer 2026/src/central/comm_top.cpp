#include "comm_top.h"
#include "world_model.h"
#include "proto.h"
#include "types.h"

#include <Arduino.h>
#include <string.h>

namespace iitasoccer {

namespace {

FrameDecoder g_decoder;
uint32_t g_frames_received = 0;
uint32_t g_bytes_received = 0;  // DIAG: bytes crudos leidos de Serial7 (link TOP->CENTRAL)
uint32_t g_snapshot_size_rejects = 0;  // CC-01: WorldSnapshot con tamano != schema (deploy v2/v3 desfasado)

#ifdef CENTRAL_TOP_LINK_SEQ
// Salud del enlace TOP->CENTRAL por SEQ (P1, desactivado por defecto): el TOP numera cada
// snapshot (top/comm_central.cpp: f.seq = g_send_seq++). Si la CENTRAL ve un salto en el SEQ,
// perdio esos frames (buffer RX lleno / ruido). Aritmetica uint8 = wrap-safe en el rollover.
uint32_t g_frames_lost = 0;
uint8_t  g_last_seq    = 0;
bool     g_seq_primed  = false;
#endif

constexpr long UART_BAUD = 230400;

void handle_frame(const Frame& f) {
    g_frames_received++;
#ifdef CENTRAL_TOP_LINK_SEQ
    if (g_seq_primed) {
        g_frames_lost += static_cast<uint8_t>(f.seq - g_last_seq - 1);  // huecos desde el ultimo
    }
    g_last_seq   = f.seq;
    g_seq_primed = true;
#endif
    if (f.type == MsgType::WORLD_SNAPSHOT) {
        if (f.payload_len == sizeof(WorldSnapshot)) {
            WorldSnapshot snap{};
            memcpy(&snap, f.payload, sizeof(WorldSnapshot));
            world_model_apply_snapshot(snap);
        } else {
            // CC-01 (eval 2026-06-04): llego un WorldSnapshot con tamano distinto
            // al schema compilado -> una placa quedo con firmware viejo tras un
            // deploy wire-breaking (v2/v3). Sin este contador, en el pit se ve
            // IGUAL que "sin link". Se expone en la telemetria DIAG (badsz=).
            g_snapshot_size_rejects++;
        }
    }
}

}  // namespace

// UART hacia TOP = Serial7 (RX7 = pin 28, TX7 = pin 29) del Teensy 4.1.
// Reasignado 2026-05-31 (decisión Gustavo, cableado en banco): antes Serial1 (0/1);
// se movió a Serial7 (28/29, pines de expansión libres del Zircon) cuando el link
// DOWN→CENTRAL tomó Serial1 (0/1). Ver MAPA-CONEXIONES-3-PLACAS.md.
//
// BUFFER RX (quick-win RT 2026-06-15, GATEADO por CENTRAL_TOP_RX_BIGBUF, DEFAULT OFF):
// el RX de Serial7 trae solo 64 B de buffer interno. El WorldSnapshot del TOP (frame
// ~37 B con header+CRC) llega a 100 Hz; si una vuelta del loop de la CENTRAL se alarga
// (p.ej. el freno de borde retiene el loop, o un tramo de strategy pesado), 64 B se
// llenan en <1 frame y los bytes nuevos se DESCARTAN en silencio → frame corrupto →
// el decoder resincroniza y se pierde ese snapshot SIN aviso (solo sube resync). El
// link DOWN→CENTRAL ya usa 512 B (comm_down); esto le da el mismo colchón al de TOP:
// 512 B ≈ 13 frames de holgura. addMemoryForRead() es API de Teensy (no host-testeable);
// es glue Arduino puro, por eso gateado y default OFF para no tocar el binario de
// competencia. Para activarlo: agregar -DCENTRAL_TOP_RX_BIGBUF al env tras validar en
// banco (chequear que comm_top_get_resync_events() baja bajo carga). Cero riesgo de
// regresión: más buffer solo puede ayudar; el costo es ~512 B de SRAM (Teensy 4.1: 512 KB).
#ifdef CENTRAL_TOP_RX_BIGBUF
static uint8_t s_top_rx_buf[512];   // estático: debe sobrevivir todo el runtime
#endif

void comm_top_init() {
    Serial7.begin(UART_BAUD);
#ifdef CENTRAL_TOP_RX_BIGBUF
    Serial7.addMemoryForRead(s_top_rx_buf, sizeof(s_top_rx_buf));
#endif
}

void comm_top_tick() {
    // CC-04: void (ningún caller usa el contador de frames procesados).
    while (Serial7.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(Serial7.read());
        g_bytes_received++;  // DIAG: contar todo byte que entra por Serial7
        if (g_decoder.feed(b)) {
            handle_frame(g_decoder.get_frame());
        }
    }
}

uint32_t comm_top_get_frames_received() { return g_frames_received; }
uint32_t comm_top_get_crc_errors()      { return g_decoder.crc_errors(); }
uint32_t comm_top_get_bytes_received()  { return g_bytes_received; }
uint32_t comm_top_get_resync_events()   { return g_decoder.resync_events(); }
uint32_t comm_top_get_snapshot_size_rejects() { return g_snapshot_size_rejects; }

#ifdef CENTRAL_TOP_LINK_SEQ
uint32_t comm_top_get_frames_lost() { return g_frames_lost; }
#endif

}  // namespace iitasoccer
