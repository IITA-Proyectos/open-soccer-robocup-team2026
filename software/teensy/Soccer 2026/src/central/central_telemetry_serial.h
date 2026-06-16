// central_telemetry_serial.h — Glue Arduino del monitor USB de la CENTRAL.
// Declaraciones SIN gatear (inocuas: nadie las referencia con el flag OFF).
// Cuerpo en .cpp dentro de #if defined(CENTRAL_USB_MONITOR).
//
// El monitor viaja DORMIDO en el binario (envs *_bb con -DCENTRAL_USB_MONITOR):
// silencio total por USB hasta que el host habla (la app con STREAM ON / PING).
// En partido no hay USB → nunca despierta (match-safe). Ver telemetry_central.h.
//
// BYTE-NEUTRALIDAD: el cuerpo de estas funciones SÓLO se compila con el flag (todo el
// .cpp está dentro de #if defined(CENTRAL_USB_MONITOR)). Por eso main_central.cpp
// GATEA cada llamada con #ifdef CENTRAL_USB_MONITOR — sin el flag NO hay ni definición
// ni llamada → cero instrucciones nuevas (binario de competencia byte-idéntico).
//
// El monitor NO drena el USB: main_central.cpp es el dueño del Serial (handler g/s/d/x).
// main arma las líneas y se las pasa a central_telemetry_consume_line; el tick del
// monitor SÓLO emite el stream según el modo. Así un solo lector convive sin race.
#pragma once
#include <cstdint>

namespace iitasoccer {

// Init del monitor (llamar en setup() tras comm_*_init).
void central_telemetry_init();

// Tick del monitor: estampa la edad de los enlaces, aplica el auto-off de 3 s, y
// emite el stream según el modo. NO lee el USB (main es el dueño). Llamar cada loop().
void central_telemetry_tick();

// Cachea el MotorCommand APLICADO el último tick para que fill_frame lo exponga.
// Llamar justo después de motors_apply_command(cmd) en el loop. Recibe los 3 campos
// que el monitor publica (vx/vy/omega/spin) — evita acoplar el .h del glue a
// MotorCommand (types.h) con el flag OFF. Sin el flag, es no-op.
void central_telemetry_note_command(int16_t vx_mm_s, int16_t vy_mm_s,
                                    int16_t omega_cd_s, int16_t spin_pwm);

// Rutea UNA línea completa del handler USB de main_central.cpp al parser del monitor
// ANTES de que sus chars caigan en el handler g/s/d/x. Resuelve la TRAMPA DE LA 'S':
// "STREAM ON"/"PING"/"STREAM OFF" se consumen acá (la 'S' nunca dispara STOP).
// Devuelve true si la línea ERA un comando del monitor (PING/STREAM) → main NO debe
// re-procesar sus chars. Devuelve false si la línea trae un control real del robot
// (g/s/d/x) o un ENTER pelado → main la maneja como siempre. Sin el flag, es no-op
// que devuelve false (main procesa todo, byte-idéntico).
bool central_telemetry_consume_line(const char* line, int len);

}  // namespace iitasoccer
