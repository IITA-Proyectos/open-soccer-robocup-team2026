// hardware_profile.h — Selector central de configuración de robot.
//
// Cualquier .cpp del firmware TOP debería incluir este archivo (NO
// config_top.h directamente — ese queda como wrapper legacy).
//
// Dispatcha al pinout específico según -DROBOT1 o -DROBOT2 que el env
// de PIO debe definir.
//
// Uso desde código:
//   #include "hardware_profile.h"
//   // ahora todo está disponible: FIELD_WIDTH_MM, PIN_TOF_XSHUT[], etc.
//
// Spec: docs/superpowers/specs/2026-05-29-hal-robot-config-design.md

#pragma once

#include "pinout_common.h"

#if defined(ROBOT1)
    #include "pinout_robot1.h"
#elif defined(ROBOT2)
    #include "pinout_robot2.h"
#else
    #error "Compilación requiere -DROBOT1 o -DROBOT2 en build_flags. Ver pinout_robot1.h / pinout_robot2.h para opciones."
#endif
