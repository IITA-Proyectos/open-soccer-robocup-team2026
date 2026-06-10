// motor_floor_scale.h — Piso de PWM por ESCALADO UNIFORME + eficiencia por rueda (PURO).
//
// EL PROBLEMA QUE RESUELVE (banco robot2 2026-06-09, arquero):
// El piso por-rueda clásico (apply_pwm_floor: clamp individual) DESTRUYE las
// proporciones entre ruedas a velocidades bajas: todas quedan clavadas en su piso
// y las CORRECCIONES finas (gyro ω, ajustes vy) se "comen" → el arquero patrullando
// PERDÍA COMPLETAMENTE EL FRENTE (cero autoridad de rumbo) o recibía patadas
// bang-bang (una corrección chica saltaba del umbral de ruido al piso 107).
//
// LA SOLUCIÓN (3 pasos, preservando la dirección EXACTA):
//   1. EFICIENCIA POR RUEDA: el PWM crudo se divide por la eficiencia relativa de
//      esa rueda (banco robot2: la TRASERA, alineada al strafe, rinde ~1.31× más
//      velocidad por PWM que las delanteras oblicuas → eff={100,100,131}).
//   2. ESCALADO UNIFORME: si alguna rueda activa queda bajo su piso, se multiplican
//      LAS TRES por el mismo factor k (el máximo déficit) → todas alcanzan su piso
//      y la DIRECCIÓN (incluidas las correcciones chicas) se conserva exacta.
//   3. CERO-O-FIEL: una rueda con componente ínfima (≤ ruido) queda en CERO y no
//      participa del escalado — nunca se la "dispara" al piso (mata el bang-bang).
//      Tope de quemado (~150 PWM) aplicado también por escalado (proporcional).
//
// CONSISTENCIA CON EL BANCO: strafe puro comandado bajo (vx=200) produce
// EXACTAMENTE la mezcla validada {70,70,~107} (test lo ancla) — el mismo strafe
// que Gustavo vio andar derecho. La diferencia es que ahora ω/vy chicos PASAN.
//
// PURO: sin Arduino, enteros + float interno. Host-testeable (run-host-tests.sh).
// Gateado: solo entra al mixer con -DCENTRAL_FLOOR_SCALE (hoy: env del arquero).

#pragma once

namespace iitasoccer {

struct FloorScaleCfg {
    int floor_pwm[3];    // piso por rueda (PWM), ej {70,70,107}
    int noise_thresh;    // |pwm| <= esto → 0 y no participa (ej 5)
    int eff_x100[3];     // eficiencia relativa ×100 por rueda (ej {100,100,131})
    int burn_cap;        // tope térmico |PWM| (ej 150); se respeta por escalado
};

// Transforma in-place los 3 PWM firmados según la política de arriba.
// Garantías: dirección (proporciones) exacta salvo ruedas-cero; toda rueda no-cero
// queda ≥ su piso (o 0 si el cap térmico obligó a bajar y quedó bajo el piso);
// ninguna rueda supera burn_cap; signos conservados.
void motor_floor_scale(int pwm[3], const FloorScaleCfg& cfg);

}  // namespace iitasoccer
