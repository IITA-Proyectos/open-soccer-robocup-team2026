// blackbox.cpp — implementación de la caja negra de la CENTRAL (ver blackbox.h).
#ifdef CENTRAL_BLACKBOX

#include "blackbox.h"

#include <Arduino.h>

#include "config_central.h"   // MOTOR_INVERT / MOTOR_MIN_PWM para la metadata del dump
#include "motors_zircon.h"
#include "strategy.h"
#include "world_model.h"

namespace iitasoccer {

namespace {

// ~50 Hz × 90 s = 4500 registros × 36 B ≈ 158 KB en RAM2 (DMAMEM). El loop y las
// variables calientes viven en RAM1 → cero competencia. Overridable por -D.
#ifndef CENTRAL_BLACKBOX_CAP
#define CENTRAL_BLACKBOX_CAP 4500
#endif
constexpr uint32_t BB_PERIOD_MS = 20;   // ~50 Hz de muestreo

struct __attribute__((packed)) BbRec {
    uint32_t    t_ms;       // millis() del sample
    const char* state;      // puntero a literal de strategy (viven para siempre)
    uint8_t     flags;      // bit0 match · 1 ball_vis · 2 goal_vis · 3 heading_valid
                            // bit4 line_det · 5 imminent · 6 snap_fresh
    int16_t     hdg_cd;     // heading en centi-grados
    int16_t     ball_x, ball_y;       // mm (frame robot)
    int16_t     ball_vx, ball_vy;     // mm/s (predicción del snapshot)
    int16_t     goal_cd;    // ángulo arco rival en centi-grados (válido si goal_vis)
    int16_t     vx, vy;     // comando mm/s
    int16_t     w_cds;      // comando omega centi-grados/s
    int16_t     pwm[3];     // PWM real aplicado por motor (signed, post-pisos/kick)
    int16_t     line_cd;    // ángulo de línea centi-grados (válido si line_det)
    uint16_t    min_obst;   // mm (ToF del TOP vía snapshot)
};

DMAMEM BbRec g_buf[CENTRAL_BLACKBOX_CAP];
uint32_t g_head  = 0;      // próxima posición a escribir
uint32_t g_count = 0;      // cuántos válidos (satura en CAP)
uint32_t g_last_sample_ms = 0;
bool     g_was_running    = false;

inline int16_t f_to_cd(float deg)  { return static_cast<int16_t>(deg * 100.0f); }
inline int16_t f_to_i16(float v)   { return static_cast<int16_t>(v); }

}  // namespace

namespace { void bb_capture(const MotorCommand& cmd, bool emergency); }

void blackbox_tick(const MotorCommand& cmd) { bb_capture(cmd, false); }

void blackbox_tick_emergency() {
    MotorCommand zero{};
    bb_capture(zero, true);
}

namespace {
void bb_capture(const MotorCommand& cmd, bool emergency) {
    const uint32_t now = millis();

    // Auto-volcado en el flanco RUN→STOP: la corrida terminó (juez/'s'/timeout)
    // → escupir el CSV ya mismo, con el robot quieto. Si fue un STOP accidental,
    // no pasa nada: la grabación sigue y un volcado extra es inocuo.
    const bool running = world_model_match_running();
    if (g_was_running && !running) {
        blackbox_dump();
    }
    g_was_running = running;

    if (now - g_last_sample_ms < BB_PERIOD_MS) return;
    g_last_sample_ms = now;

    BbRec& r = g_buf[g_head];
    r.t_ms  = now;
    r.state = strategy_get_state_name();
    r.flags = 0;
    if (running)                            r.flags |= 1u << 0;
    if (world_model_ball_visible())         r.flags |= 1u << 1;
    if (world_model_goal_opp_visible())     r.flags |= 1u << 2;
    if (world_model_heading_valid())        r.flags |= 1u << 3;
    if (world_model_line_detected())        r.flags |= 1u << 4;
    if (world_model_imminent_exit())        r.flags |= 1u << 5;
    if (world_model_snapshot_is_fresh())    r.flags |= 1u << 6;
    if (emergency)                          r.flags |= 1u << 7;  // freno de borde de main
    r.hdg_cd   = f_to_cd(world_model_get_my_heading_deg());
    r.ball_x   = f_to_i16(world_model_get_ball_x_mm());
    r.ball_y   = f_to_i16(world_model_get_ball_y_mm());
    r.ball_vx  = world_model_get_ball_vx_mm_s();
    r.ball_vy  = world_model_get_ball_vy_mm_s();
    r.goal_cd  = f_to_cd(world_model_get_goal_opp_angle_deg());
    r.vx       = cmd.vx_mm_s;
    r.vy       = cmd.vy_mm_s;
    r.w_cds    = cmd.omega_centideg_s;
    r.pwm[0]   = motors_get_applied_pwm(0);
    r.pwm[1]   = motors_get_applied_pwm(1);
    r.pwm[2]   = motors_get_applied_pwm(2);
    r.line_cd  = f_to_cd(world_model_get_line_angle_deg());
    r.min_obst = world_model_get_min_obstacle_mm();

    g_head = (g_head + 1) % CENTRAL_BLACKBOX_CAP;
    if (g_count < CENTRAL_BLACKBOX_CAP) ++g_count;
}
}  // namespace (bb_capture)

void blackbox_dump() {
    Serial.println();
    Serial.print(F("=== BLACKBOX BEGIN v1.1 n="));
    Serial.print(g_count);
    Serial.println(F(" ==="));
    // METADATA de corrida (v1.1, auditoría 2026-06-11): el hardware cambia cada
    // noche de banco — sin saber QUÉ build/robot/config generó el CSV, el análisis
    // posterior es in-interpretable. Línea '#' = la saltean los parsers de CSV.
    Serial.print(F("# build="));
    Serial.print(F(__DATE__ " " __TIME__));
#if defined(ROBOT1)
    Serial.print(F(" robot=R1"));
#elif defined(ROBOT2)
    Serial.print(F(" robot=R2"));
#endif
    Serial.print(F(" rol="));
    Serial.print(strategy_get_role() == RobotRole::ATTACKER ? F("ATK") : F("GK"));
    Serial.print(F(" invert="));
    Serial.print(MOTOR_INVERT[0]); Serial.print('/');
    Serial.print(MOTOR_INVERT[1]); Serial.print('/');
    Serial.print(MOTOR_INVERT[2]);
    Serial.print(F(" pisos="));
    Serial.print(MOTOR_MIN_PWM[0]); Serial.print('/');
    Serial.print(MOTOR_MIN_PWM[1]); Serial.print('/');
    Serial.print(MOTOR_MIN_PWM[2]);
    Serial.print(F(" cap_muestras="));
    Serial.println(CENTRAL_BLACKBOX_CAP);
    Serial.println(F("t_ms,state,match,snap,ball_vis,goal_vis,hdg_valid,line,imminent,"
                     "hdg_deg,ball_x,ball_y,ball_vx,ball_vy,goal_deg,"
                     "cmd_vx,cmd_vy,cmd_w_dps,pwm1,pwm2,pwm3,line_deg,min_obst,emerg"));
    // Orden cronológico: si el ring dio la vuelta, lo más viejo está en g_head.
    const uint32_t start = (g_count == CENTRAL_BLACKBOX_CAP) ? g_head : 0;
    for (uint32_t k = 0; k < g_count; ++k) {
        const BbRec& r = g_buf[(start + k) % CENTRAL_BLACKBOX_CAP];
        Serial.print(r.t_ms);                       Serial.print(',');
        Serial.print(r.state);                      Serial.print(',');
        Serial.print((r.flags >> 0) & 1);           Serial.print(',');
        Serial.print((r.flags >> 6) & 1);           Serial.print(',');
        Serial.print((r.flags >> 1) & 1);           Serial.print(',');
        Serial.print((r.flags >> 2) & 1);           Serial.print(',');
        Serial.print((r.flags >> 3) & 1);           Serial.print(',');
        Serial.print((r.flags >> 4) & 1);           Serial.print(',');
        Serial.print((r.flags >> 5) & 1);           Serial.print(',');
        Serial.print(r.hdg_cd / 100.0f, 1);         Serial.print(',');
        Serial.print(r.ball_x);                     Serial.print(',');
        Serial.print(r.ball_y);                     Serial.print(',');
        Serial.print(r.ball_vx);                    Serial.print(',');
        Serial.print(r.ball_vy);                    Serial.print(',');
        Serial.print(r.goal_cd / 100.0f, 1);        Serial.print(',');
        Serial.print(r.vx);                         Serial.print(',');
        Serial.print(r.vy);                         Serial.print(',');
        Serial.print(r.w_cds / 100);                Serial.print(',');
        Serial.print(r.pwm[0]);                     Serial.print(',');
        Serial.print(r.pwm[1]);                     Serial.print(',');
        Serial.print(r.pwm[2]);                     Serial.print(',');
        Serial.print(r.line_cd / 100.0f, 1);        Serial.print(',');
        Serial.print(r.min_obst);                   Serial.print(',');
        Serial.println((r.flags >> 7) & 1);         // emerg (freno de borde)
    }
    Serial.println(F("=== BLACKBOX END ==="));
}

void blackbox_reset() {
    g_head = 0;
    g_count = 0;
    Serial.println(F("[BLACKBOX] historia borrada, grabando de cero."));
}

}  // namespace iitasoccer

#endif  // CENTRAL_BLACKBOX
