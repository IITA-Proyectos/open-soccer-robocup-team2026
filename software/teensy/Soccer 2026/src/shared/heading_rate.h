// heading_rate.h — Estimador de velocidad angular (°/s) a partir de muestras de
// rumbo, robusto a la cadencia LENTA del sensor (coach 2026-06-14).
//
// POR QUÉ EXISTE: el rumbo (BNO en el TOP) llega a la CENTRAL repetido — el lazo
// corre a 100 Hz pero el dato cambia a ~20 Hz (ver
// docs/firmware/CONTROL-ARQUERO-LATERAL-Y-LATENCIAS.md). Derivar el rumbo TICK A
// TICK daría 0 cuatro de cada cinco ticks y un PICO en el quinto = ruido de
// cuantización (justo lo que la skill control-pid-zona-muerta dice que NO hay que
// hacer). Este módulo deriva SOLO cuando llega una MUESTRA NUEVA (el valor
// cambió), usando el tiempo real transcurrido desde la muestra anterior, y
// MANTIENE esa velocidad entre repeticiones. Si el rumbo no cambia por un rato
// (robot quieto), la velocidad decae a 0.
//
// Para qué se usa: alimentar el término de amortiguación (kd_rate) del control de
// rumbo PFM del arquero — la "D" de un PD, que frena el sobrepaso que la latencia
// del rumbo provoca en el strafe. Ver pfm_heading.h.
//
// PURO (sin Arduino) → host-testeable: bash scripts/run-host-tests.sh test_heading_rate
// Convención de rumbo: CCW-positivo, grados, envuelto a ±180 (igual que el resto
// del firmware). rate > 0 ⇒ el robot está girando CCW (rumbo creciente).

#pragma once
#include <stdint.h>

namespace iitasoccer {

struct HeadingRateCfg {
    float    min_change_deg;  // cambio mínimo para aceptar "muestra nueva" (ignora jitter)
    uint32_t stale_ms;        // sin muestra nueva por esto → la velocidad decae a 0 (quieto)
    float    max_abs_degps;   // clamp defensivo de la velocidad estimada
};

// Punto de partida (tunear en banco con la caja negra): aceptar cambios ≥0,1°
// (filtra el jitter del BNO quieto, ~±0,05°), declarar "quieto" tras 150 ms sin
// muestra nueva, clamp ±600°/s (el robot físico no gira más rápido).
inline HeadingRateCfg heading_rate_default_cfg() {
    return HeadingRateCfg{0.1f, 150u, 600.0f};
}

struct HeadingRateState {
    bool     primed;
    float    last_heading_deg;
    uint32_t last_change_ms;
    float    rate_degps;
};

inline void heading_rate_reset(HeadingRateState& s) {
    s.primed         = false;
    s.last_heading_deg = 0.0f;
    s.last_change_ms = 0;
    s.rate_degps     = 0.0f;
}

// Diferencia angular envuelta a ±180 (nombre propio para no colisionar con otros
// helpers de wrap del firmware).
inline float heading_rate_wrap180(float d) {
    while (d >  180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

// Un tick. heading_deg = rumbo actual (puede venir repetido). now_ms = reloj.
// Devuelve la velocidad angular estimada (°/s, CCW+). No bloquea ni asume período
// fijo: usa el delta de tiempo REAL entre muestras que cambiaron.
inline float heading_rate_update(HeadingRateState& s, float heading_deg,
                                 uint32_t now_ms, const HeadingRateCfg& cfg) {
    if (!s.primed) {
        s.primed          = true;
        s.last_heading_deg = heading_deg;
        s.last_change_ms  = now_ms;
        s.rate_degps      = 0.0f;
        return 0.0f;
    }

    const float dh  = heading_rate_wrap180(heading_deg - s.last_heading_deg);
    const float adh = (dh < 0.0f) ? -dh : dh;

    if (adh >= cfg.min_change_deg) {
        // Muestra NUEVA: derivar con el tiempo real desde el último cambio.
        const uint32_t dt_ms = now_ms - s.last_change_ms;   // wrap-safe (unsigned)
        if (dt_ms > 0) {
            float r = dh / (static_cast<float>(dt_ms) / 1000.0f);
            if (r >  cfg.max_abs_degps) r =  cfg.max_abs_degps;
            if (r < -cfg.max_abs_degps) r = -cfg.max_abs_degps;
            s.rate_degps = r;
        }
        s.last_heading_deg = heading_deg;
        s.last_change_ms   = now_ms;
    } else if ((now_ms - s.last_change_ms) >= cfg.stale_ms) {
        // Hace rato que no cambia → el robot está quieto en rumbo.
        s.rate_degps = 0.0f;
    }
    // Entre muestras (cambio < umbral y aún no stale): MANTIENE la última velocidad.
    return s.rate_degps;
}

}  // namespace iitasoccer
