// loop_monitor.h — Supervisor de loop-time (PURO, host-testeable).
//
// Por qué existe (audit 2026-06-05, R2 — confiabilidad CENTRAL)
// -------------------------------------------------------------
// El watchdog de hardware (WDOG1) recupera al robot cuando el loop YA se colgó
// (reset duro a 1 s). Pero un cuelgue total no es el único modo de falla: el loop
// puede DEGRADARSE — un sensor que tarda más, un ring de UART que se desborda y se
// drena en ráfagas, una transacción I²C que ocasionalmente roba 12 ms — sin nunca
// trabarse del todo. Esa degradación baja la tasa de strategy/motores por debajo de
// los 100 Hz nominales y se ve en cancha como un robot "lento de reflejos", pero NO
// dispara el WDT. Este módulo mide el tiempo de cada vuelta del loop y expone el
// PEOR caso y un promedio suave (EMA) para verlo en la telemetría DIAG ANTES de que
// se convierta en un cuelgue. Es diagnóstico puro: no toma ninguna acción.
//
// CÓMO SE USA: main_central toma micros() al inicio de cada loop() y llama
// loop_monitor_update(); el print de debug existente expone max_us y ema_us. Es
// barato (una resta + un par de multiplicaciones float) → corre SIEMPRE, sin flag.
//
// PURO: sin Arduino. POD + función libre → .bss zero-init y compila con g++ host.
// El tiempo entra como uint32 de micros() (el caller decide la fuente); el módulo
// solo hace aritmética wrap-safe sobre ese contador.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Peso del EMA: cuánto pesa CADA muestra nueva en el promedio suave. 0.05 = el
// promedio responde despacio (memoria ~20 muestras) → un pico aislado no mueve el
// avg, pero una degradación sostenida sí lo levanta. El peor caso vive aparte en
// max_us (que NO olvida), así que el EMA puede ser tranquilo sin perder el spike.
constexpr float LOOP_MONITOR_EMA_ALPHA = 0.05f;

// Estado del supervisor (persiste entre vueltas del loop). Zero-init (.bss) ==
// estado inicial válido: primed=false → el primer tick solo siembra last_us y no
// puede medir un dt (no hay vuelta anterior con la cual comparar).
struct LoopMonitor {
    uint32_t last_us;   // micros() de la vuelta anterior (referencia para el dt)
    uint32_t max_us;    // PEOR dt observado (no se olvida; el caller lo resetea si quiere)
    float    ema_us;    // promedio suave del dt (EMA con LOOP_MONITOR_EMA_ALPHA)
    bool     primed;    // ¿ya hubo una primera vuelta que sembró last_us?
};

// Alimenta el timestamp (micros) del inicio de esta vuelta del loop.
//
// Lógica:
//   • Primera llamada (no primed) => solo siembra last_us; no hay dt todavía.
//   • Llamadas siguientes => dt = now - last (resta unsigned, WRAP-SAFE: si micros()
//     dio la vuelta de 0xFFFFFFFF a 0, la resta unsigned igual da el dt correcto
//     mientras la vuelta dure menos que ~71 min, lo cual SIEMPRE se cumple).
//     Actualiza el peor caso (max_us) y el promedio suave (ema_us).
//
// No devuelve nada: es un sumidero de telemetría. El caller lee max_us/ema_us del
// struct cuando le conviene (p.ej. en el print de debug cada 500 ms).
inline void loop_monitor_update(LoopMonitor& m, uint32_t now_us) {
    if (!m.primed) {
        // Primera vuelta: sin referencia previa no hay dt medible. Solo sembrar.
        m.primed  = true;
        m.last_us = now_us;
        m.max_us  = 0;
        m.ema_us  = 0.0f;
        return;
    }

    // Resta unsigned: maneja el wrap de uint32 de micros() automáticamente
    // (now - last en aritmética modular da el intervalo real aunque now < last).
    const uint32_t dt = now_us - m.last_us;
    m.last_us = now_us;

    if (dt > m.max_us) m.max_us = dt;

    // EMA: la primera medición real (max_us recién dejó de ser 0 por la siembra)
    // arranca el promedio en el valor crudo para no arrastrar el 0 inicial.
    if (m.ema_us == 0.0f) {
        m.ema_us = static_cast<float>(dt);
    } else {
        m.ema_us += LOOP_MONITOR_EMA_ALPHA *
                    (static_cast<float>(dt) - m.ema_us);
    }
}

}  // namespace iitasoccer
