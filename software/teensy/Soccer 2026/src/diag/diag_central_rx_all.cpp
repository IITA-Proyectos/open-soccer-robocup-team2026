// diag_central_rx_all.cpp — Receiver de banco de la CENTRAL: escucha LAS DOS placas
// (DOWN por Serial1 + TOP por Serial7), corre el FrameDecoder del protocolo (proto.h)
// en cada enlace, DECODIFICA campo por campo cada mensaje segun el contrato, y lo
// imprime por USB como un panel legible + salud de cada enlace.
//
// NO es firmware de competencia: no decide ni mueve motores. Sirve para probar, con
// las 3 placas conectadas, que la cadena de comunicacion funciona end-to-end y que los
// campos llegan bien decodificados.
//
// VEREDICTO DE COMPLETITUD: ademas del volcado campo-por-campo, el panel cierra con
// un bloque que responde de frente "¿la CENTRAL recibe TODA la info de TOP y DOWN?":
//   - [OK]/[STALE]/[FALTA] + tasa Hz por cada mensaje esperado
//     (DOWN: LINEA + OTOS pose + OTOS vel ; TOP: SNAPSHOT),
//   - salud de cada enlace (CRC + huecos de SEQ),
//   - una linea final  >>> CENTRAL recibe TODO de DOWN+TOP: SI/NO (falta: ...) <<<.
//   El LED de la CENTRAL queda FIJO solo si TODO llega completo y fresco.
//   Completo = los 4 mensajes frescos (<500 ms) + ambos enlaces sin CRC err / sin seqGap.
//
// Enlaces (Teensy 4.1 de la CENTRAL):
//   • DOWN -> CENTRAL : Serial1 (RX1 = pin 0).  DOWN difunde por su Serial1 (TX1 = pin 1).
//       Mensajes: LINE_URGENT 0x10 (LineStatusV2 16B) + DOWN_OTOS_POSE 0x11 (Pose2D 7B)
//                 + DOWN_OTOS_VEL 0x12 (Velocity2D 7B).
//   • TOP  -> CENTRAL : Serial7 (RX7 = pin 28). TOP envia por su Serial4 (TX4 = pin 17).
//       Mensajes: WORLD_SNAPSHOT 0x60 (WorldSnapshot 27B).
//   Baud: 230400 en ambos. GND comun obligatorio entre las 3 placas.
//
// Uso:
//   pio run -e diag_central_rx_all -t upload
//   pio device monitor -b 115200
//
// Atribucion: Claude Opus 4.8 (Anthropic), pedido de Gustavo Viollaz (@gviollaz).

#include <Arduino.h>
#include <string.h>

#include "proto.h"
#include "types.h"
#include "line_view.h"
#include "pose_view.h"

using namespace iitasoccer;

namespace {

constexpr long LINK_BAUD = 230400;          // mismo baud que DOWN y TOP

#define DOWN_UART Serial1                   // RX1 = pin 0  (DOWN -> CENTRAL)
#define TOP_UART  Serial7                   // RX7 = pin 28 (TOP  -> CENTRAL)

constexpr uint32_t PANEL_INTERVAL_MS = 500;

// ---- Estado por enlace ------------------------------------------------------
struct LinkStats {
    FrameDecoder dec;
    bool     have_seq = false;
    uint8_t  last_seq = 0;
    uint32_t seq_gaps = 0;
};
LinkStats g_down;
LinkStats g_top;

void track_seq(LinkStats& lk, const Frame& f) {
    if (lk.have_seq && f.seq != static_cast<uint8_t>(lk.last_seq + 1)) lk.seq_gaps++;
    lk.last_seq = f.seq;
    lk.have_seq = true;
}

// ---- Ultimo dato de cada tipo ----------------------------------------------
LineStatusV2 g_lsv2{};  uint32_t g_lsv2_n = 0;  uint32_t g_lsv2_ms = 0;  bool g_have_lsv2 = false;
Pose2D       g_pose{};  uint32_t g_pose_n = 0;  uint32_t g_pose_ms = 0;  bool g_have_pose = false;
Velocity2D   g_vel{};   uint32_t g_vel_n  = 0;  uint32_t g_vel_ms  = 0;  bool g_have_vel  = false;
WorldSnapshot g_snap{}; uint32_t g_snap_n = 0;  uint32_t g_snap_ms = 0;  bool g_have_snap = false;
uint32_t g_down_other = 0;   // frames validos de DOWN que no son 0x10/0x11/0x12
uint32_t g_top_other  = 0;   // frames validos de TOP que no son 0x60

// Tasa (Hz) por mensaje — recalculada en cada panel sobre la ventana transcurrida.
// Sirve para distinguir "llego UN frame suelto" de "el stream fluye continuo".
uint32_t g_lsv2_prev = 0, g_pose_prev = 0, g_vel_prev = 0, g_snap_prev = 0;
uint32_t g_last_panel_ms = 0;
float    g_lsv2_hz = 0, g_pose_hz = 0, g_vel_hz = 0, g_snap_hz = 0;

// Un mensaje se considera FRESCO (llegando) si su ultimo frame tiene < 500 ms.
constexpr uint32_t FRESH_MS = 500;

elapsedMillis g_since_panel;

// Decodifica un WorldSnapshot de un frame (espejo de pose_from_frame).
bool snap_from_frame(const Frame& f, WorldSnapshot& out) {
    if (f.type != MsgType::WORLD_SNAPSHOT) return false;
    if (f.payload_len != sizeof(WorldSnapshot)) return false;
    memcpy(&out, f.payload, sizeof(WorldSnapshot));
    return true;
}

// ---- RX --------------------------------------------------------------------
void on_down_frame(const Frame& f) {
    track_seq(g_down, f);
    LineStatusV2 ls{}; Pose2D p{}; Velocity2D v{};
    if      (lsv2_from_frame(f, ls)) { g_lsv2 = ls; g_lsv2_n++; g_lsv2_ms = millis(); g_have_lsv2 = true; }
    else if (pose_from_frame(f, p))  { g_pose = p;  g_pose_n++; g_pose_ms = millis(); g_have_pose = true; }
    else if (vel_from_frame(f, v))   { g_vel  = v;  g_vel_n++;  g_vel_ms  = millis(); g_have_vel  = true; }
    else g_down_other++;
}

void on_top_frame(const Frame& f) {
    track_seq(g_top, f);
    WorldSnapshot s{};
    if (snap_from_frame(f, s)) { g_snap = s; g_snap_n++; g_snap_ms = millis(); g_have_snap = true; }
    else g_top_other++;
}

void drain(HardwareSerial& uart, LinkStats& lk, void (*on_frame)(const Frame&)) {
    while (uart.available() > 0) {
        const uint8_t b = static_cast<uint8_t>(uart.read());
        if (lk.dec.feed(b)) on_frame(lk.dec.get_frame());
    }
}

// ---- Helpers de impresion --------------------------------------------------
void p_i16_or_na(int16_t v, int16_t na, float scale, const char* unit) {
    if (v == na) { Serial.print(F("N/A")); return; }
    if (scale == 1.0f) Serial.print(v); else Serial.print(v / scale, 2);
    Serial.print(unit);
}

void print_event_flags(uint8_t ev) {
    if (ev == 0) { Serial.print(F("(ninguno)")); return; }
    if (ev & EV_IMMINENT_EXIT)     Serial.print(F("SALIDA-INMINENTE "));
    if (ev & EV_CORNER)            Serial.print(F("ESQUINA "));
    if (ev & EV_LINE_END)          Serial.print(F("FIN-LINEA "));
    if (ev & EV_LIFTED)            Serial.print(F("LEVANTADO "));
    if (ev & EV_CALIB_SUSPECT)     Serial.print(F("CALIB? "));
    if (ev & EV_MUX_DEAD)          Serial.print(F("MUX-MUERTO "));
    if (ev & EV_DEGRADED_GEOMETRY) Serial.print(F("GEOMETRIA "));
    if (ev & EV_SENSOR_NOISY)      Serial.print(F("RUIDO "));
}

void print_ref_cmd(uint8_t r) {
    switch (r) {
        case 0:  Serial.print(F("STOP"));     break;
        case 1:  Serial.print(F("START"));    break;
        case 2:  Serial.print(F("HALFTIME")); break;
        case 3:  Serial.print(F("RESET"));    break;
        default: Serial.print(F("?("));       Serial.print(r); Serial.print(')'); break;
    }
}

void print_age(bool have, uint32_t last_ms) {
    if (!have) { Serial.print(F("nunca")); return; }
    const uint32_t age = millis() - last_ms;
    Serial.print(age); Serial.print(F("ms"));
    if (age > 500) Serial.print(F(" [STALE]"));
}

// ---- Secciones del panel ---------------------------------------------------
void print_down_section() {
    Serial.println(F("+--- DOWN -> CENTRAL (Serial1) ---------------------------------+"));
    Serial.print(F(" enlace: ")); Serial.print(g_down.dec.frames_decoded()); Serial.print(F(" frames | "));
    Serial.print(g_down.dec.crc_errors()); Serial.print(F(" crcErr | "));
    Serial.print(g_down.seq_gaps); Serial.print(F(" seqGap | bytes="));
    Serial.println(g_down.dec.bytes_received());

    // --- LINEA (LineStatusV2 0x10) ---
    Serial.print(F(" LINEA  (#")); Serial.print(g_lsv2_n); Serial.print(F(", ")); print_age(g_have_lsv2, g_lsv2_ms); Serial.println(F("):"));
    if (g_have_lsv2) {
        const LineStatusV2& s = g_lsv2;
        Serial.print(F("   schema=")); Serial.print(s.schema_version);
        Serial.print(F(" valid="));    Serial.print(s.data_valid);
        Serial.print(F(" present="));  Serial.print(s.line_present);
        Serial.print(F(" on_line=")); Serial.print(s.sensors_on_line); Serial.print(F("/32"));
        Serial.print(F(" q="));        Serial.print(s.quality);
        Serial.print(F(" age="));      Serial.print(s.sample_age_ms); Serial.println(F("ms"));
        Serial.print(F("   angle="));  p_i16_or_na(s.line_angle_centideg,   LSV2_NA_I16, 100.0f, "deg");
        Serial.print(F(" escape="));   p_i16_or_na(s.escape_angle_centideg, LSV2_NA_I16, 100.0f, "deg");
        Serial.print(F(" pen="));      if (s.penetration_mm == LSV2_NA_U16) Serial.print(F("N/A")); else { Serial.print(s.penetration_mm); Serial.print(F("mm")); }
        Serial.print(F(" cross="));    p_i16_or_na(s.cross_track_mm, LSV2_NA_I16, 1.0f, "mm");
        Serial.println();
        Serial.print(F("   eventos: ")); print_event_flags(s.event_flags); Serial.println();
    } else {
        Serial.println(F("   (sin datos)"));
    }

    // --- OTOS POSE (Pose2D 0x11) ---
    Serial.print(F(" OTOS pose (#")); Serial.print(g_pose_n); Serial.print(F(", ")); print_age(g_have_pose, g_pose_ms); Serial.println(F("):"));
    if (g_have_pose) {
        Serial.print(F("   x="));   Serial.print(g_pose.x_mm); Serial.print(F("mm y=")); Serial.print(g_pose.y_mm);
        Serial.print(F("mm hdg=")); Serial.print(g_pose.heading_centideg / 100.0f, 1);
        Serial.print(F("deg conf=")); Serial.println(g_pose.confidence);
    } else Serial.println(F("   (sin datos)"));

    // --- OTOS VEL (Velocity2D 0x12) ---
    Serial.print(F(" OTOS vel  (#")); Serial.print(g_vel_n); Serial.print(F(", ")); print_age(g_have_vel, g_vel_ms); Serial.println(F("):"));
    if (g_have_vel) {
        Serial.print(F("   vx=")); Serial.print(g_vel.vx_mm_s); Serial.print(F("mm/s vy=")); Serial.print(g_vel.vy_mm_s);
        Serial.print(F("mm/s omega=")); Serial.print(g_vel.omega_centideg_s / 100.0f, 1);
        Serial.print(F("deg/s slip=")); Serial.println(g_vel.slip_estimate);
    } else Serial.println(F("   (sin datos)"));
    if (g_down_other) { Serial.print(F(" (otros frames DOWN no-0x10/11/12: ")); Serial.print(g_down_other); Serial.println(')'); }
}

void print_top_section() {
    Serial.println(F("+--- TOP -> CENTRAL (Serial7) ----------------------------------+"));
    Serial.print(F(" enlace: ")); Serial.print(g_top.dec.frames_decoded()); Serial.print(F(" frames | "));
    Serial.print(g_top.dec.crc_errors()); Serial.print(F(" crcErr | "));
    Serial.print(g_top.seq_gaps); Serial.print(F(" seqGap | bytes="));
    Serial.println(g_top.dec.bytes_received());

    Serial.print(F(" SNAPSHOT (#")); Serial.print(g_snap_n); Serial.print(F(", ")); print_age(g_have_snap, g_snap_ms); Serial.println(F("):"));
    if (g_have_snap) {
        const WorldSnapshot& s = g_snap;
        Serial.print(F("   pose: x=")); Serial.print(s.my_x_mm); Serial.print(F("mm y=")); Serial.print(s.my_y_mm);
        Serial.print(F("mm hdg=")); Serial.print(s.my_heading_centideg / 100.0f, 1); Serial.print(F("deg conf=")); Serial.println(s.my_pose_confidence);
        Serial.print(F("   ball: vis=")); Serial.print(s.ball_visible);
        Serial.print(F(" (")); Serial.print(s.ball_x_mm); Serial.print(','); Serial.print(s.ball_y_mm); Serial.print(F(")mm"));
        Serial.print(F(" v=(")); Serial.print(s.ball_vx_mm_s); Serial.print(','); Serial.print(s.ball_vy_mm_s); Serial.print(F(")mm/s conf="));
        Serial.println(s.ball_confidence);
        Serial.print(F("   goal_opp: vis=")); Serial.print(s.goal_opp_visible);
        Serial.print(F(" ang=")); Serial.print(s.goal_opp_angle_centideg / 100.0f, 1);
        Serial.print(F("deg dist=")); Serial.print(s.goal_opp_distance_mm); Serial.print(F("mm | goal_own_vis="));
        Serial.println(s.goal_own_visible);
        Serial.print(F("   min_obst=")); Serial.print(s.min_obstacle_mm); Serial.print(F("mm"));
        Serial.print(F(" | referee=")); print_ref_cmd(s.referee_cmd);
        Serial.print(F(" | flags=0x")); Serial.print(s.flags, HEX); Serial.print(F(" [ "));
        if (s.flags & 0x01) Serial.print(F("in_penalty "));
        if (s.flags & 0x02) Serial.print(F("partner_alive "));
        if (s.flags & 0x04) Serial.print(F("partner_ball "));
        if (s.flags & 0x08) Serial.print(F("MATCH_RUNNING "));
        Serial.println(']');
    } else Serial.println(F("   (sin datos)"));
    if (g_top_other) { Serial.print(F(" (otros frames TOP no-0x60: ")); Serial.print(g_top_other); Serial.println(')'); }
}

// ---- Veredicto de completitud ----------------------------------------------
// Responde de frente: "¿la CENTRAL recibe TODA la info de TOP y DOWN?"

bool fresh(bool have, uint32_t last_ms) { return have && (millis() - last_ms) < FRESH_MS; }

// Un enlace esta "limpio" si decodifico frames, sin errores de CRC y sin huecos
// de SEQ (= sin frames perdidos / corruptos).
bool link_clean(const LinkStats& lk) {
    return lk.dec.frames_decoded() > 0 && lk.dec.crc_errors() == 0 && lk.seq_gaps == 0;
}

// Una linea del veredicto: [OK]/[STALE]/[FALTA] + label + contador + Hz + edad.
// label debe venir pre-paddeado a 9 chars para que las columnas queden alineadas.
void print_stream(const char* label, uint32_t n, bool have, uint32_t last_ms, float hz) {
    if      (fresh(have, last_ms)) Serial.print(F("   [OK]    "));
    else if (have)                 Serial.print(F("   [STALE] "));
    else                           Serial.print(F("   [FALTA] "));
    Serial.print(label);
    Serial.print(F("  #")); Serial.print(n);
    Serial.print(F("  "));
    if (have) { Serial.print(hz, 1); Serial.print(F(" Hz")); } else Serial.print(F("--    "));
    Serial.print(F("  ")); print_age(have, last_ms);
    Serial.println();
}

void print_verdict_section() {
    const bool down_link_ok = link_clean(g_down);
    const bool top_link_ok  = link_clean(g_top);

    Serial.println(F("+--- VEREDICTO DE COMPLETITUD ----------------------------------+"));

    Serial.print(F(" DOWN (Serial1):  enlace ")); Serial.print(down_link_ok ? F("[OK]") : F("[REVISAR]"));
    Serial.print(F("  (crc=")); Serial.print(g_down.dec.crc_errors());
    Serial.print(F(" seqGap=")); Serial.print(g_down.seq_gaps); Serial.println(F(")"));
    print_stream("LINEA    ", g_lsv2_n, g_have_lsv2, g_lsv2_ms, g_lsv2_hz);
    print_stream("OTOS pose", g_pose_n, g_have_pose, g_pose_ms, g_pose_hz);
    print_stream("OTOS vel ", g_vel_n,  g_have_vel,  g_vel_ms,  g_vel_hz);

    Serial.print(F(" TOP  (Serial7):  enlace ")); Serial.print(top_link_ok ? F("[OK]") : F("[REVISAR]"));
    Serial.print(F("  (crc=")); Serial.print(g_top.dec.crc_errors());
    Serial.print(F(" seqGap=")); Serial.print(g_top.seq_gaps); Serial.println(F(")"));
    print_stream("SNAPSHOT ", g_snap_n, g_have_snap, g_snap_ms, g_snap_hz);

    const bool down_complete = fresh(g_have_lsv2, g_lsv2_ms) && fresh(g_have_pose, g_pose_ms)
                             && fresh(g_have_vel, g_vel_ms) && down_link_ok;
    const bool top_complete  = fresh(g_have_snap, g_snap_ms) && top_link_ok;
    const bool all_complete  = down_complete && top_complete;

    Serial.println(F(" ---------------------------------------------------------------"));
    Serial.print(F(" >>> CENTRAL recibe TODO de DOWN+TOP:  "));
    Serial.print(all_complete ? F("SI") : F("NO"));
    if (!all_complete) {
        Serial.print(F("   (falta:"));
        if (!fresh(g_have_lsv2, g_lsv2_ms)) Serial.print(F(" LINEA"));
        if (!fresh(g_have_pose, g_pose_ms)) Serial.print(F(" OTOS-pose"));
        if (!fresh(g_have_vel,  g_vel_ms))  Serial.print(F(" OTOS-vel"));
        if (!fresh(g_have_snap, g_snap_ms)) Serial.print(F(" SNAPSHOT"));
        if (!down_link_ok)                  Serial.print(F(" DOWN-link"));
        if (!top_link_ok)                   Serial.print(F(" TOP-link"));
        Serial.print(F(" )"));
    }
    Serial.println(F(" <<<"));
    // Pista: si LINEA llega pero OTOS-pose/vel no, casi siempre es DOWN que no esta
    // difundiendo el OTOS (env sin broadcast, o OTOS sin alimentacion de bateria),
    // NO un problema de la CENTRAL.
    if (fresh(g_have_lsv2, g_lsv2_ms) && !(fresh(g_have_pose, g_pose_ms) && fresh(g_have_vel, g_vel_ms))) {
        Serial.println(F("     (LINEA si / OTOS no -> revisar broadcast+bateria en DOWN, no la CENTRAL)"));
    }
    Serial.println(F("+---------------------------------------------------------------+"));
}

void print_panel() {
    // Recalcular tasas (Hz) sobre la ventana desde el ultimo panel.
    const uint32_t now = millis();
    const uint32_t dt  = (g_last_panel_ms == 0) ? 0 : (now - g_last_panel_ms);
    if (dt > 0) {
        g_lsv2_hz = (g_lsv2_n - g_lsv2_prev) * 1000.0f / dt;
        g_pose_hz = (g_pose_n - g_pose_prev) * 1000.0f / dt;
        g_vel_hz  = (g_vel_n  - g_vel_prev ) * 1000.0f / dt;
        g_snap_hz = (g_snap_n - g_snap_prev) * 1000.0f / dt;
    }
    g_lsv2_prev = g_lsv2_n; g_pose_prev = g_pose_n; g_vel_prev = g_vel_n; g_snap_prev = g_snap_n;
    g_last_panel_ms = now;

    Serial.println();
    Serial.println(F("================ CENTRAL RX (DOWN + TOP) ================"));
    print_down_section();
    print_top_section();
    print_verdict_section();
    Serial.println(F("========================================================"));
}

}  // namespace

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    Serial.begin(115200);
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* spin */ }

    DOWN_UART.begin(LINK_BAUD);   // Serial1 (pin 0) <- DOWN
    TOP_UART.begin(LINK_BAUD);    // Serial7 (pin 28) <- TOP

    Serial.println();
    Serial.println(F("=========================================================="));
    Serial.println(F("  diag_central_rx_all  -  CENTRAL escucha DOWN + TOP"));
    Serial.println(F("=========================================================="));
    Serial.println(F(" DOWN -> Serial1 (RX1=pin 0)  | TOP -> Serial7 (RX7=pin 28) | 230400"));
    Serial.println(F(" Cablear: DOWN TX1(pin1)->CEN pin0 ; TOP TX4(pin17)->CEN pin28 ; GND comun."));
    Serial.println(F(" Panel cada 500 ms (campos decodificados de cada protocolo):"));
}

void loop() {
    drain(DOWN_UART, g_down, on_down_frame);
    drain(TOP_UART,  g_top,  on_top_frame);

    // LED fijo SOLO si la CENTRAL recibe TODO completo: los 4 mensajes frescos
    // (<500 ms) Y ambos enlaces limpios (sin CRC err / sin seqGap). Si falta
    // cualquiera, parpadea. Asi el LED es un indicador de completitud de un vistazo.
    const bool all_fresh = fresh(g_have_lsv2, g_lsv2_ms) && fresh(g_have_pose, g_pose_ms)
                         && fresh(g_have_vel, g_vel_ms) && fresh(g_have_snap, g_snap_ms)
                         && link_clean(g_down) && link_clean(g_top);
    if (all_fresh) digitalWrite(LED_BUILTIN, HIGH);
    else           digitalWrite(LED_BUILTIN, (millis() / 250) % 2);

    if (g_since_panel >= PANEL_INTERVAL_MS) {
        g_since_panel = 0;
        print_panel();
    }
}
