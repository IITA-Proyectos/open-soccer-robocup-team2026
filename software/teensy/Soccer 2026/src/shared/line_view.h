// line_view.h — interpretación pura de LineStatusV2 (DOWN → CENTRAL).
//
// Helpers SIN estado y SIN dependencias de Arduino, para que el CENTRAL y los
// tests host-native (`pio test -e test_native`) compartan EXACTAMENTE la misma
// lógica de interpretación del contrato de línea. Ver:
//   docs/firmware/CONTRATO-DATOS-DOWN.md  (LineStatusV2)
//   src/shared/down_encode.cpp            (qué manda DOWN)
//
// Regla maestra del contrato: si `data_valid == 0`, NADA del frame es confiable.
// Todos los helpers respetan esa compuerta.

#pragma once
#include <stdint.h>
#include <string.h>   // memcpy
#include "types.h"
#include "proto.h"

namespace iitasoccer {

// Extrae un LineStatusV2 de un frame decodificado.
// Retorna true sólo si el frame es LINE_URGENT, con el payload del tamaño exacto
// del contrato (16 bytes) Y con schema_version == LSV2_SCHEMA. Valida tipo +
// tamaño + versión: un schema futuro del MISMO tamaño (campos reordenados,
// `reserved` reusado) se RECHAZA en vez de reinterpretarse como basura — clave
// porque DOWN y CENTRAL se flashean en sesiones/worktrees distintas (fail-safe).
inline bool lsv2_from_frame(const Frame& f, LineStatusV2& out) {
    if (f.type != MsgType::LINE_URGENT) return false;
    if (f.payload_len != sizeof(LineStatusV2)) return false;
    memcpy(&out, f.payload, sizeof(LineStatusV2));
    if (out.schema_version != LSV2_SCHEMA) return false;
    return true;
}

// ¿El robot está levantado (en el aire)? Datos de línea no confiables.
inline bool lsv2_lifted(const LineStatusV2& s) {
    return (s.event_flags & EV_LIFTED) != 0;
}

// ¿Salida de cancha inminente? Compuerta: data_valid && !lifted.
// Honra el contrato documentado en strategy.cpp:17 (lifted ⇒ false).
inline bool lsv2_imminent_exit(const LineStatusV2& s) {
    return s.data_valid != 0
        && (s.event_flags & EV_IMMINENT_EXIT) != 0
        && !lsv2_lifted(s);
}

// ¿Hay línea presente (con histéresis de DOWN)? Gateado por data_valid.
inline bool lsv2_line_present(const LineStatusV2& s) {
    return s.data_valid != 0 && s.line_present != 0;
}

// Ángulo de la línea en grados. Honra la compuerta maestra: si data_valid==0,
// el ángulo no es confiable (LINE_AVOID lo usa para el vector de retroceso, y
// con data_valid=0 un ángulo basura residual mandaría al robot a una dirección
// arbitraria). Si el campo es N/A, devuelve 0.
inline float lsv2_line_angle_deg(const LineStatusV2& s) {
    if (s.data_valid == 0) return 0.0f;
    if (s.line_angle_centideg == LSV2_NA_I16) return 0.0f;
    return s.line_angle_centideg / 100.0f;
}

// Distancia perpendicular FIRMADA del centro del robot a la línea, en mm
// (WP-3-DOWN / Capa 3). Espejo de lsv2_line_angle_deg: N/A ⇒ 0.0f.
// Convención (ver down_model.cpp): + = línea adelante del centro (+Y),
// - = atrás (-Y). El PID lateral del arquero la usa con setpoint 0 para
// mantenerse centrado sobre la línea mientras hace strafe a lo largo de ella.
// Honra la compuerta maestra: si data_valid==0, la geometría no es confiable.
inline float lsv2_cross_track_mm(const LineStatusV2& s) {
    if (s.data_valid == 0) return 0.0f;
    if (s.cross_track_mm == LSV2_NA_I16) return 0.0f;
    return static_cast<float>(s.cross_track_mm);
}

// Cantidad de sensores sobre la línea (0..32).
inline uint8_t lsv2_sensors_on_line(const LineStatusV2& s) {
    return s.sensors_on_line;
}

// ¿La muestra de línea está RANCIA (más vieja que max_age_ms)? Implementa la
// regla §3.5.5 del contrato (CONTRATO-DATOS-DOWN.md): sample_age_ms viaja en el
// frame (0..255 ms) pero hoy nadie lo consume. Helper PURO — NO lo cablea a
// world_model (esa decisión es de otro track).
//   • data_valid == 0  ⇒ false (la compuerta maestra ya descartó el frame; no
//     reportamos "stale" sobre datos que de por sí no son confiables).
//   • sample_age_ms == 255 ⇒ false (255 = sentinel N/A, "edad desconocida", NO
//     necesariamente stale — no lo tratamos como rancio).
//   • sample_age_ms > max_age_ms ⇒ true (rancio).
inline bool lsv2_sample_is_stale(const LineStatusV2& s, uint8_t max_age_ms) {
    return s.data_valid != 0
        && s.sample_age_ms != 255
        && s.sample_age_ms > max_age_ms;
}

// Penetración en la línea, clampeada a uint8 para compat con el accessor viejo
// `world_model_get_line_depth()`. Honra la compuerta maestra: si data_valid==0,
// la profundidad no es confiable (el fallback por profundidad del PID lateral
// del arquero la usa, y strafearía con dato inválido). N/A ⇒ 0; >255 mm ⇒ 255.
inline uint8_t lsv2_penetration_u8(const LineStatusV2& s) {
    if (s.data_valid == 0) return 0;
    if (s.penetration_mm == LSV2_NA_U16) return 0;
    return s.penetration_mm > 255u ? 255u
                                   : static_cast<uint8_t>(s.penetration_mm);
}

}  // namespace iitasoccer
