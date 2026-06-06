// motor_power_cap.h — Cap de potencia (duty) de motor (PURO, host-testeable).
//
// Por qué existe (SEGURIDAD E1 — docs/competencia/MEJORAS-PENDIENTES.md, fila E1)
// ------------------------------------------------------------------------------
// Los motores 2026 son **DC con escobillas COMUNES de 5 V** alimentados a **7,4 V
// (LiPo 2S)**. Si el duty supera ~70 % de la escala de PWM, el bobinado recibe más
// tensión media de la que aguanta de forma sostenida y **se quema**. Hoy el firmware
// limita la VELOCIDAD (MAX_SPEED_MM_S + saturate_wheels en kinematics.h), pero eso
// capea velocidad de RUEDA, no el % de duty que termina en analogWrite(): un comando
// agresivo, un gain mal tuneado, o un wheel_speed_to_pwm que ya devuelve cerca de
// MAX_PWM pueden mandar 100 % de duty y freír el motor. Este módulo aporta el cap
// DURO de duty, independiente de la cadena de velocidad, como red de seguridad.
//
// Qué hace
// --------
//   • motor_power_cap(duty, cap_abs): satura |duty| a cap_abs CONSERVANDO el signo.
//     - cap_abs <= 0      → PASSTHROUGH (no-op): no hay cap configurado.
//     - cap_abs >= MAX    → PASSTHROUGH (no-op): el cap es >= la escala, no recorta.
//       (MAX = MOTOR_POWER_CAP_MAX = 255, el rango de analogWrite de Arduino.)
//     - en otro caso      → clampea: |duty| > cap_abs ⇒ ±cap_abs (signo de duty).
//   • motor_cap_from_pct(max_pwm, pct): traduce un porcentaje a cap absoluto
//     = max_pwm * pct / 100 (p.ej. 255 * 70 / 100 = 178).
//
// Por qué "NaN/0-safe" siendo int: duty es int (no flota, no hay NaN posible), y el
// caso 0 es trivial (sign-preserving sobre 0 = 0). El término del prompt apunta a que
// el helper NO debe asumir entradas "lindas": duty negativo, duty=0, cap degenerado
// (<=0 o >=MAX) y INT_MIN se manejan sin UB ni overflow (ver guarda de INT_MIN abajo).
//
// CÓMO SE CABLEA (card de banco — NO aplicado aquí; ROBOT1 byte-idéntico)
// ----------------------------------------------------------------------
// El cap REAL se aplica en src/central/motors_zircon.cpp, dentro de apply_pwm_to_motor()
// (o en motors_apply_command() justo tras apply_pwm_floor), ANTES del analogWrite y
// DESPUÉS de aplicar MOTOR_INVERT, gateado por un flag + valor per-robot en
// config_central.h. DEFAULT = cap OFF ⇒ passthrough ⇒ binario de competencia IDÉNTICO.
// Este header es PURO/no-cableado: compila y se testea host; no toca pines ni binario
// hasta que el equipo lo cablee y re-flashee en banco con `pio` + OK del equipo.
//
// PURO: sin Arduino/Wire, header-only, sin estado. Solo aritmética entera. g++ host.

#pragma once

namespace iitasoccer {

// Tope de la escala de duty (= rango de analogWrite de Arduino y MAX_PWM en
// config_central.h). Un cap_abs >= este valor NO recorta nada (passthrough).
// Se deja como constante propia del módulo para no acoplar al config de central
// (el header es PURO y se testea suelto); el caller debe pasar el mismo 255.
constexpr int MOTOR_POWER_CAP_MAX = 255;

// Satura |duty| a cap_abs conservando el signo.
//
//   cap_abs <= 0                → duty SIN cambios (cap no configurado / OFF).
//   cap_abs >= MOTOR_POWER_CAP_MAX → duty SIN cambios (cap por encima de la escala).
//   |duty| > cap_abs            → +cap_abs si duty>0, -cap_abs si duty<0.
//   en otro caso (|duty| <= cap_abs, incl. duty==0) → duty intacto.
//
// Sign-preserving sin depender de unario - sobre INT_MIN (UB): comparamos por valor.
inline int motor_power_cap(int duty, int cap_abs) {
    // Cap no configurado o por encima de la escala completa → no-op passthrough.
    if (cap_abs <= 0 || cap_abs >= MOTOR_POWER_CAP_MAX) {
        return duty;
    }
    // Recorte sign-preserving. Comparamos contra ±cap_abs en vez de calcular
    // abs(duty) para evitar el UB de -INT_MIN (abs(INT_MIN) desborda).
    if (duty > cap_abs) {
        return cap_abs;
    }
    if (duty < -cap_abs) {
        return -cap_abs;
    }
    return duty;
}

// Traduce un porcentaje [0..100] a cap absoluto sobre la escala max_pwm.
//   motor_cap_from_pct(255, 70) == 178   (255*70/100, trunc entero)
//   motor_cap_from_pct(255, 0)  == 0     → motor_power_cap lo trata como OFF (passthrough)
//   motor_cap_from_pct(255, 100)== 255   → == MAX → motor_power_cap = passthrough
// pct fuera de [0..100] se clampea a ese rango (defensa: un pct=200 NO debe dar un
// cap > escala que "amplifique", ni un pct negativo dar un cap negativo raro).
inline int motor_cap_from_pct(int max_pwm, int pct) {
    if (pct <= 0) {
        return 0;
    }
    if (pct >= 100) {
        return max_pwm;
    }
    return max_pwm * pct / 100;
}

}  // namespace iitasoccer
