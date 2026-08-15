#pragma once

#include <Arduino.h>
#include <functional>

#define STREAM_START1 0x94
#define STREAM_START2 0xC3
#define STREAM_HEADER_LEN 4
#define STREAM_MAX_PAYLOAD 512

typedef std::function<void(const uint8_t *payload, size_t len)> FrameCallback;

class StreamFramer {
public:
    StreamFramer();
    
    void setCallback(FrameCallback cb);
    void feedByte(uint8_t byte);
    void feedBytes(const uint8_t *buf, size_t len);
    
    static size_t wrapFrame(uint8_t *destBuf, size_t maxDestLen, const uint8_t *payload, size_t payloadLen);
    static bool sendFrame(Stream &stream, const uint8_t *payload, size_t payloadLen);
    
    void reset();

private:
    enum State {
        STATE_WAIT_START1,
        STATE_WAIT_START2,
        STATE_LEN_MSB,
        STATE_LEN_LSB,
        STATE_PAYLOAD
    };

    State state;
    uint8_t rxBuffer[STREAM_MAX_PAYLOAD];
    size_t expectedLen;
    size_t currentLen;
    FrameCallback callback;
};
