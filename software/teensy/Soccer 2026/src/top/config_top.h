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
// === Histórico ===
// El banner original sobre XSHUT no ruteado en TOP rev 1.0 sigue siendo
// info crítica. Se conserva en `journal/2026-05-25-top-xshut-no-routed-finding.md`
// y la nueva ubicación de `PIN_TOF_XSHUT` es `pinout_robotN.h`.

#pragma once
#include "hardware_profile.h"
