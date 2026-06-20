// mix_fsm.cpp — Máquina de estados de centralmix.
//
// PORT FIEL del switch del delantero 2025
// (software/_deprecated-2025/robot-delantero/delantero-sin-zirconLib.cpp).
//
// QUÉ CAMBIA respecto del 2025 (y SOLO esto):
//   - TODA lectura de sensor se reemplaza por g_io (mix_io.h), poblado por mix_comm.
//   - TODA escritura de motor se reemplaza por las primitivas de mix_motors.h.
//   - Se AGREGA una sola regla de prioridad al inicio de mix_fsm_tick():
//       si !g_io.match_running → parar() y return (GO/STOP del árbitro RCJ; el 2025
//       no tenía esto porque arrancaba solo). Es lo ÚNICO que se añade al flujo.
//
// QUÉ NO CAMBIA (fidelidad 1:1 con el 2025):
//   - Las transiciones de estado (cada `estado = ...`) están verbatim.
//   - Los umbrales: |anguloPelota| < 15 (MIX_TOL_APUNTADO), tolerancia_centrado=30,
//     tolerancia_cercania=50, |error|<=1/50/80, etc. — todos con el valor 2025.
//   - Los timers: millis_inicio_estado, millis_inicio_centrando, millis_pelota
//     (este último = g_io.t_last_ball_seen_ms, lo sella mix_comm) y TODOS los
//     umbrales en ms (700/70/1000/9000/500/10000/20000/4000/25000/300/3000/200/400…).
//
// MAPEOS QUE REQUIRIERON DECISIÓN (marcados con  // <RE-TUNEO 2025→2026>):
//   1) `Xp <= tolerancia_cercania` (50): en 2025 Xp era la coordenada LATERAL cruda
//      de la pelota (0..~100, codificada). En el marco nuevo (+X=der, +Y=adel, en mm)
//      "estar cerca" = distancia al robot pequeña. Se mapea a:
//          dist_pelota_mm() <= MIX_TOL_CERCANIA  (valor 2025 = 50, UNIDADES DISTINTAS:
//          2025 era cuenta codificada, acá es mm → RE-TUNEAR en banco).
//      Su negación `Xp >= tolerancia_cercania` (en APUNTAR_PELOTA_horario/antihorario)
//      se mapea a dist_pelota_mm() >= MIX_TOL_CERCANIA, igual que el 2025.
//   2) `abs(Yp - Ycontrincante) <= tolerancia_centrado` (30): en 2025 era "la pelota
//      está alineada con el arco rival en el eje vertical de la cámara". En el marco
//      nuevo el dato equivalente es el ÁNGULO al arco rival (goal_yellow_angle, arco
//      rival = AMARILLO como el 2025). Se mapea a:
//          |goal_yellow_angle| <= MIX_TOL_CENTRADO  (valor 2025 = 30, UNIDADES DISTINTAS:
//          2025 era diferencia de coordenadas Y; acá son GRADOS → RE-TUNEAR en banco).
//      La guarda ARCO_CONTRINCANTE (2025: hayarco_amarillo) → g_io.goal_yellow_visible.
//   3) s1/s2/s3 >= blanco (3 sensores analógicos): en centralmix la línea llega por
//      DOWN como line_present/line_angle_deg/line_depth (mix_io). Se reconstruye el
//      branch de 3 vías por el ÁNGULO de la línea:
//          línea a la IZQUIERDA  → DETECTA_LINEA_1 (era s1, sensor izquierdo)
//          línea al FRENTE       → DETECTA_LINEA_2 (era s2, sensor centro)
//          línea a la DERECHA    → DETECTA_LINEA_3 (era s3, sensor derecho)
//      con line_present (depth>=trigger) como gate. Los SECTORES de ángulo (±30°) son
//      una elección 2026 → RE-TUNEAR. El "OR de los 3 sensores" del 2025 (centrando)
//      se mapea a line_present.
//
// ARCO RIVAL = AMARILLO, igual que el 2025 (ARCO_CONTRINCANTE = hayarco_amarillo).
//   Dejado para parametrizar: ver kArcoRivalEsAmarillo abajo. <PARAMETRIZAR>
//
// El bloque ARQUERO del 2025 NO se porta (no existe en este archivo base; este es
// el delantero).
//
// ⚠️ NO TESTEADO EN HARDWARE. Compila == NO anda: el sentido físico de cada primitiva
// y todos los mapeos marcados <RE-TUNEO 2025→2026> se validan en banco.

#include <Arduino.h>
#include <math.h>

#include "mix_config.h"
#include "mix_io.h"
#include "mix_motors.h"
#include "mix_fsm.h"

namespace iitasoccer {
namespace mix {

// ============================================================
// Estado y timers de la FSM (eran globales sueltas en el 2025).
//   millis_pelota del 2025  ==  g_io.t_last_ball_seen_ms  (lo sella mix_comm).
//   Acá quedan millis_inicio_estado y millis_inicio_centrando (locales a la FSM).
// ============================================================
static Estado estado = Estado::AVANCE_INICIO;
static unsigned long millis_inicio_estado    = 0;
static unsigned long millis_inicio_centrando = 0;

// heading_inicial (initialYaw del 2025): se captura en mix_fsm_init() desde
// g_io.heading_deg. La FSM consume g_io.heading_error_deg (== 'error' 2025), que
// mix_comm calcula como wrap180(heading_deg - heading_inicial). heading_inicial se
// guarda acá por si mix_comm quisiera leerlo; el cálculo de error lo hace mix_comm.
static float heading_inicial_deg = 0.0f;

// ============================================================
// PARAMETRIZACIÓN del arco rival. El 2025 hardcodeaba AMARILLO
// (ARCO_CONTRINCANTE = hayarco_amarillo; Ycontrincante = Yam). Se mantiene AMARILLO
// pero se deja la perilla a la vista para parametrizar a futuro. <PARAMETRIZAR>
// ============================================================
static constexpr bool kArcoRivalEsAmarillo = true;  // 2025: arco rival = AMARILLO

static inline bool arco_rival_visible() {
    return kArcoRivalEsAmarillo ? g_io.goal_yellow_visible : g_io.goal_blue_visible;
}
static inline float arco_rival_angle_deg() {
    return kArcoRivalEsAmarillo ? g_io.goal_yellow_angle : g_io.goal_blue_angle;
}

// ============================================================
// Helpers de lectura (reemplazan a las globales seriales 2025).
// ============================================================

// wrap a [-180, 180]. Usado SOLO en mix_fsm_init para alinear heading_inicial; el
// 'error' de cada tick lo da g_io.heading_error_deg (ya wrapeado por mix_comm).
static inline float wrap180(float a) {
    while (a > 180.0f)  a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}

// Distancia robot→pelota (mm). En el 2025 la cercanía se medía sobre Xp (lateral
// crudo); acá se usa la distancia euclídea del marco nuevo. <RE-TUNEO 2025→2026>
static inline float dist_pelota_mm() {
    return sqrtf(g_io.ball_x_mm * g_io.ball_x_mm + g_io.ball_y_mm * g_io.ball_y_mm);
}

// ---- Reconstrucción de los 3 "sensores de línea" del 2025 desde DOWN ----
// El 2025 tenía 3 sensores analógicos: s1=izq, s2=centro, s3=der, cada uno
// comparado contra su blanco. Acá la línea llega como line_present/line_angle_deg
// (0 = frente; signo según convención de mix_io). Se reconstruye el branch de 3 vías
// por sector angular. SECTORES (±30°) = elección 2026 → RE-TUNEAR. <RE-TUNEO 2025→2026>
static constexpr float kLineSectorDeg = 120.0f;  // semiancho del sector "frente"

static inline bool linea_presente() {
    // OR de los 3 sensores del 2025 → presencia de línea (con histéresis de DOWN).
    return g_io.line_present && (g_io.line_depth >= MIX_LINE_DEPTH_TRIGGER);
}
// s1 (izquierdo): línea a la izquierda  (angulo < -sector)   <RE-TUNEO 2025→2026>
static inline bool linea_s1() {
    return linea_presente() && (g_io.line_angle_deg < -60.0f);
}
// s2 (centro): línea al frente          (|angulo| <= sector) <RE-TUNEO 2025→2026>
static inline bool linea_s2() {
    return linea_presente() && (fabsf(g_io.line_angle_deg) <= 60.0f);
}
// s3 (derecho): línea a la derecha      (angulo > +sector)   <RE-TUNEO 2025→2026>
static inline bool linea_s3() {
    return linea_presente() && (g_io.line_angle_deg > 60.0f);
}

// ============================================================
// IMPULSOS / centrados que el 2025 escribía INLINE con analogWrite (no eran
// funciones nombradas). Se reproducen acá con mix_set_motor para no inventar
// primitivas nuevas en mix_motors.h, manteniendo PWM+sentido EXACTOS del 2025.
//
// Convención de mix_set_motor: signo + = sentido "positivo" del motor. Se mapea
// digitalWrite(INA=0,INB=1) ≡ sentido positivo (+pwm), (INA=1,INB=0) ≡ negativo
// (-pwm), como en girar()/centrar_*() del port de mix_motors. El SENTIDO FÍSICO se
// re-verifica en banco (igual que las primitivas). <RE-VERIFICAR SENTIDO EN BANCO>
// ============================================================

// IMPULSO_INICIAL_GIRANDO: girar con 150 en los 3 (sentido de girar()).
static inline void impulso_inicial_girando() {
    // 2025: PWM=150, INA=0/INB=1 en los 3 → sentido positivo a 150.
    mix_set_motor(0, +150);
    mix_set_motor(1, +150);
    mix_set_motor(2, +150);
}

// APUNTAR_PELOTA*: rotar a 100*a según signo de angulo_pelota_deg.
//   anguloPelota > 0 (pelota a la DERECHA) → 2025 enciende "antihorario" (INA0/INB1).
//   anguloPelota < 0 (pelota a la IZQUIERDA) → 2025 enciende "horario"     (INA1/INB0).
static inline void apuntar_pelota_motores() {
    const int pwm = (int)(100.0f * MIX_A);  // 100 * a (2025)
    if (g_io.angulo_pelota_deg > 0) {
        // sentido antihorario (INA0/INB1 → +)
        mix_set_motor(0, +pwm);
        mix_set_motor(1, +pwm);
        mix_set_motor(2, +pwm);
    } else {
        // sentido horario (INA1/INB0 → -)
        mix_set_motor(0, -pwm);
        mix_set_motor(1, -pwm);
        mix_set_motor(2, -pwm);
    }
}

// IMPULSO_CENTRANDO_antihorario (2025): M1/M2 a 60*ic INA1/INB0(-), M3 a 180*ic INA0/INB1(+).
static inline void impulso_centrando_antihorario() {
    mix_set_motor(0, -(int)(60.0f * MIX_IC));
    mix_set_motor(1, -(int)(60.0f * MIX_IC));
    mix_set_motor(2, +(int)(180.0f * MIX_IC));
}

// IMPULSO_CENTRANDO_horario (2025): M1/M2 a 60*ic INA0/INB1(+), M3 a 180*ic INA1/INB0(-).
static inline void impulso_centrando_horario() {
    mix_set_motor(0, +(int)(60.0f * MIX_IC));
    mix_set_motor(1, +(int)(60.0f * MIX_IC));
    mix_set_motor(2, -(int)(180.0f * MIX_IC));
}

// ============================================================
// mix_fsm_init — port de la parte de setup() relevante a la FSM.
//   2025: estado = AVANCE_INICIO; sella millis_inicio_estado; captura initialYaw.
// ============================================================
void mix_fsm_init() {
    estado = Estado::AVANCE_INICIO;
    millis_inicio_estado    = millis();
    millis_inicio_centrando = millis();

    // initialYaw 2025: aquí se captura desde g_io.heading_deg (fuente seleccionada
    // por mix_comm: BNO por default, OTOS con -DMIX_HEADING_OTOS). El 'error' de cada
    // tick = wrap180(heading_deg - heading_inicial) lo computa mix_comm en
    // g_io.heading_error_deg. Guardamos heading_inicial para trazabilidad/A-B.
    heading_inicial_deg = wrap180(g_io.heading_deg);
}

// ============================================================
// mix_fsm_tick — port FIEL del switch(estado) del loop() 2025.
// ============================================================
void mix_fsm_tick() {
    // --- AGREGADO 2026 (no estaba en el 2025): GO/STOP del árbitro RCJ. ---
    // Si el partido no está corriendo, frenar y NO mover. Única regla nueva.
    if (!g_io.match_running) {
        parar();
        return;
    }

    // 'error' del control de rumbo 2025 == g_io.heading_error_deg (ya wrapeado).
    const float error = g_io.heading_error_deg;
    // 'haypelota' 2025 == g_io.ball_visible.
    const bool  haypelota = g_io.ball_visible;
    // 'anguloPelota' 2025 == g_io.angulo_pelota_deg (calculado por mix_comm).
    const float anguloPelota = g_io.angulo_pelota_deg;
    // 'millis_pelota' 2025 == g_io.t_last_ball_seen_ms (lo sella mix_comm).
    const unsigned long millis_pelota = g_io.t_last_ball_seen_ms;

    switch (estado) {
        // ----------------------------------------------------
        case Estado::AVANCE_INICIO:
            avanzar_patear();
            if (millis() - millis_inicio_estado >= 700) {
                parar();
                millis_inicio_estado = millis();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
            }
            break;

        // ----------------------------------------------------
        // PRIMER_IMPULSO_INICIAL_GIRANDO: declarado en el enum 2025 pero SIN case en
        // el switch (estado muerto). Se conserva en el enum por fidelidad; aquí NO
        // tiene case, igual que el 2025.
        // ----------------------------------------------------

        // ----------------------------------------------------
        case Estado::IMPULSO_INICIAL_GIRANDO:
            // girar con más potencia (150) — 2025 inline.
            impulso_inicial_girando();

            if (millis() - millis_inicio_estado >= 70) {
                millis_inicio_estado = millis();
                estado = Estado::GIRANDO;
            }

            if (haypelota) {
                parar();
                if (millis() - millis_inicio_estado >= 1000) {  // esperar 1s por la inercia
                    estado = Estado::APUNTAR_PELOTA;
                    millis_inicio_estado = millis();
                }
            }

            if (linea_s1()) {  // s1 >= blanco1
                estado = Estado::DETECTA_LINEA_1;
                millis_inicio_estado = millis();
            }
            if (linea_s2()) {  // s2 >= blanco2
                estado = Estado::DETECTA_LINEA_2;
                millis_inicio_estado = millis();
            }
            if (linea_s3()) {  // s3 >= blanco3
                estado = Estado::DETECTA_LINEA_3;
                millis_inicio_estado = millis();
            }
            break;

        // ----------------------------------------------------
        case Estado::GIRANDO:
            if (haypelota) {
                parar();
                if (millis() - millis_inicio_estado >= 700) {  // esperar 700ms por la inercia
                    estado = Estado::APUNTAR_PELOTA;
                    millis_inicio_estado = millis();
                }
            } else {
                girar();
            }

            // si pasaron >9s y |error| <= 50, avanzar por tiempo
            if ((millis() - millis_inicio_estado >= 9000) && (fabsf(error) <= 50)) {
                millis_inicio_estado = millis();
                estado = Estado::AVANZANDO_POR_TIEMPO;
            }

            if (linea_s1()) {
                estado = Estado::DETECTA_LINEA_1;
                millis_inicio_estado = millis();
            }
            if (linea_s2()) {
                estado = Estado::DETECTA_LINEA_2;
                millis_inicio_estado = millis();
            }
            if (linea_s3()) {
                estado = Estado::DETECTA_LINEA_3;
                millis_inicio_estado = millis();
            }
            break;

        // ----------------------------------------------------
        case Estado::AVANZANDO_POR_TIEMPO:
            avanzar();

            if (millis() - millis_inicio_estado >= 500) {
                millis_inicio_estado = millis();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
            }

            if (haypelota == true) {
                millis_inicio_estado = millis();
                estado = Estado::APUNTAR_PELOTA;
            }

            if (linea_s1()) {
                estado = Estado::DETECTA_LINEA_1;
                millis_inicio_estado = millis();
            }
            if (linea_s2()) {
                estado = Estado::DETECTA_LINEA_2;
                millis_inicio_estado = millis();
            }
            if (linea_s3()) {
                estado = Estado::DETECTA_LINEA_3;
                millis_inicio_estado = millis();
            }
            break;

        // ----------------------------------------------------
        case Estado::APUNTAR_PELOTA:
            if (fabsf(anguloPelota) >= MIX_TOL_APUNTADO) {   // tolerancia_apuntado (15)
                apuntar_pelota_motores();
            } else {
                millis_inicio_estado = millis();
                estado = Estado::AVANZANDO;
            }

            if (millis() - millis_pelota >= 500) {  // si deja de ver la pelota
                millis_inicio_estado = millis();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
            }

            if (millis() - millis_inicio_estado >= 10000) {  // timeout
                millis_inicio_estado = millis();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
            }

            if (linea_s1()) {
                estado = Estado::DETECTA_LINEA_1;
                millis_inicio_estado = millis();
            }
            if (linea_s2()) {
                estado = Estado::DETECTA_LINEA_2;
                millis_inicio_estado = millis();
            }
            if (linea_s3()) {
                estado = Estado::DETECTA_LINEA_3;
                millis_inicio_estado = millis();
            }
            break;

        // ----------------------------------------------------
        case Estado::AVANZANDO:
            avanzar();

            // si ya está lo suficientemente cerca de la pelota
            // 2025: haypelota && (Xp <= tolerancia_cercania) <RE-TUNEO 2025→2026>
            if (haypelota && (dist_pelota_mm() <= MIX_TOL_CERCANIA)) {
                millis_inicio_centrando = millis();
                millis_inicio_estado = millis();
                estado = Estado::CENTRANDO_horario;
            }

            // si deja de apuntar a la pelota
            if (fabsf(anguloPelota) >= MIX_TOL_APUNTADO) {
                millis_inicio_estado = millis();
                estado = Estado::APUNTAR_PELOTA;
            }

            // si deja de ver la pelota
            if (millis() - millis_pelota >= 500) {
                millis_inicio_estado = millis();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
            }

            // timeout
            if (millis() - millis_inicio_estado >= 20000) {
                millis_inicio_estado = millis();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
            }

            if (linea_s1()) {
                estado = Estado::DETECTA_LINEA_1;
                millis_inicio_estado = millis();
            }
            if (linea_s2()) {
                estado = Estado::DETECTA_LINEA_2;
                millis_inicio_estado = millis();
            }
            if (linea_s3()) {
                estado = Estado::DETECTA_LINEA_3;
                millis_inicio_estado = millis();
            }
            break;

        // ----------------------------------------------------
        case Estado::CENTRANDO_horario:
            centrar_horario();  // 2025: M1/M2 60*c (+), M3 180*c (-)

            // si está apuntando al arco idealmente
            // 2025: ARCO_CONTRINCANTE && haypelota && (abs(Yp - Ycontrincante) <= tol_centrado)
            //   ARCO_CONTRINCANTE → arco_rival_visible() (amarillo)
            //   abs(Yp - Ycontrincante) <= 30 → |arco_rival_angle| <= MIX_TOL_CENTRADO  <RE-TUNEO 2025→2026>
            if (arco_rival_visible() && haypelota &&
                (fabsf(arco_rival_angle_deg()) <= MIX_TOL_CENTRADO)) {
                millis_inicio_centrando = millis();
                millis_inicio_estado = millis();
                estado = Estado::PATEANDO_pausa_inicial;
            }
            // si |error|<=1 y ya pasó 4s centrando
            else if ((millis() - millis_inicio_centrando >= 4000) && (fabsf(error) <= 1)) {
                millis_inicio_centrando = millis();
                millis_inicio_estado = millis();
                estado = Estado::PATEANDO_pausa_inicial;
            }

            // corrección para que orbite bien la pelota
            if (fabsf(anguloPelota) >= MIX_TOL_APUNTADO) {
                millis_inicio_estado = millis();
                estado = Estado::APUNTAR_PELOTA_horario;
            }

            // si deja de ver la pelota por más de 3s
            if (millis() - millis_pelota >= 3000) {
                millis_inicio_estado = millis();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
            }

            // timeout
            if (millis() - millis_inicio_centrando >= 25000) {
                millis_inicio_estado = millis();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
            }

            // salidas líneas blancas (OR de los 3 sensores 2025 → linea_presente)
            if (linea_s1() || linea_s2() || linea_s3()) {
                if (fabsf(error) <= 80) {  // 🛑 2025: tal vez bajar/subir después
                    millis_inicio_estado = millis();
                    estado = Estado::PATEANDO_corto_pausa_inicial;
                } else {
                    millis_inicio_estado = millis();
                    estado = Estado::IMPULSO_CENTRANDO_antihorario;
                }
            }
            break;

        // ----------------------------------------------------
        case Estado::IMPULSO_CENTRANDO_antihorario:
            impulso_centrando_antihorario();  // 2025 inline: M1/M2 60*ic (-), M3 180*ic (+)

            if (millis() - millis_inicio_estado >= 500) {
                millis_inicio_estado = millis();
                estado = Estado::CENTRANDO_antihorario;
            }
            break;

        // ----------------------------------------------------
        case Estado::CENTRANDO_antihorario:
            centrar_antihorario();  // 2025: M1/M2 60*c (-), M3 180*c (+)

            // si está apuntando al arco idealmente
            if (arco_rival_visible() && haypelota &&
                (fabsf(arco_rival_angle_deg()) <= MIX_TOL_CENTRADO)) {  // <RE-TUNEO 2025→2026>
                millis_inicio_centrando = millis();
                millis_inicio_estado = millis();
                estado = Estado::PATEANDO_pausa_inicial;
            }
            // acá se supone que patee al lado opuesto del contrincante
            else if ((millis() - millis_inicio_centrando >= 4000) && (fabsf(error) <= 1)) {
                millis_inicio_centrando = millis();
                millis_inicio_estado = millis();
                estado = Estado::PATEANDO_pausa_inicial;
            }

            // corrección para que orbite bien la pelota
            if (fabsf(anguloPelota) >= MIX_TOL_APUNTADO) {
                millis_inicio_estado = millis();
                estado = Estado::APUNTAR_PELOTA_antihorario;
            }

            // si deja de ver la pelota por más de 3s
            if (millis() - millis_pelota >= 3000) {
                millis_inicio_estado = millis();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
            }

            // timeout
            if (millis() - millis_inicio_centrando >= 25000) {
                millis_inicio_estado = millis();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
            }

            // salidas líneas blancas
            if (linea_s1() || linea_s2() || linea_s3()) {
                if (fabsf(error) <= 80) {
                    millis_inicio_estado = millis();
                    estado = Estado::PATEANDO_corto_pausa_inicial;
                } else {
                    millis_inicio_estado = millis();
                    estado = Estado::IMPULSO_CENTRANDO_horario;
                }
            }
            break;

        // ----------------------------------------------------
        case Estado::IMPULSO_CENTRANDO_horario:
            impulso_centrando_horario();  // 2025 inline: M1/M2 60*ic (+), M3 180*ic (-)

            if (millis() - millis_inicio_estado >= 300) {
                millis_inicio_estado = millis();
                estado = Estado::CENTRANDO_horario;
            }
            break;

        // ----------------------------------------------------
        case Estado::APUNTAR_PELOTA_antihorario:
            if (fabsf(anguloPelota) >= MIX_TOL_APUNTADO) {
                apuntar_pelota_motores();
            } else {
                millis_inicio_estado = millis();
                estado = Estado::CENTRANDO_antihorario;
            }

            // 2025: haypelota && (Xp >= tolerancia_cercania) 🛑 <RE-TUNEO 2025→2026>
            if (haypelota && (dist_pelota_mm() >= MIX_TOL_CERCANIA)) {
                millis_inicio_estado = millis();
                estado = Estado::APUNTAR_PELOTA;
            }

            if (millis() - millis_pelota >= 500) {  // si deja de ver la pelota
                millis_inicio_estado = millis();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
            }

            // 2025: timeout sobre millis_inicio_centrando (NO _estado) — fiel 🛑
            if (millis() - millis_inicio_centrando >= 10000) {
                millis_inicio_estado = millis();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
            }
            break;

        // ----------------------------------------------------
        case Estado::APUNTAR_PELOTA_horario:
            if (fabsf(anguloPelota) >= MIX_TOL_APUNTADO) {
                apuntar_pelota_motores();
            } else {
                millis_inicio_estado = millis();
                estado = Estado::CENTRANDO_horario;
            }

            // 2025: haypelota && (Xp >= tolerancia_cercania) 🛑 <RE-TUNEO 2025→2026>
            if (haypelota && (dist_pelota_mm() >= MIX_TOL_CERCANIA)) {
                millis_inicio_estado = millis();
                estado = Estado::APUNTAR_PELOTA;
            }

            if (millis() - millis_pelota >= 500) {  // si deja de ver la pelota
                millis_inicio_estado = millis();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
            }

            // 2025: timeout sobre millis_inicio_estado (NO _centrando) — fiel
            if (millis() - millis_inicio_estado >= 10000) {
                millis_inicio_estado = millis();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
            }
            break;

        // --- PATADA CORTA ---
        // ----------------------------------------------------
        case Estado::PATEANDO_corto_pausa_inicial:
            parar();
            if (millis() - millis_inicio_estado >= 500) {
                estado = Estado::PATEANDO_corto_adelante;
                millis_inicio_estado = millis();
            }
            break;

        // ----------------------------------------------------
        case Estado::PATEANDO_corto_adelante:
            avanzar_patear();
            if (millis() - millis_inicio_estado >= 200) {
                parar();
                millis_inicio_estado = millis();
                estado = Estado::PATEANDO_corto_pausa;
            }
            break;

        // ----------------------------------------------------
        case Estado::PATEANDO_corto_pausa:
            parar();
            if (millis() - millis_inicio_estado >= 500) {
                estado = Estado::PATEANDO_corto_atras;
                millis_inicio_estado = millis();
            }
            break;

        // ----------------------------------------------------
        case Estado::PATEANDO_corto_atras:
            retroceder_patear();
            if (millis() - millis_inicio_estado >= 300) {
                parar();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
                millis_inicio_estado = millis();
            }
            break;

        // --- PATADA ---
        // ----------------------------------------------------
        case Estado::PATEANDO_pausa_inicial:
            parar();
            if (millis() - millis_inicio_estado >= 1000) {
                estado = Estado::PATEANDO_adelante;
                millis_inicio_estado = millis();
            }

            if (linea_s1()) {
                estado = Estado::DETECTA_LINEA_1;
                millis_inicio_estado = millis();
            }
            if (linea_s2()) {
                estado = Estado::DETECTA_LINEA_2;
                millis_inicio_estado = millis();
            }
            if (linea_s3()) {
                estado = Estado::DETECTA_LINEA_3;
                millis_inicio_estado = millis();
            }
            break;

        // ----------------------------------------------------
        case Estado::PATEANDO_adelante:
            avanzar_patear();
            if (millis() - millis_inicio_estado >= 500) {
                parar();
                millis_inicio_estado = millis();
                estado = Estado::PATEANDO_pausa;
            }

            if (linea_s1()) {
                estado = Estado::DETECTA_LINEA_1;
                millis_inicio_estado = millis();
            }
            if (linea_s2()) {
                estado = Estado::DETECTA_LINEA_2;
                millis_inicio_estado = millis();
            }
            if (linea_s3()) {
                estado = Estado::DETECTA_LINEA_3;
                millis_inicio_estado = millis();
            }
            break;

        // ----------------------------------------------------
        case Estado::PATEANDO_pausa:
            parar();
            if (millis() - millis_inicio_estado >= 500) {
                estado = Estado::PATEANDO_atras;
                millis_inicio_estado = millis();
            }

            if (linea_s1()) {
                estado = Estado::DETECTA_LINEA_1;
                millis_inicio_estado = millis();
            }
            if (linea_s2()) {
                estado = Estado::DETECTA_LINEA_2;
                millis_inicio_estado = millis();
            }
            if (linea_s3()) {
                estado = Estado::DETECTA_LINEA_3;
                millis_inicio_estado = millis();
            }
            break;

        // ----------------------------------------------------
        case Estado::PATEANDO_atras:
            retroceder_patear();
            if (millis() - millis_inicio_estado >= 200) {
                parar();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
                millis_inicio_estado = millis();
            }

            if (linea_s1()) {
                estado = Estado::DETECTA_LINEA_1;
                millis_inicio_estado = millis();
            }
            if (linea_s2()) {
                estado = Estado::DETECTA_LINEA_2;
                millis_inicio_estado = millis();
            }
            if (linea_s3()) {
                estado = Estado::DETECTA_LINEA_3;
                millis_inicio_estado = millis();
            }
            break;

        // ----------------------------------------------------
        case Estado::DETECTA_LINEA_1:
            retroceder1();
            if (millis() - millis_inicio_estado >= 400) {
                parar();
                millis_inicio_estado = millis();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
            }
            break;

        // ----------------------------------------------------
        case Estado::DETECTA_LINEA_2:
            retroceder2();
            if (millis() - millis_inicio_estado >= 400) {
                parar();
                millis_inicio_estado = millis();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
            }
            break;

        // ----------------------------------------------------
        case Estado::DETECTA_LINEA_3:
            retroceder3();
            if (millis() - millis_inicio_estado >= 400) {
                parar();
                millis_inicio_estado = millis();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
            }
            break;
    }
}

}  // namespace mix
}  // namespace iitasoccer
