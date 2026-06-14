// top_eeprom_config.h — Glue Arduino para persistir TopConfig en la EEPROM de la
// TOP. Hermano de down/eeprom_calib.h. El cuerpo (.cpp) usa <EEPROM.h> → Arduino-
// only; la (de)serialización + CRC vive en el módulo PURO src/shared/top_config.
//
// Mapa de EEPROM de la TOP (Teensy 4.0, 1080 B emulados en flash):
//   [320, 367]  calib BNO055 dual (sensors_imu.cpp, EE_BASE=320)
//   [368, 460]  TopConfig (este módulo, 93 B)   ← NO pisar la calib del IMU
//   [461, 1079] libre
// Ver docs/firmware/EEPROM-MAP.md.

#pragma once
#include "top_config.h"

namespace iitasoccer {

// Primer byte libre tras la calib del IMU ([320,367]).
constexpr int TOP_CONFIG_EEPROM_OFFSET = 368;

// Config viva del robot. La leen los apply-points (cámaras/IMU/ToF). Arranca en
// defaults hasta que setup() llame a top_config_load(). Definida en el .cpp.
extern TopConfig g_top_cfg;

// Carga la config de EEPROM a *out. SIEMPRE deja *out en estado válido: primero
// pone defaults (no-op = competencia byte-idéntica), luego, si la EEPROM tiene un
// blob válido (magic+version+CRC), lo sobreescribe. Retorna true si cargó config
// persistida, false si usó defaults (EEPROM en blanco / inválida).
bool top_config_load(TopConfig* out);

// Persiste cfg en EEPROM (EEPROM.update byte a byte → minimiza desgaste). Retorna
// false si no entra. Operación de BANCO (robot quieto), nunca en el loop.
bool top_config_save(const TopConfig& cfg);

// Borra el blob (deja la región en 0 → próxima carga cae a defaults).
void top_config_erase();

}  // namespace iitasoccer
