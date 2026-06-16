// down_blackboard.h — La PIZARRA de la placa DOWN (wrapper delgado sobre sensor_slot.h).
//
// Qué es esto (rediseño RT de DOWN — §8.2 de docs/firmware/ARQUITECTURA-LAZO-DOWN-RT.md)
// ----------------------------------------------------------------------------------
// Hoy en DOWN el PRODUCTOR (re-leer crudos → dm_update → sellar sample_age_ms) y el
// DIFUSOR (down_tx_broadcast_line) están FUSIONADOS en una función síncrona
// (comm_central_send_line_urgent). Corren en el MISMO hilo cooperativo del loop(), así
// que HOY NO hay race ni torn-read. El doble-buffer todavía no hace falta.
//
// Este módulo es el ANDAMIAJE para cuando SÍ haga falta: el día que el barrido del
// anillo pase a ADC-DMA/ISR (F6/F7), el productor dejará de correr en el hilo del loop
// y aparecerá la posibilidad de un torn-read (el difusor leyendo un frame que la ISR
// está a mitad de escribir). La PIZARRA lo previene de raíz: el productor publica una
// foto coherente, el difusor lee la última foto coherente, y NUNCA sale un frame a
// medio armar.
//
// POR QUÉ UN WRAPPER Y NO UN SEQLOCK NUEVO
// ----------------------------------------------------------------------------------
// El seqlock genérico YA EXISTE y está testeado: src/shared/sensor_slot.h
// (SensorSlot<T> + slot_publish / slot_read_latest / slot_read_latest_capped /
// slot_is_fresh / slot_age_ms). La placa TOP ya lo usa (snapshot_emitter.cpp). NO se
// reimplementa nada acá: DownBlackboard sólo INSTANCIA los slots que el DOWN necesita,
// con los tipos REALES que el DOWN produce/difunde (src/shared/types.h):
//   • g_slot_line  : LineStatusV2  — el contrato DOWN→CENTRAL que viaja por el cable
//                    (TYPE 0x10), @200 Hz.
//   • g_slot_pose  : Pose2D        — pose fusionada de los OTOS, @100 Hz.
//   • g_slot_vel   : Velocity2D    — velocidades + slip del OTOS dual, @100 Hz.
// (Las tasas se conservan poniendo el gate de cadencia en el DIFUSOR, no acá. La pizarra
//  sólo guarda la última foto; quién y cuándo difunde es decisión del loop.)
//
// GATEADO -DDOWN_BLACKBOARD — con el gate OFF NO se instancia NADA
// ----------------------------------------------------------------------------------
// Igual disciplina que el resto del rediseño gateado (TOP F.., CENTRAL F5): el binario
// de competencia NO cambia hasta que banco valida. Con el gate OFF este header NO define
// el struct DownBlackboard ni el global g_down_bb → cero bytes de .bss nuevos, binario
// BYTE-IDÉNTICO a hoy. El struct y el global SOLO existen bajo #if defined(DOWN_BLACKBOARD).
//
// ADITIVO / NO CABLEADO (alcance honesto de esta fase)
// ----------------------------------------------------------------------------------
// Esta entrega es SOLO el header puro + su test. NO se cablea a comm_central.cpp ni al
// loop() de DOWN. El productor sigue difundiendo síncrono como hoy. La integración al
// loop vivo va en el MISMO paso que el barrido pase a ADC-DMA/ISR (F6), nunca antes
// (doble-buffer prematuro = complejidad sin beneficio mientras el hilo sea cooperativo).
//
// PURO: sin Arduino, sin Wire, sin analogRead. Sólo stdint + los headers puros
// sensor_slot.h y types.h. Compila con g++ host. El tiempo entra como uint32 de millis()
// (el caller elige la fuente: millis() en el Teensy, un contador falso en los tests).
//
// LÍMITES HONESTOS (heredados de sensor_slot.h):
//   • UN solo productor por slot (single-writer seqlock). En DOWN eso se cumple solo:
//     hay UN barrido del anillo (un publisher de g_slot_line) y UN fusionador OTOS (un
//     publisher de g_slot_pose y g_slot_vel). Dos escritores sobre el mismo slot NO
//     está soportado.
//   • En el Teensy real, las 2 barreras de memoria (DMB) alrededor de la ventana de
//     `seq` y el `volatile` en `seq` son OBLIGATORIOS para que el M7/compilador no
//     reordene los stores y el seqlock no sea decorativo. Eso es glue de integración
//     (se agrega al cablear el publish dentro de la ISR), NO acá: este módulo es PURO y
//     host-testeable, y el test SIMULA la concurrencia intercalando publish/read a mano.

#pragma once

#include "sensor_slot.h"   // SensorSlot<T> + slot_publish/read_latest/is_fresh/age (PURO)
#include "types.h"         // LineStatusV2, Pose2D, Velocity2D (los tipos REALES del DOWN)

namespace iitasoccer {

#if defined(DOWN_BLACKBOARD)

// DownBlackboard — LA PIZARRA COMPLETA de la placa DOWN: un SensorSlot<T> por dato que
// el DOWN produce y difunde al CENTRAL.
//
// Cada slot lo publica UN solo productor (single-writer):
//   • line : lo publica el barrido del anillo de 32 sensores (productor de la línea).
//   • pose : lo publica el fusionador de los OTOS (pose en cancha).
//   • vel  : lo publica el fusionador de los OTOS (velocidades + slip).
// El DIFUSOR (down_tx, al final del loop) sólo LEE estos slots con slot_read_latest /
// slot_read_latest_capped y manda la última foto coherente.
//
// Zero-init (.bss) ⇒ todos los slots vacíos (published=false, seq=0 par, buffers en
// ceros). Arranque en frío seguro: antes del primer publish, read_latest devuelve el
// valor inicial (ceros) y slot_is_fresh da false → el difusor trata cada slot como
// no-fresco y manda su sentinela (default-to-safe), igual que el TOP. Sin glue.
struct DownBlackboard {
    SensorSlot<LineStatusV2> line;   // contrato DOWN→CENTRAL (TYPE 0x10), @200 Hz
    SensorSlot<Pose2D>       pose;   // pose fusionada OTOS, @100 Hz
    SensorSlot<Velocity2D>   vel;    // velocidades + slip OTOS dual, @100 Hz
};

// La instancia global. Igual que el TOP (g_bb), SOLO existe con el gate ON → .bss
// byte-neutro con el gate OFF. Single-writer por slot: la disciplina la garantiza el
// cableado del loop (cada productor llama slot_publish sobre SU slot), no este header.
inline DownBlackboard g_down_bb{};

#endif  // DOWN_BLACKBOARD

}  // namespace iitasoccer
