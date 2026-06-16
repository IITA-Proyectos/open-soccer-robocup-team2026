// adc_scan_plan.h — El PLAN de barrido dual-ADC del anillo de línea (PURO, host-testeable).
//
// Por qué existe (reingeniería RT de la placa DOWN — §3.2 de ARQUITECTURA-LAZO-DOWN-RT.md)
// ----------------------------------------------------------------------------------------
// El barrido de los 32 sensores HOY lee los 4 muxes UNO POR UNO con 4 `analogRead`
// secuenciales (`line_ring.cpp:58`). Hecho de hardware load-bearing (forum PJRC 58387,
// verificado en el doc de arquitectura): en el Teensy 4.0 (IMXRT1062) los 4 pines de mux
// `PIN_MUX_OUT[4] = {A0, A1, A8, A9}` están DUAL-MAPEADOS a AMBOS ADCs internos
// (A0=ADC1/ADC2_IN7, A1=IN8, A8=IN13, A9=IN14). Por eso CUALQUIER par de muxes se puede
// convertir EN PARALELO: uno en ADC1, otro en ADC2, con `startSynchronizedSingleRead` /
// `readSynchronizedSingle`. Eso baja el barrido de 4 lecturas simples a 2 lecturas DOBLES.
//
// Este módulo es la LÓGICA PURA del "qué se lee con qué ADC": dado cuántos muxes hay
// conectados (NUM_MUXES = NUM_LINE_SENSORS / 8), decide el APAREO — qué par de índices de
// mux se leen juntos en una conversión sincronizada — y cuántas lecturas dobles tiene el
// barrido. NO hace I2C ni toca el ADC ni un GPIO; solo computa el plan. El glue de
// `line_ring.cpp` (otra fase, gateada) consume este plan para emitir las llamadas reales al
// periférico. Separar el PLAN (puro, testeable sin hardware) de la EJECUCIÓN (Arduino/ADC)
// es lo que deja este pedazo verificable en banco-de-software antes de tocar la placa.
//
// El plan en una frase
// ----------------------------------------------------------------------------------------
// Emparejá los muxes de a dos en orden de índice {0,1}, {2,3}, … Cada par es UNA lectura
// doble (ADC1+ADC2 a la vez). Si sobra un mux impar al final, queda SOLO (lectura simple,
// sin compañero) — un fallback honesto, no un error.
//
//   NUM_MUXES = 4 → pares {0,1} y {2,3}        → 2 dobles, 0 solo   (PRODUCCIÓN)
//   NUM_MUXES = 3 → par  {0,1} + solo {2}       → 1 doble, 1 solo   (degradado)
//   NUM_MUXES = 2 → par  {0,1}                   → 1 doble, 0 solo   (degradado)
//   NUM_MUXES = 1 → solo {0}                     → 0 dobles, 1 solo  (FALLBACK 1 mux)
//   NUM_MUXES = 0 → plan vacío                   → 0 dobles, 0 solo  (defensa, no crashea)
//
// Por qué el apareo es {0,1},{2,3} y no otro
// ----------------------------------------------------------------------------------------
// Como los 4 pines están dual-mapeados a AMBOS ADCs, cualquier partición en pares es
// eléctricamente válida (no hay un par "prohibido"). Elegimos el apareo CONSECUTIVO por
// índice porque es el más simple de auditar y el que conserva el orden del barrido
// histórico (mux 0, 1, 2, 3) → el dato crudo `g_raw[]` sale en el MISMO orden lógico que
// hoy, sin re-mapear sensores. La sincronización agrupa, no reordena: dentro de un par,
// el primer índice va por ADC1 y el segundo por ADC2 (convención fija — el glue la respeta
// al cosechar `readSynchronizedSingle`).
//
// FAIL-SAFE / por qué NO produce un plan tramposo
// ----------------------------------------------------------------------------------------
//   • n_muxes fuera de rango (negativo, o > MAX) se SATURA a [0, MAX] en vez de indexar
//     fuera de los arrays. Un plan con basura adentro es peor que un plan vacío: el glue
//     leería un índice de mux inexistente y dispararía un `analogRead` a un pin que no es.
//   • El mux SOLO (impar) nunca se "inventa" como medio-par: queda explícito en `solo_mux`
//     con `has_solo=true`, así el glue emite una lectura SIMPLE para él (no una doble con
//     un compañero fantasma que leería un ADC vacío).
//   • Es determinista y sin estado: la misma entrada da el mismo plan, siempre. Se calcula
//     UNA vez al boot (o en un test) y se cachea; no corre en el hot-loop.
//   • Degrada coherente: con la placa a 1 mux (fallback de `config_down.h`) el plan no se
//     rompe — da el barrido simple de 1 mux, exactamente lo que la placa degradada necesita.
//
// PURO: solo <stdint.h> + lógica entera. Sin Arduino, sin Wire, sin analogRead. Compila con
// g++ host. Zero-init (.bss) de AdcScanPlan == plan vacío y coherente (n_pairs=0, sin solo).
// Mismo patrón "andamiaje gateado, header-only" que sensor_slot.h / vel_lateral.h.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Tope físico de muxes en la placa DOWN: 4 CD4051 (32 sensores / 8 por mux). El plan nunca
// genera más de MAX/2 pares. Es una constante de DISEÑO de la placa, no de runtime.
constexpr int ADC_SCAN_MAX_MUXES = 4;
constexpr int ADC_SCAN_MAX_PAIRS = ADC_SCAN_MAX_MUXES / 2;   // 2 pares máx.

// AdcScanPlan — el plan de barrido para una cantidad dada de muxes.
//
// Layout pensado para que el zero-init (.bss) sea un plan VACÍO y coherente:
//   n_pairs=0, has_solo=false → el glue no emite ninguna lectura (placa sin muxes).
struct AdcScanPlan {
    // pairs[k] = {idxA, idxB}: el par k-ésimo. idxA se convierte por ADC1, idxB por ADC2,
    // EN PARALELO (lectura sincronizada). El orden de los pares ES el orden del barrido:
    // primero pairs[0], luego pairs[1], … (conserva el orden lógico del mux histórico).
    uint8_t pairs[ADC_SCAN_MAX_PAIRS][2];

    // Cuántos pares válidos hay en `pairs` (0..ADC_SCAN_MAX_PAIRS).
    int n_pairs;

    // ¿Sobró un mux impar sin compañero? Si has_solo, `solo_mux` es su índice y el glue
    // emite una lectura SIMPLE (un solo ADC) para él, DESPUÉS de los pares.
    bool has_solo;
    uint8_t solo_mux;

    // Total de muxes que el plan cubre = 2*n_pairs + (has_solo ? 1 : 0). Redundante con lo
    // anterior, pero explícito para que el caller verifique "el plan cubre los N que pedí".
    int n_muxes;
};

namespace adc_scan_detail {
// Satura n al rango físico [0, ADC_SCAN_MAX_MUXES]. Un n negativo o gigante NO debe
// terminar indexando fuera de los arrays del plan — es la defensa de borde del módulo.
inline int clamp_muxes(int n) {
    if (n < 0) return 0;
    if (n > ADC_SCAN_MAX_MUXES) return ADC_SCAN_MAX_MUXES;
    return n;
}
}  // namespace adc_scan_detail

// adc_plan_build(n_muxes) — construye el plan de barrido dual-ADC para n_muxes conectados.
//
// Empareja consecutivo {0,1}, {2,3}, …; el impar sobrante queda SOLO. n_muxes fuera de
// rango se satura a [0, MAX] (fail-safe: nunca un índice de mux inexistente en el plan).
//
// Determinista, sin estado, sin hardware. Pensado para llamarse UNA vez (boot/test) y
// cachear el resultado; NO corre en el hot-loop del barrido.
inline AdcScanPlan adc_plan_build(int n_muxes) {
    const int n = adc_scan_detail::clamp_muxes(n_muxes);

    AdcScanPlan plan{};        // value-init: pairs=0, n_pairs=0, has_solo=false, solo=0, n_muxes=0
    plan.n_muxes = n;

    // Pares consecutivos: idx 0..n-1 de a dos. El último mux impar (si n es impar) se deja
    // para el bloque SOLO de abajo; por eso el lazo avanza de a 2 y corta en n-1.
    int idx = 0;
    while (idx + 1 < n && plan.n_pairs < ADC_SCAN_MAX_PAIRS) {
        plan.pairs[plan.n_pairs][0] = static_cast<uint8_t>(idx);       // → ADC1
        plan.pairs[plan.n_pairs][1] = static_cast<uint8_t>(idx + 1);   // → ADC2
        ++plan.n_pairs;
        idx += 2;
    }

    // ¿Quedó un mux impar sin compañero? (idx == n-1). Lectura SIMPLE, no media-doble.
    if (idx < n) {
        plan.has_solo = true;
        plan.solo_mux = static_cast<uint8_t>(idx);
    }

    return plan;
}

}  // namespace iitasoccer
