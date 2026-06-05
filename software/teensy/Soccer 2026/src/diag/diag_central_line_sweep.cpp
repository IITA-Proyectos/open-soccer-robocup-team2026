// diag_central_line_sweep.cpp — ARQUERO: patrulla lateral siguiendo la linea
//                               del area, centrandose adelante/atras sobre ella.
//                               (placa CENTRAL, Teensy 4.1). NO es competencia.
//
// NOTA: usa BNO local (imu_zircon); la CENTRAL de competencia NO lleva BNO ->
// degrada a patrulla sin correccion de heading si el BNO no esta.
//
// Comportamiento:
//   - Se desplaza LATERALMENTE a lo largo de la linea del area (los 3 motores,
//     logica de Elias/Maria: M1 y M2 contrarios, M3 acompaña).
//   - Usa cross_track_mm (que DOWN ahora calcula = posicion ADELANTE/ATRAS de la
//     linea respecto al centro del robot) para CORREGIR y mantenerse centrado
//     sobre la linea mientras patrulla. + = linea adelante, - = linea atras.
//   - Si pierde la linea (no present) un tiempo, FRENA (llego a una esquina / fin).
//   - Boton (pin 9): arranca / para.
//
// Datos de DOWN: LineStatusV2 por Serial1 (header UART del Zircon, pin 0).
//   El cable sale por U10 (Serial5) de DOWN; con down_debug, DOWN reenvia la
//   linea por U10 e incluye cross_track_mm (centroide Y de los sensores blancos).
//
// SEGURIDAD: watchdog de enlace SIEMPRE activo (sin frames de DOWN -> parar).
//   Con -DSWEEP_NO_SAFETY se saltean LEVANTADO / salida-inminente para banco.
//
// Uso:
//   pio run -e diag_central_line_sweep_nosafety_robot1 -t upload  ; banco
//   pio run -e diag_central_line_sweep_robot1 -t upload           ; con seguridad
//   pio device monitor -b 115200
//   1. SUJETÁ EL ROBOT o ruedas al aire la primera vez.
//   2. Apretá el botón (pin 9): arranca a patrullar. Apretá de nuevo: PARA.
//
// Flags opcionales:
//   -DPATRULLA_PWM=55          PWM del avance lateral (default 55)
//   -DCENTRADO_DEADBAND_MM=15  zona muerta del centrado en mm (default 15)
//   -DSWEEP_NO_SAFETY / -DSWEEP_REQUIRE_LINE
//
// Atribución:
//   Movimiento lateral: Maria/Elias. Centroide+seguidor: Claude Opus 4.8,
//   pedido de Maria Viollaz.

#include <Arduino.h>

#include "proto.h"
#include "types.h"
#include "line_view.h"
#include "imu_zircon.h"

using namespace iitasoccer;

namespace {

// ===================== AJUSTES ===============================================
// Velocidades base de los motores para el avance lateral (0..255).
constexpr int VEL_M1 = 55;
constexpr int VEL_M2 = 55;
constexpr int VEL_M3 = 100;

// Direcciones para patrullar hacia UN lado (sentido +1). Si un motor gira al
// reves, cambiale el signo. (Calibrado en marzo; ajustable.)
constexpr int DIR_M1 = -1;
constexpr int DIR_M2 = +1;   // contrario a M1
constexpr int DIR_M3 = +1;   // acompaña

// Correccion de centrado: PWM extra adelante/atras para volver a la linea.
// Se aplica a M1 y M2 (que son los que mueven adelante/atras en este robot).
#ifndef CENTRADO_PWM
#define CENTRADO_PWM 45            // fuerza de la correccion de centrado
#endif
#ifndef CENTRADO_DEADBAND_MM
#define CENTRADO_DEADBAND_MM 15    // si |cross| < esto, esta centrado: no corrige
#endif
constexpr int CENTRADO_PWM_V      = CENTRADO_PWM;
constexpr int DEADBAND_MM         = CENTRADO_DEADBAND_MM;

// --- PID de HEADING (giroscopio BNO055) -------------------------------------
// Mantiene el robot APUNTANDO al frente (heading 0) mientras patrulla, para que
// se desplace DERECHO a lo largo de la linea en vez de describir un semicirculo.
// Portado de test-motores-lateral-simple (marzo, probado). La correccion
// rotacional se suma a los 3 motores con los signos ROT_*.
constexpr float KP_HEADING       = 3.0f;
constexpr float KI_HEADING       = 0.05f;
constexpr float KD_HEADING       = 0.5f;
constexpr float FACTOR_ROTACION  = 0.5f;   // peso de la correccion de giro
constexpr float MAX_CORRECCION   = 50.0f;
constexpr float INTEGRAL_MAX     = 40.0f;
// Signo con que cada motor aplica la correccion de giro (de marzo).
constexpr int ROT_M1 = +1;
constexpr int ROT_M2 = -1;
constexpr int ROT_M3 = +1;

#ifndef PATRULLA_PWM
#define PATRULLA_PWM 55
#endif
// (PATRULLA_PWM escala las velocidades base; por simplicidad usamos las VEL_*.)

constexpr uint32_t HALF_PERIOD_MS  = 4000;  // cada cuanto cambia de lado de patrulla
constexpr long     DOWN_LINK_BAUD  = 230400;
constexpr uint32_t LINK_TIMEOUT_MS = 500;
constexpr uint32_t LOST_LINE_MS    = 800;   // sin linea este tiempo -> parar
constexpr int      PIN_BUTTON      = 9;
constexpr int      PIN_LED         = 13;

// ---- Pines de motores — Zircon Rev v15 --------------------------------------
constexpr int INA1 = 2,  INB1 = 5,  PWM1 = 3;
constexpr int INA2 = 8,  INB2 = 7,  PWM2 = 6;
constexpr int INA3 = 11, INB3 = 12, PWM3 = 4;

#define DOWN_UART Serial1

// ---- Recepción de línea -----------------------------------------------------
FrameDecoder g_decoder;
LineStatusV2 g_lsv2{};
bool         g_have_lsv2    = false;
uint32_t     g_last_lsv2_ms = 0;
uint32_t     g_last_line_ms = 0;   // ultima vez que se vio linea presente

// ---- Estado -----------------------------------------------------------------
enum class State : uint8_t { STOPPED, PATROL };
State    g_state          = State::STOPPED;
int      g_patrol_dir     = +1;          // +1 / -1: lado de la patrulla
uint32_t g_phase_start_ms = 0;

// ---- Boton ------------------------------------------------------------------
constexpr uint32_t BTN_DEBOUNCE_MS = 50;
bool g_btn_filtered = false, g_btn_raw_last = false;
uint32_t g_btn_change_ms = 0;

bool botonApretadoEdge() {
    const bool raw = (digitalRead(PIN_BUTTON) == LOW);
    const uint32_t now = millis();
    if (raw != g_btn_raw_last) { g_btn_raw_last = raw; g_btn_change_ms = now; }
    if ((now - g_btn_change_ms) >= BTN_DEBOUNCE_MS && raw != g_btn_filtered) {
        g_btn_filtered = raw;
        if (g_btn_filtered) return true;
    }
    return false;
}

// ---- Motores (logica de Elias; M2 invertido) --------------------------------
void motores_init() {
    pinMode(INA1, OUTPUT); pinMode(INB1, OUTPUT); pinMode(PWM1, OUTPUT);
    pinMode(INA2, OUTPUT); pinMode(INB2, OUTPUT); pinMode(PWM2, OUTPUT);
    pinMode(INA3, OUTPUT); pinMode(INB3, OUTPUT); pinMode(PWM3, OUTPUT);
}
void motor1(int vel, int dir) {
    analogWrite(PWM1, constrain(abs(vel), 0, 255));
    digitalWrite(INA1, dir > 0 ? 1 : 0); digitalWrite(INB1, dir < 0 ? 1 : 0);
}
void motor2(int vel, int dir) {   // INVERTIDO
    analogWrite(PWM2, constrain(abs(vel), 0, 255));
    digitalWrite(INA2, dir < 0 ? 1 : 0); digitalWrite(INB2, dir > 0 ? 1 : 0);
}
void motor3(int vel, int dir) {
    analogWrite(PWM3, constrain(abs(vel), 0, 255));
    digitalWrite(INA3, dir > 0 ? 1 : 0); digitalWrite(INB3, dir < 0 ? 1 : 0);
}
void parar() { motor1(0,0); motor2(0,0); motor3(0,0); }

// ---- PID de heading: mantiene el robot apuntando al frente (0 grados) -------
float    g_pid_error_prev = 0.0f;
float    g_pid_integral   = 0.0f;
uint32_t g_pid_last_ms    = 0;

void pid_reset() {
    g_pid_error_prev = 0.0f;
    g_pid_integral   = 0.0f;
    g_pid_last_ms    = millis();
}

// Devuelve la correccion rotacional (en "unidades de PWM") para volver a 0 grados.
float pid_heading_correccion() {
    if (!imu_is_ready()) return 0.0f;       // sin giroscopio -> sin correccion
    const float heading = imu_get_heading();
    const float error   = 0.0f - heading;   // queremos heading = 0

    const uint32_t now = millis();
    float dt = (now - g_pid_last_ms) / 1000.0f;
    if (dt <= 0.0f) dt = 0.01f;
    g_pid_last_ms = now;

    const float P = KP_HEADING * error;
    g_pid_integral += error * dt;
    g_pid_integral = constrain(g_pid_integral, -INTEGRAL_MAX, INTEGRAL_MAX);
    const float I = KI_HEADING * g_pid_integral;
    const float D = KD_HEADING * (error - g_pid_error_prev) / dt;
    g_pid_error_prev = error;

    float corr = P + I + D;
    return constrain(corr, -MAX_CORRECCION, MAX_CORRECCION);
}

// Patrulla lateral + centrado adelante/atras + correccion de GIRO (PID heading).
//   sentido: +1/-1 = direccion de patrulla a lo largo de la linea.
//   corr_fb: correccion adelante(+)/atras(-) en PWM (de cross_track_mm).
//   corr_rot: correccion rotacional del PID de heading (mantiene el frente).
void mover(int sentido, int corr_fb, float corr_rot) {
    // 1) Componente de patrulla (lateral) por motor.
    float m1 = (float)(DIR_M1 * VEL_M1 * sentido);
    float m2 = (float)(DIR_M2 * VEL_M2 * sentido);
    float m3 = (float)(DIR_M3 * VEL_M3 * sentido);

    // 2) Centrado adelante/atras: se suma a M1 y M2 (M2 invertido por hardware).
    m1 += corr_fb;
    m2 -= corr_fb;

    // 3) Correccion de GIRO (PID heading): los 3 motores con sus signos ROT.
    m1 += ROT_M1 * corr_rot * FACTOR_ROTACION;
    m2 += ROT_M2 * corr_rot * FACTOR_ROTACION;
    m3 += ROT_M3 * corr_rot * FACTOR_ROTACION;

    // 4) Saturacion PROPORCIONAL: si algun motor pasa 255, escalar los 3 igual
    //    (mantiene la direccion del movimiento). Metodo de marzo.
    float maxm = max(fabs(m1), max(fabs(m2), fabs(m3)));
    if (maxm > 255.0f) {
        const float k = 255.0f / maxm;
        m1 *= k; m2 *= k; m3 *= k;
    }

    motor1((int)fabs(m1), m1 > 0 ? 1 : (m1 < 0 ? -1 : 0));
    motor2((int)fabs(m2), m2 > 0 ? 1 : (m2 < 0 ? -1 : 0));
    motor3((int)fabs(m3), m3 > 0 ? 1 : (m3 < 0 ? -1 : 0));
}

// ---- Recepción --------------------------------------------------------------
// ¿DOWN ve la linea? Usamos cross_track_mm != N/A como señal de presencia,
// porque ese valor lo calcula DOWN con el line_ring (el detector que SI esta
// calibrado por Maria), a diferencia de line_present del DownModel que esta
// mal afinado y dice 'no' casi siempre.
inline bool linea_presente(const LineStatusV2& s) {
    return s.cross_track_mm != LSV2_NA_I16;
}

void poll_down_link() {
    while (DOWN_UART.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(DOWN_UART.read());
        if (g_decoder.feed(b)) {
            LineStatusV2 ls{};
            if (lsv2_from_frame(g_decoder.get_frame(), ls)) {
                g_lsv2 = ls;
                g_have_lsv2 = true;
                g_last_lsv2_ms = millis();
                if (linea_presente(ls)) g_last_line_ms = millis();
            }
        }
    }
}

bool enlace_ok() {
    return g_have_lsv2 && (millis() - g_last_lsv2_ms) <= LINK_TIMEOUT_MS;
}

bool safe_to_move() {
    if (!enlace_ok()) return false;            // watchdog SIEMPRE
#ifndef SWEEP_NO_SAFETY
    if (lsv2_lifted(g_lsv2)) return false;
#endif
    return true;
}

void enter_state(State s) {
    g_state = s;
    g_phase_start_ms = millis();
    if (s == State::STOPPED) {
        parar();
        Serial.println(F(">>> PARADO. Apreta el boton para patrullar."));
    } else {
        // Al arrancar: el frente ACTUAL del robot es el 0 deseado (apuntando al
        // arco). Fijamos el cero del giroscopio aca y reseteamos el PID.
        imu_recalibrate_heading_zero();
        pid_reset();
        Serial.println(F(">>> PATRULLANDO la linea del area."));
    }
}

void print_status(int corr_fb) {
    Serial.print(F(" estado="));
    Serial.print(g_state == State::STOPPED ? "PARADO" : "PATRULLA");
    Serial.print(F(" dir=")); Serial.print(g_patrol_dir > 0 ? "+" : "-");
    Serial.print(F(" | enlace="));
    Serial.print(enlace_ok() ? "OK" : "STALE!");
    Serial.print(F(" | linea="));
    Serial.print(linea_presente(g_lsv2) ? "SI" : "no");
    Serial.print(F(" cross="));
    if (g_lsv2.cross_track_mm == LSV2_NA_I16) Serial.print(F("N/A"));
    else { Serial.print(g_lsv2.cross_track_mm); Serial.print(F("mm")); }
    Serial.print(F(" corr=")); Serial.print(corr_fb);
    Serial.print(F(" | frames=")); Serial.println(g_decoder.frames_decoded());
}

elapsedMillis g_since_status;
int g_last_corr = 0;

}  // namespace

void setup() {
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    motores_init();
    parar();

    Serial.begin(115200);
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* spin */ }

    DOWN_UART.begin(DOWN_LINK_BAUD);

    // Giroscopio: init robusto (no bloquea si falla). Mantener el robot QUIETO
    // unos segundos durante el arranque para que calibre y capture el cero.
    Serial.println(F(" Inicializando giroscopio (NO mover el robot)..."));
    const bool imu_ok = imu_init();
    Serial.print(F(" Giroscopio: "));
    Serial.println(imu_ok ? F("OK (patrulla derecho con PID)")
                          : F("NO detectado (patrulla SIN correccion de giro)"));

    Serial.println();
    Serial.println(F("=================================================="));
    Serial.println(F("  ARQUERO - patrulla la linea del area + centrado"));
    Serial.println(F("=================================================="));
    Serial.println(F(" Patrulla lateral (3 motores) + correccion adelante/atras"));
    Serial.println(F(" usando cross_track_mm (+ adelante / - atras)."));
    Serial.print  (F(" Deadband centrado: ")); Serial.print(DEADBAND_MM);
    Serial.print  (F(" mm | PWM correccion: ")); Serial.println(CENTRADO_PWM_V);
    Serial.println(F(" Linea de DOWN: Serial1 @ 230400 (header UART del Zircon)."));
#ifdef SWEEP_NO_SAFETY
    Serial.println(F(" *** SIN seguridad de levantado (solo watchdog de enlace) ***"));
#endif
    Serial.println(F(" >>> SUJETA EL ROBOT o ruedas al aire la primera vez. <<<"));

    enter_state(State::STOPPED);
}

void loop() {
    poll_down_link();

    if (botonApretadoEdge()) {
        if (g_state == State::STOPPED) enter_state(State::PATROL);
        else                           enter_state(State::STOPPED);
    }

    int corr_fb = 0;

    if (g_state == State::STOPPED) {
        parar();
        digitalWrite(PIN_LED, (millis() / 250) % 2);
    } else {
        const bool linea_fresca = (millis() - g_last_line_ms) <= LOST_LINE_MS;

        if (!safe_to_move()) {
            // Enlace caido o robot levantado -> frenar.
            parar();
            pid_reset();
            g_phase_start_ms = millis();
            digitalWrite(PIN_LED, (millis() / 80) % 2);
        } else if (!linea_fresca) {
            // Perdio la linea (esquina / fin del area) -> frenar y esperar.
            parar();
            pid_reset();
            digitalWrite(PIN_LED, (millis() / 150) % 2);
        } else {
            digitalWrite(PIN_LED, HIGH);

            // --- Correccion de centrado desde cross_track_mm ---
            // cross_track_mm: + = linea adelante del centro -> ir adelante.
            //                 - = linea atras  del centro -> ir atras.
            if (g_lsv2.cross_track_mm != LSV2_NA_I16) {
                const int cross = g_lsv2.cross_track_mm;
                if (abs(cross) > DEADBAND_MM) {
                    corr_fb = (cross > 0) ? CENTRADO_PWM_V : -CENTRADO_PWM_V;
                }
            }

            // --- Correccion de GIRO (PID de heading) ---
            const float corr_rot = pid_heading_correccion();

            // Cambiar de lado de patrulla cada HALF_PERIOD_MS (barrido del arco).
            if (millis() - g_phase_start_ms >= HALF_PERIOD_MS) {
                g_patrol_dir = -g_patrol_dir;
                g_phase_start_ms = millis();
            }

            mover(g_patrol_dir, corr_fb, corr_rot);
        }
    }

    g_last_corr = corr_fb;
    if (g_since_status >= 500) {
        g_since_status = 0;
        print_status(g_last_corr);
    }
}
