// down_telemetry_serial.h — Glue Arduino de la telemetría USB de DOWN.
//
// Modo DEBUG GATEADO (-DDOWN_DEBUG_TELEMETRY, env down_debug_telemetry). Lee el
// estado vivo de DOWN (line_ring + LineStatusV2 que va a CENTRAL + OTOS), lo
// serializa con el módulo PURO src/shared/telemetry_down.{h,cpp} a JSON Lines y
// lo escribe por el USB CDC (Serial); además parsea comandos de texto del host.
//
// Estas declaraciones son INOCUAS cuando el flag está OFF: nadie las referencia
// (las llamadas en main_down.cpp están #ifdef'd) y el .cpp queda como traducción
// vacía → cero código en el binario de competencia.
//
// Contrato: docs/firmware/TELEMETRIA-DOWN.md. App de PC: tools/monitor-base/.

#pragma once
namespace iitasoccer {
void down_telemetry_init();
void down_telemetry_tick();
}  // namespace iitasoccer
