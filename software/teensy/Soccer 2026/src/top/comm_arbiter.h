// comm_arbiter.h — Comunicación del TOP con la placa COMM (ESP32-C6).
//
// La placa COMM cumple 2 funciones:
//   1. Bridge al sistema oficial de árbitros RCJ (start/stop por WiFi, display
//      OLED con QR, acelerómetro LIS3DHTR para "shake to start").
//   2. Bridge ESP-NOW al robot partner para coordinación (envío/recepción de
//      pose, pelota detectada, intención).
//
// El protocolo entre TOP y COMM usa el mismo frame proto.h. Mensajes:
//   COMM_REFEREE_CMD  (COMM → TOP): comando del árbitro (start/stop/halftime).
//     ⚠️ OBSOLETO (TASK-039, banco 2026-06-02): el árbitro YA NO viaja por UART.
//     Llega al TOP como NIVEL GPIO en los pines 5/6 (ver read_referee_gpio en
//     comm_arbiter.cpp). El UART COMM↔TOP (Serial2) queda solo para status/partner.
//   COMM_STATUS_REQ   (COMM → TOP): pide status del robot.
//   TOP_STATUS_REPLY  (TOP → COMM): responde con batería, role, errores.
//   COMM_PARTNER_DATA (COMM → TOP): mensaje recibido del partner via ESP-NOW.
//   TOP_PARTNER_DATA  (TOP → COMM): mensaje a enviar al partner.

#pragma once
#include <stdint.h>
#include "types.h"

namespace iitasoccer {

enum class RefereeCommand : uint8_t {
    STOP     = 0,
    START    = 1,
    HALFTIME = 2,
    RESET    = 3,
    UNKNOWN  = 0xFF,
};

// === Debounce del árbitro (P1-ARB-DEBOUNCE) — LÓGICA PURA host-testeable ======
//
// El nivel GPIO crudo de los pines 5/6 (OR) puede tener glitches de EMI de los
// motores: un transitorio espurio a HIGH en STOP = START falso = el robot se
// mueve en STOP, que es PENALIZABLE. Filtramos por ESTABILIDAD: el valor crudo
// tiene que mantenerse N ms estable antes de cambiar el estado OFICIAL.
//
// Esta es la máquina de debounce extraída como struct + función puras (sin
// Arduino): entra (lectura cruda OR + now_ms), sale el estado filtrado. El glue
// Arduino solo provee digitalRead()+millis(). Tests en test/test_link_health.
//
// Fail-safe preservado: al boot el estado oficial arranca en STOP (false); si el
// cable del COMM se desconecta, ambos pines leen 0 por INPUT_PULLDOWN → crudo
// estable en false → tras la ventana, estado oficial = STOP.

// Ventana de estabilidad por defecto (ms). El crudo debe mantenerse este tiempo
// para que el estado oficial cambie. 40 ms >> duración típica de un glitch de EMI
// (µs..pocos ms) y << latencia perceptible de arranque/parada.
constexpr uint32_t REFEREE_DEBOUNCE_MS = 40;

struct RefereeDebounce {
    bool     official     = false;  // estado filtrado/oficial (arranca STOP = fail-safe)
    bool     candidate    = false;  // último valor crudo observado (pendiente de confirmar)
    uint32_t candidate_ms = 0;      // millis() en que apareció el candidato actual
    bool     seeded       = false;  // ¿ya se sembró la base temporal?
};

// Avanza la máquina de debounce con una lectura cruda y el reloj actual.
// Devuelve el estado OFICIAL (filtrado) resultante.
//
// Reglas:
//   • Si el crudo == oficial → no hay nada pendiente, se resincroniza el candidato.
//   • Si el crudo difiere del oficial y se mantuvo >= debounce_ms estable →
//     el oficial cambia.
//   • Un cambio de crudo reinicia el cronómetro (glitch de 1 tick nunca llega a N ms).
//   • Primer llamado (seeded=false): siembra la base temporal sin forzar cambios;
//     el oficial queda como estaba (STOP por defecto) → fail-safe al boot.
inline bool referee_debounce_update(RefereeDebounce& d, bool raw, uint32_t now_ms,
                                     uint32_t debounce_ms = REFEREE_DEBOUNCE_MS) {
    if (!d.seeded) {
        d.seeded       = true;
        d.candidate    = raw;
        d.candidate_ms = now_ms;
        // No cambiamos d.official en el seed: el boot queda en STOP (fail-safe).
        return d.official;
    }
    if (raw == d.official) {
        // El crudo coincide con lo oficial: no hay transición pendiente.
        d.candidate    = raw;
        d.candidate_ms = now_ms;
        return d.official;
    }
    // El crudo difiere de lo oficial → hay un cambio candidato.
    if (raw != d.candidate) {
        // Nuevo candidato (o el crudo volvió a moverse) → reinicia el cronómetro.
        d.candidate    = raw;
        d.candidate_ms = now_ms;
        return d.official;
    }
    // Mismo candidato sostenido: ¿llevó estable >= la ventana?
    if ((now_ms - d.candidate_ms) >= debounce_ms) {
        d.official = d.candidate;
    }
    return d.official;
}

// === Salud de enlace por SEQ (P1-SEQ-LINK-HEALTH) — LÓGICA PURA host-testeable =
//
// El protocolo (proto.h) ya manda un SEQ 0..255 con wrap por frame, pero en RX
// los comm_* NUNCA lo leían → un enlace que pierde frames (cable flojo, vibración)
// pasaba mudo mientras llegaran dentro del heartbeat. Acá consumimos f.seq y
// contamos el GAP acumulado por enlace.
//
// ⚠️ Limitación documentada (memoria del repo, audit #23): un enlace puede llevar
// VARIOS tipos de frame con un SEQ COMPARTIDO (p.ej. DOWN manda 0x10/0x11/0x12 con
// un único contador en down_tx.cpp). El gap se cuenta POR ENLACE (un solo SEQ
// monótono), tolerando el intercalado de tipos: mientras el emisor incremente un
// único SEQ por enlace, el gap es correcto aunque se mezclen tipos. Además, un
// frame que el FrameDecoder DESCARTA (CRC malo, LEN>32, END malo) nunca llega a
// este tracker, así que su SEQ aparece como hueco → frames_lost SE SOLAPA con
// crc_errors(): huecos_de_cable ≈ frames_lost − crc_errors. El "lost enorme con
// crc=0" de la memoria era artefacto de resync, NO pérdida de cable.
//
// Esta es la lógica extraída como struct + función puras (sin Arduino).
struct LinkSeqTracker {
    bool     have_last = false;  // ¿ya vimos al menos un frame?
    uint8_t  last_seq  = 0;      // SEQ del último frame válido
    uint32_t lost      = 0;      // gap acumulado (frames presumiblemente perdidos)
};

// Consume el SEQ de un frame recibido y acumula el gap. Devuelve cuántos frames
// se presumen perdidos ENTRE el anterior y este (0 si consecutivo).
//   • Sin pérdida (seq = last+1) → +0.
//   • Salto de SEQ (seq = last+1+g) → +g.
//   • Wrap 255→0 manejado por la aritmética uint8_t (cast).
//   • Primer frame (have_last=false): siembra, no cuenta.
//   • Frame duplicado/reordenado (seq <= last) → gap (uint8_t)(seq-last-1) grande;
//     es la misma semántica que el tracker del CENTRAL (mide magnitud del hueco).
inline uint32_t link_seq_track(LinkSeqTracker& t, uint8_t seq) {
    uint32_t gap = 0;
    if (t.have_last) {
        gap = static_cast<uint8_t>(seq - t.last_seq - 1);
        t.lost += gap;
    }
    t.last_seq  = seq;
    t.have_last = true;
    return gap;
}

void comm_arbiter_init();

// Drena UART de la placa COMM y procesa frames. Llamar cada loop.
int comm_arbiter_tick();

// Último comando de árbitros recibido + timestamp.
RefereeCommand comm_arbiter_get_last_command();
uint32_t       comm_arbiter_get_last_command_ms();

// "El árbitro dio START" — true si el último comando es START y el robot
// está habilitado a moverse.
bool comm_arbiter_is_match_running();

// Frames validos decodificados desde la placa COMM (salud del enlace Serial2, pines 7/8).
// Sube con CUALQUIER frame valido del COMM (referee cmd, status req, partner),
// aunque todavia no haya START — sirve para confirmar que el enlace esta vivo.
uint32_t comm_arbiter_get_frames_received();

// Frames presumiblemente perdidos en el enlace COMM (gap de SEQ acumulado).
// Ver LinkSeqTracker arriba: SE SOLAPA con crc_errors del decoder; >0 sostenido
// = cable flojo / vibración. 0 en enlace sano. (P1-SEQ-LINK-HEALTH)
uint32_t comm_arbiter_get_frames_lost();

// Envía status al COMM (responde a COMM_STATUS_REQ o periódico).
void comm_arbiter_send_status(uint8_t role, uint8_t error_flags,
                               uint16_t battery_mv);

// Partner data (ESP-NOW transparente — el COMM hace el bridge).
struct PartnerSnapshot {
    int16_t x_mm;
    int16_t y_mm;
    int16_t heading_centideg;
    int16_t ball_x_mm;
    int16_t ball_y_mm;
    uint8_t ball_visible;
    uint8_t partner_state;
} __attribute__((packed));

bool                    comm_arbiter_partner_is_fresh();
const PartnerSnapshot&  comm_arbiter_get_partner();

void comm_arbiter_send_partner(const PartnerSnapshot& my_snapshot);

}  // namespace iitasoccer
