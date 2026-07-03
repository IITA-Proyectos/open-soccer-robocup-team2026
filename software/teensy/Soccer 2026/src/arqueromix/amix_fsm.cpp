// amix_fsm.cpp — Máquina de estados del ARQUERO (MODO QUIETO) para arqueromix.
//
// 2026-06-29 (decisión equipo): se ELIMINÓ el modo PATRULLA. El arquero es SÓLO QUIETO —
// el programa más confiable. Ya NO hay patrulla de lado a lado / rebote por arco/línea /
// profundidad: esos estados (moverce_*, salir_linea_*, avanzar_despues_de_patear) y el flag
// AMIX_QUIETO se borraron. Quedó un solo camino: lateral_izq → homing → quieto (sigue la
// pelota de costado / despeja) → post-patada → acomodar → quieto.
//
// PORT del ciclo ARQUERO de definitivo-arquero_6-9-2026 (FIEL §2). Sólo cambian las
// fuentes (g_aio en vez de sensores locales), las salidas (primitivas amix_motors) y
// se AGREGAN: el gate del árbitro y un timeout de seguridad al retroceso.
//
// MAPEOS QUE REQUIRIERON DECISIÓN (marcados <RE-TUNE> / <RE-VERIFY SIGN>):
//   FIX 2026-06-21 (banco Virginia: la cámara veía la pelota pero el robot NO se movía):
//   el seguimiento de la pelota pasa a ser POR ÁNGULO (atan2), como centralmix usa la cámara,
//   en vez de mm crudos (que caían en banda muerta para la escala sin calibrar → parar()).
//   - "Xp<=140 && abs(Yp)<=3" (cerca+centrada → patea) → dist_pelota_mm()<=CERCANIA_MM &&
//        |angulo_pelota_deg|<=KICK_DEG (ángulo + distancia euclídea). <RE-TUNE distancia>
//   - "abs(Yp)>=5" (desviada → seguir) → |angulo_pelota_deg| > CENTRADO_DEG → strafe al lado.
//   - lado: "Yp<0→der" → angulo_pelota_deg>0 → derecha. <RE-VERIFY SIGN>
//   - "s1>=blanco1 || s2>=blanco2" (borde) y "s1||s2||s3" (volví a la línea)
//        → ambos = line_present (DOWN agrega los 32 sensores en una señal). <SIMPLIFICACIÓN>
//
// ⚠️ NO TESTEADO EN HARDWARE.

#include <Arduino.h>
#include <math.h>

#include "amix_config.h"
#include "amix_io.h"
#include "amix_motors.h"
#include "amix_fsm.h"

namespace iitasoccer {
namespace arqmix {

// Estado + timers (eran globales sueltas en el 2025).
static Estado estado = Estado::inicio_lateral_izq;
static unsigned long millis_inicio_estado = 0;

// Flanco del árbitro: detecta STOP→GO para RE-HACER el homing del área chica en CADA GO
// (banco Virginia 2026-06-21: antes el homing corría una sola vez —el primer GO— porque el FSM
// no se reiniciaba entre STOP y GO sin apagar la batería).
static bool s_was_running = false;

// Ventana de AVANCE al buscar la pelota y tocar línea lateral (modo quieto, banco Virginia 2026-06-22):
// hasta este millis el arquero AVANZA al frente (en vez de strafe lateral) para despegarse del borde sin
// meterse al área. Se (re)arma al detectar línea; persiste AMIX_T_BUSCAR_AVANCE para un avance "más grande".
static unsigned long s_buscar_avance_until_ms = 0;

#ifdef ARQMIX_REHOME_NO_BALL
// RE-HOMING POR PÉRDIDA DE PELOTA (test María 2026-07-03): reloj de referencia — se resetea al VER la pelota y al
// disparar el re-homing, así no re-dispara hasta que pasen otros AMIX_T_REHOME_NO_BALL ms sin verla.
static unsigned long s_rehome_ref_ms = 0;
#endif

#ifdef ARQMIX_KICK_SHORT_ON_LINE
// PATEO CORTO SOBRE LA LÍNEA (gateado, pedido Virginia 2026-06-29, como el delantero centralmix): si el
// despeje ARRANCA con el arquero SOBRE la línea, el golpe es CORTO (menos tiempo → menos envión → no se sale
// de la cancha) en vez del completo. Se setea al ENTRAR a PATEANDO_adelante (en ALINEAR), se usa en el golpe.
static bool s_kick_corto = false;
#endif

// ---- Helpers de lectura (reemplazan las globales seriales/analógicas 2025) ----

// Línea presente (== OR de los 3 sensores 2025; el DOWN ya agrega los 32).
static inline bool linea() {
    return g_aio.line_present && (g_aio.line_depth >= AMIX_LINE_DEPTH_TRIGGER);
}
#ifdef ARQMIX_AVOID_OBSTACLE
// ANTI-CHOQUE: ¿hay un obstáculo cerca al FRENTE? (ultrasonido vía g_aio.obstacle_mm). El ultrasonido va
// montado ALTO → NO ve la pelota, SÍ robots/paredes. 0xFFFF = nada, 0 = sin lectura/glitch (ambos = "libre").
// Umbral ARQMIX_OBST_STOP_MM (0 = anti-choque apagado). Espejo de obstaculo_cerca() del delantero (Elías).
static inline bool obstaculo_cerca() {
    const uint16_t d = g_aio.obstacle_mm;
    return (ARQMIX_OBST_STOP_MM > 0) && (d > 0) && (d < 0xFFFF) && (d <= ARQMIX_OBST_STOP_MM);
}
#endif
// Distancia euclídea robot→pelota (mm) — como dist_pelota_mm() de centralmix.
static inline float dist_pelota_mm() {
    return sqrtf(g_aio.ball_x_mm * g_aio.ball_x_mm + g_aio.ball_y_mm * g_aio.ball_y_mm);
}
// ¿Pelota cerca Y razonablemente al frente → DESPEJAR? (por ÁNGULO + distancia, NO mm crudos).
// FIX 2026-06-21: el 2025 usaba Xp<=140 && abs(Yp)<=3 en píxeles; con mm crudos caía en banda
// muerta y el robot se congelaba. Ahora: distancia euclídea + ángulo de despeje. <RE-TUNE distancia>
static inline bool ball_para_despejar() {
    return g_aio.ball_visible &&
           (dist_pelota_mm() <= AMIX_TOL_CERCANIA_MM) &&
           (fabsf(g_aio.angulo_pelota_deg) <= AMIX_TOL_KICK_DEG);
}
// ¿Pelota ALINEADA al frente (banda muerta ANGULAR angosta → mantener posición)?
static inline bool ball_alineada() {
    return fabsf(g_aio.angulo_pelota_deg) <= AMIX_TOL_CENTRADO_DEG;
}
// ¿La pelota está a la DERECHA? por el SIGNO del ÁNGULO (ang>0 = derecha). <RE-VERIFY SIGN>
static inline bool ball_a_la_derecha() {
    return g_aio.angulo_pelota_deg > 0.0f;
}
// ANTICIPACIÓN (S2): ¿la pelota se MUEVE hacia la derecha del robot? por el SIGNO de vx (flippable, banco).
static inline bool ball_va_a_la_derecha() {
    return (g_aio.ball_vx_mm_s * AMIX_BALL_VX_SIGN) > 0.0f;
}
// ANTICIPACIÓN (S2): ¿la pelota se ACERCA con componente LATERAL significativa? Gate de anticipación. Sólo
// SIGNOS + umbrales crudos (la magnitud no está calibrada): acercándose (vy·signo > umbral) Y lateral
// (|vx| > umbral). Requiere velocidad válida.
static inline bool anticipar_lateral() {
    return g_aio.ball_vel_valid &&
           ((g_aio.ball_vy_mm_s * AMIX_BALL_VY_SIGN) > AMIX_ANTICIPA_VY_MIN) &&
           (fabsf(g_aio.ball_vx_mm_s) > AMIX_ANTICIPA_VX_MIN);
}
// ¿El FRENTE del robot ya apunta al ARCO RIVAL (goal_opp) para despejar HACIA ahí?
// Quién es el arco rival lo resolvió la placa TOP (goal_polarity, por ÁNGULO — sin color); acá
// sólo se usa su ÁNGULO. Si no es visible no se puede alinear → se patea recto (como el 2025).
static inline bool alineado_al_arco_opp() {
    return g_aio.goal_opp_visible &&
           (fabsf(g_aio.goal_opp_angle) <= AMIX_TOL_ARCO_OPP_DEG);
}

// Bang-bang de orientación: gira DESPACIO (al piso AMIX_GIRO_FRENTE_PWM) hasta quedar DE FRENTE al ARCO
// CONTRARIO (goal_opp_angle → 0). La CÁMARA (goal_opp) es la referencia ABSOLUTA al arco — el giroscopio
// (heading_error) DERIVA su cero a lo largo de la secuencia y dejaba al arquero mirando a un lado que NO era
// el arco (banco Virginia 2026-06-22). Fallback al giroscopio SÓLO si NO ve el arco (para no girar a ciegas;
// sin heading válido, da por hecho). Devuelve true cuando YA está de frente (= condición de salir del estado).
static bool orientar_de_frente_tick() {
    if (g_aio.goal_opp_visible) {
        if (fabsf(g_aio.goal_opp_angle) <= AMIX_TOL_ARCO_OPP_DEG) return true;   // de frente al arco contrario
        const int sentido = (g_aio.goal_opp_angle > 0.0f) ? +1 : -1;            // gira DESPACIO hacia el arco
        girar(sentido * AMIX_GIRO_FRENTE_PWM * AMIX_GIRO_ALINEAR_SIGN);
        return false;
    }
    // No ve el arco → fallback giroscopio (heading→0). Sin heading válido → no girar a ciegas (dar por hecho).
    if (!g_aio.heading_valid || (fabsf(g_aio.heading_error_deg) <= AMIX_TOL_ORIENTAR_DEG)) return true;
    const int sentido = (g_aio.heading_error_deg > 0.0f) ? +1 : -1;
    girar(sentido * AMIX_GIRO_FRENTE_PWM * AMIX_GIRO_ALINEAR_SIGN);
    return false;
}

#ifdef ARQMIX_ORIENT_LINE_ESCAPE
// ESCAPE DE LÍNEA AL ORIENTAR (gateado, pedido Virginia 2026-06-29): si el arquero pisa la línea MIENTRAS
// gira para orientarse (puede pasar si fue al borde a buscar la pelota), se DESPEGA del borde antes de seguir
// girando, para no salirse. Mismo criterio de despegue que acomodar_linea (lado OPUESTO a la línea).
static inline void escapar_de_linea(float error) {
    const float la = g_aio.line_angle_deg * AMIX_ACOMODAR_LINEA_SIGN;
    if (fabsf(la) >= AMIX_ACOMODAR_ATRAS_DEG) avanzar();                            // línea atrás → al frente
    else if (la > 0.0f)                       aiproporcional(AMIX_PD_BASE, error);  // línea der → strafe izq
    else                                      adproporcional(AMIX_PD_BASE, error);  // línea izq → strafe der
}
#endif

void amix_fsm_init() {
    // Arranca con un movimiento lateral a la izquierda y después el homing al área chica.
    estado = Estado::inicio_lateral_izq;
    millis_inicio_estado = millis();
#ifdef ARQMIX_REHOME_NO_BALL
    s_rehome_ref_ms = millis();
#endif
}

void amix_fsm_tick() {
    // --- Árbitro RCJ + RE-HOMING en cada GO (banco Virginia 2026-06-21) ---
    if (!g_aio.match_running) {
        parar();
        s_was_running = false;   // quedó parado → el próximo GO re-arranca el homing
        return;
    }
    // Flanco STOP→GO (y el primer GO): reiniciar el FSM SIEMPRE (arranca con el lateral izq).
    if (!s_was_running) {
        s_was_running = true;
        estado = Estado::inicio_lateral_izq;
        millis_inicio_estado = millis();
        s_buscar_avance_until_ms = 0;
#ifdef ARQMIX_REHOME_NO_BALL
        s_rehome_ref_ms = millis();
#endif
    }

    const float error = g_aio.heading_error_deg;   // == 'error' 2025 (ya wrapeado)
    const bool  haypelota = g_aio.ball_visible;     // == 'haypelota' 2025

#ifdef ARQMIX_AVOID_OBSTACLE
    // ANTI-CHOQUE (gateado, pedido María 2026-07-01): si hay un robot cerca al FRENTE (ultrasonido), FRENA y
    // espera — NO se mueve hacia el rival (corta el tick antes del switch → congela el despeje/búsqueda/homing).
    // Cuando el obstáculo se aleja/desaparece, la condición se apaga sola y la FSM sigue normal. Es "frenar y
    // esperar" (decisión María), NO retroceder (a diferencia del delantero). Va DESPUÉS del gate del árbitro y del
    // re-homing (para no romper el manejo de estado), pero ANTES del switch (prioridad sobre el estado actual).
    // DOS AJUSTES de la revisión adversarial 2026-07-01:
    //  (1) `millis_inicio_estado = millis()` "PAUSA" el reloj del estado en curso: sin esto, el tiempo congelado
    //      contaría para el timeout (millis sigue corriendo) → al reanudar, un estado temporizado (golpe 450 ms,
    //      alineación 300 ms) vencería de inmediato y se SALTEARÍA su acción (no patearía / no apuntaría). Al
    //      reiniciar el reloj cada tick congelado, al soltar el obstáculo el estado corre su duración COMPLETA.
    //  (2) EXCLUIR `frenar_patada`: es el contra-empuje ACTIVO (200 PWM atrás) que mata la inercia del golpe para
    //      no salirse. `parar()` es RUEDA LIBRE (coast) → congelarlo dejaría que la inercia del golpe arrastre al
    //      arquero HACIA el obstáculo del frente (lo contrario de evitar). Se deja COMPLETAR el freno activo.
    if (obstaculo_cerca() && estado != Estado::frenar_patada) {
        millis_inicio_estado = millis();   // pausa el timer del estado (no contar el congelamiento al reanudar)
        parar();
        return;
    }
#endif

    switch (estado) {
        // ----------------------------------------------------
        // --- MODO QUIETO: movimiento lateral IZQUIERDA inicial, ANTES del homing (pedido Virginia 2026-06-21) ---
        case Estado::inicio_lateral_izq:
            aiproporcional(AMIX_PD_BASE, error);     // strafe IZQUIERDA a ciegas (con corrección de rumbo)
            if (millis() - millis_inicio_estado >= AMIX_T_INICIO_LATERAL) {  // se movió "un poco"
                parar();
                millis_inicio_estado = millis();
                estado = Estado::inicio_retroceder;  // y AHORA sí, el homing (ir hacia atrás)
            }
            break;

        // ----------------------------------------------------
        // --- INICIO: homing al área chica (banco Virginia 2026-06-21) ---
        case Estado::inicio_retroceder:             // ir HACIA ATRÁS hasta detectar la línea del área
            retroceder_inicio();                     // retroceso dedicado (PWM propio, sentido flippable)
#ifdef ARQMIX_RETRO_BY_TIME
            // MODO POR TIEMPO (test María 2026-07-02): NO lee la línea; retrocede un TIEMPO FIJO y avanza.
            if (millis() - millis_inicio_estado >= AMIX_T_INICIO_RETRO_FIXED) {
                parar();
                millis_inicio_estado = millis();
                estado = Estado::inicio_avanzar;
            }
#else
            // Sale al ver la línea (blanco del área chica). Safety: si NUNCA la ve, avanza igual
            // tras AMIX_T_INICIO_RETRO_SAFETY para no quedarse retrocediendo contra la pared.
            if (linea() || (millis() - millis_inicio_estado >= AMIX_T_INICIO_RETRO_SAFETY)) {
                parar();
                millis_inicio_estado = millis();
                estado = Estado::inicio_avanzar;
            }
#endif
            break;

        case Estado::inicio_avanzar:                // avanzar para SALIR de la línea del área (homing)
            avanzar_inicio();                        // impulso suave a ciegas (75 PWM), RECTO al frente (lejos del fondo)
            {
                // El avance NO termina por reloj: sale cuando cumplió el impulso MÍNIMO **Y** ya despegó
                // de la línea (no pisa blanco), o por TOPE de seguridad (si la línea nunca se "apaga", no
                // quedarse trabado avanzando). Así NUNCA arranca a patrullar pisando el área chica.
                // El MÍNIMO cubre un parpadeo inicial de la línea; recién después se mira !linea().
                const unsigned long dt = millis() - millis_inicio_estado;
                const bool impulso_minimo_ok = dt >= AMIX_T_INICIO_AVANCE_MIN;
                const bool ya_salio_de_linea = !linea();
                const bool safety            = dt >= AMIX_T_INICIO_AVANCE_SAFETY;
                if ((impulso_minimo_ok && ya_salio_de_linea) || safety) {
                    millis_inicio_estado = millis();
                    // ANTES de quedar quieto se ACOMODA (despega de la línea + se orienta de frente).
                    estado = Estado::acomodar_linea;
                }
            }
            break;

        // ----------------------------------------------------
        // --- MODO QUIETO (-DARQMIX_QUIETO): esperar PARADO; seguir la pelota para enfrentarla; patear si cerca ---
        // SIMPLE a propósito: NADA de patrulla / rebote / profundidad (eso generaba movimiento parásito —
        // banco Virginia 2026-06-21). Solo 3 ramas. Reusa la secuencia de despeje (PATEANDO_*).
        case Estado::esperar_quieto:
#ifdef ARQMIX_REHOME_NO_BALL
            // RE-HOMING POR PÉRDIDA DE PELOTA (test María 2026-07-03): si VE la pelota, resetea el reloj; si pasan
            // AMIX_T_REHOME_NO_BALL ms SIN verla, vuelve HACIA ATRÁS hasta la línea + escape (reusa el homing:
            // inicio_retroceder → inicio_avanzar → acomodar_linea → orientar → esperar). Resetea el reloj al
            // disparar (no re-dispara hasta otros AMIX_T_REHOME_NO_BALL ms sin pelota).
            if (haypelota) {
                s_rehome_ref_ms = millis();
            } else if (millis() - s_rehome_ref_ms >= AMIX_T_REHOME_NO_BALL) {
                s_rehome_ref_ms = millis();
                parar();
                millis_inicio_estado = millis();
                estado = Estado::inicio_retroceder;   // atrás hasta la línea + escape → vuelve a esperar
                break;
            }
#endif
            if (haypelota && ball_para_despejar()) {           // CERCA + al frente → DESPEJA (igual que siempre)
                parar();
                millis_inicio_estado = millis();
                estado = Estado::PATEANDO_pausa_inicial;
            } else if (haypelota && !ball_alineada()) {        // DESCENTRADA → buscar la pelota
                // CONSCIENCIA DE LÍNEA al buscar (banco Virginia 2026-06-22): el strafe lateral para
                // enfrentar la pelota NO miraba la línea → se METÍA al área chica de costado. Ahora: si hay
                // LÍNEA (borde del área) NO sigue lateral, AVANZA al frente a buscarla (la aleja del fondo y
                // la acerca a la pelota). El avance es "un poco más grande": al tocar línea (o si DOWN no está
                // fresco) se ARMA una ventana de AMIX_T_BUSCAR_AVANCE ms → sigue avanzando un toque MÁS allá
                // del borde antes de volver al lateral (banco: el avance corto quedaba pegado a la línea).
                if (linea() || !g_aio.down_link_fresh) {
                    s_buscar_avance_until_ms = millis() + AMIX_T_BUSCAR_AVANCE;  // (re)arma la ventana de avance
                }
                if (millis() < s_buscar_avance_until_ms) {
                    avanzar();                                 // avanza al frente (un poco más allá del borde)
                } else if (ball_a_la_derecha()) {
                    adproporcional(AMIX_PD_BALL, error);       // strafe lateral hacia la pelota (sin línea)
                } else {
                    aiproporcional(AMIX_PD_BALL, error);
                }
            } else if (AMIX_ANTICIPA && haypelota && anticipar_lateral()) {
                // ANTICIPACIÓN (S2, gateada -DARQMIX_ANTICIPA): pelota CENTRADA (cae acá porque NO está
                // descentrada) pero VIENE acercándose con componente LATERAL → arranca el strafe hacia el lado
                // del SIGNO de vx ANTES de que el ángulo cruce la banda muerta (gana 1-2 frames = el atraso que
                // la hacía pasar por al lado). Consciente de la LÍNEA (igual que la búsqueda): si toca línea →
                // avanza, no se mete. Fallback: si se descentra, la rama de arriba toma el control; sin
                // velocidad válida no entra (conducta de hoy). El candidato (AMIX_ANTICIPA=false) NO ejecuta esto.
                if (linea() || !g_aio.down_link_fresh) {
                    s_buscar_avance_until_ms = millis() + AMIX_T_BUSCAR_AVANCE;
                }
                if (millis() < s_buscar_avance_until_ms) {
                    avanzar();
                } else if (ball_va_a_la_derecha()) {
                    adproporcional(AMIX_PD_BALL, error);       // la pelota se mueve a la derecha → strafe der
                } else {
                    aiproporcional(AMIX_PD_BALL, error);       // se mueve a la izquierda → strafe izq
                }
            } else {                                           // sin pelota, o ya alineada y sin velocidad lateral → QUIETO
                parar();
            }
            break;

        // --- SECUENCIA DE DESPEJE ---
        // ----------------------------------------------------
        case Estado::PATEANDO_pausa_inicial:        // L1151-1160
            parar();                                 // deja pasar la inercia lateral
            if (millis() - millis_inicio_estado >= AMIX_T_PAT_PAUSA_INI) {  // 200 ms
                millis_inicio_estado = millis();
                estado = Estado::ALINEAR_arco_opp;   // antes de patear: apuntar el frente al ARCO RIVAL
            }
            break;

        // ----------------------------------------------------
        case Estado::ALINEAR_arco_opp:              // NUEVO (pedido Gustavo 2026-06-21): apuntar al ARCO RIVAL
            // Antes de patear, GIRA en el lugar para poner el ARCO RIVAL (goal_opp) al frente; así el
            // despeje (avanzar_patear, que sale RECTO al frente) va DIRIGIDO al arco contrario, alineado,
            // SIN IMPORTAR EL COLOR. Quién es el arco rival lo decidió la placa TOP (goal_polarity, por
            // ÁNGULO). Acotado: si no ve el arco rival, ya está alineado, o se acaba el tiempo → patea
            // igual (fallback robusto = el "recto al frente" del arquero 2025; no demora el despeje).
            if (!g_aio.goal_opp_visible ||
                alineado_al_arco_opp() ||
                (millis() - millis_inicio_estado >= AMIX_T_ALINEAR_OPP)) {
                parar();                             // frena el giro (y resetea la rampa del golpe)
                millis_inicio_estado = millis();
#ifdef ARQMIX_KICK_SHORT_ON_LINE
                s_kick_corto = linea();              // ¿arranca el golpe SOBRE la línea? → golpe CORTO
#endif
                estado = Estado::PATEANDO_adelante;
            } else {
                // Gira hacia el arco: ang>0 (arco a la DERECHA) → girar a la derecha para traerlo al
                // frente. El sentido físico de girar() se RE-VERIFICA en banco (AMIX_GIRO_ALINEAR_SIGN).
                const int sentido = (g_aio.goal_opp_angle > 0.0f) ? +1 : -1;
                girar(sentido * AMIX_GIRO_ALINEAR_PWM * AMIX_GIRO_ALINEAR_SIGN);
            }
            break;

        // ----------------------------------------------------
        case Estado::PATEANDO_adelante:             // L1162-1172
            avanzar_patear();                        // golpe con RAMPA simétrica 0→AMIX_KICK_VEL_FINAL (M1=+vel, M2=-vel, recto al frente)
            // QUIETO: AÚN PATEANDO sigue leyendo la LÍNEA — si la detecta, CORTA el golpe (pedido Virginia
            // 2026-06-22). Pero parar() NO frena el impulso (la inercia del golpe lo sacaba igual) → va a
            // frenar_patada: contra-empuje FUERTE atrás para matar el impulso ANTES de la pausa post-golpe.
            // Patrulla: golpe completo, directo a la pausa (sin cambios).
#ifdef ARQMIX_KICK_SHORT_ON_LINE
            // golpe CORTO si arrancó SOBRE la línea (s_kick_corto): menos tiempo → menos envión → no salirse.
            // Completo si había lugar. La línea + frenar_patada siguen igual (red de seguridad).
            if (linea()) {
                parar();
                millis_inicio_estado = millis();
                estado = Estado::frenar_patada;
            } else if (millis() - millis_inicio_estado >=
                       (s_kick_corto ? AMIX_T_PAT_ADELANTE_CORTO : AMIX_T_PAT_ADELANTE)) {
                parar();
                millis_inicio_estado = millis();
                estado = Estado::PATEANDO_pausa;
            }
#else
            if (linea()) {
                parar();                             // cierra la rampa del golpe
                millis_inicio_estado = millis();
                estado = Estado::frenar_patada;      // FRENO ACTIVO antes de seguir
            } else if (millis() - millis_inicio_estado >= AMIX_T_PAT_ADELANTE) {  // 450 ms (golpe completo)
                parar();
                millis_inicio_estado = millis();
                estado = Estado::PATEANDO_pausa;
            }
#endif
            break;

        // ----------------------------------------------------
        // --- MODO QUIETO: FRENO ACTIVO tras detectar línea pateando (pedido Virginia 2026-06-22) ---
        case Estado::frenar_patada:
            // Contra-empuje FUERTE hacia atrás (frenar_atras, AMIX_FRENO_PATADA_PWM) por un tiempo CORTO para
            // MATAR el impulso del golpe y despegarse de la línea → no salirse de la cancha. NO chequea línea
            // (está SOBRE ella; pararía al toque) → corta por tiempo. Después sigue la secuencia post-patada.
            frenar_atras();
            if (millis() - millis_inicio_estado >= AMIX_T_FRENO_PATADA) {
                parar();
                millis_inicio_estado = millis();
                estado = Estado::PATEANDO_pausa;     // sigue: pausa → orientar → retroceder → quieto
            }
            break;

#ifdef ARQMIX_RETRO_BRAKE_ON_LINE
        // ----------------------------------------------------
        // --- MODO QUIETO: ESCAPE HACIA ADELANTE tras tocar la línea volviendo del pateo (gateado, María 2026-07-01) ---
        case Estado::escapar_adelante:
            // Al VOLVER del pateo el retroceso pisó la línea; en vez de un freno por TIEMPO (a ciegas), AVANZA al
            // frente HASTA que DEJA de pisar la línea — EXACTAMENTE como el homing (inicio_avanzar): usa la LÍNEA
            // para saber cuándo parar de escapar, no un reloj. Sale con impulso MÍNIMO (cubre el parpadeo de la
            // línea) + ya NO pisa línea, o por RED de seguridad (si la línea nunca se apaga, no quedar trabado
            // avanzando). Reusa las MISMAS constantes y primitiva del homing (avanzar_inicio / AMIX_T_INICIO_AVANCE_*).
            avanzar_inicio();                            // avance suave (75 PWM) RECTO al frente (despegarse de la línea)
            {
                const unsigned long dt            = millis() - millis_inicio_estado;
                const bool impulso_minimo_ok      = dt >= AMIX_T_INICIO_AVANCE_MIN;
                const bool ya_salio_de_linea      = !linea();
                const bool safety                 = dt >= AMIX_T_INICIO_AVANCE_SAFETY;
                if ((impulso_minimo_ok && ya_salio_de_linea) || safety) {
                    millis_inicio_estado = millis();
                    estado = Estado::acomodar_linea;     // ya despegado de la línea → acomodar → quieto
                }
            }
            break;
#endif

        // ----------------------------------------------------
        case Estado::PATEANDO_pausa:                // L1174-1182
            parar();
            if (millis() - millis_inicio_estado >= AMIX_T_PAT_PAUSA) {  // 1000 ms
                millis_inicio_estado = millis();
                // Primero GIRA LENTO buscando el cero (cámara/giroscopio), después vuelve atrás.
                estado = Estado::orientar_frente;
            }
            break;

        // ----------------------------------------------------
        case Estado::PATEANDO_atras:                // L1184-1195 (retroceso recto)
            // FASE 2a (banco Virginia 2026-06-21): retroceso RECTO hasta la línea. ANTES en quieto usaba
            // retroceder_rumbo_opp (corrección por CÁMARA, poca autoridad) que lo DESVIABA → se salía de la
            // cancha / se metía al área. Ahora va DERECHO; el robot ya quedó MIRANDO al oponente (orientar
            // por giroscopio), así que recto = derecho hacia su arco → para en la línea del borde del área.
            // QUIETO: retroceso LENTO (AMIX_ATRAS_QUIETO) para parar JUSTO en la línea sin cruzarla (a 120 se
            // metía por inercia/latencia). SEGURIDAD (pedido Virginia "nunca salirse de la cancha"): si el
            // enlace con DOWN NO está fresco → NO retroceder a ciegas (sin dato de línea confiable, FRENA).
            {
#ifdef ARQMIX_RETRO_BY_TIME
                // MODO POR TIEMPO (test María 2026-07-02, -DARQMIX_RETRO_BY_TIME): NO lee la línea; retrocede un
                // TIEMPO FIJO (AMIX_T_ATRAS_FIXED) y pasa a acomodar. Para cancha donde la línea no se detecta
                // confiable. El resto de la secuencia post-pateo (acomodar → orientar → quieto) no cambia.
                retroceder_quieto();
                if (millis() - millis_inicio_estado >= AMIX_T_ATRAS_FIXED) {
                    parar();
                    millis_inicio_estado = millis();
                    estado = Estado::acomodar_linea;
                }
#elif defined(ARQMIX_RETRO_BRAKE_ON_LINE)
                // RETROCESO COMO EL HOMING (banco María 2026-07-01): vuelve HASTA DETECTAR la línea y NADA MÁS.
                // Las condiciones extra que tenía (gate de frescura de DOWN + safety corto de 4 s) lo cortaban
                // ANTES de pisar la línea → el escape sobre la línea no llegaba a actuar → se metía "no siempre
                // pero muchas veces". Ahora es INCONDICIONAL como inicio_retroceder (el retroceso del arranque,
                // que "siempre anda bien"): retroceder_quieto() hasta linea() → escapar_adelante. El safety queda
                // SÓLO como red anti-cuelgue GRANDE (AMIX_T_ATRAS_SAFETY_RETRO ~ el del homing): no debería
                // dispararse en juego; sólo si DOWN muere y la línea nunca se ve (para no retroceder infinito).
                retroceder_quieto();
                if (linea()) {
                    parar();
                    millis_inicio_estado = millis();
                    estado = Estado::escapar_adelante;   // pisó la línea → avanza al frente hasta despegarse → acomodar
                } else if (!g_aio.down_link_fresh) {
                    // SEGURIDAD (revisión adversarial 2026-07-01): si el enlace con DOWN MUERE a mitad del retroceso,
                    // line_present queda CONGELADO (amix_comm apply_down_line nunca lo resetea al perder el enlace) →
                    // sin este corte el retroceso CIEGO correría hasta AMIX_T_ATRAS_SAFETY_RETRO (50 s) y se saldría de
                    // la cancha. NO es un corte de juego normal (en juego DOWN llega fresco → sigue "hasta la línea");
                    // sólo actúa si el sensor de línea está MUERTO (ahí "hasta la línea" es imposible). Espejo de lo
                    // que ya hacía la rama sin flag (if !down_ok → parar). El equipo lo confirma en banco.
                    parar();
                    millis_inicio_estado = millis();
                    estado = Estado::acomodar_linea;
                } else if (millis() - millis_inicio_estado >= AMIX_T_ATRAS_SAFETY_RETRO) {  // red anti-cuelgue (última)
                    parar();
                    millis_inicio_estado = millis();
                    estado = Estado::acomodar_linea;
                }
#else
                const bool down_ok = g_aio.down_link_fresh;   // ¿la línea (DOWN) llega fresca? (<500 ms)
                if (down_ok) retroceder_quieto();             // retroceso lento hacia la línea
                else         parar();                         // sin línea fresca → no retroceder a ciegas
                // Sale al PISAR la línea (la lee cada tick; amix_comm la refresca antes del FSM). También si
                // DOWN se cae (no seguir a ciegas) o por safety. <MEJORA 2026: safety + frescura>
                const bool sin_linea_confiable = !down_ok;
                if (linea() || sin_linea_confiable ||
                    (millis() - millis_inicio_estado >= AMIX_T_ATRAS_SAFETY)) {  // 4000 ms safety
                    parar();
                    millis_inicio_estado = millis();
                    // ANTES de quedar quieto se ACOMODA (despega de la línea + se orienta de frente).
                    estado = Estado::acomodar_linea;
                }
#endif
            }
            break;

        // ----------------------------------------------------
        // --- MODO QUIETO: orientar de frente al ARCO CONTRARIO tras el despeje (pedido Virginia 2026-06-22) ---
        case Estado::orientar_frente:
            // La patada dejó al robot GIRADO. Lo re-orienta a quedar DE FRENTE al ARCO CONTRARIO girando
            // DESPACIO (orientar_de_frente_tick). Usa la CÁMARA (goal_opp) como referencia ABSOLUTA al arco
            // —el giroscopio derivaba su cero y lo dejaba mirando a un lado—, con fallback al giroscopio si
            // no ve el arco. Cuando queda de frente (o safety) → retrocede.
#ifdef ARQMIX_ORIENT_LINE_ESCAPE
            // Si pisa la línea girando → ESCAPA del borde antes de seguir orientando (no salirse). Mientras
            // no venza el safety; al vencerlo, orienta/sale igual (no quedarse trabado escapando).
            if (linea() && g_aio.down_link_fresh &&
                (millis() - millis_inicio_estado < AMIX_T_ORIENTAR_SAFETY)) {
                escapar_de_linea(error);
                break;
            }
#endif
            if (orientar_de_frente_tick() ||
                (millis() - millis_inicio_estado >= AMIX_T_ORIENTAR_SAFETY)) {
                parar();
                millis_inicio_estado = millis();
                estado = Estado::PATEANDO_atras;     // de frente al arco contrario → vuelve para atrás
            }
            break;

        // ----------------------------------------------------
        // --- MODO QUIETO: ACOMODARSE antes de quedar quieto (pedido Virginia 2026-06-22) ---
        // (1) DESPEGARSE de la línea (no quedar quieto sobre la línea / dentro del área).
        case Estado::acomodar_linea:
            // Si NO toca línea (o DOWN no fresco, o safety) → ya está → corregir la ORIENTACIÓN.
            if (!linea() || !g_aio.down_link_fresh ||
                (millis() - millis_inicio_estado >= AMIX_T_ACOMODAR_LINEA_SAFETY)) {
                parar();
                millis_inicio_estado = millis();
                estado = Estado::acomodar_orientar;
            } else {
                // Despegarse "un poco" hacia el lado OPUESTO a la línea. line_angle_deg: 0=frente, >0=der,
                // <0=izq, ±180=atrás (convención DOWN — A VALIDAR; signo flippable con AMIX_ACOMODAR_LINEA_SIGN).
                // Línea ATRÁS → AVANZAR; línea a la DERECHA → strafe IZQ; a la IZQUIERDA → strafe DER.
                const float la = g_aio.line_angle_deg * AMIX_ACOMODAR_LINEA_SIGN;
                if (fabsf(la) >= AMIX_ACOMODAR_ATRAS_DEG) {
                    avanzar();                                   // línea atrás → despegarse al frente
                } else if (la > 0.0f) {
                    aiproporcional(AMIX_PD_BASE, error);         // línea a la derecha → strafe izquierda
                } else {
                    adproporcional(AMIX_PD_BASE, error);         // línea a la izquierda → strafe derecha
                }
            }
            break;

        // (2) Quedar DE FRENTE al ARCO CONTRARIO antes de quedar quieto: gira DESPACIO hacia el arco
        //     (orientar_de_frente_tick, MISMO criterio que orientar_frente) → así "siempre que se queda
        //     quieto, mira de frente al arco contrario". Cámara = referencia absoluta; giroscopio = fallback.
        case Estado::acomodar_orientar:
#ifdef ARQMIX_ORIENT_LINE_ESCAPE
            // Si pisa la línea girando → ESCAPA del borde antes de seguir orientando (no salirse / no quedar
            // quieto sobre la línea). Mientras no venza el safety; al vencerlo, orienta/sale igual.
            if (linea() && g_aio.down_link_fresh &&
                (millis() - millis_inicio_estado < AMIX_T_ORIENTAR_SAFETY)) {
                escapar_de_linea(error);
                break;
            }
#endif
            if (orientar_de_frente_tick() ||
                (millis() - millis_inicio_estado >= AMIX_T_ORIENTAR_SAFETY)) {
                parar();
                millis_inicio_estado = millis();
                estado = Estado::esperar_quieto;             // quieto, de frente al arco contrario
            }
            break;
    }
}

}  // namespace arqmix
}  // namespace iitasoccer
