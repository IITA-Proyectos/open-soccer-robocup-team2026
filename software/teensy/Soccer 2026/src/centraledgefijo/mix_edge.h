// mix_edge.h — Núcleo PURO del rodeo estilo "Edge" (回り込み) — VERSIÓN POSICIÓN PURA.
//
// QUÉ ES. Traducción del estado A==10 ("回り込むやつ" = "el que rodea") del delantero
// campeón mundial 2024 Team Edge (github.com/shiokara0525/Edge_2025_main, lib/process/
// Attack.cpp). LA IDEA: una SOLA fórmula reactiva convierte el ángulo de la pelota en una
// dirección de avance AMPLIFICADA → cuanto más al costado la pelota, más al costado apunto
// → la barro POR DETRÁS; rodeando, el ángulo se achica solo y termino empujándola al arco.
//
// ⭐ DIFERENCIA con la carpeta `centraledge` (pedido de Elías 2026-06-23):
//   Esta versión NO usa la VELOCIDAD de la pelota — ni la predicción (posición+velocidad·t)
//   ni el bump por velocidad. Es el rodeo SIMPLE de Edge: SOLO la curva FIJA de amplificación
//   de ángulo, calculada con la POSICIÓN actual de la pelota. Más simple y robusta (no le
//   entra el ruido de la velocidad de cámara ni el ego-movimiento). Sin pateador: el empuje
//   al arco es por inercia (avanzar_patear).
//
// PURO: sin Arduino, sin g_io, sin hardware. Entra datos, sale decisión. Host-testeable
// (test/test_mix_edge_fijo/). La FSM (mix_fsm_edge.cpp) arma la entrada desde g_io y actúa.

#pragma once

namespace iitasoccer {
namespace mix {

// Parámetros tuneables de la curva de rodeo + del disparo de empuje. Los DEFAULTS viven en
// mix_config.h (MIX_EDGE_*). Se pasan como struct (no #define) para testear con distintos
// valores en host. (Esta versión NO tiene parámetros de velocidad — es posición pura.)
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
};

// Entrada: lo que la FSM lee de g_io. Marco robot: +X=derecha, +Y=frente; cm.
// (Pelota en x/y CRUDOS — el ángulo/distancia se calculan adentro. SIN velocidad.)
struct EdgeIn {
    float ball_x_cm;        // g_io.ball_x_cm  (+ = derecha)
    float ball_y_cm;        // g_io.ball_y_cm  (+ = frente)
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

// El delantero completo de un tick: arma go_ang (con la curva, POR POSICIÓN) y decide push_ready.
EdgeOut mix_edge_attack(const EdgeIn& in, const EdgeParams& p);

}  // namespace mix
}  // namespace iitasoccer
