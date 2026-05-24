// proto.h — protocolo UART entre placas (TOP, DOWN, Zircon, COMM)
//
// Frame layout:
//   ┌──────┬─────┬──────┬─────┬──────────────┬──────────┬──────┐
//   │ 0xAA │ LEN │ TYPE │ SEQ │   PAYLOAD    │ CRC16 BE │ 0x55 │
//   │  1B  │  1B │  1B  │  1B │  LEN bytes   │    2B    │  1B  │
//   └──────┴─────┴──────┴─────┴──────────────┴──────────┴──────┘
//
// Características:
//   - START byte 0xAA + END byte 0x55 (distintos para reducir falsos sincs).
//   - CRC-16/CCITT-FALSE sobre LEN+TYPE+SEQ+PAYLOAD.
//   - SEQ wrap 0-255 detecta packets perdidos.
//   - Decoder con state machine, byte por byte, resincroniza automáticamente.
//
// Garantías:
//   - Un byte basura no contamina el siguiente frame.
//   - CRC detecta corrupción.
//   - SEQ detecta pérdida.
//
// Lecciones aplicadas del análisis del firmware viejo:
//   docs/internal/analisis-definitivo-delantero.md BUG 1 y BUG R6.

#pragma once
#include <stdint.h>
#include <stddef.h>

namespace iitasoccer {

// Bytes de sincronización
constexpr uint8_t PROTO_START = 0xAA;
constexpr uint8_t PROTO_END   = 0x55;

// Tipos de mensaje (alocados por subsistema según arquitectura "STAR centric
// con bus de emergencia" — ver docs/ARQUITECTURA-3-PLACAS-2026.md).
enum class MsgType : uint8_t {
    // DOWN → ARRIBA (100 Hz)  — odometría OTOS para fusión sensorial
    DOWN_OTOS_POSE      = 0x11,  // Pose2D
    DOWN_OTOS_VEL       = 0x12,  // Velocity2D (con slip_estimate)

    // DOWN → CENTRAL (100-200 Hz)  — bus de emergencia, línea pura
    LINE_URGENT         = 0x10,  // LineStatus — measurement crudo (sin output PID)

    // CENTRAL → DOWN (eventos)
    CENTRAL_RESET_OTOS  = 0x20,  // uint8 flag
    CENTRAL_CALIB_LINE  = 0x21,  // uint8: 0=carpet, 1=white

    // ARRIBA → CENTRAL (100 Hz)  — world snapshot completo
    WORLD_SNAPSHOT      = 0x60,  // WorldSnapshot (pose, ball, goals, refcmd, partner)

    // CENTRAL → ARRIBA (eventos)
    CENTRAL_RESET_TOP   = 0x61,  // reset world model / recalibrar cámaras
    CENTRAL_TOP_CMD     = 0x62,  // comandos administrativos genéricos

    // ARRIBA ↔ COMM (placa árbitros + inter-robot ESP-NOW)
    COMM_REFEREE_CMD    = 0x30,  // uint8: 0=stop, 1=start, 2=halftime, 3=reset
    COMM_STATUS_REQ     = 0x31,  // empty (COMM pide status)
    TOP_STATUS_REPLY    = 0x32,  // estado + batería + errores
    COMM_PARTNER_DATA   = 0x40,  // snapshot recibido del partner via ESP-NOW
    TOP_PARTNER_DATA    = 0x41,  // mi snapshot a enviar al partner

    // (LEGACY — no usar en firmware nuevo) ARRIBA → Zircon como motor server
    MOTOR_COMMAND       = 0x50,
    MOTOR_STATUS_REQ    = 0x51,
    ZIRCON_STATUS       = 0x52,

    // Reservado para tests / debug
    DEBUG_ECHO          = 0xFE,
    DEBUG_PING          = 0xFF,
};

// Alias retrocompatibles para no romper código existente — irán deprecando.
constexpr MsgType DOWN_LINE_STATUS = MsgType::LINE_URGENT;
constexpr MsgType TOP_RESET_OTOS   = MsgType::CENTRAL_RESET_OTOS;
constexpr MsgType TOP_CALIB_LINE   = MsgType::CENTRAL_CALIB_LINE;

constexpr size_t PROTO_MAX_PAYLOAD     = 32;
constexpr size_t PROTO_FRAME_OVERHEAD  = 7;   // START + LEN + TYPE + SEQ + CRC(2) + END
constexpr size_t PROTO_MAX_FRAME       = PROTO_FRAME_OVERHEAD + PROTO_MAX_PAYLOAD;

// Frame decodificado.
struct Frame {
    MsgType type;
    uint8_t seq;
    uint8_t payload[PROTO_MAX_PAYLOAD];
    uint8_t payload_len;
};

// Encoder: serializa frame a buffer. Retorna bytes escritos, 0 si error.
size_t proto_encode(const Frame& frame, uint8_t* out, size_t out_size);

// Decoder con state machine. Alimentar byte por byte; cuando un frame
// válido completa, feed() retorna true y se puede leer con get_frame().
class FrameDecoder {
public:
    FrameDecoder();

    // Procesa un byte recibido. Retorna true si terminó de decodificar
    // un frame válido (CRC OK, sync OK).
    bool feed(uint8_t byte);

    // Frame decodificado más reciente (válido solo después de feed() == true).
    const Frame& get_frame() const;

    // Reinicia el state machine (descarta frame en progreso).
    void reset();

    // Stats para diagnóstico:
    uint32_t bytes_received() const { return bytes_received_; }
    uint32_t frames_decoded() const { return frames_decoded_; }
    uint32_t crc_errors() const { return crc_errors_; }
    uint32_t resync_events() const { return resync_events_; }

private:
    enum class State : uint8_t {
        WAIT_START,
        READ_LEN,
        READ_TYPE,
        READ_SEQ,
        READ_PAYLOAD,
        READ_CRC_HIGH,
        READ_CRC_LOW,
        READ_END,
    };

    State state_;
    Frame frame_;
    uint8_t payload_idx_;
    uint16_t crc_expected_;

    uint32_t bytes_received_;
    uint32_t frames_decoded_;
    uint32_t crc_errors_;
    uint32_t resync_events_;
};

}  // namespace iitasoccer
