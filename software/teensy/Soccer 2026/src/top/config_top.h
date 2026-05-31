// config_top.h — Wrapper legacy del HAL refactor (2026-05-29).
//
// Este archivo era el config monolítico original. Tras el HAL Sprint A,
// las constantes se movieron a:
//   - pinout_common.h: lo idéntico en ambos robots
//   - pinout_robot1.h / pinout_robot2.h: lo específico por robot
//   - hardware_profile.h: selector central que dispatcha
//
// Este wrapper se mantiene para que `#include "config_top.h"` en código
// existente (Sprint 1 localización, sensors_tof, main_top, etc.) siga
// funcionando sin cambios.
//
// **Código nuevo debería usar `#include "hardware_profile.h"` directamente.**
//
// Si querés agregar una constante nueva:
//   - ¿Idéntica en ambos robots? → pinout_common.h
//   - ¿Específica de un robot? → pinout_robot1.h o pinout_robot2.h
//   - Nunca acá.
//
// === Histórico ToF / XSHUT ===
// El forense del 2026-05-25 concluyó que los pads XSHUT/LP de los 4 ToF NO
// estaban ruteados en TOP rev 1.0 (máximo 2 ToF sin rework). Eso quedó
// SUPERADO por el bodge de Enzo (2026-05-30): los 4 ToF cuelgan de `Wire`
// con LP cableado por bodge a pines {9,10,11,12}, y enumeran a
// 0x2A..0x2D (confirmado en banco). `PIN_TOF_XSHUT` vive en `pinout_robotN.h`.
// Ver journal/2026-05-30-top-tof-4-en-bus-unico-enumeracion-ok.md.

#pragma once
#include "hardware_profile.h"
