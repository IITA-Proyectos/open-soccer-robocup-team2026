// down_telemetry_serial.h — Glue Arduino de la telemetría USB de DOWN.
//
// Un gate (detalle en el .cpp): -DDOWN_USB_MONITOR (COMPETENCIA down/down_robot2).
// El monitor de telemetría USB viaja DORMIDO en el binario de partido: se despierta
// con la app (STREAM ON + PING) o con un ENTER en el monitor serie crudo (arranca el
// envío 3 s SIN tipear STREAM ON; auto-apagado a 3 s de silencio — TASK-306). Lee el
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
