// read_stamp.h — "Ratchet de vida" por-sensor para la pizarra del TOP (PURO, host-testeable).
//
// Por qué existe (FASE 2 de la arquitectura RT no-bloqueante — INC-2 2026-06-16)
// ----------------------------------------------------------------------------------
// El emisor del WorldSnapshot (snapshot_emitter) corre @100 Hz desde un IntervalTimer y
// SOLO lee la pizarra (sensor_slot.h). El LOOP vuelca cada getter a su slot. El fail-safe
// de la pizarra es: un slot que VENCE colapsa a su sentinela (snapshot_assembler.h). PERO
// si el LOOP estampa cada slot con `now` cada vuelta (status quo de Fase 1), la frescura
// del slot mide la cadencia del LOOP, no la del SENSOR: cubre el LOOP-MUERTO (deja de
// publicar → todo vence), pero NO la muerte de UN sensor con el loop vivo (el getter
// devuelve su último valor cacheado y el loop lo re-estampa como fresco para siempre).
//
// Este módulo cierra ese hueco con un RATCHET: el stamp del slot avanza SÓLO cuando el
// sensor dio señal de vida ESTE publish. Si el sensor se calló, el stamp se CONGELA en su
// último instante con vida → el slot envejece al ritmo del SENSOR → tras su umbral,
// slot_is_fresh() = false → sentinela POR-SENSOR, aunque el loop siga publicando.
//
// Quién decide "señal de vida" (alive_now): el CALLER, a partir de las señales que cada
// fuente YA expone (cámara: cameras_*_alive(); ToF: min != TOF_NO_READING; heading:
// sensors_imu_get_heading_valid(); pose: LocalizationPose.valid; OTOS: pose_age_is_fresh
// de comm_down_pose_age_ms). Este módulo NO lee hardware: recibe el bool ya decidido.
//
// Relación con la validez intrínseca (visible/valid_src): son CAPAS COMPLEMENTARIAS. La
// validez intrínseca viaja DENTRO del valor y colapsa el campo al instante (más rápido);
// el ratchet es el BACKSTOP temporal para cuando esa validez queda "pegada en válido"
// (p.ej. un sensor congelado que sigue reportando un dato viejo como bueno) o cuando hace
// falta un techo de frescura por-sensor. Las dos disparan el mismo sentinela; tener ambas
// es defensa en profundidad (el porqué de que la pizarra exista).
//
// PURO: sin Arduino, sin millis(). El tiempo entra como uint32 por parámetro (millis() en
// el Teensy; un reloj falso en los tests). Zero-init (.bss) == "nunca vivo". Header-only.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Offset "vencido para siempre": un slot cuyo sensor NUNCA dio señal de vida se estampa
// READ_STAMP_STALE_OFFSET_MS en el pasado, para que slot_is_fresh() dé STALE contra
// CUALQUIER umbral razonable — INCLUSO en los primeros ms post-boot, donde now < umbral
// y un stamp 0 (last_alive_ms zero-init) daría falso-FRESCO (now - 0 = now < umbral). El
// valor es enorme frente a los umbrales (≤ 1000 ms) y ≤ 2^31 (sin ambigüedad de signo en
// la resta unsigned). ~12,4 días.
constexpr uint32_t READ_STAMP_STALE_OFFSET_MS = 0x40000000u;

// Estado por-sensor del ratchet. UNO por slot, lo guarda el caller (el emisor). Zero-init
// (.bss) == nunca vivo, que es el arranque en frío seguro.
struct ReadStampState {
    uint32_t last_alive_ms = 0;   // último `now` en que el sensor estuvo VIVO
    bool     ever_alive    = false;
};

// read_stamp_tick — avanza/congela el ratchet y devuelve el TIMESTAMP con el que estampar
// el slot ESTE publish (se pasa como `now` a slot_publish).
//
//   alive_now=true  → el sensor vive AHORA: ratchet avanza a `now`; devuelve `now`
//                     → el slot queda FRESCO aguas abajo (idéntico al `now` plano).
//   alive_now=false (tras haber vivido) → NO avanza: devuelve el último `now` con vida
//                     → el slot envejece a su ritmo → tras el umbral, STALE → sentinela.
//   alive_now=false (NUNCA vivo) → no hay "último vivo" honesto: devuelve un stamp
//                     forzado-VENCIDO (now - READ_STAMP_STALE_OFFSET_MS) → STALE en frío.
//
// INVARIANTE de seguridad (cold start): con ever_alive=false,
//   now - read_stamp_tick(st,false,now) == READ_STAMP_STALE_OFFSET_MS  (exacto, unsigned).
// WRAP-SAFE: aritmética unsigned; el wrap de millis() (~49,7 días) no produce falso-fresco
// mientras el intervalo real sea ≪ 2^31 (siempre cierto para edades de ms/segundos).
inline uint32_t read_stamp_tick(ReadStampState& st, bool alive_now, uint32_t now) {
    if (alive_now) {
        st.last_alive_ms = now;
        st.ever_alive    = true;
        return now;
    }
    if (!st.ever_alive) {
        // Nunca vivo: stamp forzado-vencido. Resta unsigned → wrap-safe aunque now sea
        // chico (now - OFFSET envuelve, pero (now - (now - OFFSET)) == OFFSET aguas abajo).
        return now - READ_STAMP_STALE_OFFSET_MS;
    }
    // Vivió y ahora se calló: CONGELA el último stamp con vida (el slot envejece solo).
    return st.last_alive_ms;
}

}  // namespace iitasoccer
