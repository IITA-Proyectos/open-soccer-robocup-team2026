// central_eeprom_config.h — Glue Arduino de la calibración persistente de la CENTRAL.
// Gateado por -DCENTRAL_EEPROM_CALIB. Sin el flag: 0 bytes al binario (competencia
// byte-idéntica). El módulo PURO (struct + CRC + parser) vive en src/shared/central_config.
//
// Patrón espejo de top_eeprom_config.cpp / down/eeprom_calib.cpp.
//
// Defaults = los constexpr POR ROBOT de config_central.h (MOTOR_MIN_PWM / MOTOR_EFF_X100).
// La EEPROM es un OVERRIDE opcional: si el blob es válido (magic+version+CRC), sobreescribe;
// si está en blanco/corrupta, quedan los constexpr → comportamiento idéntico al de hoy.

#pragma once
#include "central_config.h"

namespace iitasoccer {

#ifdef CENTRAL_EEPROM_CALIB

// Config viva (defaults de los constexpr + override de EEPROM). La pueblan central_eeprom_init().
extern CentralConfig g_central_cfg;

// Valores EFECTIVOS que consumen los motores (motors_zircon.cpp). Son enteros (no int16)
// para encajar directo en el código del mixer. Se inicializan de g_central_cfg.
extern int g_cfg_min_pwm[3];   // piso PWM por motor
extern int g_cfg_eff[3];       // eficiencia ×100 por rueda

// Inicializa la config: defaults de los constexpr por robot + carga EEPROM si es válida.
// Llamar UNA vez en setup(), ANTES del primer motors_apply_command.
void central_eeprom_init();

// Aplica un comando SET ya parseado (cc_parse_command) a la config viva (y, si aplica al
// motor, a g_cfg_*). NO persiste (eso es SAVE). value se asume ya clampeado por el parser.
void central_eeprom_apply_set(CcParam param, int idx, int value);

// Persiste la config viva a la EEPROM. Retorna true si se escribió OK.
bool central_eeprom_save();

#endif  // CENTRAL_EEPROM_CALIB

}  // namespace iitasoccer
