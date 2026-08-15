#include "TerminalUI.h"
#include "Colors.h"
#include <cstdio>
#include <cstring>

TerminalUI::TerminalUI(NodeStore &nStore, MessageStore &mStore, MeshtasticClient &cli)
    : nodeStore(nStore), msgStore(mStore), client(cli),
      currentPane(ActivePane::MESSAGES), selectedNodeIdx(0), selectedChannel(0),
      dmMode(false), showNodeDetails(false), isDirty(true), lastRenderTime(0),
      inputCursorPos(0) {
    memset(currentInputText, 0, sizeof(currentInputText));
}

void TerminalUI::begin() {
    vgaController.begin();
    vgaController.setResolution(VGA_640x480_60Hz);

    terminal.begin(&vgaController, 80, 30);
    terminal.loadFont(&fabgl::FONT_8x16);
    terminal.enableCursor(false);
    terminal.write("\x1B[?7l"); // Disable auto-wrap
    terminal.clear();
    markDirty();
}

void TerminalUI::markDirty() {
    isDirty = true;
}

void TerminalUI::moveCursor(int row, int col) {
    if (row < 1) row = 1;
    if (row > 30) row = 30;
    if (col < 1) col = 1;
    if (col > 80) col = 80;
    terminal.printf("\x1B[%d;%dH", row, col);
}

void TerminalUI::setInputBuffer(const char *buf, size_t cursor) {
    if (buf) {
        strncpy(currentInputText, buf, sizeof(currentInputText) - 1);
        currentInputText[sizeof(currentInputText) - 1] = '\0';
    } else {
        currentInputText[0] = '\0';
    }
    inputCursorPos = cursor;
    markDirty();
}

void TerminalUI::selectNextNode() {
    size_t count = nodeStore.getNodeCount();
    if (count > 0) {
        selectedNodeIdx = (selectedNodeIdx + 1) % count;
        markDirty();
    }
}

void TerminalUI::selectPrevNode() {
    size_t count = nodeStore.getNodeCount();
    if (count > 0) {
        selectedNodeIdx = (selectedNodeIdx - 1 + count) % count;
        markDirty();
    }
}

void TerminalUI::selectNextChannel() {
    selectedChannel = (selectedChannel + 1) % MAX_CHANNELS;
    markDirty();
}

void TerminalUI::selectPrevChannel() {
    selectedChannel = (selectedChannel - 1 + MAX_CHANNELS) % MAX_CHANNELS;
    markDirty();
}

void TerminalUI::togglePane() {
    if (currentPane == ActivePane::NODES) currentPane = ActivePane::MESSAGES;
    else if (currentPane == ActivePane::MESSAGES) currentPane = ActivePane::NODES;
    markDirty();
}

void TerminalUI::setPane(ActivePane pane) {
    currentPane = pane;
    markDirty();
}

void TerminalUI::toggleDMMode() {
    dmMode = !dmMode;
    if (dmMode) {
        currentPane = ActivePane::MESSAGES;
    }
    markDirty();
}

void TerminalUI::toggleNodeDetails() {
    showNodeDetails = !showNodeDetails;
    markDirty();
}

void TerminalUI::notifyIncomingMessage(const ChatMessage &msg) {
    if (!msg.is_incoming) return;

    notification.fromNode = msg.from_node;
    strncpy(notification.fromName, msg.from_name, sizeof(notification.fromName) - 1);
    notification.channel = msg.channel;
    notification.isDirect = msg.is_direct;
    notification.timestamp = millis();
    notification.active = true;

    // Create single line preview (replace newlines with spaces)
    size_t previewLen = 0;
    for (size_t i = 0; msg.text[i] != '\0' && previewLen < sizeof(notification.previewText) - 1; i++) {
        char c = msg.text[i];
        if (c == '\n' || c == '\r' || (unsigned char)c < 32) {
            notification.previewText[previewLen++] = ' ';
        } else {
            notification.previewText[previewLen++] = c;
        }
    }
    notification.previewText[previewLen] = '\0';

    markDirty();
}

void TerminalUI::jumpToNotification() {
    if (!notification.active) return;

    if (notification.isDirect) {
        dmMode = true;
        int idx = nodeStore.findIndex(notification.fromNode);
        if (idx >= 0) {
            selectedNodeIdx = idx;
        }
    } else {
        dmMode = false;
        selectedChannel = notification.channel;
    }

    currentPane = ActivePane::MESSAGES;
    showNodeDetails = false;
    notification.active = false;
    markDirty();
}

void TerminalUI::dismissNotification() {
    if (notification.active) {
        notification.active = false;
        markDirty();
    }
}

uint32_t TerminalUI::getTargetNodeNum() const {
    if (!dmMode) return NODENUM_BROADCAST;
    NodeData node;
    if (const_cast<NodeStore&>(nodeStore).getNodeByIndex(selectedNodeIdx, node)) {
        return node.num;
    }
    return NODENUM_BROADCAST;
}

void TerminalUI::render() {
    uint32_t now = millis();
    // Auto-expire notification after 15s
    if (notification.active && (now - notification.timestamp > 15000)) {
        notification.active = false;
        isDirty = true;
    }

    if (showNodeDetails) {
        if (!isDirty) return; // Completely avoid repainting popup if state hasn't changed
        isDirty = false;
        lastRenderTime = now;
        drawStatusBar();
        drawDividers();
        drawNodeDetailsPopup();
        drawInputBar();
        drawFooter();
        return;
    }

    // Only re-render if dirty or periodic refresh every 1s
    if (!isDirty && (now - lastRenderTime < 1000)) {
        return;
    }
    isDirty = false;
    lastRenderTime = now;

    if (currentPane == ActivePane::LOGS) {
        drawLogsView();
        return;
    }

    drawStatusBar();
    drawDividers();
    drawNodesPane();
    drawMessagesPane();
    drawInputBar();
    drawFooter();
}

static const char* getHardwareModelName(int model) {
    switch (model) {
        case 0: return "UNSET";
        case 1: return "TLora V2";
        case 2: return "TLora V1";
        case 3: return "TLora V2.1-1.6";
        case 4: return "LilyGo T-Beam";
        case 5: return "Heltec V2.0";
        case 6: return "T-Beam V0.7";
        case 7: return "LilyGo T-Echo";
        case 8: return "TLora V1.1";
        case 9: return "RAK4631 WisBlock";
        case 10: return "Heltec V2.1";
        case 11: return "Heltec V1";
        case 12: return "LilyGo T-Beam S3";
        case 13: return "RAK11200";
        case 14: return "Nano G1";
        case 15: return "TLora V2.1-1.8";
        case 16: return "TLora T3-S3";
        case 17: return "Nano G1 Explorer";
        case 18: return "Nano G2 Ultra";
        case 19: return "LoRa Type";
        case 20: return "WiPhone";
        case 21: return "Wio WM1110";
        case 22: return "RAK2560 Solar";
        case 23: return "Heltec HRU-3601";
        case 24: return "Heltec Bridge";
        case 25: return "Station G1";
        case 26: return "RAK11310 (RP2040)";
        case 27: return "MakerFabs Tracker";
        case 28: return "MakerFabs Reserved";
        case 29: return "CanaryOne";
        case 30: return "Waveshare RP2040";
        case 31: return "Station G2";
        case 32: return "LoRa Relay V1";
        case 33: return "T-Echo Plus";
        case 34: return "PPR";
        case 35: return "GenieBlocks";
        case 36: return "nRF52 Unknown";
        case 37: return "Portduino Host";
        case 38: return "Android Sim";
        case 39: return "DIY V1 (NanoVHF)";
        case 40: return "nRF52840 Dongle";
        case 41: return "Disaster Radio";
        case 42: return "M5Stack ESP32";
        case 43: return "Heltec V3 (ESP32-S3)";
        case 44: return "Heltec WSL V3";
        case 45: return "BetaFPV 2.4G TX";
        case 46: return "BetaFPV 900 TX";
        case 47: return "Raspberry Pi Pico";
        case 48: return "Heltec Tracker V1.1";
        case 49: return "Heltec Paper";
        case 50: return "LilyGo T-Deck";
        case 51: return "LilyGo T-Watch S3";
        case 52: return "Picomputer S3";
        case 53: return "Heltec HT62";
        case 54: return "EBYTE ESP32-S3";
        case 55: return "ESP32-S3-PICO";
        case 56: return "Chatter 2";
        case 57: return "Heltec Paper V1.0";
        case 58: return "Heltec Tracker V1.0";
        case 59: return "unPhone";
        case 60: return "TD-LORAC M.2";
        case 61: return "CDEBYTE EoRa-S3";
        case 62: return "TWC Mesh V4";
        case 63: return "nRF52 ProMicro DIY";
        case 64: return "Bandit Nano 900";
        case 65: return "Heltec Capsule V3";
        case 66: return "Vision Master T190";
        case 67: return "Vision Master E213";
        case 68: return "Vision Master E290";
        case 69: return "Heltec T114 (nRF52840)";
        case 70: return "SenseCAP Indicator";
        case 71: return "Tracker T1000-E";
        case 72: return "RAK3172 (STM32WL)";
        case 73: return "Seeed Wio-E5 (STM32WL)";
        case 74: return "RadioMaster Bandit";
        case 75: return "Minewsemi ME25LS01";
        case 76: return "RP2040 Feather";
        case 77: return "M5Stack CoreBasic";
        case 78: return "M5Stack Core2";
        case 79: return "Raspberry Pi Pico2";
        case 80: return "M5Stack CoreS3";
        case 81: return "Seeed XIAO S3";
        case 82: return "MS24SF1 nRF52840";
        case 83: return "LilyGo TLora-C6";
        case 84: return "WisMesh Tap";
        case 85: return "Routastic Linux";
        case 86: return "Mesh-Tab ESP32";
        case 87: return "MeshLink nRF52";
        case 88: return "XIAO nRF52 Kit";
        case 89: return "ThinkNode M1";
        case 90: return "ThinkNode M2";
        case 91: return "LilyGo T-ETH-Elite";
        case 92: return "Heltec Sensor Hub";
        case 93: return "Muzi-Base";
        case 94: return "Heltec Mesh Pocket";
        case 95: return "Seeed Solar Node";
        case 96: return "NomadStar Meteor";
        case 97: return "Elecrow CrowPanel";
        case 98: return "LilyGo LINK32";
        case 99: return "Wio Tracker L1";
        case 100: return "Wio Tracker L1 EInk";
        case 101: return "Muzi R1 Neo";
        case 102: return "LilyGo T-Deck Pro";
        case 103: return "LilyGo TLora Pager";
        case 104: return "M5Stack Cardputer";
        case 105: return "WisMesh Tag";
        case 106: return "RAK3312";
        case 107: return "ThinkNode M5";
        case 108: return "Heltec MeshSolar";
        case 109: return "LilyGo T-Echo Lite";
        case 110: return "Heltec V4";
        case 111: return "M5Stack C6L";
        case 112: return "M5 Cardputer Adv";
        case 113: return "Heltec Tracker V2";
        case 114: return "T-Watch Ultra";
        case 115: return "ThinkNode M3";
        case 116: return "WisMesh Tap V2";
        case 117: return "RAK3401";
        case 118: return "RAK6421 Hat+";
        case 119: return "ThinkNode M4";
        case 120: return "ThinkNode M6";
        case 121: return "Meshstick 1262";
        case 122: return "T-Beam 1-Watt";
        case 123: return "T5 S3 ePaper Pro";
        case 124: return "T-Beam BPF";
        case 125: return "Mini ePaper S3";
        case 126: return "T-Display S3 Pro";
        case 127: return "Heltec Node T096";
        case 128: return "Mesh Tracker X1";
        case 129: return "ThinkNode M7";
        case 130: return "ThinkNode M8";
        case 131: return "ThinkNode M9";
        case 132: return "Heltec V4 R8";
        case 133: return "Heltec Node T1";
        case 134: return "Station G3";
        case 135: return "T-Impulse Plus";
        case 136: return "T-Echo Card";
        case 137: return "Wio Tracker L2";
        case 138: return "CrowPanel P4";
        case 139: return "Heltec Tower V2";
        case 140: return "Meshnology W10";
        case 141: return "Heltec RC32 S3";
        case 142: return "Heltec RC52 NRF";
        case 143: return "Heltec RCC6 C6";
        case 255: return "Private HW";
        default: return (model > 0 ? "Meshtastic Node" : "UNSET");
    }
}

static const char* getNodeRoleName(int role) {
    switch (role) {
        case 0: return "CLIENT";
        case 1: return "CLIENT_MUTE";
        case 2: return "ROUTER";
        case 3: return "ROUTER_CLIENT";
        case 4: return "REPEATER";
        case 5: return "TRACKER";
        case 6: return "SENSOR";
        case 7: return "TAK";
        case 8: return "CLIENT_HIDDEN";
        case 9: return "LOST_AND_FOUND";
        case 10: return "TAK_TRACKER";
        case 11: return "ROUTER_LATE";
        case 12: return "CLIENT_BASE";
        default: return "CLIENT";
    }
}

static void formatUptimeStr(uint32_t secs, char *out, size_t maxLen) {
    if (secs == 0) {
        snprintf(out, maxLen, "Unknown");
        return;
    }
    uint32_t days = secs / 86400;
    uint32_t hours = (secs % 86400) / 3600;
    uint32_t mins = (secs % 3600) / 60;
    uint32_t s = secs % 60;
    if (days > 0) {
        snprintf(out, maxLen, "%ud %uh %um", days, hours, mins);
    } else if (hours > 0) {
        snprintf(out, maxLen, "%uh %um %us", hours, mins, s);
    } else {
        snprintf(out, maxLen, "%um %us", mins, s);
    }
}

static void formatLastHeardStr(uint32_t timestamp, char *out, size_t maxLen) {
    if (timestamp == 0) {
        snprintf(out, maxLen, "Never / Unknown");
        return;
    }
    uint32_t now = millis() / 1000;
    if (timestamp > 1700000000) {
        snprintf(out, maxLen, "Recorded");
    } else if (now >= timestamp) {
        uint32_t diff = now - timestamp;
        if (diff < 60) snprintf(out, maxLen, "%us ago", diff);
        else if (diff < 3600) snprintf(out, maxLen, "%um ago", diff / 60);
        else snprintf(out, maxLen, "%uh ago", diff / 3600);
    } else {
        snprintf(out, maxLen, "Recent");
    }
}

void TerminalUI::drawNodeDetailsPopup() {
    NodeData node;
    if (!nodeStore.getNodeByIndex(selectedNodeIdx, node)) return;

    int top = 5, height = 17;
    char uptimeBuf[20];
    formatUptimeStr(node.uptime_seconds, uptimeBuf, sizeof(uptimeBuf));
    char lastHeardBuf[24];
    formatLastHeardStr(node.last_heard, lastHeardBuf, sizeof(lastHeardBuf));

    // Clear top margin row 4
    moveCursor(4, 1);
    terminal.write(ANSI_BG_BLACK "                                                                               \x1B[K");

    for (int r = 0; r < height; r++) {
        moveCursor(top + r, 1);
        terminal.write(ANSI_BG_BLACK "            "); // 12 spaces left margin
        terminal.write(ANSI_BG_BLUE ANSI_FG_BRIGHT_WHITE);

        if (r == 0) {
            terminal.write("+-------------------- NODE DETAILS --------------------+");
        } else if (r == 5) {
            terminal.write("+----------------- TELEMETRY & SIGNAL -----------------+");
        } else if (r == 10) {
            terminal.write("+-------------------- GPS POSITION --------------------+");
        } else if (r == 14) {
            terminal.write("+------------------------------------------------------+");
        } else if (r == height - 1) {
            terminal.write("+------------------------------------------------------+");
        } else {
            char lineBuf[58];
            char rawContent[54];
            rawContent[0] = '\0';

            switch (r) {
                case 1:
                    snprintf(rawContent, sizeof(rawContent), "ID: %-10s (Num: %-10u)", node.id_str, (unsigned int)node.num);
                    break;
                case 2:
                    snprintf(rawContent, sizeof(rawContent), "Name: %-8s  Long: %-22s",
                             node.short_name[0] ? node.short_name : "Anon",
                             node.long_name[0] ? node.long_name : "Unknown");
                    break;
                case 3:
                    snprintf(rawContent, sizeof(rawContent), "Hardware: %-20s",
                             getHardwareModelName(node.hardware_model));
                    break;
                case 4:
                    snprintf(rawContent, sizeof(rawContent), "Role: %-12s  [PKI: %s]",
                             getNodeRoleName(node.role),
                             node.has_pki_key ? "Curve25519" : "None");
                    break;
                case 6:
                    if (node.voltage > 0.0f) {
                        snprintf(rawContent, sizeof(rawContent), "Battery: %2u%% (%.2fV)   Uptime: %s",
                                 (unsigned int)node.battery_level, node.voltage, uptimeBuf);
                    } else {
                        snprintf(rawContent, sizeof(rawContent), "Battery: %2u%% (Powered)  Uptime: %s",
                                 (unsigned int)node.battery_level, uptimeBuf);
                    }
                    break;
                case 7:
                    snprintf(rawContent, sizeof(rawContent), "Signal:  SNR: %+5.1fdB   RSSI: %-4ddBm",
                             node.snr, (int)node.rssi);
                    break;
                case 8:
                    snprintf(rawContent, sizeof(rawContent), "Hops:    %-2u hop(s)      Last Seen: %s",
                             (unsigned int)node.hops_away, lastHeardBuf);
                    break;
                case 9:
                    snprintf(rawContent, sizeof(rawContent), "Metrics: ChUtil: %.1f%%   AirUtil TX: %.1f%%",
                             node.channel_util, node.air_util_tx);
                    break;
                case 11:
                    if (node.has_position) {
                        double lat = node.latitude_i * 1e-7;
                        double lon = node.longitude_i * 1e-7;
                        char latDir = (lat >= 0) ? 'N' : 'S';
                        char lonDir = (lon >= 0) ? 'E' : 'W';
                        if (lat < 0) lat = -lat;
                        if (lon < 0) lon = -lon;
                        snprintf(rawContent, sizeof(rawContent), "Pos: %.5f%c, %.5f%c",
                                 lat, latDir, lon, lonDir);
                    } else {
                        snprintf(rawContent, sizeof(rawContent), "Pos: No GPS lock / Position not available");
                    }
                    break;
                case 12:
                    if (node.has_position && node.altitude != 0) {
                        snprintf(rawContent, sizeof(rawContent), "Altitude: %d m MSL", (int)node.altitude);
                    } else {
                        snprintf(rawContent, sizeof(rawContent), "Altitude: N/A");
                    }
                    break;
                case 15:
                    if (notification.active) {
                        snprintf(rawContent, sizeof(rawContent), ">> 🔔 NEW MSG! [F6]/[Enter] Jump | [ESC] Close <<");
                    } else {
                        snprintf(rawContent, sizeof(rawContent), "Press [ESC]/[F1] Close | [Up]/[Down] Browse Nodes");
                    }
                    break;
                default:
                    rawContent[0] = '\0';
                    break;
            }

            bool isAlertLine = (r == 15 && notification.active);
            if (isAlertLine) {
                terminal.write(ANSI_BG_YELLOW ANSI_FG_BLACK ANSI_BOLD);
            }
            snprintf(lineBuf, sizeof(lineBuf), "| %-52s |", rawContent);
            terminal.write(lineBuf);
            if (isAlertLine) {
                terminal.write(ANSI_BG_BLUE ANSI_FG_BRIGHT_WHITE);
            }
        }

        terminal.write(ANSI_BG_BLACK "            " ANSI_RESET "\x1B[K"); // 12 spaces right margin
    }

    // Clear bottom margin rows 22..24
    for (int r = top + height; r <= 24; r++) {
        moveCursor(r, 1);
        terminal.write(ANSI_BG_BLACK "                                                                               \x1B[K");
    }
    terminal.write(ANSI_RESET);
}

void TerminalUI::drawStatusBar() {
    moveCursor(1, 1);
    LocalRadioInfo info = client.getRadioInfo();
    ChannelData ch = client.getChannel(selectedChannel);

    terminal.write(ANSI_BG_BRIGHT_BLUE ANSI_FG_WHITE ANSI_BOLD);
    
    char statusBuf[80];
    const char *stateStr = info.is_synced ? "SYNC OK" : (info.state == SyncState::CONNECTING ? "CONNECT" : "SYNCING");
    const char *myName = (info.short_name[0] != '\0') ? info.short_name : info.id_str;
    
    snprintf(statusBuf, sizeof(statusBuf),
             " SERIALTASTIC | %-8s | Ch:%-8s | Nodes:%-2u | Batt:%2u%% | [%s]",
             myName[0] != '\0' ? myName : "No Radio",
             ch.name[0] != '\0' ? ch.name : "Primary",
             (unsigned int)nodeStore.getNodeCount(),
             (unsigned int)info.battery_percent,
             stateStr);

    // Keep length at 79 chars to avoid wrapping line 1
    size_t len = strlen(statusBuf);
    while (len < 79) {
        statusBuf[len++] = ' ';
    }
    statusBuf[79] = '\0';

    terminal.write(statusBuf);
    terminal.write(ANSI_RESET);
}

void TerminalUI::drawDividers() {
    // Header divider (Row 2)
    moveCursor(2, 1);
    terminal.write(ANSI_FG_BRIGHT_BLACK);
    terminal.write("----------------------------+-------------------------------------------------");
    terminal.write(ANSI_RESET);

    // Vertical divider on rows 3..23
    for (int r = 3; r <= 23; r++) {
        moveCursor(r, 28);
        terminal.write(ANSI_FG_BRIGHT_BLACK "|" ANSI_RESET);
    }

    // Bottom divider (Row 24)
    moveCursor(24, 1);
    terminal.write(ANSI_FG_BRIGHT_BLACK);
    terminal.write("----------------------------+-------------------------------------------------");
    terminal.write(ANSI_RESET);
}

void TerminalUI::drawNodesPane() {
    bool isFocused = (currentPane == ActivePane::NODES);
    moveCursor(3, 1);
    if (isFocused) {
        terminal.write(ANSI_BG_DARK_GRAY ANSI_FG_BRIGHT_YELLOW ANSI_BOLD " NODES (F1/Tab)           " ANSI_RESET);
    } else {
        terminal.write(ANSI_FG_BRIGHT_CYAN ANSI_BOLD " NODES                     " ANSI_RESET);
    }

    size_t totalNodes = nodeStore.getNodeCount();
    int visibleRows = 20; // rows 4 to 23

    for (int i = 0; i < visibleRows; i++) {
        moveCursor(4 + i, 1);
        if (i < (int)totalNodes) {
            NodeData node;
            nodeStore.getNodeByIndex(i, node);

            bool isSelected = (i == selectedNodeIdx);
            if (isSelected) {
                terminal.write(ANSI_BG_CYAN ANSI_FG_BLACK ANSI_BOLD);
            } else if (node.is_local) {
                terminal.write(ANSI_FG_BRIGHT_GREEN);
            } else {
                terminal.write(ANSI_FG_WHITE);
            }

            char nodeLine[28];
            const char *displayName = (node.short_name[0] != '\0') ? node.short_name : node.id_str;
            snprintf(nodeLine, sizeof(nodeLine), "%c%-9s %4.1fdB %2u%%",
                     isSelected ? '>' : (node.is_local ? '*' : ' '),
                     displayName,
                     node.snr,
                     (unsigned int)node.battery_level);
            
            size_t l = strlen(nodeLine);
            while (l < 27) nodeLine[l++] = ' ';
            nodeLine[27] = '\0';

            terminal.write(nodeLine);
            terminal.write(ANSI_RESET);
        } else {
            terminal.write("                           ");
        }
    }
}

void TerminalUI::drawMessagesPane() {
    bool isFocused = (currentPane == ActivePane::MESSAGES);
    moveCursor(3, 29);
    
    char headerTitle[52];
    if (dmMode) {
        NodeData target;
        nodeStore.getNodeByIndex(selectedNodeIdx, target);
        snprintf(headerTitle, sizeof(headerTitle), " DIRECT MSG: @%-20s", target.short_name[0] ? target.short_name : target.id_str);
    } else {
        ChannelData ch = client.getChannel(selectedChannel);
        snprintf(headerTitle, sizeof(headerTitle), " MESSAGES: #%-20s", ch.name[0] ? ch.name : "Primary");
    }

    if (isFocused) {
        terminal.write(ANSI_BG_DARK_GRAY ANSI_FG_BRIGHT_YELLOW ANSI_BOLD);
    } else {
        terminal.write(ANSI_FG_BRIGHT_CYAN ANSI_BOLD);
    }
    
    size_t hl = strlen(headerTitle);
    while (hl < 51) headerTitle[hl++] = ' ';
    headerTitle[51] = '\0';
    terminal.write(headerTitle);
    terminal.write(ANSI_RESET);

    // Fetch recent messages (up to 30)
    std::vector<ChatMessage> msgs;
    if (dmMode) {
        NodeData target;
        if (nodeStore.getNodeByIndex(selectedNodeIdx, target)) {
            msgStore.getDMMessages(target.num, msgs, 30);
        }
    } else {
        msgStore.getChannelMessages(selectedChannel, msgs, 30);
    }

    struct FormattedLine {
        char senderPrefix[24];
        char content[52];
        bool isIncoming;
        bool isContinuation;
    };

    std::vector<FormattedLine> displayLines;
    const size_t PANE_WIDTH = 50; // columns 29 to 79 width

    for (const auto &m : msgs) {
        char prefix[24];
        if (m.is_incoming) {
            snprintf(prefix, sizeof(prefix), "<%s> ", m.from_name[0] ? m.from_name : "Anon");
        } else {
            snprintf(prefix, sizeof(prefix), "<Me> ");
        }
        size_t prefixLen = strlen(prefix);

        char fullText[MAX_TEXT_PAYLOAD_LEN + 16];
        if (!m.is_incoming && m.is_acknowledged) {
            snprintf(fullText, sizeof(fullText), "%s [ACK]", m.text);
        } else {
            snprintf(fullText, sizeof(fullText), "%s", m.text);
        }

        // Split message by explicit newlines first ('\n' or '\r')
        const char *p = fullText;
        bool isFirstOverall = true;

        while (*p != '\0') {
            const char *lineEnd = p;
            while (*lineEnd != '\0' && *lineEnd != '\n' && *lineEnd != '\r') {
                lineEnd++;
            }

            char segment[MAX_TEXT_PAYLOAD_LEN + 16];
            size_t segLen = lineEnd - p;
            if (segLen >= sizeof(segment)) segLen = sizeof(segment) - 1;
            strncpy(segment, p, segLen);
            segment[segLen] = '\0';

            // Sanitize tabs or control chars to spaces
            for (size_t i = 0; i < segLen; i++) {
                if ((unsigned char)segment[i] < 32) segment[i] = ' ';
            }

            const char *segPtr = segment;
            bool isFirstInSegment = true;

            if (segLen == 0) {
                FormattedLine fl;
                fl.isIncoming = m.is_incoming;
                fl.isContinuation = !isFirstOverall;
                if (isFirstOverall) {
                    strncpy(fl.senderPrefix, prefix, sizeof(fl.senderPrefix) - 1);
                    fl.senderPrefix[sizeof(fl.senderPrefix) - 1] = '\0';
                } else {
                    fl.senderPrefix[0] = '\0';
                }
                fl.content[0] = '\0';
                displayLines.push_back(fl);
                isFirstOverall = false;
            } else {
                while (*segPtr != '\0') {
                    FormattedLine fl;
                    fl.isIncoming = m.is_incoming;
                    bool isInitialLine = (isFirstOverall && isFirstInSegment);
                    fl.isContinuation = !isInitialLine;

                    size_t avail = isInitialLine ? (PANE_WIDTH > prefixLen ? PANE_WIDTH - prefixLen : 10) : (PANE_WIDTH - 2);
                    if (avail > 50) avail = 50;

                    if (isInitialLine) {
                        strncpy(fl.senderPrefix, prefix, sizeof(fl.senderPrefix) - 1);
                        fl.senderPrefix[sizeof(fl.senderPrefix) - 1] = '\0';
                    } else {
                        fl.senderPrefix[0] = '\0';
                    }

                    size_t textLen = strlen(segPtr);
                    if (textLen <= avail) {
                        strncpy(fl.content, segPtr, sizeof(fl.content) - 1);
                        fl.content[sizeof(fl.content) - 1] = '\0';
                        displayLines.push_back(fl);
                        break;
                    } else {
                        size_t breakPos = avail;
                        while (breakPos > 0 && segPtr[breakPos] != ' ') {
                            breakPos--;
                        }
                        if (breakPos == 0) {
                            breakPos = avail;
                        }

                        size_t copyLen = breakPos;
                        if (copyLen >= sizeof(fl.content)) copyLen = sizeof(fl.content) - 1;
                        strncpy(fl.content, segPtr, copyLen);
                        fl.content[copyLen] = '\0';
                        displayLines.push_back(fl);

                        segPtr += breakPos;
                        while (*segPtr == ' ') segPtr++;
                    }
                    isFirstInSegment = false;
                    isFirstOverall = false;
                }
            }

            isFirstOverall = false;

            p = lineEnd;
            if (*p == '\r' && *(p + 1) == '\n') {
                p += 2;
            } else if (*p == '\n' || *p == '\r') {
                p++;
            }
        }
    }

    int visibleRows = 20; // rows 4 to 23
    int startLineIdx = 0;
    if (displayLines.size() > (size_t)visibleRows) {
        startLineIdx = displayLines.size() - visibleRows;
    }

    for (int r = 0; r < visibleRows; r++) {
        moveCursor(4 + r, 29);
        int currentLine = startLineIdx + r;
        if (currentLine < (int)displayLines.size()) {
            const auto &dl = displayLines[currentLine];
            if (!dl.isContinuation) {
                if (dl.isIncoming) {
                    terminal.write(ANSI_FG_BRIGHT_CYAN);
                } else {
                    terminal.write(ANSI_FG_BRIGHT_GREEN);
                }
                terminal.write(dl.senderPrefix);
                terminal.write(ANSI_RESET ANSI_FG_BRIGHT_WHITE);
            } else {
                terminal.write("  " ANSI_FG_BRIGHT_WHITE);
            }

            const char *ackPos = strstr(dl.content, " [ACK]");
            if (ackPos) {
                char msgPart[52];
                size_t textPartLen = ackPos - dl.content;
                if (textPartLen >= sizeof(msgPart)) textPartLen = sizeof(msgPart) - 1;
                strncpy(msgPart, dl.content, textPartLen);
                msgPart[textPartLen] = '\0';
                terminal.write(msgPart);
                terminal.write(ANSI_FG_BRIGHT_YELLOW " [ACK]" ANSI_RESET);
            } else {
                terminal.write(dl.content);
            }

            terminal.write(ANSI_RESET "\x1B[K");
        } else {
            terminal.write("\x1B[K");
        }
    }
}

void TerminalUI::drawInputBar() {
    // Row 25: Target info line OR Notification Banner
    moveCursor(25, 1);
    char targetLine[80];

    if (notification.active) {
        // High-contrast yellow notification banner
        terminal.write(ANSI_BG_YELLOW ANSI_FG_BLACK ANSI_BOLD);
        if (notification.isDirect) {
            snprintf(targetLine, sizeof(targetLine), " 🔔 [NEW DM from @%s]: \"%s\" -> Press [F6] to Jump",
                     notification.fromName[0] ? notification.fromName : "Anon",
                     notification.previewText);
        } else {
            ChannelData notifCh = client.getChannel(notification.channel);
            snprintf(targetLine, sizeof(targetLine), " 🔔 [NEW MSG on #%s from <%s>]: \"%s\" -> Press [F6] to Jump",
                     notifCh.name[0] ? notifCh.name : "Chan",
                     notification.fromName[0] ? notification.fromName : "Anon",
                     notification.previewText);
        }
    } else {
        terminal.write(ANSI_FG_BRIGHT_YELLOW ANSI_BOLD);
        if (dmMode) {
            NodeData target;
            nodeStore.getNodeByIndex(selectedNodeIdx, target);
            snprintf(targetLine, sizeof(targetLine), " [TARGET: DIRECT MESSAGE -> @%s] (Press [F4] to Broadcast)",
                     target.short_name[0] ? target.short_name : target.id_str);
        } else {
            ChannelData ch = client.getChannel(selectedChannel);
            snprintf(targetLine, sizeof(targetLine), " [TARGET: CHANNEL #%s (Broadcast)] (Press [F4] to DM / [F2] Channel)",
                     ch.name[0] ? ch.name : "Primary");
        }
    }

    size_t tl = strlen(targetLine);
    while (tl < 78) targetLine[tl++] = ' ';
    targetLine[78] = '\0';
    terminal.write(targetLine);
    terminal.write(ANSI_RESET);

    // Row 26: Input Box with In-Line Length Counter & Block Cursor
    moveCursor(26, 1);
    terminal.write(ANSI_BG_DARK_GRAY ANSI_FG_BRIGHT_GREEN ANSI_BOLD " > Text: " ANSI_RESET);

    const int VIEW_WIDTH = 55;
    size_t textLen = strlen(currentInputText);
    
    // Calculate scroll offset to keep cursor in view
    int scrollOffset = 0;
    if ((int)inputCursorPos >= VIEW_WIDTH - 2) {
        scrollOffset = (int)inputCursorPos - (VIEW_WIDTH - 10);
    }
    if (scrollOffset + VIEW_WIDTH > (int)textLen + 2 && (int)textLen >= VIEW_WIDTH) {
        scrollOffset = (int)textLen - VIEW_WIDTH + 2;
    }
    if (scrollOffset < 0) scrollOffset = 0;

    terminal.write(ANSI_BG_BLACK ANSI_FG_BRIGHT_WHITE);

    for (int col = 0; col < VIEW_WIDTH; col++) {
        int charIdx = scrollOffset + col;
        bool isCursor = (charIdx == (int)inputCursorPos);

        char c = ' ';
        if (charIdx < (int)textLen) {
            c = currentInputText[charIdx];
        }

        if (col == 0 && scrollOffset > 0 && !isCursor) {
            terminal.write(ANSI_FG_BRIGHT_YELLOW "<" ANSI_FG_BRIGHT_WHITE);
        } else if (col == VIEW_WIDTH - 1 && scrollOffset + VIEW_WIDTH < (int)textLen && !isCursor) {
            terminal.write(ANSI_FG_BRIGHT_YELLOW ">" ANSI_FG_BRIGHT_WHITE);
        } else if (isCursor) {
            // High-visibility inverse block cursor
            terminal.write(ANSI_BG_WHITE ANSI_FG_BLACK ANSI_BOLD);
            if (c == ' ') {
                terminal.write("_");
            } else {
                char s[2] = { c, '\0' };
                terminal.write(s);
            }
            terminal.write(ANSI_RESET ANSI_BG_BLACK ANSI_FG_BRIGHT_WHITE);
        } else {
            char s[2] = { c, '\0' };
            terminal.write(s);
        }
    }
    terminal.write(ANSI_RESET);

    // In-Line Length & Limit indicator on right side of Row 26
    char lenBuf[16];
    snprintf(lenBuf, sizeof(lenBuf), " [%3u/%3u] ", (unsigned int)textLen, (unsigned int)MAX_TEXT_PAYLOAD_LEN);
    terminal.write(ANSI_BG_DARK_GRAY ANSI_FG_BRIGHT_CYAN ANSI_BOLD);
    terminal.write(lenBuf);
    terminal.write(ANSI_RESET "   ");
}

void TerminalUI::drawFooter() {
    // Row 27: Action / Shortcut Keys Bar (Strictly 78 characters)
    moveCursor(27, 1);
    terminal.write(ANSI_BG_DARK_GRAY ANSI_FG_BRIGHT_WHITE ANSI_BOLD);
    char footerBuf[80];
    if (notification.active) {
        snprintf(footerBuf, sizeof(footerBuf), " [F1]Info [F2]Chan [F3]Chat [F4]DM [F5]Logs [F6]JUMP [Tab]Focus [Enter]Send");
    } else {
        snprintf(footerBuf, sizeof(footerBuf), " [F1]Info [F2]Chan [F3]Chat [F4]DM [F5]Logs [Tab]Focus [Enter]Send");
    }
    size_t flen = strlen(footerBuf);
    while (flen < 78) footerBuf[flen++] = ' ';
    footerBuf[78] = '\0';
    terminal.write(footerBuf);
    terminal.write(ANSI_RESET);

    // Row 28: Persistent System, PSRAM & Heap Status Bar (Strictly 78 characters)
    moveCursor(28, 1);
    terminal.write(ANSI_BG_BLACK ANSI_FG_BRIGHT_CYAN);
    char statBuf[80];
    LocalRadioInfo radio = client.getRadioInfo();
    uint32_t freeHeap = ESP.getFreeHeap();

    bool hasPsram = psramFound();
    char psramStr[18] = "";
    if (hasPsram) {
        snprintf(psramStr, sizeof(psramStr), " | PSRAM:%uMB", (unsigned int)(ESP.getFreePsram() / (1024 * 1024)));
    }

    if (radio.radio_free_heap > 0) {
        snprintf(statBuf, sizeof(statBuf), " ESP32:%uKB%s | Radio:%s | Node Heap:%u B (Min:%u B)",
                 (unsigned int)(freeHeap / 1024),
                 psramStr,
                 client.isSynced() ? "SYNCED" : "SYNCING",
                 (unsigned int)radio.radio_free_heap,
                 (unsigned int)radio.radio_min_heap);
    } else {
        snprintf(statBuf, sizeof(statBuf), " ESP32:%uKB%s | Radio:%s (%s) | Logs:%u",
                 (unsigned int)(freeHeap / 1024),
                 psramStr,
                 client.isSynced() ? "SYNCED" : "SYNCING",
                 radio.fw_version[0] ? radio.fw_version : "Meshtastic",
                 (unsigned int)client.getLogCount());
    }
    size_t slen = strlen(statBuf);
    while (slen < 78) {
        statBuf[slen++] = ' ';
    }
    statBuf[78] = '\0';
    terminal.write(statBuf);
    terminal.write(ANSI_RESET);

    // Park cursor safely at (1, 1)
    moveCursor(1, 1);
}

void TerminalUI::scrollLogUp(int lines) {
    size_t total = client.getLogCount();
    int maxOffset = (int)total - 25;
    if (maxOffset < 0) maxOffset = 0;
    logScrollOffset += lines;
    if (logScrollOffset > maxOffset) logScrollOffset = maxOffset;
    markDirty();
}

void TerminalUI::scrollLogDown(int lines) {
    logScrollOffset -= lines;
    if (logScrollOffset < 0) logScrollOffset = 0;
    markDirty();
}

void TerminalUI::scrollLogToTop() {
    size_t total = client.getLogCount();
    int maxOffset = (int)total - 25;
    logScrollOffset = (maxOffset > 0) ? maxOffset : 0;
    markDirty();
}

void TerminalUI::scrollLogToBottom() {
    logScrollOffset = 0;
    markDirty();
}

void TerminalUI::scrollLogLeft(int cols) {
    logHorizOffset -= cols;
    if (logHorizOffset < 0) logHorizOffset = 0;
    markDirty();
}

void TerminalUI::scrollLogRight(int cols) {
    logHorizOffset += cols;
    if (logHorizOffset > 80) logHorizOffset = 80;
    markDirty();
}

void TerminalUI::resetLogScroll() {
    logScrollOffset = 0;
    logHorizOffset = 0;
    markDirty();
}

void TerminalUI::drawLogsView() {
    // Header (Row 1)
    moveCursor(1, 1);
    char headBuf[81];
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t minHeap = ESP.getMinFreeHeap();
    LocalRadioInfo radio = client.getRadioInfo();
    bool hasPsram = psramFound();

    if (radio.radio_free_heap > 0) {
        if (hasPsram) {
            snprintf(headBuf, sizeof(headBuf), " MESHTASTIC LOGS | ESP32: %uKB | PSRAM: %uMB | Node Heap: %u B ",
                     (unsigned int)(freeHeap / 1024),
                     (unsigned int)(ESP.getFreePsram() / (1024 * 1024)),
                     (unsigned int)radio.radio_free_heap);
        } else {
            snprintf(headBuf, sizeof(headBuf), " MESHTASTIC LOGS | ESP32: %uKB | Node Heap: %u B (Min: %u B) ",
                     (unsigned int)(freeHeap / 1024),
                     (unsigned int)radio.radio_free_heap,
                     (unsigned int)radio.radio_min_heap);
        }
    } else {
        if (hasPsram) {
            snprintf(headBuf, sizeof(headBuf), " MESHTASTIC LOGS | ESP32: %uKB Free | PSRAM: %uMB Free ",
                     (unsigned int)(freeHeap / 1024), (unsigned int)(ESP.getFreePsram() / (1024 * 1024)));
        } else {
            snprintf(headBuf, sizeof(headBuf), " MESHTASTIC LOGS | ESP32: %uKB Free (Min: %uKB) ",
                     (unsigned int)(freeHeap / 1024), (unsigned int)(minHeap / 1024));
        }
    }
    terminal.write(ANSI_BG_DARK_GRAY ANSI_FG_BRIGHT_GREEN ANSI_BOLD);
    terminal.write(headBuf);
    terminal.write("\x1B[K" ANSI_RESET);

    // Top border (Row 2)
    moveCursor(2, 1);
    terminal.write(ANSI_FG_BRIGHT_BLACK "+------------------------------------------------------------------------------+" ANSI_RESET);

    size_t totalLogs = client.getLogCount();
    int visibleRows = 23; // rows 3 to 25
    int startIndex = (int)totalLogs - visibleRows - logScrollOffset;
    if (startIndex < 0) startIndex = 0;

    // Viewport rows (Rows 3 to 25 = 23 lines)
    for (int r = 0; r < visibleRows; r++) {
        moveCursor(3 + r, 1);
        terminal.write(ANSI_FG_BRIGHT_BLACK "|" ANSI_RESET);

        size_t logIdx = startIndex + r;
        if (logIdx < totalLogs) {
            LogEntry entry;
            if (client.getLogEntry(logIdx, entry)) {
                uint32_t secs = entry.timestampMs / 1000;
                uint32_t m = (secs / 60) % 60;
                uint32_t s = secs % 60;

                bool isHeapLog = (strstr(entry.text, "heap") != nullptr || strstr(entry.tag, "heap") != nullptr || strstr(entry.tag, "HEAP") != nullptr);
                const char *tagColor = ANSI_FG_BRIGHT_GREEN;
                if (isHeapLog) tagColor = ANSI_FG_BRIGHT_YELLOW ANSI_BOLD;
                else if (strstr(entry.tag, "WARN") != nullptr) tagColor = ANSI_FG_BRIGHT_YELLOW;
                else if (strstr(entry.tag, "ERR") != nullptr || strstr(entry.tag, "FATAL") != nullptr) tagColor = ANSI_FG_BRIGHT_RED;
                else if (strstr(entry.tag, "DEBUG") != nullptr) tagColor = ANSI_FG_CYAN;
                else if (strstr(entry.tag, "SYNC") != nullptr) tagColor = ANSI_FG_BRIGHT_CYAN;
                else if (strstr(entry.tag, "PROTO") != nullptr) tagColor = ANSI_FG_BRIGHT_MAGENTA;
                else if (strstr(entry.tag, "INFO") != nullptr) tagColor = ANSI_FG_BRIGHT_WHITE;

                char fullLine[160];
                snprintf(fullLine, sizeof(fullLine), " %02u:%02u [%s] %s",
                         (unsigned int)m, (unsigned int)s, entry.tag, entry.text);

                size_t fullLen = strlen(fullLine);
                char visibleBuf[78];
                size_t copyStart = (logHorizOffset < (int)fullLen) ? logHorizOffset : fullLen;
                size_t copyLen = fullLen - copyStart;
                if (copyLen > 76) copyLen = 76;

                memcpy(visibleBuf, &fullLine[copyStart], copyLen);
                visibleBuf[copyLen] = '\0';

                terminal.write(tagColor);
                terminal.write(visibleBuf);
                size_t written = copyLen;
                while (written < 76) {
                    terminal.write(' ');
                    written++;
                }
            } else {
                terminal.write("                                                                            ");
            }
        } else {
            terminal.write("                                                                            ");
        }

        terminal.write(ANSI_FG_BRIGHT_BLACK "|" ANSI_RESET "\x1B[K");
    }

    // Bottom border (Row 26)
    moveCursor(26, 1);
    terminal.write(ANSI_FG_BRIGHT_BLACK "+------------------------------------------------------------------------------+" ANSI_RESET);

    // Help bar (Row 27)
    moveCursor(27, 1);
    terminal.write(ANSI_BG_DARK_GRAY ANSI_FG_BRIGHT_WHITE);
    char footBuf[80];
    if (logHorizOffset > 0) {
        snprintf(footBuf, sizeof(footBuf), " [F5]/[ESC] Exit  [Up]/[Dn] Scroll  [Left]/[Right] Horiz (+%d)  [End] Reset", logHorizOffset);
    } else {
        snprintf(footBuf, sizeof(footBuf), " [F5]/[ESC] Exit  [Up]/[Dn] Scroll  [Left]/[Right] Horiz Pan  [End] Latest");
    }
    size_t fblen = strlen(footBuf);
    while (fblen < 78) footBuf[fblen++] = ' ';
    footBuf[78] = '\0';
    terminal.write(footBuf);
    terminal.write(ANSI_RESET);

    // Status bar (Row 28)
    moveCursor(28, 1);
    terminal.write(ANSI_BG_BLACK ANSI_FG_BRIGHT_CYAN);
    char statBuf[80];
    if (radio.radio_free_heap > 0) {
        snprintf(statBuf, sizeof(statBuf), " Lines: %u | Offset: V:%d H:+%d | Radio Heap: %u B (Min: %u B)",
                 (unsigned int)totalLogs,
                 logScrollOffset,
                 logHorizOffset,
                 (unsigned int)radio.radio_free_heap,
                 (unsigned int)radio.radio_min_heap);
    } else {
        snprintf(statBuf, sizeof(statBuf), " Lines: %u | Offset: V:%d H:+%d | Radio: %s",
                 (unsigned int)totalLogs,
                 logScrollOffset,
                 logHorizOffset,
                 client.isSynced() ? "SYNCED" : "SYNCING");
    }
    size_t lslen = strlen(statBuf);
    while (lslen < 78) {
        statBuf[lslen++] = ' ';
    }
    statBuf[78] = '\0';
    terminal.write(statBuf);
    terminal.write(ANSI_RESET);
    moveCursor(1, 1);
}
