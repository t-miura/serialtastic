#include "NodeStore.h"
#include <cstdio>
#include <cstring>

NodeStore::NodeStore() {
    mutex = xSemaphoreCreateMutex();
}

NodeStore::~NodeStore() {
    if (mutex) {
        vSemaphoreDelete(mutex);
    }
}

int NodeStore::findIndex_unlocked(uint32_t num) {
    for (size_t i = 0; i < nodes.size(); i++) {
        if (nodes[i].num == num) {
            return (int)i;
        }
    }
    return -1;
}

void NodeStore::upsertNode(const NodeData &node) {
    if (!mutex) return;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        int idx = findIndex_unlocked(node.num);
        if (idx >= 0) {
            // Update existing fields if non-empty
            if (node.long_name[0] != '\0') {
                strncpy(nodes[idx].long_name, node.long_name, sizeof(nodes[idx].long_name) - 1);
            }
            if (node.short_name[0] != '\0') {
                strncpy(nodes[idx].short_name, node.short_name, sizeof(nodes[idx].short_name) - 1);
            }
            if (node.id_str[0] != '\0') {
                strncpy(nodes[idx].id_str, node.id_str, sizeof(nodes[idx].id_str) - 1);
            }
            if (node.snr != 0.0f) nodes[idx].snr = node.snr;
            if (node.rssi != 0) nodes[idx].rssi = node.rssi;
            if (node.last_heard != 0) nodes[idx].last_heard = node.last_heard;
            if (node.battery_level != 0) nodes[idx].battery_level = node.battery_level;
            if (node.voltage != 0.0f) nodes[idx].voltage = node.voltage;
            if (node.hardware_model != 0) nodes[idx].hardware_model = node.hardware_model;
            if (node.role != 0) nodes[idx].role = node.role;
            if (node.hops_away != 0) nodes[idx].hops_away = node.hops_away;
            if (node.uptime_seconds != 0) nodes[idx].uptime_seconds = node.uptime_seconds;
            if (node.channel_util != 0.0f) nodes[idx].channel_util = node.channel_util;
            if (node.air_util_tx != 0.0f) nodes[idx].air_util_tx = node.air_util_tx;
            if (node.has_position) {
                nodes[idx].latitude_i = node.latitude_i;
                nodes[idx].longitude_i = node.longitude_i;
                nodes[idx].altitude = node.altitude;
                nodes[idx].has_position = true;
            }
            if (node.is_local) {
                nodes[idx].is_local = true;
                nodes[idx].has_pki_key = true;
            } else if (node.has_pki_key) {
                nodes[idx].has_pki_key = true;
            }
        } else {
            if (nodes.size() < MAX_NODES_IN_STORE) {
                NodeData n = node;
                if (n.is_local) n.has_pki_key = true;
                nodes.push_back(n);
            }
        }
        xSemaphoreGive(mutex);
    }
}

void NodeStore::updateTelemetry(uint32_t num, uint32_t batt, float volt, uint32_t uptime, float chanUtil, float airUtil, float snr, int32_t rssi) {
    if (!mutex) return;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        int idx = findIndex_unlocked(num);
        if (idx >= 0) {
            if (batt > 0) nodes[idx].battery_level = batt;
            if (volt > 0.0f) nodes[idx].voltage = volt;
            if (uptime > 0) nodes[idx].uptime_seconds = uptime;
            if (chanUtil >= 0.0f) nodes[idx].channel_util = chanUtil;
            if (airUtil >= 0.0f) nodes[idx].air_util_tx = airUtil;
            if (snr != 0.0f) nodes[idx].snr = snr;
            if (rssi != 0) nodes[idx].rssi = rssi;
            nodes[idx].last_heard = millis() / 1000;
        }
        xSemaphoreGive(mutex);
    }
}

void NodeStore::updatePosition(uint32_t num, int32_t lat_i, int32_t lon_i, int32_t alt) {
    if (!mutex) return;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        int idx = findIndex_unlocked(num);
        if (idx >= 0) {
            nodes[idx].latitude_i = lat_i;
            nodes[idx].longitude_i = lon_i;
            nodes[idx].altitude = alt;
            nodes[idx].has_position = (lat_i != 0 || lon_i != 0);
        }
        xSemaphoreGive(mutex);
    }
}

void NodeStore::updateLastHeard(uint32_t num, uint32_t timestamp, float snr, int32_t rssi, uint8_t hops) {
    if (!mutex) return;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        int idx = findIndex_unlocked(num);
        if (idx >= 0) {
            nodes[idx].last_heard = timestamp;
            if (snr != 0.0f) nodes[idx].snr = snr;
            if (rssi != 0) nodes[idx].rssi = rssi;
            nodes[idx].hops_away = hops;
        }
        xSemaphoreGive(mutex);
    }
}

bool NodeStore::getNode(uint32_t num, NodeData &outNode) {
    if (!mutex) return false;
    bool found = false;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        int idx = findIndex_unlocked(num);
        if (idx >= 0) {
            outNode = nodes[idx];
            found = true;
        }
        xSemaphoreGive(mutex);
    }
    return found;
}

bool NodeStore::getNodeByIndex(size_t index, NodeData &outNode) {
    if (!mutex) return false;
    bool found = false;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (index < nodes.size()) {
            outNode = nodes[index];
            found = true;
        }
        xSemaphoreGive(mutex);
    }
    return found;
}

int NodeStore::findIndex(uint32_t num) {
    if (!mutex) return -1;
    int idx = -1;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        idx = findIndex_unlocked(num);
        xSemaphoreGive(mutex);
    }
    return idx;
}

size_t NodeStore::getNodeCount() {
    if (!mutex) return 0;
    size_t count = 0;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        count = nodes.size();
        xSemaphoreGive(mutex);
    }
    return count;
}

void NodeStore::getNodeDisplayName(uint32_t num, char *outBuf, size_t maxLen) {
    if (!outBuf || maxLen == 0) return;
    if (num == NODENUM_BROADCAST) {
        snprintf(outBuf, maxLen, "All");
        return;
    }
    NodeData node;
    if (getNode(num, node)) {
        if (node.short_name[0] != '\0') {
            snprintf(outBuf, maxLen, "%s", node.short_name);
        } else if (node.long_name[0] != '\0') {
            snprintf(outBuf, maxLen, "%s", node.long_name);
        } else {
            snprintf(outBuf, maxLen, "!%08x", num);
        }
    } else {
        snprintf(outBuf, maxLen, "!%08x", num);
    }
}

void NodeStore::clear() {
    if (!mutex) return;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        nodes.clear();
        xSemaphoreGive(mutex);
    }
}
