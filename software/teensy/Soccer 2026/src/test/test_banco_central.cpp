// test_banco_central.cpp — BANCO de pruebas para la placa CENTRAL real (Teensy).
//
// UN solo programa con ESTADOS: cada estado es un test que IMPRIME por USB justo la
// información que ese test necesita y, si hace falta, mueve los motores DESPACIO (modo
// lento, techo de PWM). Vos elegís el test cambiando la variable `estado` (y re-flasheás),
// o —más cómodo— tecleando el número del test en el Monitor Serie (no hace falta recompilar).
//
// CÓMO USARLO
//   1) Flashear el env del test (carpeta AUTOCONTENIDA src/test/, ver platformio.ini):
//        pio run -e central_robot1_test -t upload
//   2) Abrí el Monitor Serie a 115200:   pio device monitor -b 115200
//   3) El robot arranca en T_SENSORES (no se mueve). Para cambiar de test: tecleá el dígito
//      (0..9) y Enter, o cambiá `estado` abajo y re-flasheá.
//
// SEGURIDAD
//   - Los tests que NO mueven llaman parar() siempre.
//   - Los tests que mueven usan el MODO LENTO de mix_motors (techo MIX_LENTO_MAX_PWM, 30 PWM).
//     Si el robot zumba y no se mueve, NO está roto: subí MIX_LENTO_MAX_PWM (ver mix_motors.cpp).
//   - Probá los tests de motor con el robot LEVANTADO (ruedas en el aire) la primera vez.
//
// ⚠️ NO TESTEADO EN HARDWARE. Solo el equipo cierra una TASK de banco.

#include <Arduino.h>
#include <math.h>

#include "mix_config.h"
#include "mix_io.h"
#include "mix_comm.h"
#include "mix_motors.h"

// Esta carpeta src/test/ es AUTOCONTENIDA: trae mix_seguir, mix_mover_vector y los campos
// de velocidad (ball + otos) ya cableados → los 3 flags van en 1 por defecto y TODOS los
// tests andan. Si reusás este .cpp contra el centralmix BASE (que NO tiene mix_seguir ni
// mix_mover_vector ni otos_vx), poné el flag que corresponda en 0 (o por build flag -D...).

// T_ESTRATEGIA (test 9) — corre mix_seguir completo.
#ifndef TEST_CON_SEGUIR
#define TEST_CON_SEGUIR 1
#endif
#if TEST_CON_SEGUIR
#include "mix_seguir.h"
#endif

// T_AVANZAR / T_GIRO_LUGAR (tests 7 y 8) — usan mix_mover_vector (primitiva holonómica).
#ifndef TEST_TIENE_MOVER_VECTOR
#define TEST_TIENE_MOVER_VECTOR 1
#endif

// Prints de velocidad de T_BALL_VEL / T_OTOS_VEL (tests 2 y 3): campos ball_vx/otos_vx en MixIO.
#ifndef TEST_TIENE_VELOCIDADES
#define TEST_TIENE_VELOCIDADES 1
#endif

using namespace iitasoccer::mix;

// ============================================================
//   ELEGÍ EL TEST ACÁ  (o tecleá el dígito en el Monitor Serie)
// ============================================================
enum Test {
    T_SENSORES   = 0,  // imprime TODO, NO mueve. Mové la pelota y girá el robot a mano.
    T_HEADING    = 1,  // heading BNO + error, NO mueve. Girá el robot a mano: ¿signo? ¿deriva?
    T_BALL_VEL   = 2,  // velocidad de la pelota, NO mueve. Empujá la pelota a mano.
    T_OTOS_VEL   = 3,  // velocidad del ROBOT por OTOS, NO mueve. Empujá el robot a mano.
    T_MOTOR_1    = 4,  // SOLO M1 despacio. ¿Gira? ¿para qué lado?
    T_MOTOR_2    = 5,  // SOLO M2 despacio.
    T_MOTOR_3    = 6,  // SOLO M3 despacio.
    T_AVANZAR    = 7,  // traslación ADELANTE despacio. ¿Va derecho o sale de costado?
    T_GIRO_LUGAR = 8,  // giro PURO en el lugar despacio. ¿Para qué lado gira?
    T_ESTRATEGIA = 9   // corre mix_seguir COMPLETO despacio (todo junto).
};

Test estado = T_SENSORES;   // <<<<<<<<<< CAMBIÁ ESTE VALOR Y RE-FLASHEÁ (o tecleá 0..9)

// PWM de los tests de motor (T_MOTOR_*, T_AVANZAR, T_GIRO_LUGAR). El modo lento de
// mix_motors lo recorta a MIX_LENTO_MAX_PWM igual; este valor es por si lo apagás.
static const int TEST_PWM = 80;

// ============================================================
//   Utilidades
// ============================================================
static inline float wrap180(float d){ while(d>180)d-=360; while(d<-180)d+=360; return d; }

static unsigned long s_print_prev = 0;
static const unsigned long PRINT_PERIOD_MS = 150;   // ~6,7 Hz, no floodea
static Test s_estado_prev = (Test)255;              // para imprimir el encabezado al cambiar

// ¿pasaron PRINT_PERIOD_MS? (throttle del print)
static bool toca_imprimir(){
    const unsigned long now = millis();
    if (now - s_print_prev < PRINT_PERIOD_MS) return false;
    s_print_prev = now;
    return true;
}

// Encabezado (una vez por cambio de estado): qué hace el test y qué mirar.
static void imprimir_encabezado(){
    if (estado == s_estado_prev) return;
    s_estado_prev = estado;
    Serial.println();
    Serial.print("======== TEST "); Serial.print((int)estado); Serial.print("  ");
    switch (estado){
        case T_SENSORES:   Serial.println("SENSORES (no mueve) ========");
            Serial.println("Mové la pelota y girá el robot a mano. Mirá que TODO cambie con sentido fisico."); break;
        case T_HEADING:    Serial.println("HEADING BNO (no mueve) ========");
            Serial.println("Girá el robot a la DERECHA. ¿hdg sube o baja? Quieto: ¿deriva con el tiempo?"); break;
        case T_BALL_VEL:   Serial.println("VELOCIDAD PELOTA (no mueve) ========");
            Serial.println("Empujá la pelota a mano. vx>0=a la derecha, vy>0=alejandose. ¿Se mueve el dato?"); break;
        case T_OTOS_VEL:   Serial.println("VELOCIDAD ROBOT / OTOS (no mueve) ========");
            Serial.println("Empujá el robot a su DERECHA: otos_vx debe dar + y otos_vy ~0. Adelante: vy +."); break;
        case T_MOTOR_1:    Serial.println("MOTOR 1 solo (LEVANTAR robot) ========");
            Serial.println("Solo M1 debe girar. Anotá para que lado (+TEST_PWM)."); break;
        case T_MOTOR_2:    Serial.println("MOTOR 2 solo (LEVANTAR robot) ========");
            Serial.println("Solo M2 debe girar."); break;
        case T_MOTOR_3:    Serial.println("MOTOR 3 solo (LEVANTAR robot) ========");
            Serial.println("Solo M3 debe girar."); break;
        case T_AVANZAR:    Serial.println("AVANZAR adelante despacio ========");
            Serial.println("Debe ir DERECHO hacia adelante. Si sale de costado: revisar cinematica/signos."); break;
        case T_GIRO_LUGAR: Serial.println("GIRO en el lugar despacio ========");
            Serial.println("Debe girar SOBRE SU EJE sin trasladarse. Anotá el sentido."); break;
        case T_ESTRATEGIA: Serial.println("ESTRATEGIA seguir COMPLETA despacio ========");
            Serial.println("Corre mix_seguir entero. Poné la pelota adelante y miralo perseguir/escoltar."); break;
    }
    Serial.println("(Tecleá 0..9 para cambiar de test)");
}

// Lee el Monitor Serie: si tecleás un dígito 0..9, cambia el test en vivo.
static void leer_serial(){
    while (Serial.available()){
        const int c = Serial.read();
        if (c >= '0' && c <= '9') estado = (Test)(c - '0');
    }
}

// ============================================================
//   Prints por test
// ============================================================
static void print_sensores(){
    const float dist = sqrtf(g_io.ball_x_cm*g_io.ball_x_cm + g_io.ball_y_cm*g_io.ball_y_cm);
    Serial.print("PELOTA vis="); Serial.print(g_io.ball_visible);
    Serial.print(" x=");  Serial.print(g_io.ball_x_cm,0);
    Serial.print(" y=");  Serial.print(g_io.ball_y_cm,0);
    Serial.print(" ang=");Serial.print(g_io.angulo_pelota_deg,0);
    Serial.print(" d=");  Serial.print(dist,0);
    Serial.print(" | ARCO vis="); Serial.print(g_io.goal_opp_visible);
    Serial.print(" ang=");        Serial.print(g_io.goal_opp_angle,0);
    Serial.print(" dist=");       Serial.print(g_io.goal_opp_dist,0);
    Serial.print(" | HDG=");      Serial.print(g_io.heading_deg,1);
    Serial.print(" val=");        Serial.print(g_io.heading_valid);
    Serial.print(" err=");        Serial.print(g_io.heading_error_deg,1);
    Serial.print(" | LINEA p=");  Serial.print(g_io.line_present);
    Serial.print(" | match=");    Serial.print(g_io.match_running);
    Serial.print(" topFresh=");   Serial.print(g_io.top_link_fresh);
    Serial.print(" downFresh=");  Serial.println(g_io.down_link_fresh);
}

static void print_heading(){
    Serial.print("HDG="); Serial.print(g_io.heading_deg,2);
    Serial.print("  valid="); Serial.print(g_io.heading_valid);
    Serial.print("  error_vs_inicial="); Serial.print(g_io.heading_error_deg,2);
    Serial.print("  | otos_hdg="); Serial.print(g_io.otos_heading_deg,2);
    Serial.print(" otos_conf="); Serial.println(g_io.otos_confidence);
}

static void print_ball_vel(){
    Serial.print("PELOTA vis="); Serial.print(g_io.ball_visible);
    Serial.print(" x=");  Serial.print(g_io.ball_x_cm,0);
    Serial.print(" y=");  Serial.print(g_io.ball_y_cm,0);
#if TEST_TIENE_VELOCIDADES
    Serial.print(" | vx="); Serial.print(g_io.ball_vx_cm_s,1);
    Serial.print(" vy=");   Serial.print(g_io.ball_vy_cm_s,1);
    const float sp = sqrtf(g_io.ball_vx_cm_s*g_io.ball_vx_cm_s + g_io.ball_vy_cm_s*g_io.ball_vy_cm_s);
    Serial.print(" |v|=");  Serial.println(sp,1);
#else
    Serial.println(" | (velocidad: falta plumbing guia 4.1/8 -> TEST_TIENE_VELOCIDADES 1)");
#endif
}

static void print_otos_vel(){
    Serial.print("OTOS conf="); Serial.print(g_io.otos_confidence);
    Serial.print(" hdg=");      Serial.print(g_io.otos_heading_deg,1);
#if TEST_TIENE_VELOCIDADES
    Serial.print(" | vx="); Serial.print(g_io.otos_vx_cm_s,1);
    Serial.print(" vy=");   Serial.print(g_io.otos_vy_cm_s,1);
    const float v = sqrtf(g_io.otos_vx_cm_s*g_io.otos_vx_cm_s + g_io.otos_vy_cm_s*g_io.otos_vy_cm_s);
    Serial.print(" |v|=");  Serial.print(v,1);
    if (v > 1.0f){ Serial.print(" dir_real="); Serial.print(atan2f(g_io.otos_vx_cm_s,g_io.otos_vy_cm_s)*57.29578f,0); }
    Serial.print(" slip="); Serial.println(g_io.otos_slip);
#else
    Serial.println(" | (velocidad: falta plumbing guia 8 -> TEST_TIENE_VELOCIDADES 1)");
#endif
}

// ============================================================
void setup(){
    Serial.begin(115200);
    mix_comm_init();
    mix_motors_init();
#if TEST_CON_SEGUIR
    mix_seguir_init();
#endif
}

void loop(){
    mix_comm_tick();    // SIEMPRE: mantiene g_io fresco + watchdog de enlaces
    leer_serial();      // cambio de test en vivo (tecla 0..9)
    imprimir_encabezado();

    const bool toca = toca_imprimir();

    switch (estado){
        // ---- Tests que NO mueven (parar() + imprimir) ----
        case T_SENSORES: parar(); if(toca) print_sensores(); break;
        case T_HEADING:  parar(); if(toca) print_heading();  break;
        case T_BALL_VEL: parar(); if(toca) print_ball_vel(); break;
        case T_OTOS_VEL: parar(); if(toca) print_otos_vel(); break;

        // ---- Tests de MOTOR individual (despacio; levantar el robot) ----
        case T_MOTOR_1:
            mix_set_motor(0, TEST_PWM); mix_set_motor(1, 0); mix_set_motor(2, 0);
            if(toca){ Serial.println("M1 = +TEST_PWM (capado por modo lento). Solo M1 debe girar."); } break;
        case T_MOTOR_2:
            mix_set_motor(0, 0); mix_set_motor(1, TEST_PWM); mix_set_motor(2, 0);
            if(toca){ Serial.println("M2 = +TEST_PWM. Solo M2 debe girar."); } break;
        case T_MOTOR_3:
            mix_set_motor(0, 0); mix_set_motor(1, 0); mix_set_motor(2, TEST_PWM);
            if(toca){ Serial.println("M3 = +TEST_PWM. Solo M3 debe girar."); } break;

        // ---- Tests de movimiento de CUERPO (usan mix_mover_vector) ----
        case T_AVANZAR:
#if TEST_TIENE_MOVER_VECTOR
            mix_mover_vector(0.0f, TEST_PWM, 0);   // 0° = adelante, sin giro
            if(toca){ Serial.println("AVANZAR adelante. ¿Va derecho? (mira la deriva lateral)"); }
#else
            parar(); if(toca) Serial.println("T_AVANZAR: falta mix_mover_vector (TEST_TIENE_MOVER_VECTOR 0).");
#endif
            break;
        case T_GIRO_LUGAR:
#if TEST_TIENE_MOVER_VECTOR
            mix_mover_vector(0.0f, 0, TEST_PWM);   // speed 0, solo omega = giro puro
            if(toca){ Serial.println("GIRO en el lugar. ¿Para que lado? ¿se traslada algo?"); }
#else
            parar(); if(toca) Serial.println("T_GIRO_LUGAR: falta mix_mover_vector (TEST_TIENE_MOVER_VECTOR 0).");
#endif
            break;

        // ---- Estrategia completa ----
        case T_ESTRATEGIA:
#if TEST_CON_SEGUIR
            mix_seguir_tick();
            if(toca){
                Serial.print("ESTADO="); Serial.print(mix_seguir_estado_nombre());
                Serial.print(" dist="); Serial.print(mix_seguir_dist_cm(),0);
                Serial.print(" aim=");  Serial.print(mix_seguir_aim_deg(),0);
                Serial.print(" | pelota vis="); Serial.print(g_io.ball_visible);
                Serial.print(" ang="); Serial.print(g_io.angulo_pelota_deg,0);
                Serial.print(" | hdg="); Serial.println(g_io.heading_deg,0);
            }
#else
            parar();
            if(toca) Serial.println("T_ESTRATEGIA deshabilitado: copiá mix_seguir y poné TEST_CON_SEGUIR 1.");
#endif
            break;
    }
}
