#include "comm_top.h"
#include "config_down.h"
#include "otos.h"
#include "proto.h"
#include "types.h"
#include "down_tx.h"
#include "telemetry_sat.h"

#include <Arduino.h>
#include <string.h>

namespace iitasoccer {

namespace {

FrameDecoder g_decoder;
uint32_t g_frames_received = 0;

void handle_frame(const Frame& f) {
    switch (f.type) {
        case MsgType::CENTRAL_RESET_OTOS:
            otos_reset();
            break;
        // NOTA (audit 2026-06-03 #19): el handler de CENTRAL_CALIB_LINE por el
        // enlace TOP (Serial5) se ELIMINÓ. La calibración de línea es canal
        // CANÓNICO de CENTRAL únicamente (Serial1, ver comm_central.cpp y
        // proto.h "CENTRAL → DOWN"): allí además persiste en EEPROM (P0.2) y
        // re-deriva el DownModel. Calibrar por TOP recalibraba el line_ring pero
        // NO el DownModel ni la EEPROM → calib divergente que se perdía al
        // power-cycle ("anda por un camino, falla por el otro"). Además la calib
        // bloquea el loop ~320-500 ms, inaceptable en el path RX del enlace TOP.
        // Hoy ninguna placa emite 0x21 por Serial5, así que quitarlo no cambia
        // comportamiento observable; sólo elimina la trampa latente.
        default:
            // Frames no relevantes para DOWN — ignorar silenciosamente.
            break;
    }
}

}  // namespace

void comm_top_init() {
    Serial5.begin(UART_TOP_BAUD);
}

int comm_top_tick() {
    int processed = 0;
    while (Serial5.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(Serial5.read());
        if (g_decoder.feed(b)) {
            handle_frame(g_decoder.get_frame());
            g_frames_received++;
            processed++;
        }
    }
    return processed;
}

void comm_top_send_status() {
    // NOTA: en la arquitectura nueva, LineStatus YA NO se envía a ARRIBA.
    // ARRIBA solo necesita la odometría OTOS para fusión sensorial.
    // La línea (LINE_URGENT) viaja por el bus de emergencia DOWN → CENTRAL
    // (ver src/down/comm_central.{h,cpp}).

    // OTOS POSE — pose central del robot (fusión de los 2 OTOS, o el único disponible).
    // confidence refleja la salud ACTUAL de cada OTOS (otos_is_*_ready ahora se
    // deriva de la máquina de salud, no del boot — audit #6/#15): si un OTOS muere
    // en partido, confidence cae a 60 (single) o 0, en vez de mentir 100.
    Pose2D pose{};
    pose.x_mm = sat_i16(otos_get_x_mm());
    pose.y_mm = sat_i16(otos_get_y_mm());
    pose.heading_centideg = sat_i16(otos_get_heading_deg() * 100.0f);
    pose.confidence = (otos_is_left_ready() && otos_is_right_ready()) ? 100
                    : (otos_is_left_ready() || otos_is_right_ready()) ? 60
                    : 0;
    down_tx_broadcast_pose(pose);

    // OTOS VEL — velocidades + slip_estimate (DOWN-specific).
    // Todos los campos se SATURAN antes del cast (audit #14): omega es el único
    // que físicamente desborda int16 (umbral ~5.72 rad/s; un omni rota a 8-15
    // rad/s al alinear) y sin clamp wrappea de signo. vx/vy se saturan por
    // consistencia defensiva (costo cero).
    Velocity2D vel{};
    vel.vx_mm_s = sat_i16(otos_get_vx_mm_s());
    vel.vy_mm_s = sat_i16(otos_get_vy_mm_s());
    // omega: el getter da rad/s; Velocity2D usa centideg/s. Convertimos + saturamos.
    vel.omega_centideg_s = omega_rad_s_to_centideg_s_sat(otos_get_omega_rad_s());
    // DC-2 (2026-06-04): sat_u8 también cubre el negativo (el cast crudo a uint8
    // wrappea -1 → 255, un "slip" enorme falso) y NaN, no solo el techo de 255.
    vel.slip_estimate = sat_u8(otos_get_slip_estimate());
    down_tx_broadcast_vel(vel);
}

uint32_t comm_top_get_frames_received() { return g_frames_received; }
uint32_t comm_top_get_frames_sent()    { return down_tx_get_sent(1); }
uint32_t comm_top_get_frames_dropped() { return down_tx_get_dropped(1); }
uint32_t comm_top_get_crc_errors()      { return g_decoder.crc_errors(); }

}  // namespace iitasoccer
