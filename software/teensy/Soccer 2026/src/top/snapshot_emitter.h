// snapshot_emitter.h — Emisor DESACOPLADO del WorldSnapshot @100 Hz (GATEADO, BANCO).
//
// QUE RESUELVE
// ------------
// Hoy build_snapshot() + comm_central_send_snapshot() corren SECUENCIAL dentro del
// superloop (main_top.cpp:474-478). Si un read de sensor se atrasa (ToF ~15 ms, el
// pulseIn del HC-SR04 ~12 ms), el ENVIO del snapshot a CENTRAL se atrasa con el (el
// loop llego a caer a ~6 Hz con los 4 ToF juntos). Este modulo DESACOPLA el envio del
// ritmo impredecible de los reads:
//   1. el LOOP llama snapshot_emitter_publish() tras los reads: vuelca el ultimo valor
//      de cada sensor a su SLOT de la pizarra (sensor_slot.h, con timestamp millis()).
//   2. un IntervalTimer @100 Hz (snapshot_emitter_init) corre la ISR que SOLO lee la
//      pizarra (RAM, cero bus), arma el snapshot con FAIL-SAFE por frescura
//      (inputs_from_slots -> assemble_snapshot: slot viejo -> SENTINELA honesto, nunca
//      dato rancio disfrazado de fresco) y lo manda no-bloqueante por Serial4. El timer
//      PREEMPTA al loop -> el snapshot sale CLAVADO a 100 Hz aunque el loop este trabado
//      leyendo un ToF.
//
// LO QUE GANA Y LO QUE NO (honesto): desacopla el EMISOR del bus; NO acelera el bus I2C
// Wire (BNO + 4 ToF comparten un solo SDA/SCL -> paralelismo CERO; ningun timer lo
// cambia). El HEADING sigue siendo la raiz del mapa.
//
// GATEADO -DTOP_ENABLE_SNAPSHOT_TIMER: con el flag OFF (competencia), TODO snapshot_emitter.cpp
// es traduccion VACIA y main_top NO lo llama (calls gateados) -> binario BYTE-IDENTICO al
// emit-en-loop de hoy. Con el flag ON, el emit se mueve del loop a la ISR del timer.
// *** BANCO LO CIERRA EL EQUIPO (regla #1 del repo) ***: la ISR/IntervalTimer, el WCET,
// la reentrancia Serial4 (loop hace read, ISR hace write), el ordering volatile/__DMB()
// de la pizarra y que el snapshot no se frene NO son host-testeables.
//
// INVARIANTES LOAD-BEARING (no romper):
//   - El WDT (watchdog_feed) queda SOLO en el loop (main_top.cpp:391). La ISR NUNCA lo
//     alimenta: si lo hiciera, un loop muerto quedaria ENMASCARADO (el timer seguiria
//     emitiendo snapshots cada vez mas rancios). Loop muerto -> WDOG resetea a 1 s ->
//     auto-recuperacion. (El emit con loop muerto sale TODO sentinela -> CENTRAL frena.)
//   - La ISR NO toca ningun bus (Wire/pulseIn), NO hace Serial.print ni float pesado:
//     solo read_latest de RAM + assemble (puro) + Serial4.write no-bloqueante.
//   - UN solo escritor por slot: el publish corre SOLO desde el loop (single-writer del
//     seqlock); la ISR solo lee. Dos escritores por slot corromperian el seqlock.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// setup(): arma el IntervalTimer @100 Hz que corre la ISR del emisor. No-op si el flag
// esta OFF (no se llama desde main_top en ese caso).
void snapshot_emitter_init();

// loop(): vuelca los getters cacheados de cada sensor a su slot de la pizarra. SINGLE-WRITER
// (solo el loop publica). No bloquea. Se llama tras los reads de sensores, antes de que el
// timer (que corre en paralelo) lea la pizarra.
void snapshot_emitter_publish();

// Telemetria de banco (T5 del doc ARQUITECTURA-SENSORIAL-TOP-NO-BLOQUEANTE.md).
uint32_t snapshot_emitter_frames();        // frames emitidos por la ISR
uint32_t snapshot_emitter_isr_wcet_us();   // peor WCET visto de la ISR (us)

}  // namespace iitasoccer
