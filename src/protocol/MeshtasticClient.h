#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "StreamFramer.h"
#include "model/Types.h"
#include "model/NodeStore.h"
#include "model/MessageStore.h"
#include "meshtastic/mesh.pb.h"
#include "meshtastic/portnums.pb.h"
#include "meshtastic/telemetry.pb.h"

class MeshtasticClient {
public:
    MeshtasticClient(NodeStore &nodeStore, MessageStore &msgStore);

    void begin(HardwareSerial *serialPort);
    void update(); // call periodically from FreeRTOS task
    typedef std::function<void(const ChatMessage &msg)> MessageReceivedCallback;
    void setMessageReceivedCallback(MessageReceivedCallback cb);

    bool sendTextMessage(const char *text, uint32_t toNode = NODENUM_BROADCAST, uint8_t channel = 0, bool wantAck = false);
    void requestConfigSync();
    void requestTelemetry(uint32_t toNode = 0);
    
    LocalRadioInfo getRadioInfo();
    ChannelData getChannel(uint8_t index);
    bool isSynced() const;

    // Structured logs for [F5] viewer
    size_t getLogCount() const;
    bool getLogEntry(size_t index, LogEntry &entry) const;

private:
    NodeStore &nodeStore;
    MessageStore &msgStore;
    HardwareSerial *serial;
    StreamFramer framer;

    LocalRadioInfo localInfo;
    ChannelData channels[MAX_CHANNELS];
    uint32_t activeSyncNonce;
    uint32_t lastHeartbeatTime;
    uint32_t lastSyncAttemptTime;
    uint32_t lastTelemetryPollTime;

    static constexpr size_t MAX_LOG_ENTRIES = 400;
    LogEntry logEntries[MAX_LOG_ENTRIES];
    size_t logEntryCount;
    size_t logEntryHead;
    MessageReceivedCallback messageCallback;

    void onFrameReceived(const uint8_t *payload, size_t len);
    void handleFromRadio(const meshtastic_FromRadio &fromRadio);
    void handleMeshPacket(const meshtastic_MeshPacket &mp);
    void handleTelemetry(const meshtastic_MeshPacket &mp, const meshtastic_Telemetry &telemetry);
    void appendLog(const char *tag, const char *msg);
    void sendHeartbeat();
    void sendToRadio(const meshtastic_ToRadio &toRadio);
};
