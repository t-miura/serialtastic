#include "MeshtasticClient.h"
#include "pb_helpers.h"
#include <cstdio>
#include <cstring>

MeshtasticClient::MeshtasticClient(NodeStore &nStore, MessageStore &mStore)
    : nodeStore(nStore), msgStore(mStore), serial(nullptr),
      activeSyncNonce(0), lastHeartbeatTime(0), lastSyncAttemptTime(0), lastTelemetryPollTime(0),
      logEntryCount(0), logEntryHead(0) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        channels[i].index = i;
        channels[i].is_active = (i == 0);
        snprintf(channels[i].name, sizeof(channels[i].name), i == 0 ? "Primary" : "Chan%d", i);
    }
}

void MeshtasticClient::begin(HardwareSerial *serialPort) {
    serial = serialPort;
    framer.setCallback([this](const uint8_t *payload, size_t len) {
        this->onFrameReceived(payload, len);
    });
    
    localInfo.state = SyncState::CONNECTING;
    requestConfigSync();
}

void MeshtasticClient::setMessageReceivedCallback(MessageReceivedCallback cb) {
    messageCallback = cb;
}

void MeshtasticClient::requestConfigSync() {
    activeSyncNonce = (uint32_t)random(1000, 999999);
    meshtastic_ToRadio toRadio = meshtastic_ToRadio_init_zero;
    toRadio.which_payload_variant = meshtastic_ToRadio_want_config_id_tag;
    toRadio.want_config_id = activeSyncNonce;

    sendToRadio(toRadio);
    localInfo.state = SyncState::SYNCING_CONFIG;
    lastSyncAttemptTime = millis();
    appendLog("SYNC", "Sent want_config_id request to radio");
}

void MeshtasticClient::requestTelemetry(uint32_t toNode) {
    if (!serial || localInfo.node_num == 0) return;
    if (toNode == 0) toNode = localInfo.node_num;

    meshtastic_ToRadio toRadio = meshtastic_ToRadio_init_zero;
    toRadio.which_payload_variant = meshtastic_ToRadio_packet_tag;

    meshtastic_MeshPacket &p = toRadio.packet;
    p.to = toNode;
    p.from = localInfo.node_num;
    p.channel = 0;
    p.want_ack = false;
    p.hop_limit = 3;
    p.id = (uint32_t)random(1, 0x7FFFFFFF);

    p.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    p.decoded.portnum = meshtastic_PortNum_TELEMETRY_APP;
    p.decoded.want_response = true;

    meshtastic_Telemetry req = meshtastic_Telemetry_init_zero;
    req.which_variant = meshtastic_Telemetry_device_metrics_tag;
    req.variant.device_metrics = meshtastic_DeviceMetrics_init_zero;

    pb_ostream_t stream = pb_ostream_from_buffer(p.decoded.payload.bytes, sizeof(p.decoded.payload.bytes));
    if (pb_encode(&stream, meshtastic_Telemetry_fields, &req)) {
        p.decoded.payload.size = stream.bytes_written;
        sendToRadio(toRadio);
    }
}

void MeshtasticClient::sendHeartbeat() {
    meshtastic_ToRadio toRadio = meshtastic_ToRadio_init_zero;
    toRadio.which_payload_variant = meshtastic_ToRadio_heartbeat_tag;
    sendToRadio(toRadio);
    lastHeartbeatTime = millis();
}

void MeshtasticClient::sendToRadio(const meshtastic_ToRadio &toRadio) {
    if (!serial) return;
    uint8_t pbBuffer[STREAM_MAX_PAYLOAD];
    size_t pbLen = pb_encode_to_bytes(pbBuffer, sizeof(pbBuffer), meshtastic_ToRadio_fields, &toRadio);
    if (pbLen > 0) {
        StreamFramer::sendFrame(*serial, pbBuffer, pbLen);
    }
}

void MeshtasticClient::update() {
    if (!serial) return;

    // Drain serial buffer
    while (serial->available()) {
        uint8_t b = serial->read();
        framer.feedByte(b);
    }

    uint32_t now = millis();
    // Heartbeat every 10s
    if (now - lastHeartbeatTime > MESH_HEARTBEAT_INTERVAL_MS) {
        sendHeartbeat();
    }

    // Poll local telemetry every 15s once synced
    if (localInfo.is_synced && (now - lastTelemetryPollTime > 15000)) {
        lastTelemetryPollTime = now;
        requestTelemetry(localInfo.node_num);
    }

    // Re-attempt sync if timed out
    if (!localInfo.is_synced && (now - lastSyncAttemptTime > MESH_SYNC_TIMEOUT_MS)) {
        appendLog("SYNC", "Sync timed out, retrying...");
        requestConfigSync();
    }
}

void MeshtasticClient::onFrameReceived(const uint8_t *payload, size_t len) {
    if (len == 0 || !payload) return;

    meshtastic_FromRadio fromRadio = meshtastic_FromRadio_init_zero;
    if (!pb_decode_from_bytes(payload, len, meshtastic_FromRadio_fields, &fromRadio)) {
        appendLog("PROTO", "Failed to decode FromRadio packet");
        return;
    }

    localInfo.last_packet_received = millis();
    handleFromRadio(fromRadio);
}

void MeshtasticClient::handleFromRadio(const meshtastic_FromRadio &fromRadio) {
    switch (fromRadio.which_payload_variant) {
        case meshtastic_FromRadio_my_info_tag: {
            localInfo.node_num = fromRadio.my_info.my_node_num;
            snprintf(localInfo.id_str, sizeof(localInfo.id_str), "!%08x", localInfo.node_num);
            
            NodeData me;
            me.num = localInfo.node_num;
            strncpy(me.id_str, localInfo.id_str, sizeof(me.id_str) - 1);
            me.is_local = true;
            me.has_pki_key = true;
            nodeStore.upsertNode(me);
            
            appendLog("INFO", "Received MyNodeInfo");
            break;
        }

        case meshtastic_FromRadio_node_info_tag: {
            NodeData node;
            node.num = fromRadio.node_info.num;
            snprintf(node.id_str, sizeof(node.id_str), "!%08x", node.num);
            if (fromRadio.node_info.has_user) {
                strncpy(node.long_name, fromRadio.node_info.user.long_name, sizeof(node.long_name) - 1);
                strncpy(node.short_name, fromRadio.node_info.user.short_name, sizeof(node.short_name) - 1);
                node.has_pki_key = (fromRadio.node_info.user.public_key.size > 0);
                node.hardware_model = fromRadio.node_info.user.hw_model;
                node.role = fromRadio.node_info.user.role;
            }
            if (fromRadio.node_info.has_position) {
                node.latitude_i = fromRadio.node_info.position.latitude_i;
                node.longitude_i = fromRadio.node_info.position.longitude_i;
                node.altitude = fromRadio.node_info.position.altitude;
                node.has_position = (node.latitude_i != 0 || node.longitude_i != 0);
            }
            if (fromRadio.node_info.has_device_metrics) {
                node.battery_level = fromRadio.node_info.device_metrics.battery_level;
                node.voltage = fromRadio.node_info.device_metrics.voltage;
                node.uptime_seconds = fromRadio.node_info.device_metrics.uptime_seconds;
                node.channel_util = fromRadio.node_info.device_metrics.channel_utilization;
                node.air_util_tx = fromRadio.node_info.device_metrics.air_util_tx;
            }
            node.snr = fromRadio.node_info.snr;
            node.last_heard = fromRadio.node_info.last_heard;
            node.hops_away = fromRadio.node_info.hops_away;
            node.is_local = (node.num == localInfo.node_num);
            
            if (node.is_local) {
                node.has_pki_key = true;
                strncpy(localInfo.long_name, node.long_name, sizeof(localInfo.long_name) - 1);
                strncpy(localInfo.short_name, node.short_name, sizeof(localInfo.short_name) - 1);
            }

            nodeStore.upsertNode(node);
            break;
        }

        case meshtastic_FromRadio_channel_tag: {
            uint8_t idx = fromRadio.channel.index;
            if (idx < MAX_CHANNELS) {
                channels[idx].index = idx;
                channels[idx].role = fromRadio.channel.role;
                channels[idx].is_active = (fromRadio.channel.role != meshtastic_Channel_Role_DISABLED);
                if (fromRadio.channel.has_settings && fromRadio.channel.settings.name[0] != '\0') {
                    strncpy(channels[idx].name, fromRadio.channel.settings.name, sizeof(channels[idx].name) - 1);
                }
            }
            break;
        }

        case meshtastic_FromRadio_config_complete_id_tag: {
            if (fromRadio.config_complete_id == activeSyncNonce || activeSyncNonce == 0) {
                localInfo.is_synced = true;
                localInfo.state = SyncState::READY;
                appendLog("SYNC", "NodeDB & Config Sync Complete!");
                lastTelemetryPollTime = millis();
                requestTelemetry(localInfo.node_num);
            }
            break;
        }

        case meshtastic_FromRadio_packet_tag: {
            handleMeshPacket(fromRadio.packet);
            break;
        }

        case meshtastic_FromRadio_log_record_tag: {
            appendLog(fromRadio.log_record.source, fromRadio.log_record.message);
            break;
        }

        case meshtastic_FromRadio_metadata_tag: {
            strncpy(localInfo.fw_version, fromRadio.metadata.firmware_version, sizeof(localInfo.fw_version) - 1);
            localInfo.hw_model = fromRadio.metadata.hw_model;
            localInfo.role = fromRadio.metadata.role;

            NodeData me;
            me.num = localInfo.node_num;
            me.hardware_model = fromRadio.metadata.hw_model;
            me.role = fromRadio.metadata.role;
            me.is_local = true;
            me.has_pki_key = true;
            nodeStore.upsertNode(me);
            break;
        }

        case meshtastic_FromRadio_rebooted_tag: {
            appendLog("WARN", "Attached Meshtastic Radio rebooted!");
            localInfo.is_synced = false;
            localInfo.state = SyncState::CONNECTING;
            requestConfigSync();
            break;
        }

        default:
            break;
    }
}

void MeshtasticClient::handleMeshPacket(const meshtastic_MeshPacket &mp) {
    // Update node last heard / SNR
    uint32_t nowSecs = mp.has_rx_time ? mp.rx_time : (millis() / 1000);
    nodeStore.updateLastHeard(mp.from, nowSecs, mp.rx_snr, mp.rx_rssi, mp.hop_limit);

    if (mp.which_payload_variant == meshtastic_MeshPacket_decoded_tag) {
        const meshtastic_Data &data = mp.decoded;

        if (data.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP) {
            ChatMessage msg;
            msg.id = mp.id;
            msg.from_node = mp.from;
            msg.to_node = mp.to;
            msg.channel = mp.channel;
            msg.timestamp = nowSecs;
            msg.is_incoming = (mp.from != localInfo.node_num);
            msg.is_direct = (mp.to != NODENUM_BROADCAST);
            msg.is_acknowledged = false;

            nodeStore.getNodeDisplayName(mp.from, msg.from_name, sizeof(msg.from_name));
            
            size_t copyLen = data.payload.size;
            if (copyLen > MAX_TEXT_PAYLOAD_LEN) copyLen = MAX_TEXT_PAYLOAD_LEN;
            memcpy(msg.text, data.payload.bytes, copyLen);
            msg.text[copyLen] = '\0';

            msgStore.addMessage(msg);

            if (msg.is_incoming && messageCallback) {
                messageCallback(msg);
            }
        } else if (data.portnum == meshtastic_PortNum_ROUTING_APP) {
            if (data.request_id != 0) {
                msgStore.markAcknowledged(data.request_id);
            }
        } else if (data.portnum == meshtastic_PortNum_NODEINFO_APP) {
            meshtastic_User user = meshtastic_User_init_zero;
            if (pb_decode_from_bytes(data.payload.bytes, data.payload.size, meshtastic_User_fields, &user)) {
                NodeData node;
                node.num = mp.from;
                snprintf(node.id_str, sizeof(node.id_str), "!%08x", node.num);
                strncpy(node.long_name, user.long_name, sizeof(node.long_name) - 1);
                strncpy(node.short_name, user.short_name, sizeof(node.short_name) - 1);
                node.hardware_model = user.hw_model;
                node.role = user.role;
                node.has_pki_key = (user.public_key.size > 0);
                node.is_local = (node.num == localInfo.node_num);
                nodeStore.upsertNode(node);
            }
        } else if (data.portnum == meshtastic_PortNum_POSITION_APP) {
            meshtastic_Position pos = meshtastic_Position_init_zero;
            if (pb_decode_from_bytes(data.payload.bytes, data.payload.size, meshtastic_Position_fields, &pos)) {
                nodeStore.updatePosition(mp.from, pos.latitude_i, pos.longitude_i, pos.altitude);
            }
        } else if (data.portnum == meshtastic_PortNum_TELEMETRY_APP) {
            meshtastic_Telemetry telemetry = meshtastic_Telemetry_init_zero;
            if (pb_decode_from_bytes(data.payload.bytes, data.payload.size, meshtastic_Telemetry_fields, &telemetry)) {
                handleTelemetry(mp, telemetry);
            }
        }
    }
}

void MeshtasticClient::handleTelemetry(const meshtastic_MeshPacket &mp, const meshtastic_Telemetry &telemetry) {
    if (telemetry.which_variant == meshtastic_Telemetry_device_metrics_tag) {
        uint32_t batt = telemetry.variant.device_metrics.battery_level;
        float volt = telemetry.variant.device_metrics.voltage;
        uint32_t uptime = telemetry.variant.device_metrics.uptime_seconds;
        float chanUtil = telemetry.variant.device_metrics.channel_utilization;
        float airUtil = telemetry.variant.device_metrics.air_util_tx;
        
        if (mp.from == localInfo.node_num) {
            localInfo.battery_percent = batt;
            localInfo.voltage = volt;
            localInfo.channel_util = chanUtil;
            localInfo.air_util_tx = airUtil;
        }
        nodeStore.updateTelemetry(mp.from, batt, volt, uptime, chanUtil, airUtil, mp.rx_snr, mp.rx_rssi);
    }
}

bool MeshtasticClient::sendTextMessage(const char *text, uint32_t toNode, uint8_t channel, bool wantAck) {
    if (!text || strlen(text) == 0 || !serial) return false;

    meshtastic_ToRadio toRadio = meshtastic_ToRadio_init_zero;
    toRadio.which_payload_variant = meshtastic_ToRadio_packet_tag;

    meshtastic_MeshPacket &p = toRadio.packet;
    p.to = toNode;
    p.from = localInfo.node_num;
    p.channel = channel;
    p.want_ack = wantAck;
    p.hop_limit = 3;
    p.id = (uint32_t)random(1, 0x7FFFFFFF);

    p.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    p.decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    
    size_t textLen = strlen(text);
    if (textLen > sizeof(p.decoded.payload.bytes)) {
        textLen = sizeof(p.decoded.payload.bytes);
    }
    memcpy(p.decoded.payload.bytes, text, textLen);
    p.decoded.payload.size = textLen;

    sendToRadio(toRadio);

    // Add to local message store as outgoing message
    ChatMessage msg;
    msg.id = p.id;
    msg.from_node = localInfo.node_num;
    msg.to_node = toNode;
    msg.channel = channel;
    msg.timestamp = millis() / 1000;
    msg.is_incoming = false;
    msg.is_direct = (toNode != NODENUM_BROADCAST);
    msg.is_acknowledged = false;
    nodeStore.getNodeDisplayName(localInfo.node_num, msg.from_name, sizeof(msg.from_name));
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    
    msgStore.addMessage(msg);
    return true;
}

void MeshtasticClient::appendLog(const char *tag, const char *msg) {
    if (!msg || msg[0] == '\0') return;

    size_t idx = (logEntryHead + logEntryCount) % MAX_LOG_ENTRIES;
    if (logEntryCount < MAX_LOG_ENTRIES) {
        logEntryCount++;
    } else {
        logEntryHead = (logEntryHead + 1) % MAX_LOG_ENTRIES;
    }

    logEntries[idx].timestampMs = millis();

    // Clean source paths (e.g. "src/modules/AdminModule.cpp" -> "AdminModule.cpp")
    const char *cleanTag = tag ? tag : "LOG";
    const char *slash = strrchr(cleanTag, '/');
    if (slash) {
        cleanTag = slash + 1;
    }

    strncpy(logEntries[idx].tag, cleanTag, sizeof(logEntries[idx].tag) - 1);
    logEntries[idx].tag[sizeof(logEntries[idx].tag) - 1] = '\0';
    
    // Copy msg and strip/replace newlines
    size_t outPos = 0;
    for (size_t i = 0; msg[i] != '\0' && outPos < sizeof(logEntries[idx].text) - 1; i++) {
        char c = msg[i];
        if (c == '\r' || c == '\n') {
            if (outPos > 0 && logEntries[idx].text[outPos - 1] != ' ') {
                logEntries[idx].text[outPos++] = ' ';
            }
        } else if ((unsigned char)c >= 32) {
            logEntries[idx].text[outPos++] = c;
        }
    }
    logEntries[idx].text[outPos] = '\0';

    // Parse attached radio heap if present (e.g. "[heap 14708]" or "heap 14708")
    const char *heapPtr = strstr(msg, "[heap ");
    if (!heapPtr) heapPtr = strstr(msg, "heap ");
    if (!heapPtr && tag) heapPtr = strstr(tag, "heap ");
    if (heapPtr) {
        const char *numPtr = heapPtr;
        while (*numPtr && (*numPtr < '0' || *numPtr > '9')) numPtr++;
        if (*numPtr >= '0' && *numPtr <= '9') {
            uint32_t val = (uint32_t)strtoul(numPtr, nullptr, 10);
            if (val > 0) {
                localInfo.radio_free_heap = val;
                if (localInfo.radio_min_heap == 0 || val < localInfo.radio_min_heap) {
                    localInfo.radio_min_heap = val;
                }
                if (localInfo.radio_max_heap == 0 || val > localInfo.radio_max_heap) {
                    localInfo.radio_max_heap = val;
                }
            }
        }
    }
}

size_t MeshtasticClient::getLogCount() const {
    return logEntryCount;
}

bool MeshtasticClient::getLogEntry(size_t index, LogEntry &entry) const {
    if (index >= logEntryCount) return false;
    size_t actualIdx = (logEntryHead + index) % MAX_LOG_ENTRIES;
    entry = logEntries[actualIdx];
    return true;
}

LocalRadioInfo MeshtasticClient::getRadioInfo() {
    return localInfo;
}

ChannelData MeshtasticClient::getChannel(uint8_t index) {
    if (index < MAX_CHANNELS) return channels[index];
    return ChannelData();
}

bool MeshtasticClient::isSynced() const {
    return localInfo.is_synced;
}
