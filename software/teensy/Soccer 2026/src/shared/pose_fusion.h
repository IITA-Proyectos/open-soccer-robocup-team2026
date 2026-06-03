// pose_fusion.h — Fusión complementaria ToF(absoluto) + OTOS(odometría) para la
// pose x/y del TOP. Logica PURA host-testeable (sin Arduino, Wire ni hardware).
//
// QUÉ HACE
// --------
// Resuelve el problema raíz de los "dos mapas":
//   - La pose ToF (localization_compute) es ABSOLUTA y mata el drift, pero salta,
//     es intermitente y con el HW actual casi nunca da valid.
//   - La OTOS de DOWN es SUAVE y de alta frecuencia (100 Hz) pero deriva sin cota.
// El filtro complementario integra el DELTA de la OTOS sobre la pose fusionada
// (PREDICCIÓN) y, cuando hay pose ToF válida y consistente, tira suavemente hacia
// ella (CORRECCIÓN: pose += K*(pose_tof - pose)) para anclar al absoluto.
//
// QUÉ NO HACE
// -----------
// NO fusiona heading. El heading SIEMPRE viene del BNO055 y solo pasa de largo
// (pass-through) por este módulo, igual que en main_top hoy.
//
// TODO EN ENTEROS: pose en mm, heading en centideg. El factor K se expresa como
// entero /256 (Q8) para no usar float ni en target ni en host.
//
// CONVENCIÓN DE EJES (docs/CONVENCION-EJES-ROBOT.md): X largo=2430mm,
// Y corto=1820mm; origen en una esquina; heading 0 = robot mira +Y.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// --- Bits de source_flags en la salida ---
constexpr uint8_t POSE_FUSION_FLAG_OTOS_PRED   = 0x01;  // bit0: usó predicción OTOS
constexpr uint8_t POSE_FUSION_FLAG_TOF_CORR    = 0x02;  // bit1: aceptó corrección ToF
constexpr uint8_t POSE_FUSION_FLAG_TOF_REJECT  = 0x04;  // bit2: rechazó ToF por gating

// --- CONFIG (estática, tuning) ---
struct PoseFusionConfig {
    uint16_t field_width_mm;          // 2430 (clamp de salida en X)
    uint16_t field_height_mm;         // 1820 (clamp de salida en Y)
    uint16_t correction_gain_q8;      // K en Q8: K_real = gain/256. Default 26 (~0.10).
    uint16_t tof_jump_gate_mm;        // gating: si |pose_tof - pose_pred| > esto, ToF rechazada. Default 400.
    uint16_t otos_stale_ms;           // si dt desde último OTOS fresco > esto => sin predicción. Default 60.
    uint16_t tof_stale_ms;            // si pasaron > esto sin corrección ToF, confidence decae. Default 500.
    uint16_t max_step_mm;             // clamp del módulo del delta OTOS por tick (anti-glitch). Default 80.
    uint8_t  conf_tof_anchor;         // confidence objetivo cuando recién se ancló a ToF (0-100). Default 90.
    uint8_t  conf_otos_only;          // confidence base cuando solo hay OTOS sin ToF reciente. Default 50.
    uint8_t  conf_decay_per_100ms;    // cuántos puntos/100ms baja la confidence en deriva pura. Default 5.
    uint8_t  conf_min;                // piso de confidence mientras el estado siga inicializado. Default 10.
};

// Helper con los defaults documentados arriba.
PoseFusionConfig pose_fusion_default_config();

// --- ESTADO (persiste entre ticks; lo dueña el runtime, NO globals en el módulo) ---
struct PoseFusionState {
    int32_t  x_mm_q0;                 // pose fusionada X en mm (int32: acumula sin overflow intermedio)
    int32_t  y_mm_q0;                 // pose fusionada Y en mm
    int16_t  heading_centideg;        // pass-through del BNO (NO se fusiona)
    uint8_t  confidence;              // 0-100 confianza combinada de salida
    bool     initialized;             // false hasta el primer anclaje válido (ToF u OTOS+seed)
    int16_t  otos_prev_x_mm;          // último x OTOS consumido (para calcular delta)
    int16_t  otos_prev_y_mm;          // último y OTOS consumido
    bool     otos_prev_valid;         // ya hay un sample OTOS previo del cual sacar delta
    uint32_t ms_since_tof_corr;       // ms acumulados desde la última corrección ToF aceptada
};

// Pone todo en 0 / no inicializado.
void pose_fusion_reset(PoseFusionState& st);

// --- ENTRADAS del tick (todo por parámetro; gating de freshness lo hace el caller) ---
struct PoseFusionInputs {
    // Fuente absoluta (ToF + IMU): la pose de localization_compute().
    int16_t tof_x_mm;
    int16_t tof_y_mm;
    bool    tof_valid;                // = LocalizationPose.valid (gateado por el caller)
    // Fuente odometría (OTOS de DOWN): pose ABSOLUTA acumulada (no delta).
    // El módulo calcula el delta internamente vs otos_prev_*.
    int16_t otos_x_mm;
    int16_t otos_y_mm;
    bool    otos_fresh;               // = comm_down_is_pose_fresh() (gateado por el caller)
    // Heading del BNO (pass-through; nunca se fusiona acá).
    int16_t bno_heading_centideg;
    // dt del tick en ms (del scheduler del runtime; típico 10).
    uint16_t dt_ms;
};

// --- SALIDA ---
struct PoseFusionOutput {
    int16_t x_mm;                     // pose fusionada X, clamp [0, field_width_mm]
    int16_t y_mm;                     // pose fusionada Y, clamp [0, field_height_mm]
    int16_t heading_centideg;         // == inputs.bno_heading_centideg (pass-through)
    uint8_t confidence;               // 0-100
    bool    valid;                    // true si initialized (hay algo que reportar)
    uint8_t source_flags;             // bit0=usó pred OTOS, bit1=aceptó corr ToF, bit2=rechazó ToF
};

// --- API PRINCIPAL (pura; muta st, devuelve output) ---
//  - st se MUTA (estado del filtro). cfg es const. Sin side effects fuera de st.
//  - Si !initialized y llega tof_valid -> seed: pose=pose_tof, conf=conf_tof_anchor, initialized=true.
//  - Si !initialized y NO hay tof_valid pero sí otos_fresh -> NO inicializa (la OTOS sola no da
//    absoluto; se guarda otos_prev_* para tener delta listo, pero valid=false hasta ver ToF).
//  - heading: output.heading = in.bno_heading_centideg SIEMPRE (incluso si !initialized).
PoseFusionOutput pose_fusion_update(PoseFusionState& st,
                                    const PoseFusionInputs& in,
                                    const PoseFusionConfig& cfg);

}  // namespace iitasoccer
