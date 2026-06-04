// diag_top_cameras.cpp — Verifica el enlace OpenMV → TOP (Teensy 4.0) en banco.
//
// v3 (2026-05-31): escucha LOS 7 UART del Teensy 4.0 (Serial1..Serial7) a la vez,
//   cada uno con el parser de PRODUCCIÓN (cameras.cpp). Imprime UNA LÍNEA POR UART,
//   así se ve EXPLÍCITAMENTE en cuál cae cada cámara (no más "scan" de una sola línea).
//
// v3.1 (2026-06-03): además de bytes/paquetes, cada línea muestra crc= y resync=
//   (contadores del parser v2). Sirve para CONTAR errores de enlace en banco
//   (TASK-015 / P0-CAM-CRC): un enlace sano deja ambos ~0 en 10 min; si crc sube,
//   hay bit-flips en el cable cámara→TOP.
//
//   NO es firmware de competencia. Standalone: solo abre los UART + imprime por USB.
//
// Para qué sirve:
//   Confirmar que las cámaras OpenMV hablan con el TOP y que el FORMATO (9 bytes,
//   headers 201/202/203) es el que el TOP espera. Si un UART recibe bytes y el
//   parser forma paquetes → "FORMATO OK". Sirve para ENCONTRAR en qué pin/UART
//   quedó cableada cada cámara.
//
// Pines RX de cada UART en Teensy 4.0:
//   Serial1=0  Serial2=7  Serial3=15 (FRONTAL/U8)  Serial4=16
//   Serial5=21 (CÁMARA TRASERA, soldada acá)  Serial6=25  Serial7=28 (→CENTRAL, es TX)
//   + GND común entre cada cámara y el Teensy. Cámaras alimentadas + corriendo main.py.
//
// Uso:
//   pio run -e diag_top_cameras -t upload
//   pio device monitor -b 115200
//
// Atribución: Claude Opus (Anthropic), vía Claude Code. Pedido: Gustavo Viollaz.

#include <Arduino.h>
#include "cameras.h"

using namespace iitasoccer;

constexpr long     CAM_BAUD     = 19200;
constexpr long     USB_BAUD     = 115200;
constexpr uint32_t REPORT_MS    = 800;
constexpr int      DRAIN_BUDGET = 256;
constexpr int      RAW_N        = 12;

struct Link {
    const char*     name;
    HardwareSerial* uart;
    CameraParser    parser;
    uint32_t        bytes_total  = 0;
    uint32_t        bytes_win    = 0;
    uint32_t        pkts_last    = 0;
    uint8_t         raw[RAW_N]   = {0};
    uint8_t         raw_pos      = 0;
    uint32_t        last_byte_ms = 0;
};

// LOS 7 UART del Teensy 4.0. Frontal=Serial3, Trasera=Serial5 (ambas verificadas).
Link g_links[] = {
    { "Serial1 (RX pin 0 )",                 &Serial1 },
    { "Serial2 (RX pin 7 )",                 &Serial2 },
    { "Serial3 (RX pin 15) [FRONTAL / U8]",  &Serial3 },
    { "Serial4 (RX pin 16)",                 &Serial4 },
    { "Serial5 (RX pin 21) [CAMARA TRASERA]",&Serial5 },
    { "Serial6 (RX pin 25)",                 &Serial6 },
    { "Serial7 (RX pin 28) [link CENTRAL/TX]",&Serial7 },
};

void drain(Link& c, uint32_t now) {
    int budget = DRAIN_BUDGET;
    while (c.uart->available() && budget-- > 0) {
        uint8_t b = static_cast<uint8_t>(c.uart->read());
        c.bytes_total++;
        c.bytes_win++;
        c.last_byte_ms = now;
        c.raw[c.raw_pos] = b;
        c.raw_pos = (c.raw_pos + 1) % RAW_N;
        c.parser.feed(b);
    }
}

void report(Link& c, uint32_t now) {
    const uint32_t pkts  = c.parser.packets_decoded();
    const uint32_t dpkts = pkts - c.pkts_last;
    c.pkts_last          = pkts;
    const bool viva      = (c.bytes_total > 0) && ((now - c.last_byte_ms) < 1500);

    Serial.print(viva ? "  [DATOS] " : "  [ ---- ] ");
    Serial.print(c.name);
    Serial.print("  bytes=+"); Serial.print(c.bytes_win);
    Serial.print(" (tot ");    Serial.print(c.bytes_total); Serial.print(")");
    Serial.print("  paq=+");   Serial.print(dpkts);
    // Telemetría de integridad del enlace (parser v2 — TASK-015 / P0-CAM-CRC):
    //   crc    = frames de 11 B con CRC8 malo (bit-flip en el cable cámara→TOP).
    //   resync = pérdida de framing (header/END fuera de lugar → re-lock).
    // Acumulados; en un enlace sano deben quedar ~0 en 10 min de banco.
    Serial.print("  crc=");    Serial.print(c.parser.crc_errors());
    Serial.print("  resync="); Serial.print(c.parser.resync_events());

    if (viva && dpkts > 0) {
        const CameraPacket& p = c.parser.get_packet();
        Serial.print("  -> FORMATO OK  pelota(vis=");
        Serial.print(p.ball_visible ? "SI" : "no");
        Serial.print(" X="); Serial.print(p.ball_x);
        Serial.print(" Y="); Serial.print(p.ball_y);  Serial.print(")");
    } else if (viva && dpkts == 0) {
        Serial.print("  -> llegan bytes pero NO forma paquetes (¿baud? ¿orden?). crudo:");
        for (int i = 0; i < RAW_N; i++) { Serial.print(' '); Serial.print(c.raw[(c.raw_pos + i) % RAW_N]); }
    }
    Serial.println();
    c.bytes_win = 0;
}

void setup() {
    Serial.begin(USB_BAUD);
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* esperar host hasta 2 s */ }

    for (auto& c : g_links) c.uart->begin(CAM_BAUD);

    Serial.println();
    Serial.println("=============================================================");
    Serial.println(" diag_top_cameras v3 — escucha LOS 7 UART (Serial1..Serial7)");
    Serial.print  (" build: "); Serial.print(__DATE__); Serial.print(' '); Serial.println(__TIME__);
    Serial.println(" Baud 19200. Parser de PRODUCCION (cameras.cpp).");
    Serial.println(" [DATOS] = llega algo por ese UART ; [ ---- ] = sin bytes.");
    Serial.println(" Frontal ya verificada = Serial3. Buscando la trasera en el resto.");
    Serial.println("=============================================================");
}

void loop() {
    const uint32_t now = millis();
    for (auto& c : g_links) drain(c, now);

    static uint32_t t_last = 0;
    if (now - t_last >= REPORT_MS) {
        t_last = now;
        Serial.print("\n----- t="); Serial.print(now / 1000); Serial.println(" s -----");
        for (auto& c : g_links) report(c, now);
    }
}
