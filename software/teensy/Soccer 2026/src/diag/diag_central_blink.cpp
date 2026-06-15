// diag_central_blink.cpp — CENTRAL inofensiva: solo parpadea el LED, motores OFF.
//
// Para qué sirve:
//   Para CALIBRAR la línea (placa DOWN) con la BATERÍA puesta sin que la CENTRAL
//   mueva los motores. La calibración (`diag_down_calibracion`) corre entera en la
//   DOWN, pero los LEDs de los sensores de línea pueden necesitar el riel de la
//   batería para encender; si dejás la batería puesta con el firmware de partido
//   (o el `diag_central_motors`) flasheado, la CENTRAL maneja los motores y el
//   robot se mueve. Este sketch deja la CENTRAL viva pero QUIETA:
//     • Fuerza los 3 drivers a estado APAGADO (INA=INB=LOW, PWM=0) una sola vez
//       y NO los vuelve a tocar — así las entradas del H-bridge no quedan
//       flotando (al bootear, los pines del Teensy arrancan en INPUT/alta
//       impedancia, y un driver con entradas flotando puede mover el motor).
//     • Parpadea el LED de a bordo (pin 13) para que se vea que está vivo.
//   NO inicializa BNO, ToF, UART ni nada más. NO es firmware de competencia.
//
// Cómo se usa:
//   1. pio run -e diag_central_blink -t upload   (USB en la CENTRAL)
//   2. Poné la batería: las 3 placas encienden, el LED de la CENTRAL parpadea,
//      los motores quedan QUIETOS.
//   3. Calibrá la línea por USB en la DOWN (diag_down_calibracion).
//   4. Al terminar, reflasheá la CENTRAL con el env que toque
//      (ej. central_robot1_delantero_practica_bb).
//
// Pinout de los 3 drivers (Zircon Rev v15, ROBOT1) — copiado de
//   diag_central_motors.cpp (la fuente de verdad del cableado de motores):
//     M1 - driver U5  : INA=2,  INB=5,  PWM=3
//     M2 - driver U17 : INA=8,  INB=7,  PWM=6
//     M3 - driver U7  : INA=11, INB=12, PWM=4
//   Si el cableado físico cambia, esta tabla es lo único a tocar.
//
// Atribución:
//   Author: Claude Opus 4.8 (Anthropic)
//   Requested-by: Elías Cordero (@3liasCo)

#include <Arduino.h>

namespace {

// Entradas de los 3 drivers (INA, INB, PWM). Mismo mapeo que diag_central_motors.cpp.
constexpr int MOTOR_PINS[3][3] = {
    {  2,  5, 3 },   // M1 - driver U5
    {  8,  7, 6 },   // M2 - driver U17
    { 11, 12, 4 },   // M3 - driver U7
};

constexpr int PIN_LED = 13;   // LED_BUILTIN del Teensy 4.1

// Pone los 3 drivers en estado APAGADO seguro (coast/stop): INA=INB=LOW, PWM=0.
void motores_apagados() {
    for (int m = 0; m < 3; ++m) {
        const int ina = MOTOR_PINS[m][0];
        const int inb = MOTOR_PINS[m][1];
        const int pwm = MOTOR_PINS[m][2];
        pinMode(ina, OUTPUT);
        pinMode(inb, OUTPUT);
        pinMode(pwm, OUTPUT);
        digitalWrite(ina, LOW);
        digitalWrite(inb, LOW);
        analogWrite(pwm, 0);
    }
}

}  // namespace

void setup() {
    // Lo PRIMERO: callar los motores antes de cualquier otra cosa.
    motores_apagados();

    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    Serial.begin(115200);
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* spin */ }

    Serial.println();
    Serial.println(F("=== CENTRAL BLINK (motores OFF) ==="));
    Serial.println(F("La CENTRAL esta VIVA pero QUIETA: los 3 drivers en LOW/LOW, PWM=0."));
    Serial.println(F("Usalo para calibrar la linea (placa DOWN) con la bateria puesta"));
    Serial.println(F("sin que el robot se mueva. Al terminar, reflashea el env de partido."));
    Serial.println(F("==================================="));
}

void loop() {
    // Re-afirmar el apagado por las dudas (barato, no cuesta nada) y parpadear.
    motores_apagados();
    digitalWrite(PIN_LED, (millis() / 500) % 2);
}
