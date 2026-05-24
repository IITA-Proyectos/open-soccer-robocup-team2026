#include "cameras.h"

namespace iitasoccer {

namespace {
constexpr uint8_t HEADER1 = 201;
constexpr uint8_t HEADER2 = 202;
constexpr uint8_t HEADER3 = 203;
constexpr int16_t Y_OFFSET = 100;  // protocolo viejo codifica Y como (Y + 100)

// Heurística de visibilidad del protocolo viejo:
// OpenMV firmware viejo envía (X=0, Y_coded=0) cuando no detecta el objeto.
// Después del offset, Y_coded=0 → Y=-100. Así que "no visible" = X==0 AND Y==-100.
inline bool is_visible(int16_t x, int16_t y) {
    return !(x == 0 && y == -Y_OFFSET);
}
}  // namespace

CameraParser::CameraParser()
    : state_(State::WAIT_HEADER1),
      packets_decoded_(0),
      resync_events_(0) {
    packet_ = CameraPacket{};
}

void CameraParser::reset() {
    state_ = State::WAIT_HEADER1;
}

bool CameraParser::feed(uint8_t byte) {
    switch (state_) {
        case State::WAIT_HEADER1:
            if (byte == HEADER1) {
                state_ = State::READ_BALL_X;
            }
            // Cualquier otro byte se descarta silenciosamente — resync natural.
            return false;

        case State::READ_BALL_X:
            packet_.ball_x = static_cast<int16_t>(byte);
            state_ = State::READ_BALL_Y;
            return false;

        case State::READ_BALL_Y:
            packet_.ball_y = static_cast<int16_t>(byte) - Y_OFFSET;
            state_ = State::WAIT_HEADER2;
            return false;

        case State::WAIT_HEADER2:
            if (byte != HEADER2) {
                resync_events_++;
                // Si por casualidad este byte es HEADER1, ya estamos sincronizando
                // de nuevo desde el principio.
                state_ = (byte == HEADER1) ? State::READ_BALL_X : State::WAIT_HEADER1;
                return false;
            }
            state_ = State::READ_YELLOW_X;
            return false;

        case State::READ_YELLOW_X:
            packet_.goal_yellow_x = static_cast<int16_t>(byte);
            state_ = State::READ_YELLOW_Y;
            return false;

        case State::READ_YELLOW_Y:
            packet_.goal_yellow_y = static_cast<int16_t>(byte) - Y_OFFSET;
            state_ = State::WAIT_HEADER3;
            return false;

        case State::WAIT_HEADER3:
            if (byte != HEADER3) {
                resync_events_++;
                state_ = (byte == HEADER1) ? State::READ_BALL_X : State::WAIT_HEADER1;
                return false;
            }
            state_ = State::READ_BLUE_X;
            return false;

        case State::READ_BLUE_X:
            packet_.goal_blue_x = static_cast<int16_t>(byte);
            state_ = State::READ_BLUE_Y;
            return false;

        case State::READ_BLUE_Y:
            packet_.goal_blue_y = static_cast<int16_t>(byte) - Y_OFFSET;

            // Aplicar heurística de visibilidad (T2 mitigation)
            packet_.ball_visible        = is_visible(packet_.ball_x, packet_.ball_y);
            packet_.goal_yellow_visible = is_visible(packet_.goal_yellow_x, packet_.goal_yellow_y);
            packet_.goal_blue_visible   = is_visible(packet_.goal_blue_x, packet_.goal_blue_y);

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
