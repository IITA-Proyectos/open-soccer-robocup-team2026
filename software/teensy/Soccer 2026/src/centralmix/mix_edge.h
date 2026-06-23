// mix_edge.h — Núcleo PURO del rodeo estilo "Edge" (回り込み / wrap-around).
//
// QUÉ ES. Esta es la traducción del estado A==10 ("回り込むやつ" = "el que rodea")
// del delantero campeón mundial 2024 Team Edge (github.com/shiokara0525/Edge_2025_main,
// lib/process/Attack.cpp) a la convención de datos de centralmix.
//
// LA IDEA EN UNA FRASE: en vez de "apuntar → avanzar → orbitar" en estados separados
// (lento), una SOLA fórmula reactiva convierte el ángulo de la pelota en una dirección
// de avance AMPLIFICADA. Cuanto más al costado está la pelota, más al costado apunto →
// en vez de chocarla, la barro POR DETRÁS; y a medida que rodeo, el ángulo se achica
// solo hasta 0 y termino empujándola hacia el arco. (El "mirar al arco" lo maneja el
// giro, aparte de la traslación — ver mix_fsm_edge.cpp.)
//
// DIFERENCIA vs Edge (honesta): Edge además ADELANTA el ángulo cuando la pelota se
// MUEVE rápido (feedforward con ball.vec_velocity → go_ang += 30). centralmix HOY no
// tiene velocidad de pelota en mix_io → ese feedforward NO está acá (queda como mejora
// futura cuando se cablee la velocidad de DOWN a g_io). Sin pateador: el "empuje al
// arco" es por inercia (avanzar_patear), NO solenoide.
//
// PURO: sin Arduino, sin g_io, sin hardware. Entra datos, sale decisión. Host-testeable
// (mix_edge_test.cpp). La FSM (mix_fsm_edge.cpp) arma la entrada desde g_io y actúa.

#pragma once

namespace iitasoccer {
namespace mix {

// Parámetros tuneables de la curva de rodeo + del disparo de empuje. Los DEFAULTS
// viven en mix_config.h (MIX_EDGE_*); este struct se llena con esos valores. Se pasan
// como struct (no #define) para poder testear la curva con distintos valores en host.
struct EdgeParams {
    // --- Curva de rodeo: ángulo de pelota → ángulo de avance (piecewise lineal CONTINUA) ---
    // Tres tramos por |ángulo de pelota|:  NEAR (casi al frente) · SIDE (al costado) · WIDE (muy
    // al costado/atrás). La pendiente de cada tramo es la "fuerza de rodeo" de esa zona.
    float k_near;       // pendiente zona cercana (≈1.2: pelota al frente → voy casi derecho)
    float b1_deg;       // fin de la zona cercana (≈20°)
    float k_side;       // pendiente zona lateral (≈2.0: apunto al DOBLE → rodeo fuerte)
    float b2_deg;       // fin de la zona lateral (≈75°)
    float k_wide;       // pendiente zona ancha (≈1.0: sigo barriendo para meterme detrás)
    float go_max_deg;   // tope del ángulo de avance (≈170°: nunca apuntar 100% hacia atrás)

    // --- Disparo del empuje (gol por inercia, sin pateador) ---
    float push_dist_cm;     // pelota más cerca que esto = candidata a empujar
    float push_align_deg;   // y dentro de este ángulo al frente
    float push_goal_deg;    // si se VE el arco rival, debe estar dentro de esto para empujar

    // --- Feedforward de velocidad (anticipar la pelota en movimiento) ---
    // Si la pelota se mueve más rápido que vel_min, se apunta a dónde ESTARÁ dentro de lead_s
    // segundos (posición + velocidad·lead_s), en vez de dónde está. El adelanto se TOPEA en
    // lead_max_cm para no amplificar el ruido de la velocidad de cámara. vel_min ALTO = apagado.
    float vel_min_cm_s;     // umbral: por debajo, ignorar la velocidad (ruido / pelota casi quieta)
    float lead_s;           // cuántos segundos adelantar la posición de la pelota
    float lead_max_cm;      // tope del adelanto (cm) — acota el ruido
};

// Entrada: lo que la FSM lee de g_io. Marco robot: +X=derecha, +Y=frente; cm y cm/s.
// (Se pasa la pelota en x/y CRUDOS — el ángulo/distancia y el adelanto por velocidad se
//  calculan adentro, así la decisión es auto-contenida y testeable.)
struct EdgeIn {
    float ball_x_cm;        // g_io.ball_x_cm  (+ = derecha)
    float ball_y_cm;        // g_io.ball_y_cm  (+ = frente)
    float ball_vx_cm_s;     // g_io.ball_vx_cm_s (velocidad pelota, marco robot)
    float ball_vy_cm_s;     // g_io.ball_vy_cm_s
    bool  ball_visible;     // g_io.ball_visible
    bool  goal_visible;     // g_io.goal_opp_visible (arco RIVAL, resuelto por el TOP)
    float goal_angle_deg;   // ángulo robot→arco rival (0=frente, +=derecha) = g_io.goal_opp_angle
};

// Salida: la DECISIÓN del delantero para este tick.
struct EdgeOut {
    float go_ang_deg;   // dirección de TRASLACIÓN deseada (0=frente, +=derecha). La FSM la
                        // pasa a mix_mover_vector. El giro (mirar al arco) se decide aparte.
    bool  push_ready;   // true = comprometerse al EMPUJE recto (pelota cerca + al frente +
                        // arco alineado o no visible). La FSM transiciona a EMPUJAR.
};

// LA CURVA (el corazón). |ball_angle| → ángulo de avance, con el signo (izq/der) restaurado.
// Piecewise lineal CONTINUA (sin saltos en b1/b2). Pura, sin estado.
float mix_edge_wrap_angle(float ball_angle_deg, const EdgeParams& p);

// El delantero completo de un tick: arma go_ang (con la curva) y decide push_ready.
EdgeOut mix_edge_attack(const EdgeIn& in, const EdgeParams& p);

}  // namespace mix
}  // namespace iitasoccer
