// otos_health.h — Máquina de salud por OTOS (PURA, host-testeable).
//
// Por qué existe (audit 2026-06-03 #6, #13, #15)
// ----------------------------------------------
// otos.cpp llamaba getPosition/getVelocity e IGNORABA el retorno I²C. Los flags
// g_left_ready/g_right_ready se seteaban SOLO en el boot y nunca se re-evaluaban.
// Si un OTOS se desconectaba en partido (Qwiic flojo, ESD, brownout del riel 3.3V):
//   • la lectura I²C falla y el struct queda en (0,0,0) → la pose COLAPSA al origen,
//   • pero el ready-flag seguía true → confidence=60/100 (mentía),
//   • CENTRAL no podía distinguir "OTOS murió" de "robot quieto" (Pose2D no lleva
//     campo de edad; el watchdog de enlace de 500 ms ve frames llegando igual).
//
// Esta unidad encapsula la decisión de salud SIN tocar Wire: el wrapper Arduino
// (otos.cpp) sólo captura el bool ok/fail de cada lectura y se lo pasa a
// oh_update(). La política, deliberada (ver audit #6, decisión de diseño):
//   • Histéresis para CAER: N fallos consecutivos bajan ready (un fallo aislado
//     NO lo baja → no flapea ante glitches transitorios de bus).
//   • LATCH una vez caído: lecturas OK posteriores NO re-habilitan solo. Re-armar
//     requiere oh_reset() (gatillado por CENTRAL_RESET_OTOS / reboot), porque tras
//     un brownout el chip pierde su config (unidades/tracking) y re-habilitar a
//     ciegas daría pose en unidades equivocadas.
//   • La confianza emitida se deriva de la salud ACTUAL de ambos OTOS, no del boot.
//
// Es POD + funciones libres → cabe en .bss (zero-init) y se testea con g++.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Cuántos fallos I²C consecutivos bajan un OTOS de "ready" a "caído".
// 3 absorbe glitches de 1–2 ticks pero cae rápido (<~30 ms a 100 Hz).
constexpr uint8_t OTOS_HEALTH_FAIL_THRESHOLD = 3;

// Estado de salud de UN OTOS. Zero-init (.bss) == estado inicial válido:
// present=false → todavía no se detectó (boot lo activa con oh_set_present).
struct OtosHealth {
    bool    present;         // ¿se detectó al boot? (begin() respondió)
    bool    alive;           // ¿sigue respondiendo? (cae tras N fallos, latch)
    uint8_t consec_fail;     // contador de fallos I²C consecutivos
};

// Lo deja en estado "no presente, no vivo". Llamado al boot ANTES de detectar.
inline void oh_reset(OtosHealth& h) {
    h.present = false;
    h.alive = false;
    h.consec_fail = 0;
}

// Marca el resultado de la detección de boot (begin()). Si detectó, arranca vivo.
inline void oh_set_present(OtosHealth& h, bool detected) {
    h.present = detected;
    h.alive = detected;
    h.consec_fail = 0;
}

// Alimenta el resultado de una lectura I²C de este tick.
//   read_ok=true  → lectura buena: resetea el contador de fallos (NO re-habilita
//                   si ya estaba caído — política latch).
//   read_ok=false → lectura fallida: incrementa fallos; al alcanzar el umbral baja
//                   alive (con saturación del contador para evitar wrap).
// Retorna el estado `alive` resultante (true = la pose de este OTOS es confiable).
inline bool oh_update(OtosHealth& h, bool read_ok) {
    if (!h.present) return false;          // nunca estuvo → nada que actualizar
    if (read_ok) {
        h.consec_fail = 0;                 // racha de fallos cortada
        return h.alive;                    // latch: no re-sube solo
    }
    if (h.consec_fail < 0xFF) h.consec_fail++;   // saturar, no wrappear
    if (h.consec_fail >= OTOS_HEALTH_FAIL_THRESHOLD) {
        h.alive = false;                   // caída latcheada
    }
    return h.alive;
}

// Confianza de pose 0..100 derivada de la salud ACTUAL de ambos OTOS:
//   ambos vivos → 100 ; uno vivo (degradado a single) → 60 ; ninguno → 0.
// Reemplaza el cálculo viejo basado en los flags de boot (que mentían).
inline uint8_t oh_pose_confidence(const OtosHealth& left, const OtosHealth& right) {
    const bool l = left.alive;
    const bool r = right.alive;
    if (l && r) return 100;
    if (l || r) return 60;
    return 0;
}

}  // namespace iitasoccer
