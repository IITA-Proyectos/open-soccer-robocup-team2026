// diag_central_drive_straight.cpp — Avance recto adelante/atrás con PID heading
//
// Para qué sirve:
//   Validar EN BANCO (o cancha despejada) que la cadena completa de control de
//   movimiento del CENTRAL funciona end-to-end:
//     WorldSnapshot (TOP→CENTRAL Serial7 (RX7 pin28))
//       → world_model
//         → HeadingPID
//           → kinematics inversa omni-3
//             → motors_zircon (PWM a los 3 H-bridges del Zircon Rev v15).
//
//   El sketch hace una secuencia simple controlada por botón:
//     Apretón 1 → avanza +Y a velocidad fija durante 3 s
//                 con HeadingPID corrigiendo desvíos.
//     Apretón en cualquier momento, o timeout 3 s, → pausa 1 s.
//     Después de la pausa → reverse -Y durante 3 s con la misma corrección.
//     Apretón en cualquier momento, o timeout 3 s, → STOP + FIN.
//
// Sirve para:
//   1. Confirmar que motors_apply_command + kinematics inversa producen
//      movimiento lineal (sin deriva ni rotación parásita).
//   2. Tunear las ganancias del HeadingPID (Kp/Ki/Kd) viendo la respuesta
//      por Serial USB cada 250 ms.
//   3. Validar que el WorldSnapshot llega del TOP a tiempo (watchdog 500 ms).
//   4. Validar que la convención cinemática (WHEEL_ANGLES_DEG, WHEEL_RADIUS_MM)
//      en config_central.h matchea el robot armado.
//
// NO sirve (todavía):
//   - PID DIFERENCIAL por diferencia de las 2 OTOS crudas. CENTRAL no recibe
//     las 2 lecturas separadas (TOP las fusiona y manda 1 pose). Para hacer
//     el diferencial hay que ampliar el contrato o agregar bypass DOWN→CENTRAL
//     para OTOS — diseño aspiracional, ver TODO_DIFFERENTIAL_OTOS abajo.
//
// Pre-requisitos OPERATIVOS:
//   - PLACA CENTRAL flasheada con este sketch (Zircon Rev v15 + Teensy 4.1).
//   - PLACA TOP corriendo y enviando WorldSnapshot por Serial7 (RX7 pin28) a 230400.
//     (la cadena DOWN→TOP→CENTRAL tiene que estar al menos a nivel TOP→CENTRAL).
//   - TASK-036 cerrada → motores validados, mapeo motor↔rueda conocido,
//     conflicto pines 7/8 resuelto. Sin esto, los resultados del PID son
//     basura porque algún motor puede no responder.
//   - Batería cargada (los H-bridges NO se alimentan por USB).
//   - Robot SUJETO al banco o ruedas al aire, O cancha despejada con espacio
//     suficiente (avanza ~900 mm en 3 s a 300 mm/s).
//
// Seguridad:
//   - Velocidad limitada a 300 mm/s (~30% de MAX_SPEED_MM_S).
//   - Watchdog de snapshot: si > 500 ms sin recibir WorldSnapshot, motores
//     stop automático.
//   - Botón también funciona como ABORT: durante FORWARD/REVERSE cualquier
//     apretón corta inmediatamente.
//
// Build:
//   pio run -e diag_central_drive_robot1 -t upload   (arquero)
//   pio run -e diag_central_drive_robot2 -t upload   (delantero — sin kicker físico)
//
// Flags opcionales:
//   -DDIAG_DRIVE_SPEED_MM_S=200      Velocidad de avance (default 300)
//   -DDIAG_DRIVE_DURATION_MS=2000    Duración por dirección (default 3000)
//   -DDIAG_DRIVE_WITH_LINE           Habilita comm_down + abort por imminent_exit.
//                                    Requiere que el conflicto pines 7/8 esté
//                                    resuelto (Serial2 NO conflicta con motores).
//
// Atribución:
//   Author: Claude Opus 4.7 (Anthropic)
//   Requested-by: Gustavo Viollaz (@gviollaz)

#include <Arduino.h>

#include "config_central.h"
#include "types.h"
#include "world_model.h"
#include "comm_top.h"
#ifdef DIAG_DRIVE_WITH_LINE
  #include "comm_down.h"
#endif
#include "motors_zircon.h"
#include "pids.h"

using namespace iitasoccer;

namespace {

// ============================================================
// Configuración
// ============================================================

constexpr int PIN_BUTTON = 9;        // Botón 1 del Zircon (pullup interno)
constexpr int PIN_LED    = 13;       // LED_BUILTIN

#ifndef DIAG_DRIVE_SPEED_MM_S
  constexpr float DRIVE_SPEED_MM_S = 300.0f;
#else
  constexpr float DRIVE_SPEED_MM_S = DIAG_DRIVE_SPEED_MM_S;
#endif

#ifndef DIAG_DRIVE_DURATION_MS
  constexpr uint32_t DRIVE_DURATION_MS = 3000;
#else
  constexpr uint32_t DRIVE_DURATION_MS = DIAG_DRIVE_DURATION_MS;
#endif

constexpr uint32_t BETWEEN_PAUSE_MS = 1000;
constexpr uint32_t SNAPSHOT_WAIT_MS = 5000;
constexpr uint32_t DEBOUNCE_MS      = 30;
constexpr uint32_t PRINT_PERIOD_MS  = 250;

// ============================================================
// Estado
// ============================================================

enum class State : uint8_t {
    WAITING_START,       // mostrar banner y esperar primer apretón
    WAITING_SNAPSHOT,    // botón apretado pero todavía no llegó snapshot del TOP
    FORWARD,             // avanzando +Y con PID heading
    PAUSED,              // pausa 1 s entre fw y rv (motores stop)
    REVERSE,             // avanzando -Y con PID heading
    DONE,                // terminal — motores stop
};

State    g_state            = State::WAITING_START;
uint32_t g_state_start_ms   = 0;
float    g_heading_setpoint = 0.0f;

HeadingPID g_pid;

bool     g_button_last      = false;
uint32_t g_button_change_ms = 0;

elapsedMillis g_since_print;

// ============================================================
// Helpers — botón / serial
// ============================================================

bool button_pressed_edge() {
    const bool now_pressed = (digitalRead(PIN_BUTTON) == LOW);
    const uint32_t now = millis();
    if (now_pressed != g_button_last && (now - g_button_change_ms) >= DEBOUNCE_MS) {
        g_button_change_ms = now;
        g_button_last = now_pressed;
        if (now_pressed) return true;
    }
    return false;
}

bool serial_advance_request() {
    bool got_any = false;
    while (Serial.available() > 0) {
        Serial.read();
        got_any = true;
    }
    return got_any;
}

const char* state_name(State s) {
    switch (s) {
        case State::WAITING_START:    return "WAITING_START";
        case State::WAITING_SNAPSHOT: return "WAITING_SNAPSHOT";
        case State::FORWARD:          return "FORWARD";
        case State::PAUSED:           return "PAUSED";
        case State::REVERSE:          return "REVERSE";
        case State::DONE:             return "DONE";
    }
    return "?";
}

// ============================================================
// Control de motor — single source of motor commands en este sketch
// ============================================================

void apply_drive(float vy_mm_s, uint32_t now_ms) {
    const float heading = world_model_get_my_heading_deg();
    const float omega_deg_s = heading_pid_tick(g_pid, heading, now_ms);

    MotorCommand cmd{};
    cmd.vx_mm_s          = 0;
    cmd.vy_mm_s          = static_cast<int16_t>(vy_mm_s);
    cmd.omega_centideg_s = static_cast<int16_t>(omega_deg_s * 100.0f);
    cmd.dribbler_pwm     = 0;
    motors_apply_command(cmd);
}

// ============================================================
// Transiciones
// ============================================================

void enter_state(State s) {
    g_state = s;
    g_state_start_ms = millis();

    switch (s) {
        case State::WAITING_START: {
            motors_stop();
            Serial.println();
            Serial.println("==================================================");
            Serial.println(" diag_central_drive_straight  -  Zircon Rev v15");
            Serial.println("==================================================");
            Serial.print  (" Build robot: ");
        #if defined(ROBOT1)
            Serial.println("ROBOT1 (arquero)");
        #elif defined(ROBOT2)
            Serial.println("ROBOT2 (delantero)");
        #endif
            Serial.print  (" Velocidad de avance: ");
            Serial.print  (DRIVE_SPEED_MM_S, 0); Serial.println(" mm/s");
            Serial.print  (" Duracion por direccion: ");
            Serial.print  (DRIVE_DURATION_MS); Serial.println(" ms");
            Serial.println();
            Serial.println(" Pre-flight check:");
            Serial.println("   - Bateria conectada y cargada (los drivers NO van por USB)");
            Serial.println("   - Robot SUJETO o ruedas al aire o cancha despejada");
            Serial.println("   - Placa TOP corriendo, enviando WorldSnapshot por Serial7 (RX7 pin28)");
            Serial.println("   - TASK-036 cerrada (motores validados, conflicto 7/8 resuelto)");
            Serial.println();
            Serial.println(" Operativa:");
            Serial.println("   Apreton #1  -> capturar heading actual + arrancar FORWARD");
            Serial.println("   Apreton #2  -> abortar FORWARD anticipadamente (sino timeout 3s)");
            Serial.println("   Apreton #3  -> abortar REVERSE anticipadamente (sino timeout 3s)");
            Serial.println("   ENTER por Serial USB tambien sirve de boton");
            Serial.println();
            Serial.println(" Apreta el boton para arrancar.");
            Serial.println();
            break;
        }
        case State::WAITING_SNAPSHOT: {
            motors_stop();
            Serial.println(">>> Esperando primer WorldSnapshot fresco del TOP...");
            Serial.println("    (timeout 5s -> vuelve a WAITING_START con error)");
            break;
        }
        case State::FORWARD: {
            heading_pid_reset(g_pid);
            heading_pid_set_target(g_pid, g_heading_setpoint);
            Serial.println(">>> FORWARD: avanzando +Y con PID heading.");
            Serial.print  ("    Setpoint heading = ");
            Serial.print  (g_heading_setpoint, 1); Serial.println(" deg");
            Serial.println("    Apretar boton para abortar antes del timeout.");
            break;
        }
        case State::PAUSED: {
            motors_stop();
            Serial.println(">>> PAUSED: motores stop por 1 s antes de revertir.");
            break;
        }
        case State::REVERSE: {
            heading_pid_reset(g_pid);
            heading_pid_set_target(g_pid, g_heading_setpoint);
            Serial.println(">>> REVERSE: avanzando -Y con PID heading.");
            Serial.print  ("    Manteniendo setpoint heading = ");
            Serial.print  (g_heading_setpoint, 1); Serial.println(" deg");
            break;
        }
        case State::DONE: {
            motors_stop();
            Serial.println();
            Serial.println("==================================================");
            Serial.println(" FIN. Motores parados.");
            Serial.println(" Reset del Teensy para repetir.");
            Serial.println("==================================================");
            break;
        }
    }
}

// ============================================================
// Telemetria periodica
// ============================================================

void print_status() {
    Serial.print  ("[t=");          Serial.print(millis());
    Serial.print  ("] state=");     Serial.print(state_name(g_state));
    Serial.print  (" snap=");       Serial.print(world_model_snapshot_is_fresh() ? "FRESH" : "STALE");
    Serial.print  (" hdg=");        Serial.print(world_model_get_my_heading_deg(), 1);
    Serial.print  (" sp=");         Serial.print(g_heading_setpoint, 1);
    Serial.print  (" err=");        Serial.print(wrap_diff_deg(g_heading_setpoint,
                                                                world_model_get_my_heading_deg()), 1);
#ifdef DIAG_DRIVE_WITH_LINE
    Serial.print  (" line_fresh=");
    Serial.print  (world_model_line_is_fresh() ? "Y" : "N");
    Serial.print  (" imm_exit=");
    Serial.print  (world_model_imminent_exit() ? "Y" : "N");
#endif
    Serial.println();
}

// ============================================================
// TODO_DIFFERENTIAL_OTOS
//   Hoy CENTRAL recibe SOLO la pose fusionada del TOP (my_x/my_y/my_heading
//   en WorldSnapshot). NO recibe las 2 lecturas crudas OTOS por separado.
//   Para implementar el "PID diferencial por diferencia de las 2 OTOS" hay 2
//   caminos:
//
//     1. Ampliar WorldSnapshot v3 con 2 poses crudas OTOS (+12 bytes). Toca
//        types.h + firmware TOP + comm_top en CENTRAL + romper static_assert
//        de v2. ~3-4 archivos modificados.
//     2. Bypass DOWN→CENTRAL directo: que DOWN mande las 2 OTOS por Serial2
//        además del LINE_URGENT, y agregar handler en comm_down.cpp.
//        Más chico, pero bloqueado por conflicto pines 7/8 + TASK-031.
//
//   Recomendación coach: hacer (1) post-Incheon — el contrato v3 puede traer
//   también ball_vx/vy mejorado, y otros campos que vayan apareciendo.
//   Por ahora este sketch usa my_heading_centideg fusionado, que para
//   "ir derecho" es suficiente. El PID diferencial OTOS daría redundancia +
//   detección de slip, no precision adicional.
// ============================================================

}  // namespace

// ============================================================
// Lifecycle Arduino
// ============================================================

void setup() {
    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    Serial.begin(115200);
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* spin */ }

    world_model_init();
    comm_top_init();
#ifdef DIAG_DRIVE_WITH_LINE
    comm_down_init();
#endif
    motors_init();
    heading_pid_reset(g_pid);

    enter_state(State::WAITING_START);
}

void loop() {
    // Drenar UARTs (no bloquea)
    comm_top_tick();
#ifdef DIAG_DRIVE_WITH_LINE
    comm_down_tick();
#endif

    // Heartbeat LED:
    //   - parpadeo lento (2 Hz) en WAITING/PAUSED/DONE
    //   - fijo en FORWARD/REVERSE
    const bool moving = (g_state == State::FORWARD || g_state == State::REVERSE);
    digitalWrite(PIN_LED, moving ? HIGH : ((millis() / 250) % 2));

    const bool advance = button_pressed_edge() || serial_advance_request();
    const uint32_t now = millis();

    switch (g_state) {
        case State::WAITING_START: {
            motors_stop();
            if (advance) {
                if (world_model_snapshot_is_fresh()) {
                    g_heading_setpoint = world_model_get_my_heading_deg();
                    enter_state(State::FORWARD);
                } else {
                    enter_state(State::WAITING_SNAPSHOT);
                }
            }
            break;
        }

        case State::WAITING_SNAPSHOT: {
            motors_stop();
            if (world_model_snapshot_is_fresh()) {
                g_heading_setpoint = world_model_get_my_heading_deg();
                enter_state(State::FORWARD);
            } else if (now - g_state_start_ms > SNAPSHOT_WAIT_MS) {
                Serial.println();
                Serial.println("!!! ERROR: NO llego ningun WorldSnapshot del TOP en 5 s.");
                Serial.println("    Revisar: cable UART al TOP / firmware TOP corriendo / Serial1 OK.");
                Serial.println("    Volviendo a WAITING_START.");
                enter_state(State::WAITING_START);
            }
            if (advance) {
                enter_state(State::WAITING_START);
            }
            break;
        }

        case State::FORWARD: {
            if (!world_model_snapshot_is_fresh()) {
                Serial.println("!!! ERROR: snapshot del TOP perdido durante FORWARD. STOP.");
                enter_state(State::DONE);
                break;
            }
        #ifdef DIAG_DRIVE_WITH_LINE
            if (world_model_imminent_exit() && world_model_line_is_fresh()) {
                Serial.println("!!! LINE imminent_exit detectado durante FORWARD. STOP.");
                motors_brake();
                enter_state(State::DONE);
                break;
            }
        #endif
            apply_drive(+DRIVE_SPEED_MM_S, now);
            if (advance || (now - g_state_start_ms > DRIVE_DURATION_MS)) {
                enter_state(State::PAUSED);
            }
            break;
        }

        case State::PAUSED: {
            motors_stop();
            if (now - g_state_start_ms > BETWEEN_PAUSE_MS) {
                enter_state(State::REVERSE);
            }
            if (advance) {
                enter_state(State::DONE);
            }
            break;
        }

        case State::REVERSE: {
            if (!world_model_snapshot_is_fresh()) {
                Serial.println("!!! ERROR: snapshot del TOP perdido durante REVERSE. STOP.");
                enter_state(State::DONE);
                break;
            }
        #ifdef DIAG_DRIVE_WITH_LINE
            if (world_model_imminent_exit() && world_model_line_is_fresh()) {
                Serial.println("!!! LINE imminent_exit detectado durante REVERSE. STOP.");
                motors_brake();
                enter_state(State::DONE);
                break;
            }
        #endif
            apply_drive(-DRIVE_SPEED_MM_S, now);
            if (advance || (now - g_state_start_ms > DRIVE_DURATION_MS)) {
                enter_state(State::DONE);
            }
            break;
        }

        case State::DONE: {
            motors_stop();
            break;
        }
    }

    if (g_since_print >= PRINT_PERIOD_MS) {
        g_since_print = 0;
        print_status();
    }
}
