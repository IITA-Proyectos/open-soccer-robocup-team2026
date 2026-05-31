// diag_top_cameras.cpp — Verifica el enlace OpenMV → TOP (Teensy 4.0) en banco.
//
// Para qué sirve:
//   Confirmar que las 2 cámaras OpenMV (frontal + trasera) hablan con el TOP, y
//   — clave — que el FORMATO que mandan es el que el TOP espera. Para eso usa el
//   **parser de PRODUCCIÓN** (`src/top/cameras.cpp`, el mismo `CameraParser` que
//   corre en el firmware real): si ese parser engancha los headers 201/202/203 y
//   decodifica X/Y, entonces el formato coincide. Si llegaran bytes pero el parser
//   no formara paquetes, el formato NO coincide (baud/orden de bytes).
//
//   NO es firmware de competencia. Standalone: solo abre los 2 UART de cámara +
//   imprime por USB. No toca IMU/ToF/CENTRAL.
//
// Cableado (Teensy 4.0 de la placa TOP):
//   • Cámara FRONTAL: su TX → Teensy RX3 = pin 15 (Serial3, conector U8). GND común.
//   • Cámara TRASERA: su TX → Teensy RX7 = pin 28 (Serial7, conector U9). GND común.
//   • Baud 19200 (igual que cam-frontal-n6.py / cam-trasera-n6.py / el generic).
//
// Uso:
//   pio run -e diag_top_cameras -t upload
//   pio device monitor -b 115200
//
// Qué vas a ver, por cámara, cada ~0.7 s:
//   - bytes recibidos (total + delta) → ¿llega algo por el cable?
//   - paquetes decodificados (total + delta) + resyncs → ¿el FORMATO engancha?
//   - últimos bytes crudos (deberías ver 201, 202, 203 repetidos)
//   - si decodifica: pelota / arco amarillo / arco azul (visible + X + Y)
//   - VEREDICTO: SIN BYTES / BYTES PERO FORMATO RARO / FORMATO OK
//
// Atribución: Claude Opus (Anthropic), vía Claude Code. Pedido: Gustavo Viollaz.

#include <Arduino.h>
#include "cameras.h"

using namespace iitasoccer;

constexpr long     CAM_BAUD   = 19200;    // baud de las cámaras (NO cambiar)
constexpr long     USB_BAUD   = 115200;   // monitor humano
constexpr uint32_t REPORT_MS  = 700;
constexpr int      DRAIN_BUDGET = 256;    // bytes máx por vuelta y por UART (no bloquear)
constexpr int      RAW_N      = 12;       // cuántos bytes crudos mostrar

struct CamLink {
    const char*    name;
    HardwareSerial* uart;
    CameraParser   parser;
    uint32_t       bytes_total   = 0;
    uint32_t       bytes_win     = 0;     // bytes desde el último reporte
    uint32_t       pkts_last     = 0;     // snapshot de packets_decoded() para el delta
    uint8_t        raw[RAW_N]    = {0};   // ring de los últimos RAW_N bytes (orden de llegada)
    uint8_t        raw_pos       = 0;
    uint32_t       last_byte_ms  = 0;
};

CamLink g_front{ "FRONTAL  (Serial3 / RX pin 15 / conector U8)", &Serial3 };
CamLink g_back { "TRASERA  (Serial7 / RX pin 28 / conector U9)", &Serial7 };
// La trasera ANTES estaba en Serial5 (pines 20/21) y su conector físico U9 nunca
// se confirmó. La escuchamos también en Serial5 por si el cable quedó ahí.
CamLink g_back5{ "TRASERA? (Serial5 / RX pin 21 — candidato, era su puerto viejo)", &Serial5 };

// Scan crudo (solo cuenta bytes) de otros UART, para ubicar dónde cae la trasera.
struct RawScan { const char* name; HardwareSerial* uart; uint32_t win = 0; };
RawScan g_scan[] = {
    { "Serial1(RX0)",  &Serial1 },
    { "Serial4(RX16)", &Serial4 },
    { "Serial6(RX25)", &Serial6 },
};

void drain(CamLink& c, uint32_t now) {
    int budget = DRAIN_BUDGET;
    while (c.uart->available() && budget-- > 0) {
        uint8_t b = static_cast<uint8_t>(c.uart->read());
        c.bytes_total++;
        c.bytes_win++;
        c.last_byte_ms = now;
        c.raw[c.raw_pos] = b;
        c.raw_pos = (c.raw_pos + 1) % RAW_N;
        c.parser.feed(b);                 // alimenta el parser de PRODUCCIÓN
    }
}

void report(CamLink& c, uint32_t now) {
    const uint32_t pkts   = c.parser.packets_decoded();
    const uint32_t dpkts  = pkts - c.pkts_last;
    c.pkts_last           = pkts;
    const bool viva       = (c.bytes_total > 0) && ((now - c.last_byte_ms) < 1500);

    Serial.print("== "); Serial.print(c.name); Serial.println(" ==");
    Serial.print("   bytes: ");   Serial.print(c.bytes_total);
    Serial.print(" (+");          Serial.print(c.bytes_win);     Serial.print(")");
    Serial.print("   paquetes: ");Serial.print(pkts);
    Serial.print(" (+");          Serial.print(dpkts);           Serial.print(")");
    Serial.print("   resyncs: "); Serial.println(c.parser.resync_events());

    // Dump crudo: buscá 201, 202, 203 (los headers) entre estos bytes.
    Serial.print("   crudo: ");
    for (int i = 0; i < RAW_N; i++) {
        Serial.print(c.raw[(c.raw_pos + i) % RAW_N]);  // en orden de llegada
        Serial.print(' ');
    }
    Serial.println();

    if (!viva) {
        Serial.println("   >>> SIN BYTES. Revisar: TX de la cámara -> RX del Teensy (pin correcto),");
        Serial.println("       GND común, que la cámara corra su main.py, y baud 19200.");
    } else if (dpkts == 0) {
        Serial.println("   >>> LLEGAN BYTES PERO EL FORMATO NO ENGANCHA (0 paquetes nuevos).");
        Serial.println("       El formato NO coincide. Deberías ver 201/202/203 en el crudo de arriba.");
        Serial.println("       Revisar baud (19200) y el orden de bytes del packet.");
    } else {
        const CameraPacket& p = c.parser.get_packet();
        Serial.println("   >>> FORMATO OK — el parser de produccion decodifica. Ultimo paquete:");
        Serial.print("       pelota:   vis="); Serial.print(p.ball_visible ? "SI" : "no");
        Serial.print("  X="); Serial.print(p.ball_x);  Serial.print("  Y="); Serial.println(p.ball_y);
        Serial.print("       amarillo: vis="); Serial.print(p.goal_yellow_visible ? "SI" : "no");
        Serial.print("  X="); Serial.print(p.goal_yellow_x); Serial.print("  Y="); Serial.println(p.goal_yellow_y);
        Serial.print("       azul:     vis="); Serial.print(p.goal_blue_visible ? "SI" : "no");
        Serial.print("  X="); Serial.print(p.goal_blue_x);   Serial.print("  Y="); Serial.println(p.goal_blue_y);
        Serial.println("       (sin calibrar -> los vis pueden dar 'no' o detectar ruido; lo que importa");
        Serial.println("        ahora es que LLEGAN paquetes y el formato engancha.)");
    }
    c.bytes_win = 0;
}

void setup() {
    Serial.begin(USB_BAUD);
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* esperar host hasta 2 s */ }

    Serial3.begin(CAM_BAUD);   // frontal (U8)
    Serial7.begin(CAM_BAUD);   // trasera (U9, según firmware actual)
    Serial5.begin(CAM_BAUD);   // trasera candidato (su puerto viejo)
    Serial1.begin(CAM_BAUD); Serial4.begin(CAM_BAUD); Serial6.begin(CAM_BAUD);  // scan

    Serial.println();
    Serial.println("=====================================================");
    Serial.println(" diag_top_cameras — verificacion enlace OpenMV -> TOP");
    Serial.println("=====================================================");
    Serial.println(" Frontal = Serial3 (RX pin 15, U8) | Trasera = Serial7 (RX pin 28, U9)");
    Serial.println(" Baud camaras = 19200. Usa el parser de PRODUCCION (cameras.cpp).");
    Serial.println(" Cada camara manda 9 bytes [201,Xp,Yp, 202,Xam,Yam, 203,Xaz,Yaz] ~30 Hz.");
    Serial.println(" Esperando datos...");
}

void loop() {
    const uint32_t now = millis();
    drain(g_front, now);
    drain(g_back,  now);
    drain(g_back5, now);
    for (auto& s : g_scan) {
        int budget = DRAIN_BUDGET;
        while (s.uart->available() && budget-- > 0) { s.uart->read(); s.win++; }
    }

    static uint32_t t_last = 0;
    if (now - t_last >= REPORT_MS) {
        t_last = now;
        Serial.print("\n----- t="); Serial.print(now / 1000); Serial.println(" s -----");
        report(g_front, now);
        report(g_back,  now);
        report(g_back5, now);
        Serial.print("   SCAN otros UART (buscando la trasera, bytes nuevos): ");
        for (auto& s : g_scan) {
            Serial.print(s.name); Serial.print("=+"); Serial.print(s.win); Serial.print("  ");
            s.win = 0;
        }
        Serial.println();
    }
}
