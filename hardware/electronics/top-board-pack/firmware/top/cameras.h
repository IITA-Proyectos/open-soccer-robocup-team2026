// ═══════════════════════════════════════════════════════════════════════════
//  ⚠️  SNAPSHOT v1 SUPERADO — NO ES LA FUENTE NI EL BUILD PATH  ⚠️
//
//  Esta es una copia foto-curada (2026-05-24) del parser de cámaras en su
//  versión v1: 9 bytes/packet, X SIN offset, sentinel (X=0 & Y=-100), SIN CRC.
//  Quedó STALE: el contrato cámara→TOP saltó a v2 el 2026-06-03 (commit d230de5)
//  y esta copia NO se actualizó al v2.
//
//  Contrato VIVO v2: 11 bytes/packet · X e Y simétricos (coded = valor + 100) ·
//  sentinel = 255 · CRC8 (XOR de los 9 bytes de datos) + END = 254.
//
//  FUENTE CANÓNICA (lo que compila/flashea PlatformIO env top_robot1/2):
//      software/teensy/Soccer 2026/src/top/cameras.{cpp,h}
//  CONTRATO CANÓNICO:
//      docs/firmware/CONTRATO-DATOS-CAMARAS.md
//
//  NO compiles ni flashees desde este archivo. Si contradice el repo vivo,
//  gana el repo vivo (regla de oro del pack — ver README.md del pack).
// ═══════════════════════════════════════════════════════════════════════════

// cameras.h — Parser robusto del protocolo viejo de OpenMV (9 bytes con headers 201/202/203)
//
// Decisión del coach (Q6, 2026-05-10): mantener protocolo viejo del firmware OpenMV
// inicialmente. La robustez se implementa en el receptor (este parser en el TOP)
// porque tocar el OpenMV está fuera de scope para Hito 1.
//
// Lecciones aplicadas de docs/internal/analisis-definitivo-delantero.md:
//   • BUG 1   — sin sincronización robusta. Solución: state machine que descarta bytes
//               basura hasta encontrar HEADER1 (201). Si se desincroniza al medio del
//               packet (HEADER2 o HEADER3 no esperados), reinicia búsqueda.
//   • BUG R6  — valores que coinciden con headers. Mitigación: validamos los 3 headers
//               (201/202/203) en posiciones fijas; si fallan, descartamos packet.
//   • T2      — Xp == 0 interpretado como "no pelota". Mitigación parcial: marcamos
//               un objeto como "no visible" SÓLO cuando AMBOS X e Y son 0 (sentinel
//               del firmware OpenMV viejo). La pelota en el centro exacto pierde un
//               frame pero no se interpreta como ausente.
//
// Layout del protocolo viejo (9 bytes por packet):
//
//   byte 0: 201          (HEADER1, sync)
//   byte 1: Xp           (coordenada X de la pelota, 0..200)
//   byte 2: Yp_coded     (coordenada Y de la pelota codificada como Y + 100)
//   byte 3: 202          (HEADER2, sync)
//   byte 4: Xam          (coordenada X del arco amarillo)
//   byte 5: Yam_coded    (Y + 100)
//   byte 6: 203          (HEADER3, sync)
//   byte 7: Xaz          (coordenada X del arco azul)
//   byte 8: Yaz_coded    (Y + 100)
//
// El receptor decodifica Y = Y_coded - 100 para recuperar el signo.

#pragma once
#include <stdint.h>

namespace iitasoccer {

struct CameraPacket {
    // Coordenadas decodificadas (Y ya tiene su offset de 100 restado)
    int16_t ball_x;
    int16_t ball_y;
    int16_t goal_yellow_x;
    int16_t goal_yellow_y;
    int16_t goal_blue_x;
    int16_t goal_blue_y;

    // Flags de visibilidad (heurística: X == 0 AND Y == 0 → no visible)
    bool ball_visible;
    bool goal_yellow_visible;
    bool goal_blue_visible;

    uint32_t timestamp_ms;  // populado por el caller (no por el parser)
};

class CameraParser {
public:
    CameraParser();

    // Alimenta un byte recibido por UART. Retorna true cuando completa
    // un packet válido (los 3 headers se validaron OK).
    bool feed(uint8_t byte);

    // Último packet decodificado. Válido solo después de que feed() retornó true.
    const CameraPacket& get_packet() const;

    // Estadísticas:
    uint32_t packets_decoded() const { return packets_decoded_; }
    uint32_t resync_events() const   { return resync_events_; }

    void reset();

private:
    enum class State : uint8_t {
        WAIT_HEADER1,
        READ_BALL_X,
        READ_BALL_Y,
        WAIT_HEADER2,
        READ_YELLOW_X,
        READ_YELLOW_Y,
        WAIT_HEADER3,
        READ_BLUE_X,
        READ_BLUE_Y,
    };

    State state_;
    CameraPacket packet_;
    uint32_t packets_decoded_;
    uint32_t resync_events_;
};

}  // namespace iitasoccer
