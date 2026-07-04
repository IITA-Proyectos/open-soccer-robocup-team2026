// main_tech_challenge.cpp — Technical Challenge (RCJ): moverse con heading-hold del BNO.
//
// DOS FUNCIONES DE MOVIMIENTO parametrizadas (potencia + dirección), ambas mantienen el rumbo
// con el BNO (no rotan mientras trasladan):
//   - mover_lateral_bno(potencia, direccion) → strafe (izquierda/derecha).
//   - mover_recto_bno(potencia, direccion)   → adelante/atrás.
// Un SELECTOR DE PRUEBA (TEST_MODO) para elegir cuál correr desde una sola variable, tipo el
// banco de pruebas: cambiás TEST_MODO / TEST_POT / TEST_DIR, flasheás, y probás esa función.
//
// CÓMO (reusa lo de centralmix, NO zirconLib):
//   - HEADING → del BNO del TOP por el WorldSnapshot (Serial7), vía mix_comm
//               (g_io.heading_error_deg). Requiere -DMIX_HEADING_SNAPSHOT (ver el env).
//   - LÍNEA   → de la placa DOWN, vía mix_comm (g_io.line_present). Frena y se queda quieto.
//   - MOTORES → primitivas de mix_motors (mix_set_motor / parar).
//   - CINEMÁTICA (omni-3 a 120°, verificada contra avanzar()/retroceder() de mix_motors):
//       lateral (+X, derecha): M1=+0.5·vx, M2=+0.5·vx, M3=-vx
//       recto   (+Y, adelante): M1=+0.87·vy, M2=-0.87·vy, M3=0
//       giro puro (heading-hold): la MISMA corrección en las 3 ruedas (= ωR).
//
// NO usa el FSM de centralmix — solo mix_comm + mix_motors. Env: central_robot1_tech.
// REQUISITOS HW: TOP conectado (BNO) y DOWN conectado (línea), igual que centralmix.
//
// ⚠️ NO TESTEADO EN HARDWARE. En banco confirmar: (1) que la dirección sea la esperada
// (si va al revés, invertí TEST_DIR); (2) el SIGNO del heading-hold (TECH_CORR_SIGN): si en
// vez de mantener el rumbo se va rotando cada vez más, poné TECH_CORR_SIGN = -1.

#include <Arduino.h>

#include "mix_config.h"
#include "mix_io.h"
#include "mix_comm.h"
#include "mix_motors.h"

using namespace iitasoccer::mix;

// ============================================================
// SELECTOR DE PRUEBA — cambiá esto, flasheá, y probás esa función.
// ============================================================
enum TestModo {
    TEST_LATERAL,   // mover_lateral_bno(TEST_POT, TEST_DIR)  → dir +1=DERECHA / -1=IZQUIERDA
    TEST_RECTO,     // mover_recto_bno(TEST_POT, TEST_DIR)    → dir +1=ADELANTE / -1=ATRÁS
    TEST_QUIETO,    // no se mueve (para chequear sensores por el debug USB)
};
static TestModo  TEST_MODO = TEST_LATERAL;   // ← ELEGÍ la prueba acá
static const int TEST_POT  = 100;            // ← potencia (PWM) de la prueba
static const int TEST_DIR  = +1;             // ← dirección (+1 / -1), ver arriba qué es cada una

// ============================================================
// Perillas del heading-hold (BNO) y arranque.
// ============================================================
static constexpr unsigned long TECH_START_DELAY_MS = 3000;  // espera al encender antes de moverse (0 = ya)
static constexpr float TECH_KP        = 2.0f;   // corrección de rumbo: PWM de giro por GRADO de error
static constexpr int   TECH_CORR_MAX  = 60;     // tope (PWM) del término de giro correctivo
static constexpr int   TECH_CORR_SIGN = +1;     // signo del heading-hold (⚠️ a confirmar en banco: -1 si diverge)
static constexpr float TECH_HDG_TOL   = 1.0f;   // banda muerta del rumbo (grados) → no jitterear

static bool s_stopped = false;   // latch: una vez que ve la línea, se queda quieto (hasta reset)

// ¿Hay línea? (mismo criterio que centralmix: presencia con umbral de profundidad de DOWN).
static inline bool linea_presente_tech() {
    return g_io.line_present && (g_io.line_depth >= MIX_LINE_DEPTH_TRIGGER);
}

// Corrección de rumbo (heading-hold): término de GIRO proporcional al error del BNO, IGUAL en las
// 3 ruedas (= ωR de la cinemática → giro puro, sin traslación parásita). Clampeado + banda muerta.
static int correccion_rumbo() {
    const float err = g_io.heading_error_deg;   // BNO del TOP, wrap180, 0 = rumbo de arranque
    if (err > -TECH_HDG_TOL && err < TECH_HDG_TOL) return 0;   // banda muerta
    int c = (int)(TECH_KP * err) * TECH_CORR_SIGN;
    if (c >  TECH_CORR_MAX) c =  TECH_CORR_MAX;
    if (c < -TECH_CORR_MAX) c = -TECH_CORR_MAX;
    return c;
}

// Escribe las 3 ruedas preservando la DIRECCIÓN: si el pico pasa 255, escala las 3 por el mismo
// factor (no recorta cada una por separado, que torcería el vector). Igual patrón que la patada.
static void escribir_ruedas(int w0, int w1, int w2) {
    int a0 = (w0 < 0) ? -w0 : w0;
    int a1 = (w1 < 0) ? -w1 : w1;
    int a2 = (w2 < 0) ? -w2 : w2;
    int peak = a0; if (a1 > peak) peak = a1; if (a2 > peak) peak = a2;
    if (peak > MIX_MAX_PWM) {
        w0 = (int)((long)w0 * MIX_MAX_PWM / peak);
        w1 = (int)((long)w1 * MIX_MAX_PWM / peak);
        w2 = (int)((long)w2 * MIX_MAX_PWM / peak);
    }
    mix_set_motor(0, w0);
    mix_set_motor(1, w1);
    mix_set_motor(2, w2);
}

// ============================================================
// FUNCIÓN 1 — LATERAL con heading-hold del BNO.
//   potencia  = magnitud del strafe (≈ PWM de la rueda TRASERA, la dominante; las delanteras
//               salen a la MITAD por la cinemática del omni).
//   direccion = +1 → DERECHA   ·   -1 → IZQUIERDA.
// Base cinemática: M1=+0.5·vx, M2=+0.5·vx, M3=-vx  (vx = direccion·potencia). + giro correctivo.
// ============================================================
void mover_lateral_bno(int potencia, int direccion) {
    const int vx   = direccion * potencia;   // + = derecha
    const int corr = correccion_rumbo();
    escribir_ruedas( vx / 2 + corr,          // M1 (delantera IZQ)
                     vx / 2 + corr,          // M2 (delantera DER)
                    -vx     + corr );        // M3 (trasera)
}

// ============================================================
// FUNCIÓN 2 — DERECHO (adelante/atrás) con heading-hold del BNO.
//   potencia  = magnitud (≈ PWM de las DELANTERAS, las dominantes; la trasera solo corrige rumbo).
//   direccion = +1 → ADELANTE   ·   -1 → ATRÁS.
// Base cinemática: M1=+0.87·vy, M2=-0.87·vy, M3=0  (vy = direccion·potencia). + giro correctivo.
// ============================================================
void mover_recto_bno(int potencia, int direccion) {
    const int f    = direccion * potencia;   // + = adelante; PWM de las delanteras
    const int corr = correccion_rumbo();
    escribir_ruedas(  f + corr,              // M1 (delantera IZQ)
                     -f + corr,              // M2 (delantera DER)
                          corr );            // M3 (trasera: solo giro correctivo)
}

// Debug por USB (115200), throttleado. Solo si hay PC (`if (Serial)`).
static void tech_debug() {
    static unsigned long prev = 0;
    const unsigned long now = millis();
    if (now - prev < 150) return;
    prev = now;
    if (!Serial) return;
    Serial.print("modo=");      Serial.print((int)TEST_MODO);
    Serial.print(" pot=");      Serial.print(TEST_POT);
    Serial.print(" dir=");      Serial.print(TEST_DIR);
    Serial.print(" | herr=");   Serial.print(g_io.heading_error_deg, 1);
    Serial.print(" hvalid=");   Serial.print(g_io.heading_valid);
    Serial.print(" corr=");     Serial.print(correccion_rumbo());
    Serial.print(" | line=");   Serial.print(g_io.line_present);
    Serial.print(" | downFresh="); Serial.print(g_io.down_link_fresh);
    Serial.print(" topFresh=");    Serial.print(g_io.top_link_fresh);
    Serial.print(" stopped=");     Serial.println(s_stopped);
}

void setup() {
    Serial.begin(115200);   // debug USB (no espera al host: arranca igual sin PC)
    mix_comm_init();
    mix_motors_init();
}

void loop() {
    mix_comm_tick();   // llena g_io: LÍNEA (DOWN) + HEADING del BNO (TOP snapshot)
    tech_debug();

    // Latch: si YA vio la línea, quedarse quieto (hasta reset/power-cycle).
    if (s_stopped) { parar(); return; }

    // Espera de arranque + SEGURIDAD: no moverse si NO llega la línea de DOWN (si DOWN está
    // desconectado, line_present quedaría siempre false y el robot se iría de largo sin frenar).
    if (millis() < TECH_START_DELAY_MS || !g_io.down_link_fresh) { parar(); return; }

    // Frena al ver la línea (en TODAS las pruebas).
    if (linea_presente_tech()) { s_stopped = true; parar(); return; }

    // Corre la función elegida en el selector.
    switch (TEST_MODO) {
        case TEST_LATERAL: mover_lateral_bno(TEST_POT, TEST_DIR); break;
        case TEST_RECTO:   mover_recto_bno(TEST_POT, TEST_DIR);   break;
        case TEST_QUIETO:  parar();                               break;
    }
}
