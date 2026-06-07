// diag_central_motors.cpp — Test individual de los 3 motores del Zircon Rev v15
//
// ESTADO ROBOT1: mapeo motor<->driver<->sentido VALIDADO en banco (M1=U5 normal,
// M2=U17 invertido HW, M3=U7 normal). Este sketch sigue util para RE-verificar
// tras rearmar o para mapear ROBOT2.
//
// Para qué sirve:
//   Validar EN BANCO que los 3 motores del robot están cableados, que el PWM
//   funciona en todo su rango, y mapear qué número de motor (1, 2, 3 según el
//   firmware) corresponde a qué motor FÍSICO del robot.
//
//   Bonus: el "conflicto 7/8" está RESUELTO (7/8 = motor 2/U17; el link
//   DOWN->CENTRAL se movió a Serial1). Los pines 7/8/6 son del driver U17
//   (motor 2), NO de Serial2. Este sketch confirma empíricamente que el motor
//   del driver U17 se mueve cuando le toca el turno.
//   Ver hardware/electronics/central-board-pack/01-pinout-y-hardware.md §8
//   para el detalle.
//
// Procedimiento operativo:
//   1. Flashear:                   pio run -e diag_central_motors -t upload
//   2. Conectar USB y abrir:       pio device monitor -b 115200
//   3. SUJETAR EL ROBOT o ponerlo con las ruedas al aire
//      (¡puede salir disparado del banco!).
//   4. Apretar el botón físico de la placa (pin 9) para arrancar el test.
//      También sirve mandar cualquier línea por Serial Monitor (backup por
//      si el pulsador no está cableado).
//   5. Cada apretón avanza al siguiente motor:
//        Apretón 1  → motor 1 arranca con onda ondulante creciente/decreciente
//        Apretón 2  → motor 1 para, motor 2 arranca
//        Apretón 3  → motor 2 para, motor 3 arranca
//        Apretón 4  → motor 3 para, fin
//   6. Anotar qué rueda física se mueve en cada paso. Eso cierra la incógnita
//      de "qué número de motor del firmware es qué rueda del robot".
//
// Notas de seguridad:
//   - PWM máximo está limitado a 128/255 (50%) — suficiente para validar
//     dirección y rango sin lanzar el robot del banco.
//   - Cada motor se prueba en UN sentido a la vez. El sentido se controla:
//       • POR MOTOR  → arreglo MOTOR_DIR[] de abajo (editás el signo del
//         motor que gira al revés y recompilás). Es lo normal en este robot:
//         las ruedas omni están montadas con orientaciones distintas.
//       • LOS 3 A LA VEZ → flag de build -DDIAG_MOTORS_REVERSE (sin editar).
//     Default: los 3 en horario (INA=HIGH, INB=LOW). Los dos se combinan
//     (sentido efectivo = DIRECTION_SIGN × MOTOR_DIR[i]).
//   - El sketch NO depende de config_central.h ni del rol — los pines están
//     hardcodeados acá según hardware/electronics/mapa-pines-teensy-ambos-robots.md
//     y el doc del 2026-03-20. Si el cableado físico difiere, ajustar la
//     tabla MOTORS[] de abajo.
//
// Atribución:
//   Author: Claude Opus 4.7 (Anthropic)
//   Requested-by: Gustavo Viollaz (@gviollaz)

#include <Arduino.h>

// ============================================================
// Pinout — Zircon Rev v15 + Teensy 4.1
// Agnóstico al rol: testea los pines tal cual aparecen en el firmware
// (config_central.h). El número de "Motor X" corresponde al ORDEN del firmware
// para ROBOT1 (arquero). Si flasheás un ROBOT2 (delantero), el mismo "Motor 1"
// del test es el motor 3 físico del delantero, etc. — ver tabla en el doc
// hardware/electronics/central-board-pack/01-pinout-y-hardware.md §5.
// ============================================================

struct MotorPins {
    const char* label;
    int ina;
    int inb;
    int pwm;
};

constexpr MotorPins MOTORS[3] = {
    { "MOTOR 1 - driver U5  (INA=2, INB=5, PWM=3)",  2, 5,  3 },
    { "MOTOR 2 - driver U17 (INA=8, INB=7, PWM=6)  [7/8 RESUELTO: motor 2/U17; DOWN->CENTRAL=Serial1]", 8, 7, 6 },
    { "MOTOR 3 - driver U7  (INA=11, INB=12, PWM=4)", 11, 12, 4 },
};

constexpr int PIN_BUTTON = 9;     // Botón 1 (pullup interno) — LOW = apretado
constexpr int PIN_LED    = 13;    // LED_BUILTIN del Teensy 4.1

constexpr int MAX_PWM_TEST       = 128;     // 50% del rango — SEGURO para banco
constexpr uint32_t WAVE_PERIOD_MS = 2000;   // 2 s por ciclo de onda (sub-baja)
constexpr uint32_t DEBOUNCE_MS    = 50;     // el botón debe mantenerse estable 50 ms (anti-ruido motor)
constexpr uint32_t MIN_DWELL_MS   = 300;    // ignora apretones los primeros 300 ms de cada motor (anti-cascada)

// ============================================================
// Sentido de giro POR MOTOR.  +1 = horario, -1 = antihorario.
// Editá ESTE arreglo cuando un motor gire al revés de lo que querés.
// El índice coincide con la tabla MOTORS[] de arriba:
//     MOTOR_DIR[0] → Motor 1,  [1] → Motor 2,  [2] → Motor 3.
//
// Flujo de banco: flasheás con { +1, +1, +1 }, mirás qué rueda gira para el
// lado equivocado, le cambiás el signo a ESE motor, recompilás y reflasheás.
// Cuando los 3 empujen para donde corresponde, esos mismos signos son los que
// van al firmware de producción (config_central.h / motors_zircon.cpp).
// ============================================================
// RESULTADO YA CONOCIDO ROBOT1: el sentido correcto es {+1,-1,+1} (M2/U17
// INVERTIDO por HW), validado banco 2026-06-01 + re-confirmado 2026-06-06.
// Fuente canonica = MOTOR_INVERT en config_central.h. Este {+1,+1,+1} es solo
// el arranque neutro para RE-validar o para mapear ROBOT2.
constexpr int MOTOR_DIR[3] = { +1, +1, +1 };

// Multiplicador GLOBAL: invierte los 3 motores a la vez (se combina con
// MOTOR_DIR). Se activa con el flag de build -DDIAG_MOTORS_REVERSE, sin
// editar código.
#ifdef DIAG_MOTORS_REVERSE
constexpr int DIRECTION_SIGN = -1;
#else
constexpr int DIRECTION_SIGN = +1;    // sentido horario (default)
#endif

// ============================================================
// Estado del sketch (máquina de estados simple)
// ============================================================

enum class State {
    WAITING_START,
    TESTING_M1,
    TESTING_M2,
    TESTING_M3,
    DONE,
};

State    g_state          = State::WAITING_START;
uint32_t g_state_start_ms = 0;
int      g_motor_active   = -1;   // 0..2 mientras se mueve un motor

bool     g_button_last     = false;   // false = no apretado (pullup HIGH)
uint32_t g_button_change_ms = 0;

// ============================================================
// Helpers de motor
// ============================================================

void motor_set(int idx, int pwm_signed) {
    if (idx < 0 || idx > 2) return;
    const MotorPins& m = MOTORS[idx];
    if (pwm_signed > 0) {
        digitalWrite(m.ina, HIGH);
        digitalWrite(m.inb, LOW);
        analogWrite(m.pwm, pwm_signed);
    } else if (pwm_signed < 0) {
        digitalWrite(m.ina, LOW);
        digitalWrite(m.inb, HIGH);
        analogWrite(m.pwm, -pwm_signed);
    } else {
        digitalWrite(m.ina, LOW);
        digitalWrite(m.inb, LOW);
        analogWrite(m.pwm, 0);
    }
}

void all_motors_off() {
    for (int i = 0; i < 3; i++) motor_set(i, 0);
}

// Onda senoidal positiva (semi-rectificada): 0 → +MAX → 0 → +MAX → 0 ...
// El motor avanza siempre en el mismo sentido pero la potencia ondula.
// Esto permite escuchar (cambio de tono) y ver (cambio de velocidad) la
// respuesta al PWM en todo el rango sin invertir dirección.
void apply_wave(int motor_idx, uint32_t elapsed_ms) {
    if (motor_idx < 0 || motor_idx > 2) return;   // guard: indexamos MOTOR_DIR[]
    const float phase = (elapsed_ms % WAVE_PERIOD_MS) / static_cast<float>(WAVE_PERIOD_MS);
    const float wave  = (1.0f - cosf(phase * 2.0f * static_cast<float>(M_PI))) * 0.5f;  // 0..1..0
    const int   sign  = DIRECTION_SIGN * MOTOR_DIR[motor_idx];  // global × por-motor
    const int   pwm   = sign * static_cast<int>(wave * MAX_PWM_TEST);
    motor_set(motor_idx, pwm);
}

// ============================================================
// Detección de evento "avanzar al siguiente estado"
// Acepta dos formas:
//   1. Flanco de bajada del botón físico (pin 9, INPUT_PULLUP)
//   2. Un ENTER (newline/CR) recibido por Serial USB — el resto de los bytes
//      se ignora (la basura del USB al conectar el monitor no debe avanzar).
// El (2) es backup por si el pulsador físico no está cableado en la placa.
// ============================================================

bool button_pressed_edge() {
    // Antirebote por ESTABILIDAD: la lectura cruda del pin tiene que mantenerse
    // igual durante DEBOUNCE_MS para que el estado "oficial" (g_button_last)
    // cambie. Así los picos cortos de RUIDO DE LOS MOTORES en el pin 9 NO se
    // leen como apretones (era lo que hacía saltar M1->M2->M3 solo al girar el
    // motor). g_button_last = estado ya filtrado (false = suelto).
    static bool raw_last = false;
    const bool raw = (digitalRead(PIN_BUTTON) == LOW);
    const uint32_t now = millis();

    if (raw != raw_last) {            // la lectura cruda cambió: reiniciar timer
        raw_last = raw;
        g_button_change_ms = now;
    }
    if ((now - g_button_change_ms) >= DEBOUNCE_MS && raw != g_button_last) {
        g_button_last = raw;          // adoptar la lectura ya estable
        if (g_button_last) return true;   // flanco estable suelto -> apretado
    }
    return false;
}

// Avanza SOLO con un fin de línea (ENTER). Los demás bytes se drenan sin
// avanzar, así la basura que mete el USB al abrir el Serial Monitor NO dispara
// transiciones (era la causa del salto M1->M2->M3 al arrancar).
// Backup del botón físico: escribí cualquier cosa + ENTER en el Serial Monitor.
bool serial_advance_request() {
    bool advance = false;
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') advance = true;
    }
    return advance;
}

// ============================================================
// Transiciones
// ============================================================

void enter_state(State s) {
    g_state = s;
    g_state_start_ms = millis();
    all_motors_off();   // siempre apagar al entrar a un nuevo estado

    switch (s) {
        case State::WAITING_START: {
            g_motor_active = -1;
            Serial.println();
            Serial.println("==============================================");
            Serial.println(" diag_central_motors  -  Zircon Rev v15");
            Serial.println("==============================================");
            Serial.println(" Apreta el BOTON (pin 9) para arrancar el test.");
            Serial.println(" (Backup: escribi algo + ENTER en el Serial Monitor.)");
            Serial.println();
            Serial.println(" Cada apreton avanza al siguiente motor:");
            Serial.println("   #1  -> Motor 1 arranca (onda)");
            Serial.println("   #2  -> Motor 1 para, Motor 2 arranca");
            Serial.println("   #3  -> Motor 2 para, Motor 3 arranca");
            Serial.println("   #4  -> Motor 3 para, FIN.");
            Serial.println();
            Serial.println(" >>> SUJETA EL ROBOT o ruedas al aire <<<");
            Serial.print  (" PWM maximo: ");
            Serial.print  (MAX_PWM_TEST);
            Serial.println("/255 (50%).");
            Serial.print  (" Direccion global: ");
            Serial.println(DIRECTION_SIGN > 0 ? "horario (default)" : "antihorario (-DDIAG_MOTORS_REVERSE)");
            Serial.print  (" Sentido efectivo (M1 M2 M3): ");
            for (int i = 0; i < 3; i++) {
                Serial.print((DIRECTION_SIGN * MOTOR_DIR[i]) > 0 ? "CW " : "CCW ");
            }
            Serial.println();
            Serial.println();
            break;
        }
        case State::TESTING_M1: {
            g_motor_active = 0;
            Serial.println(">>> Arrancando MOTOR 1");
            Serial.print  ("    "); Serial.println(MOTORS[0].label);
            Serial.println("    Anota: motor 1 = rueda ____ (frente / izq / der)");
            break;
        }
        case State::TESTING_M2: {
            g_motor_active = 1;
            Serial.println(">>> Arrancando MOTOR 2");
            Serial.print  ("    "); Serial.println(MOTORS[1].label);
            Serial.println("    Conflicto 7/8 RESUELTO: 7/8 = motor 2/U17;");
            Serial.println("    link DOWN->CENTRAL = Serial1. ROBOT1: M2/U17 invertido HW.");
            Serial.println("    Anota: motor 2 = rueda ____");
            break;
        }
        case State::TESTING_M3: {
            g_motor_active = 2;
            Serial.println(">>> Arrancando MOTOR 3");
            Serial.print  ("    "); Serial.println(MOTORS[2].label);
            Serial.println("    Anota: motor 3 = rueda ____");
            break;
        }
        case State::DONE: {
            g_motor_active = -1;
            Serial.println();
            Serial.println("==============================================");
            Serial.println(" FIN. Motores parados.");
            Serial.println(" Reset del Teensy para repetir el test.");
            Serial.println("==============================================");
            break;
        }
    }
}

// ============================================================
// Lifecycle Arduino
// ============================================================

void setup() {
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    pinMode(PIN_BUTTON, INPUT_PULLUP);

    for (const auto& m : MOTORS) {
        pinMode(m.ina, OUTPUT);
        pinMode(m.inb, OUTPUT);
        pinMode(m.pwm, OUTPUT);
    }
    all_motors_off();

    Serial.begin(115200);
    // Esperar conexión USB hasta 2 s (el Teensy NO bloquea si no hay host).
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* spin */ }

    // Anti-arranque-fantasma: dejar asentar el pin del botón y DRENAR los bytes
    // que el USB/monitor serie mete al conectarse. Sin esto, esos bytes se leían
    // como "avanzar de estado" y la FSM saltaba sola M1->M2->M3 al arrancar.
    delay(300);
    while (Serial.available() > 0) Serial.read();
    g_button_last = (digitalRead(PIN_BUTTON) == LOW);  // baseline = estado real

    enter_state(State::WAITING_START);
}

void loop() {
    // Heartbeat LED:
    //   - parpadeo 2 Hz mientras WAITING_START o DONE
    //   - fijo mientras se está moviendo un motor
    if (g_state == State::WAITING_START || g_state == State::DONE) {
        digitalWrite(PIN_LED, (millis() / 250) % 2);
    } else {
        digitalWrite(PIN_LED, HIGH);
    }

    // Avanzar de estado si:
    //   - hubo flanco estable del botón, O
    //   - llegó un ENTER por Serial USB
    // ...pero NO en los primeros MIN_DWELL_MS de cada estado: así el transitorio
    // de arranque del motor no se lee como apretón y cascadea.
    const bool dwell_ok = (millis() - g_state_start_ms) >= MIN_DWELL_MS;
    if (dwell_ok && (button_pressed_edge() || serial_advance_request())) {
        switch (g_state) {
            case State::WAITING_START: enter_state(State::TESTING_M1); break;
            case State::TESTING_M1:    enter_state(State::TESTING_M2); break;
            case State::TESTING_M2:    enter_state(State::TESTING_M3); break;
            case State::TESTING_M3:    enter_state(State::DONE);       break;
            case State::DONE:          /* terminal, ignora */          break;
        }
    }

    // Generar la onda PWM sobre el motor activo (si hay alguno).
    if (g_motor_active >= 0) {
        const uint32_t elapsed = millis() - g_state_start_ms;
        apply_wave(g_motor_active, elapsed);
    }
}
