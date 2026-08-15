#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>
#include "Types.h"
#include "config.h"

class NodeStore {
public:
    NodeStore();
    ~NodeStore();

    void upsertNode(const NodeData &node);
    void updateTelemetry(uint32_t num, uint32_t batt, float volt, uint32_t uptime, float chanUtil, float airUtil, float snr, int32_t rssi);
    void updatePosition(uint32_t num, int32_t lat_i, int32_t lon_i, int32_t alt);
    void updateLastHeard(uint32_t num, uint32_t timestamp, float snr, int32_t rssi, uint8_t hops);
    
    bool getNode(uint32_t num, NodeData &outNode);
    bool getNodeByIndex(size_t index, NodeData &outNode);
    int findIndex(uint32_t num);
    size_t getNodeCount();
    void getNodeDisplayName(uint32_t num, char *outBuf, size_t maxLen);
    void clear();

private:
    std::vector<NodeData> nodes;
    SemaphoreHandle_t mutex;
    int findIndex_unlocked(uint32_t num);
};
