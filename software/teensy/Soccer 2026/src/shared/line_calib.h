#pragma once
#include <stdint.h>
namespace iitasoccer {
// `enabled`: 1 = el sensor participa de la detección de línea; 0 = se ignora
//   (se excluye del centroide/penetración, como un sensor unhealthy). Default 1.
// `sensitivity`: ajuste por-sensor del umbral, en % del semi-margen, rango
//   [-100,+100]. Default 0 (= umbral en el punto medio, comportamiento histórico).
//   Convención (banco 2026-06-13): +% = MENOS sensible (sube el umbral, más
//   estricto); -% = más sensible. Ver lc_threshold_with_sens().
struct SensorCalib {
    uint16_t carpet;
    uint16_t white;
    uint16_t threshold;        // punto medio carpet/white (calib base, SIN sensibilidad)
    uint8_t  enabled     = 1;
    int8_t   sensitivity = 0;  // [-100,+100] %
};
// Calibración estática: umbral = punto medio carpet/white. Resetea enabled=1,
// sensitivity=0 (la calibración de superficie NO toca las perillas de sintonía).
void lc_set_static(SensorCalib& c, uint16_t carpet, uint16_t white);
// Umbral EFECTIVO con sensibilidad: parte del punto medio y lo desplaza por
// (global_sens + per_sensor_sens), cada uno en % del semi-margen (white-carpet)/2.
//   +% = MENOS sensible (umbral sube hacia `white`); -% = más sensible (baja hacia
//   `carpet`). 0+0 = punto medio = histórico. La suma se clampea a [-100,+100] y el
//   umbral resultante se clampea a [min(carpet,white), max(carpet,white)].
//   Función PURA (host-testeable); la usa dm_update() en el camino de producción.
uint16_t lc_threshold_with_sens(uint16_t carpet, uint16_t white,
                                int global_sens, int per_sensor_sens);
// Adaptación on-the-fly del baseline de carpet. Solo adapta si el sensor NO
// está sobre línea (gate anti-deriva). alpha ∈ (0,1] = velocidad de adaptación.
void lc_adapt_carpet(SensorCalib& c, uint16_t filtered, bool sensor_on_line,
                      float alpha);
// true si algún sensor no separa piso/blanco (|white-carpet| < min_margin).
bool lc_is_suspect(const SensorCalib* cs, int n, uint16_t min_margin);
// Cuenta los sensores DÉBILES (|white-carpet| < min_margin) y, si weak_mask_out
// no es null, devuelve la máscara de bits (bit i = sensor i débil; válido para
// n <= 32). Permite EXCLUIR sensores débiles uno a uno en vez de invalidar la
// calib entera (banco 2026-06-12: 2 sensores físicamente flojos invalidaban
// TODO el frame → robot ciego de línea, peor fail-safe que jugar con 30 sanos).
int lc_count_weak(const SensorCalib* cs, int n, uint16_t min_margin,
                  uint32_t* weak_mask_out);
}  // namespace iitasoccer
