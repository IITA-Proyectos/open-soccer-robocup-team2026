// bno_read_sm.h — SCHEDULER PURO de la lectura del BNO secundario (centinela).
//
// Por qué existe (el invariante LOAD-BEARING que protege)
// -------------------------------------------------------
// El BNO055 SECUNDARIO de la placa TOP vive en `Wire` (18/19), el MISMO bus que
// los 4 ToF VL53L7CX. Medido en banco (2026-06-02 y 2026-06-08): si ese BNO se
// lee demasiado seguido (< ~50 ms) o si su lectura cae PEGADA a un getRangingData
// de un ToF, el read multi-byte del BNO se corrompe y el YAW QUEDA CONGELADO (el
// chip sigue ackeando I²C pero reporta un valor clavado). El band-aid de fondo
// (`sensors_imu.cpp`: `BNO_READ_INTERVAL_MS = 50`) baja la cadencia a ~20 Hz para
// que las colisiones con los ToF caigan; y el `#error` de `TOP_BNO_FAST` prohíbe
// leer rápido el secundario salvo que se aísle el bus (primario-solo en Wire2).
//
// Este módulo NO lee el BNO ni el ToF: SOLO DECIDE el "cuándo". Hereda dos reglas
// del banco y las vuelve verificables host-native:
//   1) CADENCIA: no leer el BNO más seguido que `min_period_ms` (50 ms default;
//      10 ms SOLO si el primario está aislado en su bus → `primary_only`).
//   2) DECONFLICT: no leer el BNO si HACE MENOS de `tof_gap_ms` (8 ms) que se leyó
//      un ToF en ese mismo bus — para que el read del BNO nunca quede adyacente a
//      un ranging del ToF. (El 8 ms es el deconflict de cuarentena del banco.)
// El caller PRODUCTOR avisa al SM cuándo leyó un ToF (`note_tof_read`) y cuándo
// leyó el BNO (`note_bno_read`); antes de cada lectura del BNO pregunta `may_read`.
//
// FAIL-SAFE (default-to-safe — never-ok)
// --------------------------------------
//   • El SM NUNCA fuerza una lectura. Si `may_read` da false, el caller NO lee y
//     el slot HEADING simplemente envejece. Eso es seguro: el ensamblador del
//     snapshot (snapshot_assembler.h) ya colapsa un heading viejo → limpia el bit
//     `heading_valid` (flags bit4) y CENTRAL deja de confiar en él. Mejor un
//     rumbo "no válido" que un yaw congelado disfrazado de bueno.
//   • CONFIG INVÁLIDA → `cfg_valid` = false (NO la "arregla" sola). El caso que
//     recongela el yaw es `min_period_ms < 50` SIN `primary_only` (leer rápido el
//     secundario del bus compartido). Lo atrapamos antes de correr. El caller que
//     reciba cfg inválida debe negarse a usarla (caer al default seguro {50,8}).
//
// PURO: sin Arduino, sin Wire/Serial/millis(). El tiempo entra como `uint32_t` de
// millis() por parámetro (la fuente la elige el caller: millis() real en el Teensy,
// un contador falso en los tests). Restas de tiempo UNSIGNED → wrap-safe ante el
// vuelco de millis() (~49,7 días), mismo criterio que sensor_slot.h /
// tof_fresh_or_no_reading. Compila con g++ host. Zero-init (.bss) == estado válido:
// un BnoSched en ceros tiene `ever_read=false` → al boot puede leer en cuanto el
// gap de ToF se cumpla (no espera un period falso desde t=0).

#pragma once
#include <stdint.h>

namespace iitasoccer {

// ----------------------------------------------------------------------------
// BnoSchedCfg — la política de cadencia + deconflict.
//
// default {50, 8, false}: cadencia de banco del secundario (20 Hz) + gap 8 ms vs
//   ToF. Es el modo SEGURO para el bus compartido (Wire con los 4 ToF).
// fast    {10, 8, true}:  100 Hz, SOLO permitido si `primary_only` (el único BNO
//   activo es el primario, que vive aislado en Wire2 sin ToF → sin contención).
//   Espejo EXACTO del `#error "TOP_BNO_FAST solo es seguro con -DTOP_BNO_PRIMARY_ONLY"`
//   de sensors_imu.cpp: leer a 10 ms el secundario del bus compartido recongela el
//   yaw, así que cfg_valid() lo rechaza.
// ----------------------------------------------------------------------------
struct BnoSchedCfg {
    uint32_t min_period_ms;   // mínimo entre dos lecturas del BNO (cadencia)
    uint32_t tof_gap_ms;      // mínimo desde el último read de ToF en el bus (deconflict)
    bool     primary_only;    // ¿el secundario está fuera del control y el primario aislado?
};

// Config DEFAULT (bus compartido seguro): 50 ms / gap 8 ms / NO primary_only.
inline BnoSchedCfg bno_sched_default_cfg() {
    return BnoSchedCfg{50u, 8u, false};
}

// Config FAST (solo válida con primary_only): 10 ms / gap 8 ms / primary_only.
// El 10 ms casa con que el BNO055 en fusión entrega a 100 Hz (leer más seguido
// re-leería el mismo valor) y solo es seguro con el primario aislado en Wire2.
inline BnoSchedCfg bno_sched_fast_cfg() {
    return BnoSchedCfg{10u, 8u, true};
}

// ----------------------------------------------------------------------------
// BnoSched — el ESTADO del scheduler (cuándo se leyó por última vez cada cosa).
//
// Zero-init (.bss) == estado válido de boot: `ever_read=false`, last_* en 0. Antes
// de la primera lectura del BNO, la cadencia NO bloquea (no hay "última lectura"
// real de la cual medir el period); solo manda el gap de ToF. Así el primer dato
// de rumbo llega apenas el bus esté libre, no `min_period_ms` después de t=0.
// ----------------------------------------------------------------------------
struct BnoSched {
    uint32_t last_bno_read_ms;   // millis() de la última lectura del BNO autorizada
    uint32_t last_tof_read_ms;   // millis() de la última lectura de un ToF en el bus
    bool     ever_read;          // ¿hubo al menos una lectura del BNO?
};

// ----------------------------------------------------------------------------
// bno_sched_cfg_valid(cfg) — ¿esta política es SEGURA de usar?
//
// false SOLO si `min_period_ms < 50` Y NO `primary_only`: ese es exactamente el
// caso que recongela el yaw (leer el secundario del bus compartido más rápido que
// el band-aid de 50 ms). Con `primary_only` el BNO está aislado en Wire2 → leer
// rápido es seguro → válido. Espejo del `#error` de TOP_BNO_FAST.
//
// NO valida tof_gap_ms (un gap 0 sería "sin deconflict", subóptimo pero no el modo
// de falla del congelamiento; el caller responsable usa el default 8). El default
// {50,8,false} y el fast {10,8,true} dan true; {10,8,false} da false.
// ----------------------------------------------------------------------------
inline bool bno_sched_cfg_valid(const BnoSchedCfg& cfg) {
    if (cfg.min_period_ms < 50u && !cfg.primary_only) return false;
    return true;
}

// ----------------------------------------------------------------------------
// bno_sched_may_read(sm, now_ms, cfg) — ¿el caller PUEDE leer el BNO AHORA?
//
// Autoriza SOLO si se cumplen LAS DOS condiciones:
//   (A) CADENCIA: pasó al menos `min_period_ms` desde la última lectura del BNO.
//       En el boot (ever_read=false) esta condición NO bloquea (no hay last real).
//   (B) DECONFLICT: pasó al menos `tof_gap_ms` desde la última lectura de un ToF.
//       Si nunca se leyó un ToF (last_tof_read_ms quedó en su init), el gap se mide
//       contra ese valor — el caller que recién leyó un ToF debe haber llamado
//       note_tof_read, así que en operación normal esto refleja el último ranging.
//
// FAIL-SAFE: si la cfg es inválida, NO autoriza (default-to-safe — nunca leemos con
// una política que recongela el yaw). Restas UNSIGNED → wrap-safe.
//
// El SM no muta nada acá: solo CONSULTA. El caller, si lee, debe llamar
// bno_sched_note_bno_read(sm, now) para registrar la lectura.
// ----------------------------------------------------------------------------
inline bool bno_sched_may_read(const BnoSched& sm, uint32_t now_ms, const BnoSchedCfg& cfg) {
    // never-ok: una política peligrosa nunca autoriza una lectura.
    if (!bno_sched_cfg_valid(cfg)) return false;

    // (B) DECONFLICT vs ToF: el read del BNO no puede quedar pegado a un ranging.
    // Resta unsigned (wrap-safe). Corte EXACTO: elapsed == gap ya cuenta como "pasó"
    // (>= gap autoriza), mismo criterio de borde que el resto de los timers del repo.
    const uint32_t since_tof = now_ms - sm.last_tof_read_ms;
    if (since_tof < cfg.tof_gap_ms) return false;

    // (A) CADENCIA: respetar el period mínimo, salvo la PRIMERA lectura (boot).
    if (sm.ever_read) {
        const uint32_t since_bno = now_ms - sm.last_bno_read_ms;
        if (since_bno < cfg.min_period_ms) return false;
    }

    return true;
}

// bno_sched_note_bno_read(sm, now_ms) — el caller AVISA que leyó el BNO recién.
// Sella el timestamp de la cadencia y marca que ya hubo al menos una lectura.
inline void bno_sched_note_bno_read(BnoSched& sm, uint32_t now_ms) {
    sm.last_bno_read_ms = now_ms;
    sm.ever_read        = true;
}

// bno_sched_note_tof_read(sm, now_ms) — el caller AVISA que leyó un ToF del bus.
// Sella el timestamp que re-arma el gap de deconflict (el próximo read del BNO
// tendrá que esperar tof_gap_ms desde ACÁ). Lo llama la rutina de ToF tras cada
// ranging, así un ToF-BNO-ToF intercalado vuelve a empujar la ventana del BNO.
inline void bno_sched_note_tof_read(BnoSched& sm, uint32_t now_ms) {
    sm.last_tof_read_ms = now_ms;
}

}  // namespace iitasoccer
