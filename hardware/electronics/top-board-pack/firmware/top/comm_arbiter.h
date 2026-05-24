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

void comm_arbiter_init();

// Drena UART de la placa COMM y procesa frames. Llamar cada loop.
int comm_arbiter_tick();

// Último comando de árbitros recibido + timestamp.
RefereeCommand comm_arbiter_get_last_command();
uint32_t       comm_arbiter_get_last_command_ms();

// "El árbitro dio START" — true si el último comando es START y el robot
// está habilitado a moverse.
bool comm_arbiter_is_match_running();

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
