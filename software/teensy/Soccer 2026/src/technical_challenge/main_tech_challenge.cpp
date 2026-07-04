// main_tech_challenge.cpp — Technical Challenge (RCJ): STRAFE a la DERECHA con heading-hold
// del BNO, y FRENA cuando ve una línea (se queda quieto).
//
// QUÉ HACE:
//   Se desplaza LATERALMENTE a la DERECHA manteniendo el rumbo (no rota) gracias al BNO, y
//   apenas la placa DOWN detecta una LÍNEA, para y se queda quieto.
//
// CÓMO (reusa lo de centralmix, NO zirconLib):
//   - LÍNEA  → de la placa DOWN, vía mix_comm (g_io.line_present / line_depth). IGUAL que
//              centralmix hoy.
//   - HEADING→ del BNO del TOP por el WorldSnapshot (Serial7), vía mix_comm
//              (g_io.heading_error_deg). Requiere -DMIX_HEADING_SNAPSHOT (ver el env).
//   - MOTORES→ primitivas de mix_motors (mix_set_motor / parar).
//   - La LÓGICA de moverse lateralmente con corrección de rumbo es el port de adproporcional()
//     del delantero/arquero 2025 (3 bandas según el signo del error), pero leyendo el error del
//     BNO del TOP en vez de un BNO local.
//
// NO usa el FSM de centralmix (mix_fsm) — solo mix_comm + mix_motors. Se compila con el env
// central_robot1_tech (ver platformio.ini): mismo pinout/flags R1 que centralmix.
//
// REQUISITOS DE HARDWARE: TOP conectado (da el heading del BNO) y DOWN conectado (da la línea),
// exactamente como centralmix. Sin TOP, el heading queda inválido → strafe recto sin corrección.
// Sin DOWN, NO se mueve (medida de seguridad: no irse de largo sin poder frenar por línea).
//
// ⚠️ NO TESTEADO EN HARDWARE. El SENTIDO del strafe y el SIGNO de la corrección de rumbo se
// validan en banco (ver notas en strafe_derecha()).

#include <Arduino.h>

#include "mix_config.h"
#include "mix_io.h"
#include "mix_comm.h"
#include "mix_motors.h"

using namespace iitasoccer::mix;

// ============================================================
// Perillas del technical challenge (tuneables acá).
// ============================================================
static constexpr unsigned long TECH_START_DELAY_MS = 3000;  // espera al encender antes de moverse (0 = ya)
static constexpr float TECH_HDG_TOL = 1.0f;                 // banda muerta del rumbo (grados)

// PWM del strafe a la DERECHA — port 1:1 de adproporcional() 2025 (M1=izq, M2=der, M3=trasera):
//   recto (|error|<TOL):  M1=+50  M2=+50  M3=-89
//   error > 0:            M1=+50  M2=+50  M3=-100   (más rotación correctiva por la trasera)
//   error < 0:            M1=+40  M2=+65  M3=-40    (desbalancea delanteras + menos trasera)
static constexpr int TECH_F_BASE  = 50,  TECH_R_BASE = 89;
static constexpr int TECH_R_MORE  = 100;
static constexpr int TECH_F1_LESS = 40,  TECH_F2_MORE = 65,  TECH_R_LESS = 40;

static bool s_stopped = false;   // latch: una vez que ve la línea, se queda quieto (hasta reset)

// ¿Hay línea? (mismo criterio que centralmix: presencia con umbral de profundidad de DOWN).
static inline bool linea_presente_tech() {
    return g_io.line_present && (g_io.line_depth >= MIX_LINE_DEPTH_TRIGGER);
}

// Strafe a la DERECHA (+X) con heading-hold del BNO. Port de adproporcional() 2025 (3 bandas por
// el signo del error de rumbo), con el error del BNO del TOP (g_io.heading_error_deg) y las
// primitivas mix_set_motor. La base recta [M1=+F, M2=+F, M3=-R] es traslación PURA a la derecha
// (verificado contra la cinemática de avanzar()/retroceder() de mix_motors: front:rear = 0.5:1).
// La trasera (M3) aporta la rotación → subirla/bajarla corrige el rumbo sin dejar de trasladar.
// ⚠️ SIGNO A CONFIRMAR EN BANCO: si en vez de mantener el rumbo CURVA/ROTA cada vez más, invertí
//    las dos ramas de corrección (intercambiá el bloque `error > 0` con el `else`).
static void strafe_derecha() {
    const float error = g_io.heading_error_deg;   // BNO del TOP, wrap180, 0 = rumbo de arranque
    if (error > -TECH_HDG_TOL && error < TECH_HDG_TOL) {
        mix_set_motor(0, +TECH_F_BASE);   mix_set_motor(1, +TECH_F_BASE);   mix_set_motor(2, -TECH_R_BASE);
    } else if (error > 0.0f) {
        mix_set_motor(0, +TECH_F_BASE);   mix_set_motor(1, +TECH_F_BASE);   mix_set_motor(2, -TECH_R_MORE);
    } else {
        mix_set_motor(0, +TECH_F1_LESS);  mix_set_motor(1, +TECH_F2_MORE);  mix_set_motor(2, -TECH_R_LESS);
    }
}

// Debug por USB (115200), throttleado. Solo si hay PC (`if (Serial)`): en banco muestra el error
// de rumbo, si el heading es válido, la línea y si ya latcheó el freno.
static void tech_debug() {
    static unsigned long prev = 0;
    const unsigned long now = millis();
    if (now - prev < 150) return;
    prev = now;
    if (!Serial) return;
    Serial.print("herr=");      Serial.print(g_io.heading_error_deg, 1);
    Serial.print(" hvalid=");   Serial.print(g_io.heading_valid);
    Serial.print(" | line=");   Serial.print(g_io.line_present);
    Serial.print(" depth=");    Serial.print(g_io.line_depth);
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

    // Latch: si YA vio la línea, quedarse quieto para siempre (hasta reset/power-cycle).
    // Para que NO se latchee (frenar solo mientras pisa la línea), borrar este bloque y el
    // `s_stopped = true;` de abajo.
    if (s_stopped) { parar(); return; }

    // Espera de arranque + SEGURIDAD: no moverse si NO llega la línea de DOWN (si DOWN está
    // desconectado, line_present quedaría siempre false y el robot se iría de largo sin frenar).
    if (millis() < TECH_START_DELAY_MS || !g_io.down_link_fresh) { parar(); return; }

    if (linea_presente_tech()) {
        s_stopped = true;   // vio la línea → latchear quieto
        parar();
    } else {
        strafe_derecha();   // strafe a la derecha con heading-hold del BNO
    }
}
