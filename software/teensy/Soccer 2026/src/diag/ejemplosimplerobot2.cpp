// ejemplosimplerobot2.cpp — EJEMPLO SIMPLE de movimientos laterales (ESQUELETO).
//
// Para qué es:
//   Un punto de partida para probar movimientos laterales. El LAZO PRINCIPAL
//   (loop) está VACÍO a propósito: vos le ponés la lógica y los datos.
//
// La idea del ejercicio:
//   Mover el robot de DERECHA a IZQUIERDA hasta que TOQUE LA LÍNEA, después de
//   IZQUIERDA a DERECHA hasta que toque la línea, y así sucesivamente.
//
// Lo que YA viene resuelto (no hace falta que lo escribas), abajo en este archivo:
//   - mover_derecha()    -> el robot se desplaza de costado hacia la DERECHA (+X)
//   - mover_izquierda()  -> el robot se desplaza de costado hacia la IZQUIERDA (-X)
//   - parar()            -> frena los motores
//   - toca_linea()       -> devuelve true cuando hay línea blanca debajo (la mide
//                           la placa DOWN y llega por el cable a la CENTRAL)
//
// Lo que tenés que escribir vos: la lógica DENTRO de loop() que alterna los dos
// movimientos usando toca_linea() (ver el bloque "TU CÓDIGO ACÁ").
//
// ──────────────────────────────────────────────────────────────────────────
// Convención de ejes (kinematics.h): +X = DERECHA, +Y = FRENTE.
//   Movimiento lateral puro = velocidad en X, con vy = 0 y omega = 0 (sin giro).
//   DERECHA = +X   |   IZQUIERDA = -X.
//
// Sin realimentación de rumbo (la CENTRAL no tiene giroscopio): se comanda
// omega = 0. Para un omni con ruedas parejas y calibradas eso es traslación pura;
// cualquier rotación que veas es deriva (este ejemplo no la corrige).
//
// ⚠️ SEGURIDAD para la primera prueba: SUJETÁ el robot o ponelo con las ruedas al
//   aire. Batería cargada (los motores NO andan solo por USB).
//
// ──────────────────────────────────────────────────────────────────────────
// Nombre y robot:
//   Se llama "ejemplosimplerobot2" (como se pidió). El entorno compila con la
//   configuración de ROBOT1 (porque es "para robot 1"). Si querés la de robot 2,
//   cambiá -DROBOT1 por -DROBOT2 en el entorno [env:ejemplosimplerobot2] del
//   platformio.ini.
//
// Cómo se compila y graba:
//   pio run -e ejemplosimplerobot2 -t upload
//   pio device monitor -b 115200
//
// Autor: Claude Opus 4.8 (Anthropic) — Pedido por: Gustavo Viollaz (@gviollaz)

#include <Arduino.h>

#include "config_central.h"
#include "types.h"
#include "motors_zircon.h"
#include "world_model.h"
#include "comm_down.h"   // recibe la LÍNEA de la placa DOWN (Serial1, pin 0)

using namespace iitasoccer;

namespace {

constexpr int PIN_LED = 13;   // LED de la placa (LED_BUILTIN)

// ──────────────────────────────────────────────────────────────────────────
// DATO QUE PODÉS CAMBIAR: velocidad del desplazamiento lateral, en mm/s.
// Más alto = más rápido (más potencia a las ruedas). Empezá bajo y subí.
constexpr float VELOCIDAD_LATERAL_MM_S = 400.0f;
// ──────────────────────────────────────────────────────────────────────────

// ===== LOS 2 MOVIMIENTOS SIMPLES (ya listos para usar en loop) =============

// Movimiento hacia la DERECHA (+X). vy = 0, omega = 0 (sin giro).
void mover_derecha() {
    MotorCommand cmd{};
    cmd.vx_mm_s          = static_cast<int16_t>(+VELOCIDAD_LATERAL_MM_S);
    cmd.vy_mm_s          = 0;
    cmd.omega_centideg_s = 0;
    cmd.dribbler_pwm     = 0;
    motors_apply_command(cmd);
}

// Movimiento hacia la IZQUIERDA (-X).
void mover_izquierda() {
    MotorCommand cmd{};
    cmd.vx_mm_s          = static_cast<int16_t>(-VELOCIDAD_LATERAL_MM_S);
    cmd.vy_mm_s          = 0;
    cmd.omega_centideg_s = 0;
    cmd.dribbler_pwm     = 0;
    motors_apply_command(cmd);
}

// Frena los motores (los deja quietos).
void parar() {
    motors_stop();
}

// ¿El robot está TOCANDO la línea? La línea la detecta la placa DOWN y la manda
// a la CENTRAL; acá solo la leemos. Devuelve true si hay línea debajo Y el dato
// es reciente (si la DOWN se cuelga, devuelve false por seguridad).
bool toca_linea() {
    return world_model_line_detected() && world_model_line_is_fresh();
}

}  // namespace

void setup() {
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    motors_init();        // motores (3 ruedas omni)
    world_model_init();   // memoria del estado del robot
    comm_down_init();     // empieza a escuchar la LÍNEA de la placa DOWN

    Serial.begin(115200);
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* espera al monitor, sin colgarse */ }

    Serial.println();
    Serial.println(F("=================================================="));
    Serial.println(F("  ejemplosimplerobot2 — movimientos laterales (ESQUELETO)"));
    Serial.println(F("=================================================="));
    Serial.println(F("  Movimientos listos: mover_derecha() / mover_izquierda() / parar()"));
    Serial.println(F("  Sensor de linea:    toca_linea()  (viene de la placa DOWN)"));
    Serial.println(F("  >>> El lazo principal (loop) esta VACIO: pone tu logica ahi."));
    Serial.println(F("  SUJETA EL ROBOT en la primera prueba."));
    Serial.println();
}

void loop() {
    // ── OBLIGATORIO: dejar esta línea. Drena la LÍNEA que llega de la placa DOWN.
    //    Sin ella, toca_linea() nunca se actualiza. ────────────────────────────
    comm_down_tick();

    // ╔══════════════════════ TU CÓDIGO ACÁ ══════════════════════╗
    //
    //  Objetivo: ir de DERECHA a IZQUIERDA hasta tocar la línea, después de
    //  IZQUIERDA a DERECHA hasta tocar la línea, y repetir.
    //
    //  Herramientas que ya tenés (definidas arriba en este archivo):
    //      mover_derecha();     // desplazarse hacia +X
    //      mover_izquierda();   // desplazarse hacia -X
    //      parar();             // frenar
    //      toca_linea();        // true cuando hay línea debajo
    //
    //  Pseudocódigo para que lo completes (una forma posible):
    //      - llevá una variable con la dirección actual (derecha o izquierda)
    //      - llamá al movimiento de esa dirección
    //      - cuando toca_linea() sea true -> cambiá la dirección
    //
    //  Cuidado típico: si ya estás SOBRE la línea, toca_linea() sigue en true y
    //  podrías cambiar de dirección muchas veces seguidas. Conviene esperar a
    //  SALIR de la línea (toca_linea() vuelve a false) antes de contar el próximo
    //  toque, o frenar un instante al tocar.
    //
    // ╚════════════════════════════════════════════════════════════╝
}
