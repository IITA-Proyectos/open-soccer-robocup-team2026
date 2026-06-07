// top_telemetry_serial.h — Glue Arduino de la telemetría USB de la TOP (FASE 2).
// Declaraciones SIN gatear (inocuas: nadie las referencia con el flag OFF).
// Cuerpo en .cpp dentro de #ifdef TOP_DEBUG_TELEMETRY. Ver telemetry_top.h.
#pragma once
namespace iitasoccer {
void top_telemetry_init();
void top_telemetry_tick();
}  // namespace iitasoccer
