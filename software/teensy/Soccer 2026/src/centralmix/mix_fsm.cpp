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
//      de la pelota (0..~100, codificada). En el marco nuevo (+X=der, +Y=adel, en cm)
//      "estar cerca" = distancia al robot pequeña. Se mapea a:
//          dist_pelota_cm() <= MIX_TOL_CERCANIA  (valor 2025 = 50, UNIDADES DISTINTAS:
//          2025 era cuenta codificada, acá es cm → RE-TUNEAR en banco).
//      Su negación `Xp >= tolerancia_cercania` (en APUNTAR_PELOTA_horario/antihorario)
//      se mapea a dist_pelota_cm() >= MIX_TOL_CERCANIA, igual que el 2025.
//   2) `abs(Yp - Ycontrincante) <= tolerancia_centrado` (30): en 2025 era "la pelota
//      está alineada con el arco rival en el eje vertical de la cámara". En el marco
//      nuevo el dato equivalente es el ÁNGULO al arco rival (goal_opp_angle, RESUELTO
//      por el TOP — sin color). Se mapea a:
//          |goal_opp_angle| <= MIX_TOL_CENTRADO  (valor 2025 = 30, UNIDADES DISTINTAS:
//          2025 era diferencia de coordenadas Y; acá son GRADOS → RE-TUNEAR en banco).
//      La guarda ARCO_CONTRINCANTE (2025: hayarco_amarillo) → g_io.goal_opp_visible.
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
// ARCO RIVAL = goal_opp (POR ROL, no por color): la placa TOP resuelve la polaridad
//   (goal_polarity); el delantero NO mira color (ver arco_rival_*() abajo).
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
static Estado estado = Estado::IMPULSO_INICIAL_GIRANDO;  // placeholder; mix_fsm_init() lo fija
static unsigned long millis_inicio_estado    = 0;
static unsigned long millis_inicio_centrando = 0;

// Latch del sentido del giro-encare "pelota atrás" (jugada cámara trasera). 0 = inactivo;
// +1/-1 = girando en ese sentido (congelado al entrar para NO ditherar en ±180). Se RESETEA a 0
// en CADA salida del case APUNTAR_PELOTA (y en mix_fsm_init) para que no quede pegado entre visitas.
static int s_giro_atras_dir = 0;

// heading_inicial (initialYaw del 2025): se captura en mix_fsm_init() desde
// g_io.heading_deg. La FSM consume g_io.heading_error_deg (== 'error' 2025), que
// mix_comm calcula como wrap180(heading_deg - heading_inicial). heading_inicial se
// guarda acá por si mix_comm quisiera leerlo; el cálculo de error lo hace mix_comm.
static float heading_inicial_deg = 0.0f;

// ============================================================
// ARCO RIVAL = goal_opp (POR ROL, NO por color). El 2025 hardcodeaba AMARILLO
// (ARCO_CONTRINCANTE = hayarco_amarillo). Acá el delantero NO mira color: la placa TOP ya
// resolvió cuál arco es el rival (goal_polarity, por ÁNGULO — "el arco al frente es el rival",
// fijado al arranque) y lo entrega como goal_opp en el WorldSnapshot. El delantero apunta el
// pateo a ESE arco, sin importar el color. Se ELIMINÓ la perilla kArcoRivalEsAmarillo / el mapeo
// a yellow/blue: la polaridad vive SÓLO en el TOP. Esto además cierra el bug de inconsistencia
// color↔rol (con kArcoRivalEsAmarillo=false + el mapeo default, arco_rival leía el arco PROPIO).
// ============================================================
static inline bool arco_rival_visible() {
    return g_io.goal_opp_visible;
}
static inline float arco_rival_angle_deg() {
    return g_io.goal_opp_angle;
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

// Distancia robot→pelota (cm, dato CRUDO de la cámara). Euclídea sobre ball_x/y_cm.
static inline float dist_pelota_cm() {
    return sqrtf(g_io.ball_x_cm * g_io.ball_x_cm + g_io.ball_y_cm * g_io.ball_y_cm);
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

// ---- Anti-choque: ¿hay un obstáculo cerca al frente? (ultrasonido HC-SR04 vía g_io.obstacle_mm) ----
// El ultrasonido va montado ALTO → no ve la pelota (baja), sí robots/paredes. 0xFFFF = nada,
// 0 = sin lectura/glitch (ambos = "libre"). Umbral MIX_OBSTACULO_STOP_MM (0 = anti-choque apagado).
static inline bool obstaculo_cerca() {
    const uint16_t d = g_io.obstacle_mm;
    return (MIX_OBSTACULO_STOP_MM > 0) && (d > 0) && (d < 0xFFFF) && (d <= MIX_OBSTACULO_STOP_MM);
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
    mix_set_motor(0, -150);
    mix_set_motor(1, -150);
    mix_set_motor(2, -150);
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
    mix_set_motor(0, +(int)(60.0f * MIX_IC));
    mix_set_motor(1, +(int)(60.0f * MIX_IC));
    mix_set_motor(2, -(int)(180.0f * MIX_IC));
}

// IMPULSO_CENTRANDO_horario (2025): M1/M2 a 60*ic INA0/INB1(+), M3 a 180*ic INA1/INB0(-).
static inline void impulso_centrando_horario() {
    mix_set_motor(0, -(int)(60.0f * MIX_IC));
    mix_set_motor(1, -(int)(60.0f * MIX_IC));
    mix_set_motor(2, +(int)(180.0f * MIX_IC));
}

// ============================================================
// mix_fsm_init — setup de la FSM. El PRIMER estado es SIEMPRE KICKOFF_SEEK (arranque
// del partido). AVANCE_INICIO se QUITÓ 2026-06-21 (pedido Elías). Ningún otro estado
// transiciona a KICKOFF_SEEK → la maniobra de arranque se ejecuta UNA sola vez.
// ============================================================
void mix_fsm_init() {
    estado = Estado::KICKOFF_SEEK;   // PRIMER estado: arranque (seek pelota / medialuna)
    //estado = Estado::TEST; 
    millis_inicio_estado    = millis();
    millis_inicio_centrando = millis();
    s_giro_atras_dir        = 0;     // latch del giro-encare "pelota atrás" arranca limpio

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
    // --- Árbitro RCJ + KICKOFF una vez por "encendido del robot" (pedido Elías) ---
    static bool prev_go       = false;  // para el flanco STOP→GO
    static bool seen_stop     = false;  // ¿vimos un STOP confirmado?
    static bool kickoff_done  = false;  // ¿ya se hizo el kickoff en este "encendido"?
    static bool prev_top_link = false;  // para detectar el power-cycle del robot vía el TOP

    // POWER-CYCLE DEL ROBOT detectado por el TOP (link perdido→recuperado = el TOP rebooteó
    // con la batería). Vuelve la FSM AL ARRANQUE (kickoff). CLAVE para el banco: "cortar la
    // energía" NO siempre resetea la CENTRAL (capacitores/regulador la mantienen viva en el
    // corte breve) → estado y kickoff_done quedaban PEGADOS de la sesión anterior → al
    // iniciar, el robot iba directo a IMPULSO_INICIAL/GIRANDO en vez del kickoff. El TOP SÍ
    // rebootea (saca los sensores ~40 s) → su flanco link-recuperado delata el power-cycle
    // aunque la CENTRAL no se haya reseteado. En competencia (la CENTRAL sí resetea) también
    // aplica (el link arranca stale y se vuelve fresco → mismo flanco). El timer real del
    // medialuna se vuelve a anclar al GO en el bloque go_edge de abajo.
    if (g_io.top_link_fresh && !prev_top_link) {
        estado = Estado::KICKOFF_SEEK;
        //estado = Estado::TEST; 
        millis_inicio_estado    = millis();
        millis_inicio_centrando = millis();
        kickoff_done = false;
        prev_go      = false;
    }
    prev_top_link = g_io.top_link_fresh;

    // STOP: frenar y NO mover. Confirma que vimos un STOP real (base del flanco STOP→GO).
    if (!g_io.match_running) {
        seen_stop = true;
        prev_go = false;
        parar();
        return;
    }

    // GO sin haber visto un STOP antes (match_running venía en true desde el arranque:
    // transitorio del power-up o pin del árbitro pegado) → NO mover hasta un STOP→GO real.
    // Evita que el kickoff se "consuma" en un GO espurio del boot (y de paso, que el robot
    // se mueva solo al encender si el pin del árbitro está pegado).
    if (!seen_stop) {
        parar(); 
        return;
    }

    const bool go_edge = !prev_go;   // flanco STOP→GO (con STOP ya confirmado)
    prev_go = true;

    // KICKOFF: se arma UNA sola vez por encendido, en el PRIMER GO real, con el timer
    // ANCLADO AL GO (no al boot). Así la medialuna corre completa aunque el TOP tarde ~40 s
    // en bootear (fix del bug power-cycle, donde el timer se medía desde el arranque y ya
    // estaba vencido al llegar el GO). En los GOs siguientes (saque tras gol) NO se re-arma:
    // para volver a hacer el kickoff hay que CORTAR Y VOLVER A DAR ENERGÍA.
    if (go_edge && !kickoff_done) {
        estado = Estado::KICKOFF_SEEK;
        //estado = Estado::TEST; 
        millis_inicio_estado    = millis();
        millis_inicio_centrando = millis();
        kickoff_done = true;
    }

    // 'error' del control de rumbo 2025 == g_io.heading_error_deg (ya wrapeado).
    const float error = g_io.heading_error_deg;
    // 'haypelota' 2025 == g_io.ball_visible.
    const bool  haypelota = g_io.ball_visible;
    // 'anguloPelota' 2025 == g_io.angulo_pelota_deg (calculado por mix_comm).
    const float anguloPelota = g_io.angulo_pelota_deg;
    // 'millis_pelota' 2025 == g_io.t_last_ball_seen_ms (lo sella mix_comm).
    const unsigned long millis_pelota = g_io.t_last_ball_seen_ms;

    // --- ANTI-CHOQUE (ultrasonido): obstáculo al frente a <15 cm → RETROCEDER y volver a BUSCAR ---
    // Interrupción de MÁXIMA PRIORIDAD (después del árbitro/kickoff): salta a EVITAR_OBSTACULO desde
    // CUALQUIER estado, SIN excepción (incluido el escape de línea). La pared está justo después de
    // la línea → si el ultrasonido frena antes de la pared, el robot no llega a cruzar la línea.
    // Es seguro porque el ultrasonido es SOLO DE FRENTE: solo interrumpe si hay algo ADELANTE; si la
    // línea está a un costado (nada adelante), obstaculo_cerca()=false y el escape de línea corre
    // normal. Única exclusión: EVITAR_OBSTACULO mismo (si no, reiniciaría el timer y nunca saldría).
    if (obstaculo_cerca() && estado != Estado::EVITAR_OBSTACULO && estado != Estado::DETECTA_LINEA_1 && estado != Estado::DETECTA_LINEA_2 && estado != Estado::DETECTA_LINEA_3) {
        millis_inicio_estado = millis();
        estado = Estado::EVITAR_OBSTACULO;
    }

    switch (estado) {

        case Estado::TEST:

            impulso_centrando_horario(); 
            if (millis() - millis_inicio_estado >= 3000) {
                parar(); 
            } 
            
            break;

        // ----------------------------------------------------
        // KICKOFF_SEEK (AGREGADO 2026): PRIMER estado = arranque del partido. Se ejecuta
        // UNA sola vez (ningún otro estado vuelve a él).
        //   ve la pelota → APUNTAR_PELOTA (va hacia ella);
        //   NO la ve     → impulso FUERTE y CORTO de medialuna → luego GIRANDO;
        //   línea        → escape DETECTA_LINEA_* de siempre (no salir de cancha).
        // ----------------------------------------------------
        
        case Estado::KICKOFF_SEEK:

            if (haypelota) {  // esperar 700ms por la inercia
                parar(); 
                estado = Estado::APUNTAR_PELOTA; 
                millis_inicio_estado = millis(); 
            }
            // línea → escape de siempre (prioridad sobre la medialuna)
            if (linea_s1()) { millis_inicio_estado = millis(); estado = Estado::DETECTA_LINEA_1; break; }
            if (linea_s2()) { millis_inicio_estado = millis(); estado = Estado::DETECTA_LINEA_2; break; }
            if (linea_s3()) { millis_inicio_estado = millis(); estado = Estado::DETECTA_LINEA_3; break; }
            // no ve pelota → impulso FUERTE y CORTO de medialuna hacia el centro
            kickoff_medialuna();
            if (millis() - millis_inicio_estado >= (unsigned long)MIX_KICKOFF_ARC_MS) {
                parar(); 
                millis_inicio_estado = millis();
                estado = Estado::GIRANDO;   // tras el impulso, búsqueda por giro de siempre
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
            
            if (haypelota) {
                parar(); 
                if (millis() - millis_inicio_estado >= 1000) {  // esperar 1s por la inercia
                    estado = Estado::APUNTAR_PELOTA;
                    millis_inicio_estado = millis();
                }
            }
            
            impulso_inicial_girando();

            if (millis() - millis_inicio_estado >= 50) {
                millis_inicio_estado = millis();
                estado = Estado::GIRANDO;
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
                if (millis() - millis_inicio_estado >= 300) {  // esperar 700ms por la inercia
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
            if (fabsf(anguloPelota) >= MIX_TOL_APUNTADO) {   // tolerancia_apuntado (15): aún no apuntado
                // --- Jugada "PELOTA ATRÁS" (cámara trasera) ---
                // Se ENTRA por ball_y_cm (señal MONÓTONA), no por el ángulo (que salta ±180). Una vez
                // adentro, el sentido se LATCHEA (s_giro_atras_dir) y se gira EN EL LUGAR a
                // MIX_ATRAS_PWM (sobre el piso del motor) durante TODO el arco hasta quedar apuntada
                // (|angulo|<15), sin re-leer el signo (anti-±180) ni caer al apuntado lento de 35
                // (zona muerta). Giro PURO (3 ruedas mismo signo) → no traslada la pelota.
                const bool entrar_atras = (g_io.ball_y_cm < -MIX_ATRAS_Y_ENTRA);
                if (s_giro_atras_dir != 0 || entrar_atras) {
                    if (s_giro_atras_dir == 0) {  // LATCH una sola vez al entrar: congela el sentido
                        // sign(anguloPelota)=sign(ball_x) = de qué lado está; el signo físico de +pwm
                        // lo ajusta MIX_ATRAS_DIR_SIGN (a confirmar en banco).
                        s_giro_atras_dir = ((anguloPelota > 0.0f) ? +1 : -1) * MIX_ATRAS_DIR_SIGN;
                    }
                    const int pwm = s_giro_atras_dir * MIX_ATRAS_PWM;
                    mix_set_motor(0, pwm);
                    mix_set_motor(1, pwm);
                    mix_set_motor(2, pwm);
                } else {
                    apuntar_pelota_motores();    // pelota al frente/lateral: apuntado fino 2025 (sin cambios)
                }
            } else {
                s_giro_atras_dir = 0;
                millis_inicio_estado = millis();
                estado = Estado::AVANZANDO;
            }
            g_io.giro_atras_dir = (int8_t)s_giro_atras_dir;  // diagnóstico (debug USB)

            if (millis() - millis_pelota >= 500) {  // si deja de ver la pelota
                s_giro_atras_dir = 0;
                millis_inicio_estado = millis();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
            } 

            if (millis() - millis_inicio_estado >= 10000) {  // timeout
                s_giro_atras_dir = 0;
                millis_inicio_estado = millis();
                estado = Estado::IMPULSO_INICIAL_GIRANDO;
            } 

            if (linea_s1()) {
                s_giro_atras_dir = 0;
                estado = Estado::DETECTA_LINEA_1;
                millis_inicio_estado = millis();
            }
            if (linea_s2()) {
                s_giro_atras_dir = 0;
                estado = Estado::DETECTA_LINEA_2;
                millis_inicio_estado = millis();
            }
            if (linea_s3()) {
                s_giro_atras_dir = 0;
                estado = Estado::DETECTA_LINEA_3;
                millis_inicio_estado = millis();
            }
            break;

        // ----------------------------------------------------
        case Estado::AVANZANDO:
            avanzar();

            // si ya está lo suficientemente cerca de la pelota
            // 2025: haypelota && (Xp <= tolerancia_cercania) <RE-TUNEO 2025→2026>
            if (haypelota && (dist_pelota_cm() <= MIX_TOL_CERCANIA)) {
                millis_inicio_centrando = millis();
                millis_inicio_estado = millis();
                // CAMINO CORTO: elegir el sentido de la órbita para NO darle la vuelta LARGA a la
                // pelota, según de qué lado está el ARCO RIVAL (VISIÓN):
                //   - si se ve el arco → orbitar hacia su lado (signo de goal_opp_angle).
                //     MIX_CENTRAR_SHORT_SIGN = perilla del signo físico (banco); 0 = apagado (viejo).
                //   - si NO se ve el arco (o SHORT_SIGN=0) → horario fijo (comportamiento viejo).
                // (El respaldo por HEADING cuando no se ve el arco lo va a meter Elías a mano.)
                // ⚠️ LÍMITE: el test de signo no es el corto si el arco está casi ATRÁS (|áng|>~135°, wrap).
                if (MIX_CENTRAR_SHORT_SIGN != 0 && arco_rival_visible()) {
                    estado = ((arco_rival_angle_deg() * MIX_CENTRAR_SHORT_SIGN) < 0.0f)
                                 ? Estado::CENTRANDO_antihorario : Estado::CENTRANDO_horario;
                } else {
                    estado = Estado::CENTRANDO_horario;
                }
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
            // si |error|<=1 y ya pasó 6s centrando
            else if ((millis() - millis_inicio_centrando >= 6000) && (fabsf(error) <= 1)) {
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

            if (millis() - millis_inicio_estado >= 700) {
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
            if (haypelota && (dist_pelota_cm() >= MIX_TOL_CERCANIA)) {
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
            if (haypelota && (dist_pelota_cm() >= MIX_TOL_CERCANIA)) {
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
            if (millis() - millis_inicio_estado >= 50) {
                estado = Estado::PATEANDO_corto_adelante;
                millis_inicio_estado = millis();
            }

            break;

        // ----------------------------------------------------
        case Estado::PATEANDO_corto_adelante:
            avanzar_patear();
            if (millis() - millis_inicio_estado >= 300) {
                parar();
                millis_inicio_estado = millis();
                estado = Estado::PATEANDO_corto_pausa;
            }

            break;

        // ----------------------------------------------------
        case Estado::PATEANDO_corto_pausa:
            parar();
            if (millis() - millis_inicio_estado >= 50) {
                estado = Estado::PATEANDO_corto_atras;
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
        case Estado::PATEANDO_corto_atras:
            retroceder_patear();
            if (millis() - millis_inicio_estado >= 500) {
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

        // --- PATADA ---
        // ----------------------------------------------------
        case Estado::PATEANDO_pausa_inicial:
            parar();
            if (millis() - millis_inicio_estado >= 700) {
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

        // ----------------------------------------------------
        // EVITAR_OBSTACULO (rama ultrasonido): el ultrasonido vio algo a <15 cm al frente.
        // RETROCEDE (mismo escape que "línea al frente") por MIX_EVITAR_MS y vuelve a BUSCAR
        // la pelota girando (GIRANDO). Si al terminar el obstáculo sigue cerca, el chequeo de
        // arriba lo re-dispara solo. El ultrasonido NO ve la pelota (montado alto) → esto nunca
        // frena por la pelota, solo por robots/paredes.
        case Estado::EVITAR_OBSTACULO:
            retroceder2();
            if (millis() - millis_inicio_estado >= MIX_EVITAR_MS) {
                parar();
                millis_inicio_estado = millis();
                estado = Estado::GIRANDO;   // vuelve a BUSCAR la pelota
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
    }
}

}  // namespace mix
}  // namespace iitasoccer
