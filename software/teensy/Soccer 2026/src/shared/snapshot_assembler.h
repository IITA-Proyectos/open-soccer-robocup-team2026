// snapshot_assembler.h — ENSAMBLADOR PURO del WorldSnapshot (TOP → CENTRAL).
//
// Qué resuelve
// ------------
// La placa TOP (Teensy 4.0) percibe el mundo con sensores en buses distintos
// (cámaras por Serial3/5, BNO + 4 ToF por I2C Wire, OTOS por Serial1) y arma un
// WorldSnapshot que manda al CENTRAL a 100 Hz. El rediseño sensorial (interrupts
// + DMA + doble-buffer, decidido por el equipo) va a leer cada sensor en su
// propio "slot" a su propia cadencia; el envío del snapshot NO debe bloquear
// esperando a ninguna lectura. Este módulo es la pieza que TOMA los valores ya
// leídos de cada slot (más una marca de FRESCURA por slot) y arma el snapshot.
//
// Por qué es PURO (sin Arduino, host-testeable)
// ---------------------------------------------
// Igual que line_view.h / pose_view.h / tof_fresh_or_no_reading(): este header
// NO toca Wire/Serial/millis(). La FRESCURA llega ya decidida como un bool por
// slot — quien tiene el reloj (el glue Arduino con millis() y los timestamps de
// cada doble-buffer) decide "¿este slot es fresco?" y se lo pasa acá. Así la
// MISMA lógica de ensamblado corre en el firmware y en los tests host-native
// (`bash scripts/run-host-tests.sh`). El módulo solo depende de <stdint.h> y de
// los struct/sentinelas de types.h.
//
// FAIL-SAFE por frescura (regla maestra)
// --------------------------------------
// Un slot VIEJO (no fresco) NO propaga su último dato: colapsa al SENTINELA que
// el contrato del WorldSnapshot (types.h) ya define para "este campo no es
// válido". El consumidor (CENTRAL) ya sabe leer esos sentinelas. Mapeo:
//   • pelota no fresca   → ball_visible=0, ball_confidence=0, x/y/vx/vy=0
//   • arco rival viejo   → goal_opp_visible=0 (angle/distance N/A; se ponen 0)
//   • arco propio viejo  → goal_own_visible=0 (angle/distance N/A; se ponen 0)
//   • rumbo (heading) viejo → se LIMPIA el bit heading_valid (flags bit4); el
//     campo my_heading_centideg igual viaja, pero CENTRAL no debe confiar en él
//     (espejo de central_gate_heading_omega() en world_model.h).
//   • pose (OTOS/trilat) vieja → my_pose_confidence=0 (CENTRAL ignora x/y), y x/y=0
//   • obstáculo (ToF) viejo → min_obstacle_mm = TOF_NO_READING (0xFFFF) =
//     "sin obstáculo" (el consumidor lo trata como libre, no como pared a 65 m).
//
// Un slot fresco NO se toca: su valor pasa tal cual. La frescura de un slot NO
// afecta a los otros (un ToF colgado no invalida la pelota, etc.).
//
// ALCANCE: diseño + andamiaje. Este header NO se cablea a main_top.cpp todavía
// (la integración del doble-buffer es post-Incheon, en banco). Es ADITIVO.

#pragma once
#include <stdint.h>
#include "types.h"

namespace iitasoccer {

// ----------------------------------------------------------------------------
// Máscaras de los bits de flags del WorldSnapshot (espejo de types.h:132-137).
// Hoy main_top.cpp los escribe como literales (1<<N); acá les damos nombre para
// que el ensamblado sea legible y el bit de heading_valid se pueda limpiar sin
// números mágicos. NO cambian el layout de wire (mismos bits).
// ----------------------------------------------------------------------------
constexpr uint8_t WS_FLAG_IN_OWN_PENALTY_AREA = 0x01;  // bit 0
constexpr uint8_t WS_FLAG_PARTNER_ALIVE       = 0x02;  // bit 1
constexpr uint8_t WS_FLAG_PARTNER_SEES_BALL   = 0x04;  // bit 2
constexpr uint8_t WS_FLAG_MATCH_RUNNING       = 0x08;  // bit 3
constexpr uint8_t WS_FLAG_HEADING_VALID       = 0x10;  // bit 4

// Sentinela "sin obstáculo" del campo min_obstacle_mm (espejo de TOF_NO_READING
// de sensors_tof.h; redefinido acá para no arrastrar config_top.h/Arduino al
// header puro). 0xFFFF = libre, NO obstáculo a 65 m. Ver types.h:126.
constexpr uint16_t WS_OBSTACLE_NONE = 0xFFFF;

// ----------------------------------------------------------------------------
// Valores YA LEÍDOS de cada slot + su flag de frescura.
//
// El productor (lectura por interrupt/DMA → doble-buffer) llena estos campos con
// el último valor de cada slot y marca *_fresh según si ese slot se actualizó
// dentro de su ventana. El ensamblador NO sabe de relojes: confía en los flags.
//
// Convención de los flags: *_fresh == true  → usar el valor del slot tal cual.
//                          *_fresh == false → colapsar al sentinela (fail-safe).
// ----------------------------------------------------------------------------
struct SnapshotInputs {
    // --- Slot PELOTA (cámaras front+back ya fusionadas) ---
    bool    ball_fresh;
    bool    ball_visible;          // ¿la cámara ve la pelota? (dentro del slot)
    int16_t ball_x_mm;
    int16_t ball_y_mm;
    uint8_t ball_confidence;       // 0-100
    int16_t ball_vx_mm_s;
    int16_t ball_vy_mm_s;

    // --- Slot ARCO RIVAL (cámara) ---
    bool    goal_opp_fresh;
    bool    goal_opp_visible;
    int16_t goal_opp_angle_centideg;
    int16_t goal_opp_distance_mm;

    // --- Slot ARCO PROPIO (cámara) ---
    bool    goal_own_fresh;
    bool    goal_own_visible;
    int16_t goal_own_angle_centideg;
    int16_t goal_own_distance_mm;

    // --- Slot RUMBO (heading, BNO055) ---
    bool    heading_fresh;         // ¿el BNO entregó un yaw dentro de su ventana?
    bool    heading_valid_src;     // validez intrínseca del BNO (fused_valid en vivo)
    int16_t heading_centideg;      // -18000..+18000

    // --- Slot POSE (OTOS / trilateración) ---
    bool    pose_fresh;
    bool    pose_valid_src;        // validez intrínseca de la fusión (p.ej. ≥2 TOFs)
    int16_t pose_x_mm;
    int16_t pose_y_mm;
    uint8_t pose_confidence;       // 0-100 (ya calculada por el productor)

    // --- Slot DISTANCIAS (min de ToFs + HC-SR04) ---
    bool     obstacle_fresh;
    uint16_t min_obstacle_mm;      // ya = WS_OBSTACLE_NONE si no hay obstáculo

    // --- Slots SIEMPRE-PRESENTES (no requieren frescura por sí mismos) ---
    // El árbitro/partner llegan por COMM; su frescura ya está resuelta aguas
    // arriba (comm_arbiter_*). Acá entran como flags ya calculados.
    uint8_t referee_cmd;           // 0=stop,1=start,2=halftime,3=reset
    bool    match_running;         // → flag bit3
    bool    partner_alive;         // → flag bit1
    bool    partner_sees_ball;     // → flag bit2
    bool    in_own_penalty_area;   // → flag bit0
};

// ----------------------------------------------------------------------------
// assemble_snapshot — arma el WorldSnapshot aplicando el FAIL-SAFE por frescura.
//
// Toma los valores ya leídos de cada slot (SnapshotInputs) y devuelve un
// WorldSnapshot listo para serializar. Un slot no fresco colapsa a su sentinela;
// un slot fresco pasa tal cual. NO bloquea, NO lee hardware, NO usa el reloj.
// ----------------------------------------------------------------------------
inline WorldSnapshot assemble_snapshot(const SnapshotInputs& in) {
    WorldSnapshot s{};   // value-init: todo 0 (sentinela base para los no-frescos)

    // --- POSE propia ---
    // Fresca Y válida intrínsecamente → x/y/confidence pasan. Si no → confidence=0
    // (CENTRAL ignora la POSICIÓN) y x/y=0. (s{} ya dejó 0; sólo sobreescribimos
    // en el caso bueno.)
    if (in.pose_fresh && in.pose_valid_src) {
        s.my_x_mm            = in.pose_x_mm;
        s.my_y_mm            = in.pose_y_mm;
        s.my_pose_confidence = in.pose_confidence;
    }
    // else: my_x_mm=my_y_mm=0, my_pose_confidence=0 (del value-init).

    // --- RUMBO (heading) ---
    // El heading SIEMPRE viaja en my_heading_centideg (CENTRAL lo consume sin
    // gatearlo por confidence). El bit heading_valid le dice si confiar en él.
    // Si el slot está fresco, mandamos el valor del BNO; si NO, mandamos 0
    // (rumbo neutro) — y en ambos casos el bit refleja frescura ∧ validez.
    const bool heading_ok = in.heading_fresh && in.heading_valid_src;
    s.my_heading_centideg = in.heading_fresh ? in.heading_centideg : 0;

    // --- PELOTA ---
    if (in.ball_fresh && in.ball_visible) {
        s.ball_visible    = 1;
        s.ball_x_mm       = in.ball_x_mm;
        s.ball_y_mm       = in.ball_y_mm;
        s.ball_confidence = in.ball_confidence;
        s.ball_vx_mm_s    = in.ball_vx_mm_s;
        s.ball_vy_mm_s    = in.ball_vy_mm_s;
    }
    // else: ball_visible=0, confidence=0, x/y/vx/vy=0 (del value-init).

    // --- ARCO RIVAL ---
    if (in.goal_opp_fresh && in.goal_opp_visible) {
        s.goal_opp_visible        = 1;
        s.goal_opp_angle_centideg = in.goal_opp_angle_centideg;
        s.goal_opp_distance_mm    = in.goal_opp_distance_mm;
    }
    // else: goal_opp_visible=0, angle/distance=0 (N/A, del value-init).

    // --- ARCO PROPIO ---
    if (in.goal_own_fresh && in.goal_own_visible) {
        s.goal_own_visible        = 1;
        s.goal_own_angle_centideg = in.goal_own_angle_centideg;
        s.goal_own_distance_mm    = in.goal_own_distance_mm;
    }
    // else: goal_own_visible=0, angle/distance=0 (N/A, del value-init).

    // --- OBSTÁCULO (min ToF + HC-SR04) ---
    // Fresco → el valor (que ya puede ser WS_OBSTACLE_NONE). Viejo → NONE: un ToF
    // colgado no debe quedar como pared fantasma (mismo espíritu que tof_stale).
    s.min_obstacle_mm = in.obstacle_fresh ? in.min_obstacle_mm : WS_OBSTACLE_NONE;

    // --- ÁRBITRO + FLAGS ---
    s.referee_cmd = in.referee_cmd;
    uint8_t flags = 0;
    if (in.in_own_penalty_area) flags |= WS_FLAG_IN_OWN_PENALTY_AREA;
    if (in.partner_alive)       flags |= WS_FLAG_PARTNER_ALIVE;
    if (in.partner_sees_ball)   flags |= WS_FLAG_PARTNER_SEES_BALL;
    if (in.match_running)       flags |= WS_FLAG_MATCH_RUNNING;
    if (heading_ok)             flags |= WS_FLAG_HEADING_VALID;
    s.flags = flags;

    return s;
}

}  // namespace iitasoccer
