// ⚠️ SNAPSHOT CONGELADO (2026-05-24) — NO ES BUILD PATH NI FIRMWARE VIVO. NO FLASHEAR.
// Este pack es un retrato historico; usa contrato VIEJO (WorldSnapshot v2/27B, kicker_fire,
// UART TOP<->CENTRAL intercambiado). El firmware VIVO esta en software/teensy/Soccer 2026/src/
// (WorldSnapshot v3/31B, camara v2/11B, sin kicker, arbitro GPIO 5/6). Ver docs/FUENTES-DE-VERDAD.md.
#include "pids.h"

namespace iitasoccer {

// ============================================================================
// Utility
// ============================================================================

float wrap_diff_deg(float a, float b) {
    float d = a - b;
    while (d > 180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

namespace {
    float clamp(float v, float lo, float hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }
}

// ============================================================================
// Heading PID
// ============================================================================

void heading_pid_reset(HeadingPID& pid) {
    pid.integral = 0.0f;
    pid.prev_error = 0.0f;
    pid.primed = false;
}

void heading_pid_set_target(HeadingPID& pid, float target_deg) {
    pid.setpoint_deg = target_deg;
}

float heading_pid_tick(HeadingPID& pid, float current_heading_deg, uint32_t now_ms) {
    // Error con wrap-around.
    float error = wrap_diff_deg(pid.setpoint_deg, current_heading_deg);

    // dt — usar fallback de 10 ms (100 Hz típico) si no hay tick previo.
    float dt = 0.01f;
    if (pid.primed) {
        const uint32_t elapsed = now_ms - pid.last_tick_ms;
        dt = elapsed / 1000.0f;
        if (dt < 0.001f) dt = 0.001f;  // mínimo 1 ms (evitar div by 0)
        if (dt > 0.1f)   dt = 0.1f;    // máximo 100 ms (clamp ante hang)
    }
    pid.last_tick_ms = now_ms;

    // Integral con anti-windup.
    pid.integral += error * dt;
    pid.integral = clamp(pid.integral, -pid.integral_clamp, +pid.integral_clamp);

    // Derivada (solo si ya tenemos prev_error válido).
    float derivative = 0.0f;
    if (pid.primed) {
        derivative = (error - pid.prev_error) / dt;
    }
    pid.prev_error = error;
    pid.primed = true;

    // Output.
    float output = pid.kp * error + pid.ki * pid.integral + pid.kd * derivative;
    output = clamp(output, -pid.output_clamp, +pid.output_clamp);
    return output;
}

// ============================================================================
// Lateral PID
// ============================================================================

void lateral_pid_reset(LateralPID& pid) {
    pid.integral = 0.0f;
    pid.prev_error = 0.0f;
    pid.primed = false;
}

void lateral_pid_set_target(LateralPID& pid, float target) {
    pid.setpoint = target;
}

float lateral_pid_tick(LateralPID& pid, float measurement, uint32_t now_ms) {
    float error = pid.setpoint - measurement;

    float dt = 0.01f;
    if (pid.primed) {
        const uint32_t elapsed = now_ms - pid.last_tick_ms;
        dt = elapsed / 1000.0f;
        if (dt < 0.001f) dt = 0.001f;
        if (dt > 0.1f)   dt = 0.1f;
    }
    pid.last_tick_ms = now_ms;

    pid.integral += error * dt;
    pid.integral = clamp(pid.integral, -pid.integral_clamp, +pid.integral_clamp);

    float derivative = 0.0f;
    if (pid.primed) {
        derivative = (error - pid.prev_error) / dt;
    }
    pid.prev_error = error;
    pid.primed = true;

    float output = pid.kp * error + pid.ki * pid.integral + pid.kd * derivative;
    output = clamp(output, -pid.output_clamp, +pid.output_clamp);
    return output;
}

// ============================================================================
// Approach velocity profile
// ============================================================================

float approach_velocity(float distance_mm,
                         float close_threshold_mm,
                         float far_threshold_mm,
                         float max_speed_mm_s,
                         float min_speed_mm_s) {
    if (distance_mm < close_threshold_mm) return 0.0f;
    if (distance_mm >= far_threshold_mm) return max_speed_mm_s;
    // Interpolación lineal entre close y far.
    float range = far_threshold_mm - close_threshold_mm;
    if (range <= 0.0f) return max_speed_mm_s;
    float t = (distance_mm - close_threshold_mm) / range;  // 0..1
    return min_speed_mm_s + t * (max_speed_mm_s - min_speed_mm_s);
}

}  // namespace iitasoccer
