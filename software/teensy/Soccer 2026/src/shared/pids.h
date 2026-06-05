// pids.h — Controladores PID para la placa CENTRAL.
//
// Funciones PURAS (sin Arduino). Compilables como native para testear en host.
// La placa CENTRAL las usa desde strategy.cpp después de leer el world_model.
//
// Tres PIDs disponibles:
//   • HeadingPID  — corrige orientación del robot (input: heading actual; output: ω).
//   • LateralPID  — genérico para movimiento lateral (arquero pisando línea).
//   • approach_velocity — perfil suave de velocidad de aproximación.
//
// Convención de unidades:
//   • Heading en grados [-180, +180]. Output en grados/s (multiplicar × 100 para
//     enviar como centideg/s en el MotorCommand).
//   • LateralPID es genérico — units depende del caller.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// ============================================================================
// Clamp defensivo float -> int16 para velocidades (mm/s)
// ============================================================================
//
// Espejo de omega_degps_to_centideg pero para velocidades lineales: convierte
// un float a int16_t SATURANDO al rango, sin envolver ni cambiar de signo, y
// con guarda NaN (NaN -> 0). Defensivo: aunque el overflow de GK_INTERCEPT se
// refutó (el wire ya acota ball_x), clampear antes del cast float->int16 es
// seguro y barato — un cast directo de un valor fuera de rango es UB y podría
// invertir el signo del comando del motor (mismo modo de falla que el bug de
// omega del 2026-06-03). En el rango normal (|v| <= 32767) es BYTE-IDÉNTICO a
// (int16_t)v, así que no cambia el binario de competencia salvo en el path de
// falla.
//
// Author: Claude Opus 4.8 (Anthropic). Requested-by: Viollaz.
inline int16_t clamp_velocity_mm_s(float v) {
    if (v != v) return 0;            // NaN -> 0 (cast de NaN a int es UB)
    if (v > 32767.0f) return 32767;
    if (v < -32767.0f) return -32767;
    return static_cast<int16_t>(v);
}

// ============================================================================
// Heading PID
// ============================================================================

struct HeadingPID {
    // Ganancias (default basadas en test-4-movimientos.ino del staging IITA).
    float kp = 3.0f;
    float ki = 0.05f;
    float kd = 0.5f;

    // Setpoint en grados [-180, +180].
    float setpoint_deg = 0.0f;

    // Estado interno.
    float integral = 0.0f;
    float prev_error = 0.0f;
    uint32_t last_tick_ms = 0;
    bool primed = false;  // false = primer tick (no calcular derivada con prev_error=0)

    // Clamps (anti-windup + saturación).
    float integral_clamp = 50.0f;
    float output_clamp = 327.0f;  // grados/s. ⚠️ ≤327 A PROPÓSITO: cmd.omega_centideg_s = omega*100
                                  // es int16 (±32767); con 360 (=36000) hacía OVERFLOW y el giro se
                                  // invertía a fondo al saturar el PID (bug CRÍTICO, eval 2026-06-03).
};

// Resetea el estado interno (integral y prev_error). Usar en transiciones de FSM
// para evitar arrastrar windup del estado anterior.
void heading_pid_reset(HeadingPID& pid);

// Cambia el setpoint (no resetea el integral).
void heading_pid_set_target(HeadingPID& pid, float target_deg);

// Computa el output del PID. Retorna grados/s (signed).
//   current_heading_deg: heading actual del robot.
//   now_ms: tiempo actual en ms (millis() en hardware, sintético en tests).
float heading_pid_tick(HeadingPID& pid, float current_heading_deg, uint32_t now_ms);

// Utility: diferencia de ángulos con wrap-around a [-180, +180].
float wrap_diff_deg(float a, float b);

// Convierte ω en grados/s -> centideg/s (×100) para enviar como
// MotorCommand.omega_centideg_s (int16). SATURA a [-32767, +32767]: nunca
// envuelve ni cambia de signo. Evita el bug donde 360 deg/s -> 36000 hacía
// overflow del int16 y el robot giraba al revés a casi máxima velocidad.
//
// Author: Claude Opus 4.8 (Anthropic). Requested-by: Viollaz.
int16_t omega_degps_to_centideg(float omega_deg_s);

// ============================================================================
// Lateral PID (genérico — se usa para arquero pisando línea)
// ============================================================================

struct LateralPID {
    float kp = 50.0f;
    float ki = 5.0f;
    float kd = 10.0f;

    float setpoint = 1.0f;   // depth target (sensores en blanco esperados)

    float integral = 0.0f;
    float prev_error = 0.0f;
    uint32_t last_tick_ms = 0;
    bool primed = false;

    float integral_clamp = 20.0f;
    float output_clamp = 800.0f;  // mm/s lateral max
};

void lateral_pid_reset(LateralPID& pid);
void lateral_pid_set_target(LateralPID& pid, float target);

// Computa el output del PID lateral. Retorna velocidad lateral (mm/s) — signo
// indica dirección (+ = hacia adentro de la cancha, - = hacia la línea).
float lateral_pid_tick(LateralPID& pid, float measurement, uint32_t now_ms);

// ============================================================================
// Perfil de velocidad de aproximación (no es PID, es perfil simple)
// ============================================================================

// Mapeo de distancia → velocidad:
//   distance < close_threshold      → 0 (ya llegamos)
//   close_threshold ≤ d < far_thresh → interpolación lineal entre min_speed y max_speed
//   distance ≥ far_threshold        → max_speed
//
// Útil para aproximación a la pelota: rápido lejos, lento cerca para no patear sin alinear.
float approach_velocity(float distance_mm,
                         float close_threshold_mm,
                         float far_threshold_mm,
                         float max_speed_mm_s,
                         float min_speed_mm_s);

}  // namespace iitasoccer
