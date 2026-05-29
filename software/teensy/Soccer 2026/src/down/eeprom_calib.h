// eeprom_calib.h — Glue Arduino EEPROM <-> calib_storage. Solo target down.
//
// Tema 2 P1 del research doc. Persiste la calibración del anillo de 32
// sensores de línea entre power cycles, usando la EEPROM emulada del
// Teensy 4.0 (1080 bytes disponibles, usamos 136).
//
// Patrón de uso esperado en main_down.cpp:
//
//   void setup() {
//       ...
//       // 1) Intentar cargar calibración persistida.
//       if (ec_load_calibration(g_model.calib, NUM_LINE_SENSORS)) {
//           Serial.println("[ec] calibracion cargada de EEPROM");
//       } else {
//           Serial.println("[ec] EEPROM vacia/invalida — usar defaults");
//           // Inicializar con defaults razonables o esperar comando manual.
//       }
//   }
//
//   // Cuando termina una calibración manual exitosa (desde comm_central RX),
//   // antes de retornar al CENTRAL OK:
//   //   ec_save_calibration(g_model.calib, NUM_LINE_SENSORS);
//
// Las funciones NO se llaman desde dm_update() (no queremos write
// repetitivos que matarían la EEPROM emulada). Solo desde el handler
// del comando "save calib" explícito.

#pragma once
#include <stdint.h>
#include "line_calib.h"

namespace iitasoccer {

// Dónde guardamos el blob en la EEPROM. 0 deja espacio para crecer si más
// adelante hay otros datos persistentes (configuración táctica, etc.).
constexpr int EC_EEPROM_OFFSET = 0;

// Carga calibración desde EEPROM. Retorna true si TODO validó OK (magic,
// versión, n_sensors, CRC). En false, calib[] no se modifica.
bool ec_load_calibration(SensorCalib* calib, int n_sensors);

// Guarda calibración a EEPROM. Retorna true si la escritura fue exitosa.
// IMPORTANTE: llamar SOLO esporádicamente (manual save), no en cada tick.
bool ec_save_calibration(const SensorCalib* calib, int n_sensors);

// Borra el blob (escribe ceros). Útil para forzar recalibración manual.
void ec_erase_calibration();

}  // namespace iitasoccer
