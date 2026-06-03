// diag_top_comm_down.cpp — Verifica el enlace DOWN -> TOP (Serial1).
// NO es firmware de competencia.
//
// Para qué sirve
// --------------
// Tras desenchufar/reconectar cables entre la placa DOWN (abajo) y la TOP
// (arriba), este diag confirma que el enlace UART quedó bien: que el TOP
// RECIBE los frames que la DOWN emite, sin errores de CRC, a buen ritmo.
//
// Qué muestra (a ~2 Hz por el Serial USB del TOP):
//   • frames totales recibidos + frames/s (tasa).
//   • errores de CRC acumulados (frames corruptos = cable ruidoso/flojo).
//   • para cada uno de los 3 mensajes que manda la DOWN, si está FRESCO
//     (llegó hace < timeout) y su último valor:
//       - LINE (LineStatusV2): ¿hay línea?, ángulo, profundidad.
//       - OTOS POSE (Pose2D):  x, y, heading, confidence.
//       - OTOS VEL  (Velocity2D): vx, vy, omega, slip.
//
// Reusa el MÓDULO VIVO src/top/comm_down.cpp (mismo parser/contrato que
// producción), así que si esto recibe bien, el firmware real también.
// NO manda snapshot a CENTRAL ni mueve nada — solo escucha la DOWN.
//
// Cableado esperado (recableado 2026): DOWN -> TOP por Serial1 (RX0=pin0,
// TX1=pin1), 230400 baud (UART_FROM_DOWN_BAUD).
//
// Uso
// ---
//   pio run -e diag_top_comm_down -t upload
//   pio device monitor -b 115200
//
// Lectura del resultado:
//   ✅ ENLACE OK  -> frames/s > 0, CRC-err = 0, los 3 mensajes FRESCOS.
//   ⚠️ frames=0   -> no llega NADA: revisar TX(DOWN pin1?) -> RX(TOP pin0),
//                    GND común, y que la DOWN esté encendida y flasheada.
//   ⚠️ CRC-err sube-> llegan bytes pero corruptos: cable largo/ruidoso/flojo
//                    o baud distinto entre placas. Revisar conexión.
//   ⚠️ frames>0 pero un mensaje NO fresco -> la DOWN no está emitiendo ese
//                    tipo (p.ej. OTOS sin inicializar). El enlace está OK.

#include <Arduino.h>
#include "comm_down.h"
#include "types.h"

using namespace iitasoccer;

namespace {

uint32_t g_last_print_ms = 0;
uint32_t g_last_frames   = 0;
uint32_t g_last_rate_ms  = 0;

void print_line(const char* label, bool fresh) {
    Serial.print(label);
    Serial.print(fresh ? "[FRESCO] " : "[ -- no llega -- ] ");
}

}  // namespace

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 3000) {}

    Serial.println("\n============================================================");
    Serial.println("  TOP — Verificador del enlace DOWN -> TOP (Serial1)");
    Serial.println("============================================================");
    Serial.println("Escuchando frames de la placa DOWN (linea + OTOS pose/vel).");
    Serial.println("OK = frames/s > 0, CRC-err = 0, los 3 mensajes FRESCOS.");
    Serial.println("------------------------------------------------------------");

    comm_down_init();   // Serial1 @ 230400 (mismo que produccion)
    g_last_rate_ms = millis();
}

void loop() {
    // Drenar el UART todo el tiempo (igual que el loop real del TOP).
    comm_down_tick();

    const uint32_t now = millis();
    if (now - g_last_print_ms < 500) return;   // imprimir a ~2 Hz
    g_last_print_ms = now;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

    const uint32_t frames = comm_down_get_frames_received();
    const uint32_t crc_err = comm_down_get_crc_errors();

    // Tasa de frames/s desde el ultimo calculo.
    const uint32_t dt = now - g_last_rate_ms;
    float fps = 0.0f;
    if (dt > 0) fps = (frames - g_last_frames) * 1000.0f / dt;
    g_last_frames = frames;
    g_last_rate_ms = now;

    Serial.print("frames=");
    Serial.print(frames);
    Serial.print(" (");
    Serial.print(fps, 1);
    Serial.print("/s)  CRC-err=");
    Serial.print(crc_err);
    Serial.print("  |  ");

    if (frames == 0) {
        Serial.println("NADA todavia -> revisar TX(DOWN) -> RX(TOP pin0), GND comun, DOWN encendida.");
        return;
    }

    // Estado por mensaje.
    print_line("LINE", comm_down_is_line_fresh());
    if (comm_down_is_line_fresh()) {
        const LineStatusV2& ls = comm_down_get_line_status();
        Serial.print("(valid="); Serial.print(ls.data_valid);
        Serial.print(" ang="); Serial.print(ls.line_angle_centideg / 100);
        Serial.print(") ");
    }

    print_line(" POSE", comm_down_is_pose_fresh());
    if (comm_down_is_pose_fresh()) {
        const Pose2D& p = comm_down_get_pose();
        Serial.print("(x="); Serial.print(p.x_mm);
        Serial.print(" y="); Serial.print(p.y_mm);
        Serial.print(" hdg="); Serial.print(p.heading_centideg / 100);
        Serial.print(" conf="); Serial.print(p.confidence);
        Serial.print(") ");
    }

    print_line(" VEL", comm_down_is_vel_fresh());
    if (comm_down_is_vel_fresh()) {
        const Velocity2D& v = comm_down_get_velocity();
        Serial.print("(vx="); Serial.print(v.vx_mm_s);
        Serial.print(" vy="); Serial.print(v.vy_mm_s);
        Serial.print(" w="); Serial.print(v.omega_centideg_s / 100);
        Serial.print(") ");
    }

    Serial.println();
    if (crc_err > 0) {
        Serial.println("  ^ OJO: hay errores de CRC -> cable ruidoso/flojo o baud distinto.");
    }
}
