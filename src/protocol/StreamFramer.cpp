#include "StreamFramer.h"

StreamFramer::StreamFramer() 
    : state(STATE_WAIT_START1), expectedLen(0), currentLen(0), callback(nullptr) {
}

void StreamFramer::setCallback(FrameCallback cb) {
    callback = cb;
}

void StreamFramer::reset() {
    state = STATE_WAIT_START1;
    expectedLen = 0;
    currentLen = 0;
}

void StreamFramer::feedByte(uint8_t byte) {
    switch (state) {
        case STATE_WAIT_START1:
            if (byte == STREAM_START1) {
                state = STATE_WAIT_START2;
            }
            break;

        case STATE_WAIT_START2:
            if (byte == STREAM_START2) {
                state = STATE_LEN_MSB;
            } else if (byte == STREAM_START1) {
                // Stay in STATE_WAIT_START2 if duplicate 0x94
                state = STATE_WAIT_START2;
            } else {
                state = STATE_WAIT_START1;
            }
            break;

        case STATE_LEN_MSB:
            expectedLen = ((size_t)byte) << 8;
            state = STATE_LEN_LSB;
            break;

        case STATE_LEN_LSB:
            expectedLen |= byte;
            if (expectedLen > STREAM_MAX_PAYLOAD) {
                // Invalid length, frame corrupted - reset
                reset();
            } else if (expectedLen == 0) {
                // Empty frame
                if (callback) {
                    callback(rxBuffer, 0);
                }
                reset();
            } else {
                currentLen = 0;
                state = STATE_PAYLOAD;
            }
            break;

        case STATE_PAYLOAD:
            rxBuffer[currentLen++] = byte;
            if (currentLen >= expectedLen) {
                if (callback) {
                    callback(rxBuffer, expectedLen);
                }
                reset();
            }
            break;
    }
}

void StreamFramer::feedBytes(const uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        feedByte(buf[i]);
    }
}

size_t StreamFramer::wrapFrame(uint8_t *destBuf, size_t maxDestLen, const uint8_t *payload, size_t payloadLen) {
    if (maxDestLen < payloadLen + STREAM_HEADER_LEN) {
        return 0;
    }
    destBuf[0] = STREAM_START1;
    destBuf[1] = STREAM_START2;
    destBuf[2] = (uint8_t)((payloadLen >> 8) & 0xFF);
    destBuf[3] = (uint8_t)(payloadLen & 0xFF);
    if (payload && payloadLen > 0) {
        memcpy(destBuf + STREAM_HEADER_LEN, payload, payloadLen);
    }
    return payloadLen + STREAM_HEADER_LEN;
}

bool StreamFramer::sendFrame(Stream &stream, const uint8_t *payload, size_t payloadLen) {
    uint8_t header[STREAM_HEADER_LEN];
    header[0] = STREAM_START1;
    header[1] = STREAM_START2;
    header[2] = (uint8_t)((payloadLen >> 8) & 0xFF);
    header[3] = (uint8_t)(payloadLen & 0xFF);

    if (stream.write(header, STREAM_HEADER_LEN) != STREAM_HEADER_LEN) {
        return false;
    }
    if (payload && payloadLen > 0) {
        if (stream.write(payload, payloadLen) != payloadLen) {
            return false;
        }
    }
    return true;
}
