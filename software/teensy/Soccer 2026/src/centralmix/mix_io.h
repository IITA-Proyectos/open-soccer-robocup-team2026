// mix_io.h — VARIABLES PLANAS estilo 2025 que alimentan la FSM de centralmix.
//
// CONTRATO. Reemplaza el "world model" por un bloque de variables planas
// (como las globales sueltas del delantero 2025: haypelota, anguloPelota, error,
// Xp/Yp, etc.), pero POBLADAS desde los datos de TOP/DOWN por mix_comm.
//
// FLUJO:
//   mix_comm_tick()  → decodifica frames TOP (WORLD_SNAPSHOT) y DOWN (LineStatusV2,
//                      Pose2D/Velocity2D) y ESCRIBE estos campos.
//   mix_fsm_tick()   → LEE estos campos (nunca toca Serial ni world_model).
//
// MARCO DE REFERENCIA DE LA PELOTA (fijado por contrato, NO el del 2025):
//   +X = derecha del robot,  +Y = adelante del robot.  Unidades: cm (CRUDO de la cámara).
//   angulo_pelota_deg = atan2(ball_x_cm, ball_y_cm) en grados:
//       0°   = pelota justo adelante
//       >0   = pelota a la DERECHA
//       <0   = pelota a la IZQUIERDA
//   (Convención elegida para que |angulo_pelota_deg| < tolerancia_apuntado
//    signifique "apuntando a la pelota", igual que el 2025 usaba |anguloPelota|.)
//   ⚠️ El 2025 codificaba distinto (atan2(Yp,Xp) con Xp lateral). NO copiar esa
//   fórmula: en centralmix el ángulo lo calcula mix_comm con la convención de
//   ARRIBA. Los implementadores de la FSM consumen angulo_pelota_deg ya calculado.
//
// ⚠️ NO TESTEADO EN HARDWARE. Contrato de datos; el cableado real lo cierra banco.

#pragma once
#include <stdint.h>

namespace iitasoccer {
namespace mix {

struct MixIO {
    // ---- Pelota (marco robot: +X=derecha, +Y=adelante; cm = dato CRUDO de la cámara) ----
    float ball_x_cm = 0.0f;        // + = derecha (cm; mix_comm divide /10 el mm del snapshot)
    float ball_y_cm = 0.0f;        // + = adelante (cm)
    bool  ball_visible = false;    // ¿la cámara ve la pelota? (haypelota 2025)
    float angulo_pelota_deg = 0.0f; // atan2(ball_x_cm, ball_y_cm) en °, calculado por mix_comm

    // Velocidad de la pelota (marco robot: +X=derecha, +Y=adelante; cm/s). La CALCULA el TOP
    // (módulo ball_velocity, a partir de la pelota de cámara en el tiempo) y la manda en el
    // WorldSnapshot (ball_vx_mm_s/ball_vy_mm_s); mix_comm la pasa a cm/s (÷10). 0 = N/A o quieta.
    // La usa el rodeo estilo Edge (-DMIX_ATTACK_EDGE) para ANTICIPAR la pelota en movimiento.
    // ⚠️ Es velocidad RELATIVA al robot → incluye el ego-movimiento (cuando el robot se mueve,
    // la pelota quieta "se mueve" en este marco). El feedforward la gatea por umbral + tope.
    float ball_vx_cm_s = 0.0f;     // + = la pelota va hacia la derecha del robot (cm/s)
    float ball_vy_cm_s = 0.0f;     // + = la pelota se acerca por el frente (cm/s)

    // ---- Arcos por ROL, NO por color (ángulo ° marco robot, +=derecha; distancia mm) ----
    // ⚠️ La placa CENTRAL (este programa) NUNCA pregunta por COLOR (amarillo/azul). La placa TOP
    // ya resolvió cuál arco es el RIVAL (opp = al que se patea) y cuál el PROPIO (own), con el
    // módulo goal_polarity ("el arco al FRENTE del robot es el rival", fijado al arranque por un
    // latch). Acá sólo se consume ese rol: el delantero apunta el pateo al goal_opp por su ÁNGULO,
    // sin saber ni preguntar el color.
    bool  goal_opp_visible = false;  // ¿el TOP ve el arco RIVAL (al que patear)?
    float goal_opp_angle   = 0.0f;   // ° al arco rival: 0=frente, +=derecha. Válido sólo si visible
    float goal_opp_dist    = 0.0f;   // mm al arco rival
    bool  goal_own_visible = false;  // ¿el TOP ve el arco PROPIO? (disponible; la FSM hoy no lo usa)
    float goal_own_angle   = 0.0f;   // ° al arco propio
    float goal_own_dist    = 0.0f;   // mm al arco propio

    // ---- Heading / rumbo ----
    float heading_deg = 0.0f;        // heading ACTUAL de la fuente seleccionada (BNO por
                                     // default; OTOS con -DMIX_HEADING_OTOS), en grados.
    bool  heading_valid = false;     // ¿el heading de la fuente es confiable?
    float heading_error_deg = 0.0f;  // heading_deg - heading_inicial, normalizado a [-180,180].
                                     // Equivale al 'error' del control de rumbo 2025.
    float otos_heading_deg = 0.0f;   // heading CRUDO del OTOS (DOWN), SIEMPRE disponible para
                                     // diagnóstico / A-B aunque la fuente activa sea BNO.
    uint8_t otos_confidence = 0;     // salud del OTOS reportada por DOWN: 100 = los 2 OTOS sanos,
                                     // 60 = uno solo, 0 = ninguno (no confiar). Lo usa la patada
                                     // recta para decidir si corrige el rumbo con el OTOS.

    // ---- Línea (de DOWN; ver mix_config umbrales) ----
    bool    line_present = false;    // ¿hay línea presente? (con histéresis de DOWN)
    float   line_angle_deg = 0.0f;   // ángulo de la línea en ° (0 = frente)
    uint8_t line_depth = 0;          // profundidad / # sensores sobre la línea (0..32)

    // ---- Árbitro / partido ----
    bool match_running = false;      // ¿partido en curso? (referee START). La FSM solo
                                     // debe MOVERSE si match_running == true.

    // ---- Timers que la FSM expone (millis) ----
    // (En el 2025 eran globales sueltas: millis_inicio_estado, millis_inicio_centrando,
    //  millis_pelota. Se exponen acá para que mix_comm pueda sellar el timestamp de la
    //  última vez que se vio la pelota sin acoplarse a la FSM.)
    unsigned long t_last_ball_seen_ms = 0;  // millis_pelota: última vez con ball_visible=true.
                                            // Lo SELLA mix_comm al recibir pelota válida.

    // Frescura de enlaces (rx-watchdog liviano; lo mantiene mix_comm).
    unsigned long t_last_top_frame_ms  = 0; // último WORLD_SNAPSHOT aplicado
    unsigned long t_last_down_frame_ms = 0; // último frame DOWN aplicado (línea O pose O vel)
    unsigned long t_last_otos_pose_ms  = 0; // último Pose2D del OTOS aplicado (frescura ESPECÍFICA
                                            // de la pose; t_last_down_frame_ms también sube con la
                                            // línea, así que NO sirve para "¿llegó pose OTOS?").
    bool top_link_fresh  = false;           // ¿llegó snapshot de TOP recientemente?
    bool down_link_fresh = false;           // ¿llegó frame de DOWN recientemente?

    // ---- Diagnóstico de la patada (lo ESCRIBE mix_motors::avanzar_patear; lo lee el debug) ----
    // Para titular en banco la corrección de rumbo: ver el error y la corr en vivo por USB.
    float kick_err_deg = 0.0f;              // error de rumbo del OTOS durante la patada (0 si no corrige)
    int   kick_corr    = 0;                 // término de giro aplicado (PWM, ya clampeado; 0 si no corrige)
    int8_t giro_atras_dir = 0;              // sentido latcheado del giro-encare "pelota atrás"
                                            // (0=inactivo, +1/-1=girando). Para validar el anti-dither.
};

// Instancia global única (estilo 2025: estado compartido por variables globales).
extern MixIO g_io;

}  // namespace mix
}  // namespace iitasoccer
