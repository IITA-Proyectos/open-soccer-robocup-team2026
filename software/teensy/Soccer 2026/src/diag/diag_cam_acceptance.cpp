// diag_cam_acceptance.cpp — Diag de BANCO: aceptación de la visión recalibrada (TASK-022).
//
// Para qué sirve
// --------------
// La recalibración de las cámaras (LAB + homografía) es el bloqueante #1 para
// Incheon. Este sketch NO recalibra: VERIFICA que, una vez recalibradas, la
// cámara reporta posiciones correctas. Lee las 2 cámaras OpenMV con el MISMO
// parser que usa el TOP en competencia (`src/top/cameras.cpp`, CameraParser v2),
// así que valida de paso el contrato de 11 bytes (CRC8 + sentinel + END).
//
// Qué muestra (cada ~300 ms, por cámara):
//   • Salud del enlace: pkts decodificados, crc_errors, resyncs.
//   • Pelota: visible? + coords crudas (unidades de cámara) + mm + ángulo + distancia.
//   • Arcos amarillo/azul: visible?.
//
// Modo ACEPTACIÓN (PASS/FAIL):
//   1. Poné la pelota a una distancia MEDIDA con cinta (p.ej. 60 cm, centrada).
//   2. Escribí esa distancia en CENTÍMETROS por el monitor serie + Enter (p.ej. "60").
//   3. Apretá 'p': compara la distancia reportada vs la esperada y dice PASS/FAIL
//      (tolerancia = la mayor entre ±5 cm y ±10 %). También imprime el ángulo
//      (debería ser ~0° si la pelota está centrada al frente).
//   Comandos: <número>+Enter = distancia esperada (cm) · p = chequear ·
//             f/b = elegir cámara frontal/trasera para el chequeo · r = reset parsers.
//
// Aislamiento: este sketch vive en src/diag y NINGÚN módulo vivo lo incluye →
// regresión nula sobre el firmware de competencia. Solo compila el parser de
// producción (cameras.cpp) + este archivo.
//
// Uso:
//   pio run -e diag_cam_acceptance -t upload
//   pio device monitor -b 115200
//
// Hardware: placa TOP (Teensy 4.0). Cámara FRONTAL en Serial3 (RX3=pin 15, U8),
// TRASERA en Serial5 (RX5=pin 21, U9). Baud 19200 (= cam-*-n6.py / cameras_runtime).
//
// Author: Claude Opus 4.8 (Anthropic). Requested-by: Viollaz. (2026-06-04)

#include <Arduino.h>
#include <math.h>
#include <stdlib.h>
#include "cameras.h"   // CameraParser de PRODUCCIÓN (mismo parser que corre en el TOP)

using namespace iitasoccer;

// Mismo factor que cameras_runtime.cpp (CAMERA_UNIT_TO_MM): 1 unidad ≈ 1 cm.
static constexpr float UNIT_TO_MM = 10.0f;
// Baud del enlace cámara→TOP (= UART_BAUD de cameras_runtime y de cam-*-n6.py).
static constexpr long  CAM_BAUD   = 19200;
// Tolerancia de aceptación de la distancia: la MAYOR entre estos dos.
static constexpr float TOL_ABS_MM = 50.0f;   // ±5 cm
static constexpr float TOL_PCT    = 0.10f;   // ±10 %

static CameraParser g_front;
static CameraParser g_back;

static float    g_expected_dist_mm = 0;  // distancia esperada de la pelota (0 = sin setear)
static int      g_sel = 0;               // cámara bajo aceptación: 0 = frontal, 1 = trasera
static uint32_t g_last_print = 0;
static char     g_numbuf[8];
static int      g_numlen = 0;

struct BallView {
    bool    vis;
    int16_t x_mm, y_mm;
    float   angle_deg;   // atan2(x, y): 0 = al frente, + = a la derecha
    float   dist_mm;
};

static BallView ball_view(const CameraParser& p) {
    const CameraPacket& pk = p.get_packet();
    BallView v{};
    v.vis  = pk.ball_visible;
    v.x_mm = static_cast<int16_t>(pk.ball_x * UNIT_TO_MM);
    v.y_mm = static_cast<int16_t>(pk.ball_y * UNIT_TO_MM);
    v.angle_deg = atan2f(static_cast<float>(v.x_mm), static_cast<float>(v.y_mm)) * 180.0f / PI;
    v.dist_mm   = sqrtf(static_cast<float>(v.x_mm) * v.x_mm +
                        static_cast<float>(v.y_mm) * v.y_mm);
    return v;
}

static void feed_serial(HardwareSerial& s, CameraParser& p) {
    while (s.available() > 0) p.feed(static_cast<uint8_t>(s.read()));
}

static void print_cam(const char* name, CameraParser& p) {
    const CameraPacket& pk = p.get_packet();
    BallView b = ball_view(p);
    Serial.print(name);
    Serial.print(" pkts=");  Serial.print(p.packets_decoded());
    Serial.print(" crc=");   Serial.print(p.crc_errors());
    Serial.print(" rsy=");   Serial.print(p.resync_events());
    Serial.print(" | BALL vis="); Serial.print(b.vis ? "Y" : "N");
    if (b.vis) {
        Serial.print(" raw=(");  Serial.print(pk.ball_x); Serial.print(",");
        Serial.print(pk.ball_y); Serial.print(")");
        Serial.print(" mm=(");   Serial.print(b.x_mm);    Serial.print(",");
        Serial.print(b.y_mm);    Serial.print(")");
        Serial.print(" ang=");   Serial.print(b.angle_deg, 1); Serial.print("deg");
        Serial.print(" dist=");  Serial.print(b.dist_mm, 0);   Serial.print("mm");
    }
    Serial.print(" | YEL=");  Serial.print(pk.goal_yellow_visible ? "Y" : "N");
    Serial.print(" BLU=");    Serial.print(pk.goal_blue_visible ? "Y" : "N");
    Serial.println();
}

static void run_acceptance() {
    CameraParser& p   = (g_sel == 0) ? g_front : g_back;
    const char*  name = (g_sel == 0) ? "FRONTAL" : "TRASERA";
    BallView b = ball_view(p);
    Serial.println(F("---- ACEPTACION ----"));
    Serial.print(F("Camara: ")); Serial.println(name);
    if (!b.vis) {
        Serial.println(F("  FAIL: la camara NO ve la pelota (recalibrar LAB / acercar / luz)."));
        return;
    }
    if (g_expected_dist_mm <= 0) {
        Serial.println(F("  (sin esperado) Escribi la distancia real en cm + Enter, despues 'p'."));
        Serial.print(F("  Reportado: dist=")); Serial.print(b.dist_mm, 0);
        Serial.print(F("mm  ang="));            Serial.print(b.angle_deg, 1);
        Serial.println(F("deg"));
        return;
    }
    float err = fabsf(b.dist_mm - g_expected_dist_mm);
    float tol = TOL_ABS_MM;
    if (g_expected_dist_mm * TOL_PCT > tol) tol = g_expected_dist_mm * TOL_PCT;
    Serial.print(F("  Esperado="));  Serial.print(g_expected_dist_mm, 0);
    Serial.print(F("mm  Reportado=")); Serial.print(b.dist_mm, 0);
    Serial.print(F("mm  err="));     Serial.print(err, 0);
    Serial.print(F("mm  tol="));     Serial.print(tol, 0);
    Serial.print(F("mm  -> "));      Serial.println(err <= tol ? F("PASS") : F("FAIL"));
    Serial.print(F("  Angulo reportado=")); Serial.print(b.angle_deg, 1);
    Serial.println(F("deg (0=al frente; ~0 si la pelota esta centrada)"));
}

static void handle_serial_cmd() {
    while (Serial.available() > 0) {
        char c = static_cast<char>(Serial.read());
        if (c >= '0' && c <= '9') {
            if (g_numlen < static_cast<int>(sizeof(g_numbuf)) - 1) g_numbuf[g_numlen++] = c;
        } else if (c == '\n' || c == '\r') {
            if (g_numlen > 0) {
                g_numbuf[g_numlen] = 0;
                g_expected_dist_mm = static_cast<float>(atof(g_numbuf)) * 10.0f;  // cm -> mm
                Serial.print(F("Distancia esperada = "));
                Serial.print(g_expected_dist_mm, 0); Serial.println(F(" mm"));
                g_numlen = 0;
            }
        } else if (c == 'p' || c == 'P') {
            run_acceptance();
        } else if (c == 'f' || c == 'F') {
            g_sel = 0; Serial.println(F("Camara de aceptacion: FRONTAL"));
        } else if (c == 'b' || c == 'B') {
            g_sel = 1; Serial.println(F("Camara de aceptacion: TRASERA"));
        } else if (c == 'r' || c == 'R') {
            g_front.reset(); g_back.reset();
            Serial.println(F("Parsers reseteados."));
        }
    }
}

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 3000) { /* esperar USB hasta 3 s */ }
    Serial3.begin(CAM_BAUD);   // cámara FRONTAL (U8, RX3 = pin 15)
    Serial5.begin(CAM_BAUD);   // cámara TRASERA (U9, RX5 = pin 21)
    Serial.println();
    Serial.println(F("=== diag_cam_acceptance — aceptacion de vision (TASK-022) ==="));
    Serial.println(F("Lee las 2 camaras con el parser de PRODUCCION (cameras.cpp v2)."));
    Serial.println(F("Comandos: <numero>+Enter = distancia real de la pelota (cm) |"));
    Serial.println(F("          p = PASS/FAIL | f/b = elegir frontal/trasera | r = reset"));
    Serial.println(F("Banco: pone la pelota a una distancia MEDIDA, escribi los cm, Enter, 'p'."));
    Serial.println();
}

void loop() {
    feed_serial(Serial3, g_front);
    feed_serial(Serial5, g_back);
    handle_serial_cmd();
    if (millis() - g_last_print >= 300) {
        g_last_print = millis();
        print_cam("FRONTAL", g_front);
        print_cam("TRASERA", g_back);
    }
}
