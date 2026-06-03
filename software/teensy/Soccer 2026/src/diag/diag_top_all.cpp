// diag_top_all.cpp — TEST INTEGRAL de la placa TOP (Teensy 4.0).
//
// Inicializa y muestra por USB, en un solo panel, TODOS los sensores de la TOP:
//   • BNO055   : heading fusionado + L/R individuales + desacuerdo.
//   • 4 ToF    : distancia por sensor (frente/atras/derecha/izquierda) + min.
//   • HC-SR04  : distancia frontal (ultrasonido).
//   • Camaras  : front/back vivas + pelota (vis/x/y) + arcos + packets.
//   • Referee  : pines 5 y 6 EN CRUDO + como darian AND y OR (para decidir la logica).
//
// NO es firmware de competencia: no manda snapshot ni mueve nada. Reusa los modulos
// VIVOS del TOP (sensors_tof, sensors_imu, cameras, comm_arbiter), asi lo que ves es
// exactamente lo que veria el firmware real.
//
// Uso (las camaras deben estar alimentadas + corriendo su main.py):
//   pio run -e diag_top_all -t upload
//   (cortar y reponer energia para enumerar los ToF limpio)
//   pio device monitor -b 115200
//   -> el boot tarda ~10-15 s (carga el firmware de los 4 ToF).
//
// Atribucion: Claude Opus 4.8 (Anthropic), pedido de Gustavo Viollaz (@gviollaz).

#include <Arduino.h>

#include "sensors_tof.h"
#include "sensors_imu.h"
#include "cameras_runtime.h"
#include "comm_arbiter.h"

using namespace iitasoccer;

namespace {

// Pines del referee (nivel GPIO, TASK-039). comm_arbiter_init los pone INPUT_PULLDOWN.
constexpr uint8_t PIN_REF_A = 5;   // OUT1 (PLAY/STOP)
constexpr uint8_t PIN_REF_B = 6;   // OUT2 (espejo de OUT1)

// Etiqueta de cada indice de ToF (pinout_robot1.h: 0=FRENTE 1=ATRAS 2=DER 3=IZQ).
const char* const TOF_LABEL[4] = { "F", "A", "D", "I" };

elapsedMillis g_since_imu;
elapsedMillis g_since_tof;
elapsedMillis g_since_panel;

void print_mm(uint16_t mm) {
    if (mm == TOF_NO_READING) Serial.print(F("----"));
    else { Serial.print(mm); Serial.print(F("mm")); }
}

void print_panel() {
    Serial.println();
    Serial.println(F("============== TEST INTEGRAL TOP =============="));

    // --- BNO ---
    Serial.print(F("BNO    : hdg="));
    Serial.print(sensors_imu_get_heading_deg(), 1);
    Serial.print(F("deg | L="));
    Serial.print(sensors_imu_left_ready() ? "Y" : "N");
    Serial.print('(');  Serial.print(sensors_imu_get_left_heading_deg(), 1);  Serial.print(')');
    Serial.print(F(" R="));
    Serial.print(sensors_imu_right_ready() ? "Y" : "N");
    Serial.print('(');  Serial.print(sensors_imu_get_right_heading_deg(), 1); Serial.print(')');
    Serial.print(F(" desac="));
    Serial.print(sensors_imu_get_disagreement_deg(), 1);
    Serial.println(F("deg"));

    // --- ToF ---
    Serial.print(F("ToF    : "));
    for (uint8_t i = 0; i < 4; ++i) {
        Serial.print(TOF_LABEL[i]); Serial.print('=');
        print_mm(sensors_tof_get_distance_mm(i));
        Serial.print(' ');
    }
    Serial.print(F("| min="));
    print_mm(sensors_tof_get_min_distance_mm());
    Serial.print(F(" [ready "));
    for (uint8_t i = 0; i < 4; ++i) Serial.print(sensors_tof_is_ready(i) ? "Y" : "N");
    Serial.println(']');

    // --- HC-SR04 ---
    Serial.print(F("HC-SR04: "));
    print_mm(sensors_hcsr04_get_distance_mm());
    Serial.println();

    // --- Camaras ---
    Serial.print(F("CAM    : F="));
    Serial.print(cameras_front_alive() ? "Y" : "N");
    Serial.print(F(" B="));
    Serial.print(cameras_back_alive() ? "Y" : "N");
    Serial.print(F(" | ball vis="));
    if (cameras_ball_visible()) {
        Serial.print(F("SI ("));
        Serial.print(cameras_get_ball_x_mm()); Serial.print(',');
        Serial.print(cameras_get_ball_y_mm()); Serial.print(F(")mm conf="));
        Serial.print(cameras_get_ball_confidence());
    } else {
        Serial.print(F("no"));
    }
    Serial.print(F(" | goalY vis="));
    Serial.print(cameras_goal_yellow_visible() ? "Y" : "N");
    Serial.print(F(" ang="));   Serial.print(cameras_get_goal_yellow_angle_centideg() / 100.0f, 0);
    Serial.print(F(" dist="));  Serial.print(cameras_get_goal_yellow_distance_mm());
    Serial.print(F("mm | goalB vis="));
    Serial.println(cameras_goal_blue_visible() ? "Y" : "N");
    Serial.print(F("         pkts F/B="));
    Serial.print(cameras_packets_front()); Serial.print('/');
    Serial.print(cameras_packets_back());
    Serial.print(F(" resync="));
    Serial.println(cameras_resyncs_total());

    // --- Referee (pines 5/6 EN CRUDO + AND/OR) ---
    const int p5 = digitalRead(PIN_REF_A);
    const int p6 = digitalRead(PIN_REF_B);
    Serial.print(F("REFEREE: pin5="));
    Serial.print(p5);
    Serial.print(F(" pin6="));
    Serial.print(p6);
    Serial.print(F(" | AND="));
    Serial.print((p5 && p6) ? "GO" : "STOP");
    Serial.print(F(" OR="));
    Serial.print((p5 || p6) ? "GO" : "STOP");
    Serial.print(F(" | comm_arbiter: match="));
    Serial.print(comm_arbiter_is_match_running() ? "Y" : "N");
    Serial.print(F(" cmd="));
    Serial.println(static_cast<int>(comm_arbiter_get_last_command()));

    Serial.println(F("==============================================="));
}

}  // namespace

void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    Serial.begin(115200);
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* spin */ }

    Serial.println(F("\n=================================================="));
    Serial.println(F("  diag_top_all  -  TEST INTEGRAL de la placa TOP"));
    Serial.println(F("=================================================="));

    // Mismo orden de init que main_top (receta validada): dormir ToF -> BNO ->
    // enumerar ToF -> camaras -> arbitro (configura pines 5/6 INPUT_PULLDOWN).
    Serial.println(F("[init] durmiendo ToF + BNO..."));
    sensors_tof_predim_lp();
    sensors_imu_init();
    Serial.println(F("[init] enumerando 4 ToF (carga firmware, ~10-15 s)..."));
    sensors_tof_init();
    Serial.println(F("[init] camaras (Serial3 frontal + Serial5 trasera)..."));
    cameras_init();
    Serial.println(F("[init] arbitro GPIO (pines 5/6) + COMM (Serial2)..."));
    comm_arbiter_init();

    digitalWrite(PIN_LED_STATUS, HIGH);
    Serial.println(F("[init] listo. Panel cada 500 ms:"));
}

void loop() {
    cameras_tick();        // drena Serial3 + Serial5
    comm_arbiter_tick();   // partner UART + lee referee GPIO 5/6

    if (g_since_imu >= 10) { g_since_imu = 0; sensors_imu_tick(); }
    if (g_since_tof >= 30) { g_since_tof = 0; sensors_tof_tick(); }

    if (g_since_panel >= 500) {
        g_since_panel = 0;
        print_panel();
    }
}
