#pragma once

#include <Arduino.h>
#include "config.h"

#define NODENUM_BROADCAST 0xFFFFFFFF

enum class SyncState {
    DISCONNECTED,
    CONNECTING,
    SYNCING_CONFIG,
    SYNCING_NODES,
    READY
};

enum class ActivePane {
    NODES,
    MESSAGES,
    CHANNELS,
    LOGS
};

struct NodeData {
    uint32_t num = 0;
    char id_str[12] = {0};
    char long_name[40] = {0};
    char short_name[10] = {0};
    float snr = 0.0f;
    int32_t rssi = 0;
    uint32_t last_heard = 0;
    uint32_t battery_level = 0; // 0..100
    float voltage = 0.0f;
    uint32_t uptime_seconds = 0;
    float channel_util = 0.0f;
    float air_util_tx = 0.0f;
    bool has_position = false;
    int32_t latitude_i = 0;
    int32_t longitude_i = 0;
    int32_t altitude = 0;
    uint8_t hops_away = 0;
    int hardware_model = 0;
    int role = 0;
    bool is_local = false;
    bool has_pki_key = false;
};

struct ChatMessage {
    uint32_t id = 0;
    uint32_t from_node = 0;
    uint32_t to_node = NODENUM_BROADCAST;
    uint8_t channel = 0;
    char from_name[32] = {0};
    char text[MAX_TEXT_PAYLOAD_LEN + 1] = {0};
    uint32_t timestamp = 0;
    bool is_incoming = true;
    bool is_acknowledged = false;
    bool is_direct = false;
};

struct ChannelData {
    uint8_t index = 0;
    char name[16] = {0};
    bool is_active = false;
    uint8_t role = 0; // 0=disabled, 1=primary, 2=secondary
};

struct LocalRadioInfo {
    uint32_t node_num = 0;
    char id_str[12] = {0};
    char long_name[40] = {0};
    char short_name[10] = {0};
    uint32_t battery_percent = 0;
    float voltage = 0.0f;
    float channel_util = 0.0f;
    float air_util_tx = 0.0f;
    int hw_model = 0;
    int role = 0;
    char fw_version[16] = {0};
    bool is_synced = false;
    SyncState state = SyncState::DISCONNECTED;
    uint32_t last_heartbeat_sent = 0;
    uint32_t last_packet_received = 0;
    uint32_t radio_free_heap = 0;
    uint32_t radio_min_heap = 0;
    uint32_t radio_max_heap = 0;
};

struct LogEntry {
    uint32_t timestampMs = 0;
    char tag[24] = {0};
    char text[128] = {0};
};
