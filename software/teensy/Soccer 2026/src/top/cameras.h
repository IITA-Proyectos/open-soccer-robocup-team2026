// cameras.h — Parser robusto del protocolo de las cámaras OpenMV N6 → placa TOP.
//
// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ CONTRATO v2 (contract-schema: 2) — 11 bytes por packet. TOCA EL WIRE.      ║
// ║ Re-flashear AMBAS cámaras (cam-frontal-n6.py / cam-trasera-n6.py) + TOP    ║
// ║ en el MISMO deploy + validar en banco. NO es regresión-segura por sí solo. ║
// ╚══════════════════════════════════════════════════════════════════════════╝
//
// Historia:
//   • v1 (contract-schema 1): 9 bytes [201,Xp,Ypc,202,Xam,Yamc,203,Xaz,Yazc].
//     - X SIN offset (0..200) e Y CON offset (+100). ASIMÉTRICO: X<0 (pelota a la
//       IZQUIERDA) se clampeaba a 0 y el TOP la leía "al frente". La mitad
//       izquierda del FOV se perdía (research/2026-06-03-eje-x-codificacion-
//       asimetrica-vision.md, opción A).
//     - Sentinel "no detectado" = (X=0, Y_coded=0) → Y=-100. Frágil: colisiona con
//       un objeto real en (X=0, Y=-100); y X=0 es además una columna válida.
//     - Sin CRC ni byte de fin: un bit-flip o un dato == header desincronizaba
//       (bug R6).
//
//   • v2 (este archivo): resuelve las 3 cosas de una sola vez (revisión coherente):
//       (a) EJE-X SIMÉTRICO: X se codifica IGUAL que Y → X_coded = X + 100
//           (X ∈ [-100,100] → 0..200). El parser hace ball_x = byte - 100, igual
//           que con Y. La pelota a la izquierda (X<0) ya es representable.
//       (b) SENTINEL INEQUÍVOCO: "no detectado" = (X_coded=255, Y_coded=255).
//           Un objeto real codifica ambos en [0,200] (clamp), así que 255 es
//           INALCANZABLE desde una detección → cero colisión. (Antes el sentinel
//           podía chocar con un objeto real en el borde del FOV.)
//       (c) INTEGRIDAD: se agregan 2 bytes al final → CRC8 (XOR de los 9 bytes de
//           datos) + END (254). El parser valida los 3 headers en posición fija,
//           el END en posición fija y el CRC; si algo falla DESCARTA el frame y
//           cuenta crc_errors / resync_events. Detecta bit-flips y datos==header.
//
// Layout del protocolo v2 (11 bytes por packet):
//
//   byte  0: 201          (HEADER1, sync pelota)
//   byte  1: Xp_coded     (X pelota + 100, uint8, datos ∈ [0,200] | 255 = sentinel)
//   byte  2: Yp_coded     (Y pelota + 100, uint8, datos ∈ [0,200] | 255 = sentinel)
//   byte  3: 202          (HEADER2, sync arco amarillo)
//   byte  4: Xam_coded    (X arco amarillo + 100)
//   byte  5: Yam_coded    (Y arco amarillo + 100)
//   byte  6: 203          (HEADER3, sync arco azul)
//   byte  7: Xaz_coded    (X arco azul + 100)
//   byte  8: Yaz_coded    (Y arco azul + 100)
//   byte  9: CRC8         (XOR de los bytes 0..8 inclusive)
//   byte 10: 254          (END, fin de trama)
//
// El receptor decodifica X = X_coded - 100 e Y = Y_coded - 100 para recuperar el
// signo. "No visible" para un objeto ⇔ sus dos bytes coded valían 255 (sentinel).
//
// Rangos / por qué no hay colisión de bytes:
//   - coords coded ∈ [0,200]   (clamp en la cámara)
//   - headers       = 201/202/203
//   - END           = 254
//   - SENTINEL byte = 255
//   ⇒ ningún dato de coordenada real puede valer 201/202/203/254/255. El CRC es
//     el único byte que puede tomar cualquier valor [0,255]; por eso se valida
//     ANTES de aceptar el packet, no por posición de sync.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// === Constantes del contrato v2 (públicas: las usan parser, tests y doc) ===
constexpr uint8_t CAM_HEADER1     = 201;   // sync pelota
constexpr uint8_t CAM_HEADER2     = 202;   // sync arco amarillo
constexpr uint8_t CAM_HEADER3     = 203;   // sync arco azul
constexpr uint8_t CAM_END_BYTE    = 254;   // fin de trama (0xFE)
constexpr uint8_t CAM_SENTINEL    = 255;   // byte coded == 255 ⇒ objeto no detectado
constexpr int16_t CAM_COORD_OFFSET = 100;  // coded = valor + 100 (X e Y por igual)
constexpr int     CAM_PACKET_LEN  = 11;    // bytes por packet v2

// CRC del contrato: XOR de los 9 bytes de datos (headers + coords, bytes 0..8).
// Simple y determinista; detecta cualquier bit-flip de 1 bit y la mayoría de los
// múltiples. No es CRC16 polinómico a propósito: 1 byte mantiene el packet corto
// y el parser trivial de auditar. Si en el futuro se quiere CRC16, sube schema.
inline uint8_t cam_crc8(const uint8_t* data9) {
    uint8_t c = 0;
    for (int i = 0; i < 9; ++i) c ^= data9[i];
    return c;
}

struct CameraPacket {
    // Coordenadas decodificadas (offset de 100 ya restado en AMBOS ejes).
    int16_t ball_x;
    int16_t ball_y;
    int16_t goal_yellow_x;
    int16_t goal_yellow_y;
    int16_t goal_blue_x;
    int16_t goal_blue_y;

    // Flags de visibilidad. Un objeto es "no visible" ⇔ sus dos bytes coded eran
    // 255 (sentinel). Ya NO se infiere de (x==0 && y==-100): eso era frágil en v1.
    bool ball_visible;
    bool goal_yellow_visible;
    bool goal_blue_visible;

    uint32_t timestamp_ms;  // populado por el caller (no por el parser)
};

class CameraParser {
public:
    CameraParser();

    // Alimenta un byte recibido por UART. Retorna true SOLO cuando completa un
    // packet válido: los 3 headers, el byte END y el CRC chequearon OK. Un frame
    // con header/END/CRC malo NO actualiza get_packet() y cuenta como error.
    bool feed(uint8_t byte);

    // Último packet decodificado VÁLIDO. Solo cambia cuando feed() retornó true.
    const CameraPacket& get_packet() const;

    // Estadísticas:
    uint32_t packets_decoded() const { return packets_decoded_; }
    uint32_t resync_events()   const { return resync_events_; }
    uint32_t crc_errors()      const { return crc_errors_; }

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
        READ_CRC,
        WAIT_END,
    };

    // Decodifica un coord byte → valor con signo; setea visible=false si == sentinel.
    static int16_t decode_coord(uint8_t coded);

    // Resync: registra el evento y reposiciona el FSM (re-lock si el byte es HEADER1).
    void on_resync(uint8_t byte);

    State    state_;
    CameraPacket packet_;        // último packet VÁLIDO publicado
    uint8_t  buf_[CAM_PACKET_LEN]; // bytes 0..8 crudos del frame en curso (para CRC)
    uint8_t  rx_crc_;            // CRC recibido (byte 9)
    bool     ball_vis_, yellow_vis_, blue_vis_;  // visibilidad del frame en curso
    int16_t  ball_x_, ball_y_, yellow_x_, yellow_y_, blue_x_, blue_y_;  // decod. en curso
    uint32_t packets_decoded_;
    uint32_t resync_events_;
    uint32_t crc_errors_;
};

}  // namespace iitasoccer
