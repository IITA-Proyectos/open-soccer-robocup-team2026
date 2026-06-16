// line_reliable_gate.h — compuerta de "LECTURA CONFIABLE" del anillo de línea DOWN.
// PURO, header-only, host-testeable. Sin Arduino/Wire/analogRead: solo lógica + stdint.
//
// Diseño: docs/firmware/ARQUITECTURA-LAZO-DOWN-RT.md §6.3-6.4 (la capa MÁS crítica).
// Fase F4 del plan gateado (-DDOWN_RELIABLE_GATE). ANDAMIAJE: este header NO se cablea a
// ningún loop todavía. Solo entrega la lógica pura + su test. El glue de dm_update lo
// integrará detrás del #define cuando banco suba los umbrales (regla #1 CLAUDE.md: solo
// el equipo con la placa cierra una TASK de hardware).
//
// ── QUÉ COMPONE ───────────────────────────────────────────────────────────────────────
// La pregunta de §6: ¿cuándo es CONFIABLE una lectura, ANTES de poblar
// line_present / escape_angle / cross_track / penetration?
//
// dm_update HOY ya calcula un data_valid que ANDea (down_model.cpp:284):
//     sm_data_valid(lifted, suspect) && !any_mux_dead && !saturated
// es decir: NO levantado, NO saturado todo-blanco, NO mux muerto, calib utilizable
// (n_weak ≤ max_weak). Eso es la ENTRADA `base_data_valid` de esta compuerta — los
// puntos 1-4 del criterio §6.3, que YA existen vivos.
//
// Este módulo agrega los DOS ejes NUEVOS de §6.4 sobre esa base, sin reimplementar nada:
//   · EJE B (§6.4-B): piso de SALUD GLOBAL. Si sh_healthy_count cae bajo min_healthy,
//     el centroide se computa con sensores sesgados (ej. 22 sanos, 10 enfermos) y puede
//     MENTIR saliendo valid=1. Hoy 10+ enfermos NO bajan data_valid (solo se excluyen
//     uno a uno). El piso cierra ese caso.
//   · EJE C (§6.4-C / §6.3 punto 5): SOPORTE MÍNIMO. El vector (escape/cross/penetration)
//     solo es confiable si hay ≥ min_sensors_for_vector sensores VALIDADOS — la "dosis de
//     evidencia": tempranísima (3 del anillo externo) pero NO 1-2 ruidosos aislados.
//
//     data_valid = base_data_valid
//                  AND (healthy_count      >= min_healthy)
//                  AND (validated_count    >= min_sensors_for_vector)
//
// Y produce la ACCIÓN A (§6.4-A): el glue SELLA la geometría a sentinela cuando NO es
// confiable. Esta capa solo dice "hay que sellar"; el sellado concreto a LSV2_NA_* lo
// hace dm_update (la fuente deja de mentir, no depende de que CENTRAL respete la regla):
//
//     seal_geometry = !data_valid
//
// ── BYTE-IDENTIDAD (lo más load-bearing) ───────────────────────────────────────────────
// El camino vivo NO cambia hasta que banco suba los umbrales. Con cfg DEFAULT —
// enabled=false, O min_healthy=0, O min_sensors_for_vector=0 — el resultado DEBE degenerar
// EXACTO a base_data_valid:
//   · enabled=false        → la compuerta es transparente: data_valid == base_data_valid.
//   · min_healthy=0        → (healthy_count >= 0) es SIEMPRE verdadero (count es ≥0): no-op.
//   · min_sensors_for_vector=0 → (validated_count >= 0) es SIEMPRE verdadero: no-op.
// Un RgCfg zero-inicializado (= comportamiento actual) es por construcción un no-op total.
//
// REGLA DURA DE SEGURIDAD: esta compuerta NUNCA produce un data_valid MÁS PERMISIVO que
// base_data_valid. Solo puede RESTAR confianza (AND con condiciones extra), nunca sumarla.
// Si base_data_valid==0, el resultado es 0 pase lo que pase con los pisos. Verificado por
// construcción (data_valid empieza en base y solo se le ANDea) y por test.
//
// Fail-safe: el sellado es la red. Si esta compuerta marca data_valid=0, seal_geometry=1
// fuerza la geometría a N/A → ningún consumidor de CENTRAL puede actuar sobre un escape
// falso. La compuerta degrada hacia el lado SEGURO (sellar de más, nunca de menos).

#pragma once
#include <stdint.h>

namespace iitasoccer {

// ── Entradas (las produce dm_update; acá son datos puros, sin fuente de hardware) ──────
struct RgInputs {
    // base_data_valid: el data_valid que dm_update YA computa hoy (down_model.cpp:284):
    //   sm_data_valid(lifted, suspect) && !any_mux_dead && !saturated.
    // Captura los puntos 1-4 del criterio §6.3 (lifted / saturado / mux muerto / calib).
    bool base_data_valid;

    // healthy_count: cuántos sensores SANOS hay (sh_healthy_count). 0..32. Eje B (§6.4-B).
    int  healthy_count;

    // validated_count: cuántos sensores quedaron VALIDADOS (post salud+saturación+weak+
    // filtro espacial) — los que alimentan lg_compute_xy. Es el SOPORTE del vector. Eje C.
    int  validated_count;
};

// ── Configuración (umbrales gateados; default = no-op byte-idéntico) ───────────────────
struct RgCfg {
    // min_healthy: piso de salud global (§6.4-B, propuesto banco ≥24/32). 0 = eje OFF
    //   ((count>=0) siempre true). El piso convierte un centroide muy sesgado en data_valid=0.
    int  min_healthy;

    // min_sensors_for_vector: soporte mínimo del vector (§6.3 punto 5 / §6.4-C, propuesto 3).
    //   0 = eje OFF ((count>=0) siempre true). Es la "dosis de evidencia" mínima.
    int  min_sensors_for_vector;

    // enabled: master switch de la compuerta. false = transparente (data_valid==base),
    //   ignora ambos pisos aunque estén seteados. Para que el gate completo viva detrás de
    //   un solo flag sin tener que poner los dos pisos en 0. Default false = camino vivo.
    bool enabled;
};

// ── Resultado ──────────────────────────────────────────────────────────────────────────
struct RgResult {
    bool data_valid;     // compuerta maestra compuesta (nunca más permisiva que base).
    bool seal_geometry;  // == !data_valid → el glue sella escape/cross/penetration/angle a N/A.
};

// Default byte-idéntico al camino vivo: gate apagado, ambos pisos en 0 (doble seguro).
// Un RgCfg{} zero-init da exactamente esto, pero lo exponemos explícito para legibilidad.
inline RgCfg rg_cfg_default() {
    RgCfg c;
    c.min_healthy            = 0;
    c.min_sensors_for_vector = 0;
    c.enabled                = false;
    return c;
}

// ── La compuerta ────────────────────────────────────────────────────────────────────────
// Compone data_valid y seal_geometry. PURA: misma entrada → misma salida, sin estado.
inline RgResult rg_evaluate(const RgInputs& in, const RgCfg& cfg) {
    RgResult r;

    // Gate apagado → transparente: el resultado es EXACTAMENTE base_data_valid. Esta rama
    // garantiza la byte-identidad del camino de competencia mientras enabled=false (default).
    if (!cfg.enabled) {
        r.data_valid    = in.base_data_valid;
        r.seal_geometry = !r.data_valid;
        return r;
    }

    // Gate encendido: partir de base (NUNCA sumar confianza) y RESTAR con los dos ejes.
    // Importante: arrancamos en base_data_valid, así que si base ya es false el AND da false
    // pase lo que pase con los pisos — la compuerta jamás es más permisiva que hoy.
    bool valid = in.base_data_valid;

    // EJE B — piso de salud global. Con min_healthy=0 el AND es no-op (count siempre ≥0).
    if (in.healthy_count < cfg.min_healthy) valid = false;

    // EJE C — soporte mínimo del vector. Con min_sensors_for_vector=0 el AND es no-op.
    if (in.validated_count < cfg.min_sensors_for_vector) valid = false;

    r.data_valid    = valid;
    r.seal_geometry = !valid;   // §6.4-A: no confiable ⇒ el glue sella a sentinela LSV2_NA_*.
    return r;
}

}  // namespace iitasoccer
