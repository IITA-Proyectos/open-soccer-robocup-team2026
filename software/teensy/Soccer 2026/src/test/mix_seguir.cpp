// mix_seguir.cpp — SEGUIR la pelota en 2 FASES:
//   FASE 1 (lejos): PERSEGUIR mirando DE FRENTE a la pelota, serpenteando (apunta sobre la marcha).
//   FASE 2 (cerca, recién a ~20 cm): ACOMODARSE DETRÁS de la pelota mirando al ARCO, y quedarse
//          QUIETO (LISTO), pronto para empujar. Si la pelota se aleja mucho, vuelve a perseguir.
//   NUNCA empuja la pelota (esto es seguir + posicionarse) → imposible hacer gol en contra.
//
// Necesita: ángulo/dist de la pelota + ángulo del arco (cámara) + heading del BNO. Sin ToF.
// ⚠️ NO TESTEADO EN HARDWARE.

#include "mix_seguir.h"
#include "mix_io.h"
#include "mix_motors.h"
#include "mix_config.h"
#include "Arduino.h"
#include <math.h>

namespace iitasoccer {
namespace mix {

// ================== PERILLAS (tunear en banco) ==================
static const int   SPEED          = 230;   // PWM de traslación. Rápido.
// --- FASE 1: perseguir mirando a la pelota (serpenteo) ---
static const float FACE_BALL_KP   = 2.6f;  // giro por grado de ángulo de pelota (apunta a la pelota). SIGNO=banco.
static const float REACH_CM       = 20.0f; // a ESTA distancia de la pelota (centro a centro) pasa a acomodarse detrás.
static const float RECHASE_CM     = 45.0f; // si la pelota se aleja MÁS que esto, vuelve a perseguirla de frente.
// --- FASE 2: acomodarse detrás, mirando al arco ---
static const float BEHIND_OFFSET_CM = 22.0f; // qué tan atrás de la pelota se ubica (centro a centro; ~11 cm de gap).
static const float BEHIND_TOL_CM    = 9.0f;  // a menos que esto del punto-detrás = "ya estoy en posición".
static const float RESUME_TOL_CM    = 16.0f; // si el punto-detrás se aleja más que esto → reacomodar.
static const float FACE_TOL_DEG     = 20.0f; // |ángulo al arco| menor que esto = "mirando al arco".
// --- FASE 3: ESCOLTAR la pelota hasta el arco (empuje continuo, acompañándola) ---
static const int   ESCOLTA_SPEED    = 205;   // PWM del empuje-escolta (controlado, no un patadón).
static const float ALIGN_DEG        = 24.0f; // robot-pelota-arco en LÍNEA dentro de esto → puede escoltar.
static const float ESCOLTA_DESVIO   = 46.0f; // si la pelota se sale del eje al arco más que esto → reacomodar.
// Mantener la pelota CENTRADA adelante mientras empuja (si no, la "escupe" por un costado). El robot
// va hacia el arco PERO se desplaza de costado proporcional a cuánto está descentrada la pelota.
static const float CENTER_KP        = 0.020f; // cuánto corrige de costado por GRADO de descentrado de la pelota.
static const float CENTER_MAX       = 1.20f;  // tope de la corrección lateral (relativo al empuje=1).
static const float KEEPOUT_CM       = 30.0f; // dentro de este radio, orbita la pelota (no la pisa) si hace falta.
static const float OWNGOAL_GUARD_CM = 24.0f; // si está más cerca que esto y tocar la pelota la empuja al arco propio, se aleja.
static const float SLOW_CM          = 32.0f; // cerca del punto-detrás, desacelera (llegada/freno suaves).
static const float SLOW_FLOOR       = 0.30f;
static const float FACE_GOAL_KP   = 2.0f;  // giro para encarar el arco (fase 2). SIGNO=banco.
static const int   OMEGA_MAX      = 90;
static const int   BUSCAR_OMEGA   = 95;
// --- Predictivo (seguir la pelota en movimiento) ---
static const float LEAD_S         = 0.20f;
static const float VEL_MIN_CM_S    = 8.0f;
static const float LEAD_MAX_CM     = 35.0f;
static const unsigned long BALL_LOST_MS = 1500;
// --- FASE 0: pelota ATRÁS (cámara trasera) → girar 180° en el lugar a encararla (estilo centralmix) ---
static const float ATRAS_ENTRA_DEG  = 110.0f; // SOLO entra al giro si la pelota la ve la cámara TRASERA
                                              //   (|ángulo| > esto). La cámara delantera (±50°) NUNCA lo
                                              //   dispara → con la pelota adelante hace las 2 fases de siempre.
static const float ATRAS_SALIDA_DEG = 30.0f;  // suelta el giro cuando la pelota entra al cuadro frontal.
// (usa MIX_ATRAS_PWM, MIX_ATRAS_DIR_SIGN de mix_config.h)
// ===============================================================

enum class Estado { GIRAR_ATRAS, PERSEGUIR, ACOMODAR, ESCOLTAR, BUSCAR, ESPERA };
static Estado s_estado = Estado::PERSEGUIR;
static float s_last_ball_ang = 1.0f, s_last_go = 0.0f;
static bool  s_ever_seen = false, s_face_goal = false;   // s_face_goal: ¿el giro mira al arco (fase 2) o a la pelota (fase 1)?
static int   s_giro_atras_dir = 0;     // latch del giro-encare "pelota atrás". 0=inactivo.
static bool  s_atras_handled = false;  // ya se giró a encarar esta pelota-atrás → no re-girar en el ataque.
static bool  s_goal_have = false;
static float s_goal_heading_abs = 0.0f, s_goal_dist_cm = 150.0f;
static float s_aim = 0.0f, s_dist = 0.0f, s_dbg_goal = 0.0f;

static inline float clampf(float v,float lo,float hi){ return v<lo?lo:(v>hi?hi:v); }
static inline float wrap180(float d){ while(d>180.f)d-=360.f; while(d<-180.f)d+=360.f; return d; }

void mix_seguir_init(){
    s_estado = Estado::PERSEGUIR; s_last_ball_ang = 1.0f; s_last_go = 0.0f; s_ever_seen = false; s_face_goal = false;
    s_giro_atras_dir = 0; s_atras_handled = false;
    s_goal_have = false; s_goal_heading_abs = 0.0f; s_goal_dist_cm = 150.0f; s_aim = 0.0f; s_dist = 0.0f;
}

static float goal_angle_robot(){
    // CONVENCIÓN: heading_deg = yaw (+=CCW); goal_opp_angle = rumbo a la pelota/arco en marco robot
    // (+=DERECHA = CW). El rumbo ABSOLUTO al arco (mundo) = heading - goal_opp_angle. Y el ángulo al
    // arco en marco robot a un heading nuevo = heading_now - rumbo_absoluto. (El signo importa cuando
    // el robot gira MUCHO sin ver el arco —p. ej. el giro de 180° de "pelota atrás"—; con el signo
    // viejo (+) la estimación divergía y el robot encaraba para el lado equivocado.)
    if (g_io.goal_opp_visible){
        s_goal_have = true; s_goal_heading_abs = g_io.heading_deg - g_io.goal_opp_angle;
        if (g_io.goal_opp_dist > 1.0f) s_goal_dist_cm = g_io.goal_opp_dist / 10.0f;
        return g_io.goal_opp_angle;
    }
    if (s_goal_have) return wrap180(g_io.heading_deg - s_goal_heading_abs);
    return wrap180(g_io.heading_error_deg);   // nunca visto: arco hacia el rumbo inicial
}

void mix_seguir_tick(){
    const unsigned long now = millis();
    if (!g_io.match_running){ s_estado = Estado::ESPERA; parar(); return; }

    const float goal_ang = goal_angle_robot();
    s_dbg_goal = goal_ang;
    if (g_io.ball_visible){ s_last_ball_ang = g_io.angulo_pelota_deg; s_ever_seen = true; }

    // ===================== FASE 0: PELOTA ATRÁS (cámara trasera) → GIRAR 180° EN EL LUGAR =====================
    // PRIORIDAD MÁXIMA (estilo centralmix). Si la pelota está DETRÁS (la ve la cámara trasera), gira EN EL
    // LUGAR (giro PURO = no traslada, no la toca) hasta encararla, y recién ahí arranca el seguir normal.
    // Se ENTRA por ball_y_cm (monótona), se LATCHEA el sentido una vez (anti-±180), y se gira a MIX_ATRAS_PWM
    // hasta que la pelota entra al cuadro frontal. 's_atras_handled' evita re-girar durante el acomodo.
    if (!g_io.ball_visible && s_ever_seen && (now - g_io.t_last_ball_seen_ms) > BALL_LOST_MS){
        s_giro_atras_dir = 0; s_atras_handled = false;
    }
    // ENTRA solo si la VE LA CÁMARA TRASERA (|ángulo| grande). Delante (±50°) → no entra → 2 fases normales.
    const bool entrar_atras = g_io.ball_visible && (fabsf(g_io.angulo_pelota_deg) > ATRAS_ENTRA_DEG) && !s_atras_handled;
    if (s_giro_atras_dir != 0 || entrar_atras){
        if (g_io.ball_visible && fabsf(g_io.angulo_pelota_deg) < ATRAS_SALIDA_DEG){
            s_giro_atras_dir = 0; s_atras_handled = true;       // ya al frente → seguir normal (fase 1)
            s_estado = Estado::PERSEGUIR; s_face_goal = false;
        } else {
            if (s_giro_atras_dir == 0 && g_io.ball_visible)
                s_giro_atras_dir = ((g_io.angulo_pelota_deg > 0.0f) ? +1 : -1) * MIX_ATRAS_DIR_SIGN;
            if (s_giro_atras_dir != 0){
                s_estado = Estado::GIRAR_ATRAS;
                mix_mover_vector(0.0f, 0, s_giro_atras_dir * MIX_ATRAS_PWM);   // giro PURO en el lugar
                s_aim = 0.0f; s_dist = -1.0f;
                return;
            }
        }
    }

    // --- Giro según la FASE: fase 1 mira la PELOTA, fase 2 mira el ARCO (con anti-hueco de cámara). ---
    int omega;
    if (s_face_goal){
        float face_err = goal_ang;
        const float bb = g_io.ball_visible ? g_io.angulo_pelota_deg : s_last_ball_ang;
        const float fb = fabsf(bb), sgn = (bb>=0.0f)?1.0f:-1.0f;
        if (fb < 90.0f){ if (fb > 38.0f) face_err = bb - sgn*30.0f; }
        else           { if (fb < 142.0f) face_err = bb - sgn*150.0f; }
        omega = (int)clampf(face_err * FACE_GOAL_KP, -(float)OMEGA_MAX, (float)OMEGA_MAX);
    } else {
        const float bb = g_io.ball_visible ? g_io.angulo_pelota_deg : s_last_ball_ang;
        omega = (int)clampf(bb * FACE_BALL_KP, -(float)OMEGA_MAX, (float)OMEGA_MAX);  // mira la pelota
    }

    // Pelota no visible:
    //  - FASE 2 (acomodándose, mira el arco): PUENTE → sostener el último movimiento (el rodeo),
    //    porque la pelota cruza el hueco de cámara mientras orbita y reaparece sola por la trasera.
    //  - FASE 1 (mirando la pelota): NO trasladar (eso la empujaba al cruzar el hueco). Girar EN EL
    //    LUGAR hacia la pelota hasta volver a verla → re-apunta sin tocarla.
    const bool fresh = g_io.ball_visible || (s_ever_seen && (now - g_io.t_last_ball_seen_ms < BALL_LOST_MS));
    if (!g_io.ball_visible){
        s_dist = -1.0f;
        if (s_face_goal && fresh){ mix_mover_vector(s_last_go, SPEED, omega); return; }   // FASE 2: puente
        int w = omega;                                                                    // FASE 1: girar a re-apuntar
        if (!fresh){ const int dir = (s_last_ball_ang >= 0.0f) ? +1 : -1; w = dir * BUSCAR_OMEGA;
                     s_estado = Estado::BUSCAR; s_face_goal = false; }
        mix_mover_vector(0.0f, 0, w);
        return;
    }
    if (s_estado == Estado::BUSCAR || s_estado == Estado::ESPERA){
        s_estado = Estado::PERSEGUIR; s_face_goal = false;   // (re)arranca en fase 1: mirar la pelota
    }

    // Pelota actual + predicha.
    const float bx = g_io.ball_x_cm, by = g_io.ball_y_cm;
    s_dist = sqrtf(bx*bx + by*by);
    float tbx = bx, tby = by;
    {
        const float vx = g_io.ball_vx_cm_s, vy = g_io.ball_vy_cm_s, spd = sqrtf(vx*vx+vy*vy);
        if (spd >= VEL_MIN_CM_S){ float lead = LEAD_S; if (spd*lead > LEAD_MAX_CM) lead = LEAD_MAX_CM/spd;
                                  tbx = bx + vx*lead; tby = by + vy*lead; }
    }
    const float D2R=(float)M_PI/180.0f, R2D=180.0f/(float)M_PI;
    const float ang_ball = atan2f(tbx, tby) * R2D;

    // ===================== TRANSICIONES DE FASE =====================
    if (s_estado == Estado::PERSEGUIR && s_dist <= REACH_CM){
        s_estado = Estado::ACOMODAR; s_face_goal = true;     // llegó a ~20 cm → acomodarse detrás
    }
    if ((s_estado == Estado::ACOMODAR || s_estado == Estado::ESCOLTAR) && s_dist > RECHASE_CM){
        s_estado = Estado::PERSEGUIR; s_face_goal = false; s_atras_handled = false;   // se fue lejos → nuevo episodio
    }

    // ===================== FASE 1: PERSEGUIR (mira la pelota, serpentea) =====================
    if (s_estado == Estado::PERSEGUIR){
        s_aim = ang_ball;
        mix_mover_vector(ang_ball, SPEED, omega);   // traslada hacia la pelota + gira a mirarla
        s_last_go = ang_ball;
        return;
    }

    // ===================== FASE 2: ACOMODAR / LISTO (mira el arco, queda detrás) =====================
    const float Gx = s_goal_dist_cm*sinf(goal_ang*D2R), Gy = s_goal_dist_cm*cosf(goal_ang*D2R);
    float ugx = Gx-tbx, ugy = Gy-tby; float ugn = sqrtf(ugx*ugx+ugy*ugy); if (ugn<1e-3f) ugn=1e-3f;
    ugx/=ugn; ugy/=ugn;
    const float ang_u = atan2f(ugx, ugy) * R2D;            // dirección pelota→arco (a lo largo de la que escolta)
    const float px = tbx - ugx*BEHIND_OFFSET_CM, py = tby - ugy*BEHIND_OFFSET_CM;
    const float ang_P = atan2f(px, py) * R2D;
    const float dist_P = sqrtf(px*px + py*py);
    const bool facing_goal = fabsf(goal_ang) < FACE_TOL_DEG;
    const bool lined = fabsf(wrap180(ang_ball - ang_u)) < ALIGN_DEG;   // robot-pelota-arco en línea

    // ===================== FASE 3: ESCOLTAR — empujar la pelota hasta el arco =====================
    // Empuje CONTINUO a lo largo de la línea pelota→arco (la "escolta"): la acompaña, re-alineándose,
    // hasta el fondo. NO es un patadón: avanza con la pelota. Si se sale del eje, se reacomoda detrás.
    if (s_estado == Estado::ESCOLTAR){
        s_atras_handled = false;
        if (fabsf(wrap180(ang_ball - ang_u)) > ESCOLTA_DESVIO){
            s_estado = Estado::ACOMODAR;        // la pelota se desvió → volver a ponerse detrás
        } else {
            // Empuje hacia el arco + corrección lateral SUAVE para mantener la pelota CENTRADA adelante
            // (si no, la "escupe" por un costado). El empuje (hacia el arco) PESA mucho más que el strafe.
            const float gdx = sinf(goal_ang*D2R), gdy = cosf(goal_ang*D2R);   // dir de empuje (al arco)
            const float lat = clampf(g_io.angulo_pelota_deg * CENTER_KP, -CENTER_MAX, CENTER_MAX);
            const float vx = gdx + lat;          // componente DERECHA (bearing 90 = (1,0))
            const float vy = gdy + 1.0f;         // FRENTE con peso fuerte → el empuje domina al strafe
            const float go = atan2f(vx, vy) * R2D;
            mix_mover_vector(go, ESCOLTA_SPEED, omega);
            s_aim = go; s_last_go = go;
            return;
        }
    }
    // Entrar a ESCOLTAR cuando ya está DETRÁS + MIRANDO al arco + en LÍNEA con la pelota y el arco.
    if (dist_P < BEHIND_TOL_CM && facing_goal && lined){
        s_estado = Estado::ESCOLTAR; s_atras_handled = false;
        mix_mover_vector(ang_u, ESCOLTA_SPEED, omega); s_aim = ang_u; s_last_go = ang_u;
        return;
    }
    // SEGURIDAD: si la pelota está cerca y tocarla la empujaría hacia el arco propio (robot del lado
    // del arco), NO la toques: alejate radialmente y rodeá de nuevo. (Evita empujarla al acomodarse.)
    if (s_dist < OWNGOAL_GUARD_CM && fabsf(wrap180(ang_ball - goal_ang)) > 100.0f){
        const float go = ang_ball + 180.0f;
        mix_mover_vector(go, SPEED, omega); s_aim = go; s_last_go = go; return;
    }
    float go;
    if (s_dist > KEEPOUT_CM || dist_P < s_dist - 2.0f){
        go = ang_P;                                  // derecho al punto-detrás (no cruza la pelota)
    } else {
        const float diff = wrap180(ang_P - ang_ball);
        go = ang_ball + (diff >= 0.0f ? 90.0f : -90.0f);   // orbita la pelota hacia el punto-detrás
    }
    int spd = SPEED;
    if (dist_P < SLOW_CM) spd = (int)(SPEED * clampf(SLOW_FLOOR + (1.0f-SLOW_FLOOR)*dist_P/SLOW_CM, SLOW_FLOOR, 1.0f));
    s_aim = go;
    mix_mover_vector(go, spd, omega);
    s_last_go = go;
}

const char* mix_seguir_estado_nombre(){
    switch(s_estado){
        case Estado::GIRAR_ATRAS: return "GIRAR_ATRAS";
        case Estado::PERSEGUIR: return "PERSEGUIR";
        case Estado::ACOMODAR:  return "ACOMODAR";
        case Estado::ESCOLTAR:  return "ESCOLTAR";
        case Estado::BUSCAR:    return "BUSCAR";
        default:                return "ESPERA";
    }
}
float mix_seguir_aim_deg(){  return s_aim; }
float mix_seguir_dist_cm(){  return s_dist; }

}  // namespace mix
}  // namespace iitasoccer
