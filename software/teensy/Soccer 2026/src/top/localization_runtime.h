// localization_runtime.h — Glue de hardware para el modulo localization.
//
// Orquesta el polling de sensors_tof + sensors_imu, llama a
// localization_compute() (logica pura) y cachea el resultado para que
// main_top.cpp lo lea al armar el WorldSnapshot.

#pragma once
#include "../shared/localization.h"

namespace iitasoccer {

// Calibra bno_offset leyendo el heading actual del BNO. El robot DEBE
// estar apuntando al arco rival (+Y de la cancha) cuando se llama.
// Imprime al Serial el offset capturado para que el equipo verifique.
void localization_runtime_init();

// Lee TOF + IMU, arma LocalizationInputs, llama a localization_compute,
// cachea resultado. Llamar a ~30 Hz desde el loop del top.
void localization_runtime_tick();

// Devuelve el ultimo pose calculado. Single-producer single-consumer:
// el tick llama a esta funcion para escribir, main_top la llama para leer.
LocalizationPose localization_runtime_get_pose();

}  // namespace iitasoccer
