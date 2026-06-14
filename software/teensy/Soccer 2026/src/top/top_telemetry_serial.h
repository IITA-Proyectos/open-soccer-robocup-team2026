// top_telemetry_serial.h — Glue Arduino de la telemetría/monitoreo USB de la TOP.
// Declaraciones SIN gatear (inocuas: nadie las referencia con los flags OFF).
// Cuerpo en .cpp dentro de #if defined(TOP_DEBUG_TELEMETRY) || defined(TOP_USB_MONITOR).
// Ver telemetry_top.h y docs/firmware/TELEMETRIA-TOP.md.
#pragma once
namespace iitasoccer {
void top_telemetry_init();
void top_telemetry_tick();
}  // namespace iitasoccer
