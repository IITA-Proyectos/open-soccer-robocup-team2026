// motor_brake.h — Freno: plugging (reversa) + freno anticipado de la trasera (PURO).
//
// Capa 2b. Porta dos técnicas del histórico 2025 (MOTION-CONTROL-HISTORICO.md):
//   §4 — FRENO POR REVERSA (plugging): meterle reversa breve al motor frena MUCHO
//        más rápido que el coast. 2025: M1=250 / M2=170 asimétrico, 200-300 ms.
//   §3 — FRENO ANTICIPADO DE LA TRASERA: en un strafe lateral, apagar la rueda
//        trasera (M3, paralela al movimiento) unos ms ANTES del fin para que su
//        inercia no desvíe el rumbo. 2025: 60 ms a VEL=100, escalado x VEL/100.
//
// ⚠️ El robot 2026 es MÁS PESADO que el 2025 → más inercia → los PWM/ms base del
// 2025 se incrementan ~10% (x1.1). PERO la magnitud de reversa escalada (275/187)
// EXCEDE el cap de seguridad ~150 (motores brushed 5V a 7,4V se queman > ~70%):
// plug_brake_compute() recibe el cap como PARÁMETRO y lo aplica → la reversa real
// sale capeada a 150 y la ASIMETRÍA 275/187 se PIERDE (ambas quedan en 150). Esto
// es intencional y avisado: la seguridad del motor manda sobre la asimetría.
//
// PURO: sin Arduino/Wire/millis. El tiempo (ms_since_stop) y el estado (último
// comando, velocidad) entran como PARÁMETROS → host-testeable como kinematics.h.
// El binario de competencia es IDÉNTICO: nada de esto se cablea salvo bajo
// -DCENTRAL_PLUG_BRAKE (ver motors_zircon.cpp / motors_plug_brake()).

#pragma once
#include <stdint.h>

namespace iitasoccer {

// ── Constantes 2025 x1.1 (crudas, ANTES del cap) ───────────────────────────
// Reversa por rueda delantera. Asimétrica en 2025 (250/170) para compensar
// diferencias mecánicas entre las dos delanteras. x1.1 por la masa 2026.
constexpr int PLUG_PWM_M1_RAW = 275;   // 250 x1.1  (rueda delantera "fuerte")
constexpr int PLUG_PWM_M2_RAW = 187;   // 170 x1.1  (rueda delantera "floja")
// Ventana de reversa antes de pasar a coast. NO se escala (es tiempo). El robot
// más pesado justifica el extremo alto del rango 2025 (200-300 ms).
constexpr int PLUG_WINDOW_MS  = 250;   // medio del rango 2025 [200..300]

// Freno anticipado de la trasera (lead-time de apagado de M3 en strafe).
constexpr int BRAKE_LEAD_BASE_MS = 66;   // 60 x1.1 a VEL=100 (más masa → antes)
constexpr int BRAKE_LEAD_MAX_MS  = 300;  // clamp superior (igual que 2025)

// ── (1) PLUGGING ───────────────────────────────────────────────────────────
// Entrada: el último PWM signed que llevaba la rueda ANTES del stop (su signo
// define el sentido del movimiento) y los ms transcurridos desde que la FSM
// pidió STOP. El cap (PWM máx de reversa por seguridad) entra como parámetro.
struct PlugBrakeIn {
    int last_wheel_pwm;   // PWM signed que tenía la rueda antes del stop
    int plug_pwm_raw;     // magnitud de reversa cruda para ESTA rueda (PLUG_PWM_M*_RAW)
    int ms_since_stop;    // ms desde que se pidió el STOP (lo pasa el caller, no millis)
};

// Salida: el PWM signed a aplicar a ESTA rueda en este instante.
//   • dentro de la ventana [0, PLUG_WINDOW_MS) y con movimiento previo →
//     REVERSA: signo OPUESTO al last_wheel_pwm, magnitud = min(plug_pwm_raw, cap).
//   • fuera de la ventana, o sin movimiento previo (last==0) → 0 (coast).
//
// window_ms y cap_abs como parámetros (testear suelto / tunear per-robot).
//   cap_abs <= 0 → SIN cap (passthrough de la magnitud cruda) — útil para test.
int plug_brake_compute(const PlugBrakeIn& in, int window_ms, int cap_abs);

// ── (2) FRENO ANTICIPADO DE LA TRASERA ─────────────────────────────────────
// Lead-time (ms) con que hay que apagar la rueda trasera ANTES del fin del
// movimiento. Solo aplica si la rueda va PARALELA al movimiento (M3 en strafe);
// si no, devuelve 0 (no se anticipa nada).
//   lead = is_parallel ? clamp(base_ms * |vel_pct| / 100, 0, max_ms) : 0
//   vel_pct = velocidad como % de la nominal (100 = VEL nominal del strafe).
//
// base_ms y max_ms como parámetros (BRAKE_LEAD_BASE_MS / BRAKE_LEAD_MAX_MS).
int rear_brake_lead_ms(bool is_parallel, int vel_pct, int base_ms, int max_ms);

}  // namespace iitasoccer
