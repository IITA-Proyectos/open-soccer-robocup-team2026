#include "proto.h"
#include "crc16.h"

namespace iitasoccer {

size_t proto_encode(const Frame& frame, uint8_t* out, size_t out_size) {
    if (frame.payload_len > PROTO_MAX_PAYLOAD) return 0;
    const size_t total = PROTO_FRAME_OVERHEAD + frame.payload_len;
    if (out_size < total) return 0;

    size_t i = 0;
    out[i++] = PROTO_START;
    out[i++] = frame.payload_len;
    out[i++] = static_cast<uint8_t>(frame.type);
    out[i++] = frame.seq;
    for (size_t j = 0; j < frame.payload_len; ++j) {
        out[i++] = frame.payload[j];
    }

    // CRC sobre LEN + TYPE + SEQ + PAYLOAD (bytes out[1..i-1]).
    const uint16_t crc = crc16_ccitt(&out[1], 3 + frame.payload_len);
    out[i++] = static_cast<uint8_t>(crc >> 8);    // high byte
    out[i++] = static_cast<uint8_t>(crc & 0xFF);  // low byte
    out[i++] = PROTO_END;

    return i;
}

FrameDecoder::FrameDecoder()
    : state_(State::WAIT_START),
      payload_idx_(0),
      crc_expected_(0),
      bytes_received_(0),
      frames_decoded_(0),
      crc_errors_(0),
      resync_events_(0) {
    frame_.payload_len = 0;
}

void FrameDecoder::reset() {
    state_ = State::WAIT_START;
    payload_idx_ = 0;
}

bool FrameDecoder::feed(uint8_t byte) {
    bytes_received_++;
    switch (state_) {
        case State::WAIT_START:
            if (byte == PROTO_START) {
                state_ = State::READ_LEN;
            }
            return false;

        case State::READ_LEN:
            if (byte > PROTO_MAX_PAYLOAD) {
                resync_events_++;
                state_ = State::WAIT_START;
                return false;
            }
            frame_.payload_len = byte;
            state_ = State::READ_TYPE;
            return false;

        case State::READ_TYPE:
            frame_.type = static_cast<MsgType>(byte);
            state_ = State::READ_SEQ;
            return false;

        case State::READ_SEQ:
            frame_.seq = byte;
            payload_idx_ = 0;
            state_ = (frame_.payload_len == 0)
                ? State::READ_CRC_HIGH
                : State::READ_PAYLOAD;
            return false;

        case State::READ_PAYLOAD:
            frame_.payload[payload_idx_++] = byte;
            if (payload_idx_ >= frame_.payload_len) {
                state_ = State::READ_CRC_HIGH;
            }
            return false;

        case State::READ_CRC_HIGH:
            crc_expected_ = static_cast<uint16_t>(byte) << 8;
            state_ = State::READ_CRC_LOW;
            return false;

        case State::READ_CRC_LOW:
            crc_expected_ |= byte;
            state_ = State::READ_END;
            return false;

        case State::READ_END: {
            state_ = State::WAIT_START;
            if (byte != PROTO_END) {
                resync_events_++;
                return false;
            }
            // Verificar CRC sobre LEN + TYPE + SEQ + PAYLOAD.
            uint8_t buf[PROTO_MAX_PAYLOAD + 3];
            buf[0] = frame_.payload_len;
            buf[1] = static_cast<uint8_t>(frame_.type);
            buf[2] = frame_.seq;
            for (size_t i = 0; i < frame_.payload_len; ++i) {
                buf[3 + i] = frame_.payload[i];
            }
            const uint16_t crc_calc = crc16_ccitt(buf, 3 + frame_.payload_len);
            if (crc_calc != crc_expected_) {
                crc_errors_++;
                return false;
            }
            frames_decoded_++;
            return true;
        }
    }
    return false;
}

const Frame& FrameDecoder::get_frame() const {
    return frame_;
}

}  // namespace iitasoccer
