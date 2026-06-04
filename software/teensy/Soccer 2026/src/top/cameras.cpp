#include "cameras.h"

namespace iitasoccer {

// Decodifica un byte coded → coordenada con signo (offset -100). Si el byte es el
// SENTINEL (255), retorna 0 y el caller marca el objeto como no visible; el valor
// numérico es irrelevante en ese caso (visible=false lo desactiva aguas abajo).
int16_t CameraParser::decode_coord(uint8_t coded) {
    if (coded == CAM_SENTINEL) return 0;
    return static_cast<int16_t>(coded) - CAM_COORD_OFFSET;
}

CameraParser::CameraParser()
    : state_(State::WAIT_HEADER1),
      rx_crc_(0),
      ball_vis_(false), yellow_vis_(false), blue_vis_(false),
      ball_x_(0), ball_y_(0), yellow_x_(0), yellow_y_(0), blue_x_(0), blue_y_(0),
      packets_decoded_(0),
      resync_events_(0),
      crc_errors_(0) {
    packet_ = CameraPacket{};
    for (int i = 0; i < CAM_PACKET_LEN; ++i) buf_[i] = 0;
}

void CameraParser::reset() {
    state_ = State::WAIT_HEADER1;
}

void CameraParser::on_resync(uint8_t byte) {
    resync_events_++;
    // Si el byte inesperado es HEADER1, ya estamos re-sincronizando desde el inicio
    // de un nuevo frame; arrancamos a leer la pelota. Si no, volvemos a buscar sync.
    if (byte == CAM_HEADER1) {
        buf_[0] = CAM_HEADER1;
        state_ = State::READ_BALL_X;
    } else {
        state_ = State::WAIT_HEADER1;
    }
}

bool CameraParser::feed(uint8_t byte) {
    switch (state_) {
        case State::WAIT_HEADER1:
            if (byte == CAM_HEADER1) {
                buf_[0] = byte;
                state_ = State::READ_BALL_X;
            }
            // Cualquier otro byte se descarta silenciosamente — resync natural,
            // sin contar (basura pre-trama no es un error del enlace).
            return false;

        case State::READ_BALL_X:
            buf_[1] = byte;
            ball_x_ = decode_coord(byte);
            ball_vis_ = (byte != CAM_SENTINEL);
            state_ = State::READ_BALL_Y;
            return false;

        case State::READ_BALL_Y:
            buf_[2] = byte;
            ball_y_ = decode_coord(byte);
            // Visible solo si NINGUNO de los dos bytes fue sentinel.
            ball_vis_ = ball_vis_ && (byte != CAM_SENTINEL);
            state_ = State::WAIT_HEADER2;
            return false;

        case State::WAIT_HEADER2:
            if (byte != CAM_HEADER2) { on_resync(byte); return false; }
            buf_[3] = byte;
            state_ = State::READ_YELLOW_X;
            return false;

        case State::READ_YELLOW_X:
            buf_[4] = byte;
            yellow_x_ = decode_coord(byte);
            yellow_vis_ = (byte != CAM_SENTINEL);
            state_ = State::READ_YELLOW_Y;
            return false;

        case State::READ_YELLOW_Y:
            buf_[5] = byte;
            yellow_y_ = decode_coord(byte);
            yellow_vis_ = yellow_vis_ && (byte != CAM_SENTINEL);
            state_ = State::WAIT_HEADER3;
            return false;

        case State::WAIT_HEADER3:
            if (byte != CAM_HEADER3) { on_resync(byte); return false; }
            buf_[6] = byte;
            state_ = State::READ_BLUE_X;
            return false;

        case State::READ_BLUE_X:
            buf_[7] = byte;
            blue_x_ = decode_coord(byte);
            blue_vis_ = (byte != CAM_SENTINEL);
            state_ = State::READ_BLUE_Y;
            return false;

        case State::READ_BLUE_Y:
            buf_[8] = byte;
            blue_y_ = decode_coord(byte);
            blue_vis_ = blue_vis_ && (byte != CAM_SENTINEL);
            state_ = State::READ_CRC;
            return false;

        case State::READ_CRC:
            rx_crc_ = byte;
            state_ = State::WAIT_END;
            return false;

        case State::WAIT_END:
            if (byte != CAM_END_BYTE) {
                // Fin de trama corrupto → descartamos. Tratamos como resync.
                on_resync(byte);
                return false;
            }
            // END OK → validar CRC sobre los 9 bytes de datos.
            if (cam_crc8(buf_) != rx_crc_) {
                crc_errors_++;
                state_ = State::WAIT_HEADER1;  // frame corrupto: descartar, NO publicar
                return false;
            }
            // Frame íntegro → publicar el packet (atómico: recién acá tocamos packet_).
            packet_.ball_x = ball_x_;
            packet_.ball_y = ball_y_;
            packet_.goal_yellow_x = yellow_x_;
            packet_.goal_yellow_y = yellow_y_;
            packet_.goal_blue_x = blue_x_;
            packet_.goal_blue_y = blue_y_;
            packet_.ball_visible = ball_vis_;
            packet_.goal_yellow_visible = yellow_vis_;
            packet_.goal_blue_visible = blue_vis_;
            packets_decoded_++;
            state_ = State::WAIT_HEADER1;
            return true;
    }
    return false;
}

const CameraPacket& CameraParser::get_packet() const {
    return packet_;
}

}  // namespace iitasoccer
