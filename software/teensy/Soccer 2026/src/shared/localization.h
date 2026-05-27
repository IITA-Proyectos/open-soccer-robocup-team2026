// localization.h — Pose absoluta del robot en cancha RCJ Soccer Open 2026.
//
// Logica pura, host-testeable. NO usa Arduino, Wire, ni hardware. La capa de
// hardware vive en src/top/localization_runtime.{h,cpp}.
//
// Algoritmo: trilateracion geometrica directa con 4 TOFs cardinales + heading
// del BNO. Precision esperada: +-2-3 cm en posicion.
//
// Spec: docs/superpowers/specs/2026-05-25-localization-sprint1-trilateration-design.md

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Sentinel para indicar lectura invalida (debe matchear sensors_tof.h).
constexpr uint16_t LOCALIZATION_TOF_NO_READING = 0xFFFF;

// Datos crudos que necesita la trilateracion.
struct LocalizationInputs {
    uint16_t tof_distance_mm[4];   // [0]=frontal, [1]=trasero, [2]=izq, [3]=der
    bool     tof_valid[4];          // false si lectura era invalida o ruidosa
    int16_t  bno_heading_centideg; // -18000..18000 (= +-180 grados x 100)
};

// Configuracion estatica (cancha + montaje + tuning).
struct LocalizationConfig {
    uint16_t field_width_mm;        // 2430 — eje X de la cancha
    uint16_t field_height_mm;       // 1820 — eje Y de la cancha
    int16_t  bno_offset_centideg;   // calibrado al boot (heading apuntando al arco rival)
    uint16_t tof_mount_angle_deg[4]; // {0, 180, 90, 270} = {front, back, izq, der}
    uint16_t outlier_threshold_mm;  // umbral inconsistencia entre TOFs del mismo eje
    // Pose anterior para el outlier rejection por consistencia. Si es la primera
    // llamada, pasar {0, 0, 0, 0, false}.
    int16_t  prev_x_mm;
    int16_t  prev_y_mm;
    bool     prev_valid;
};

// Resultado: pose calculado este ciclo + diagnostico.
struct LocalizationPose {
    int16_t  x_mm;                 // 0..field_width_mm (origen = esquina propia)
    int16_t  y_mm;                 // 0..field_height_mm
    int16_t  heading_centideg;     // 0..36000 (relativo a +Y de la cancha)
    uint8_t  source_flags;         // bit i = 1 si TOF[i] se uso este ciclo
    bool     valid;                // false si <2 TOFs utiles (1 por eje minimo)
};

// Funcion pura: dados inputs y config, devuelve pose. Sin side effects.
// Esta es la API principal del modulo, testeable host-native.
LocalizationPose localization_compute(
    const LocalizationInputs& in,
    const LocalizationConfig& cfg
);

}  // namespace iitasoccer
