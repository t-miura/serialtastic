#pragma once

#include <Arduino.h>
#include "fabgl.h"
#include "model/Types.h"
#include "model/NodeStore.h"
#include "model/MessageStore.h"
#include "protocol/MeshtasticClient.h"

class TerminalUI {
public:
    TerminalUI(NodeStore &nStore, MessageStore &mStore, MeshtasticClient &client);

    void begin();
    void render();
    void markDirty();

    // Navigation and actions
    void selectNextNode();
    void selectPrevNode();
    void selectNextChannel();
    void selectPrevChannel();
    void togglePane();
    void setPane(ActivePane pane);
    ActivePane getPane() const { return currentPane; }

    void toggleDMMode();
    bool isDMMode() const { return dmMode; }
    uint32_t getTargetNodeNum() const;

    uint8_t getSelectedChannel() const { return selectedChannel; }
    void setSelectedChannel(uint8_t ch) { selectedChannel = ch; markDirty(); }

    void setInputBuffer(const char *buf, size_t cursor);
    void toggleNodeDetails();
    bool isNodeDetailsVisible() const { return showNodeDetails; }

    // Notifications
    void notifyIncomingMessage(const ChatMessage &msg);
    void jumpToNotification();
    void dismissNotification();
    bool hasActiveNotification() const { return notification.active; }

    // Log scrolling
    void scrollLogUp(int lines = 1);
    void scrollLogDown(int lines = 1);
    void scrollLogToTop();
    void scrollLogToBottom();
    void scrollLogLeft(int cols = 8);
    void scrollLogRight(int cols = 8);
    void resetLogScroll();

    fabgl::Terminal& getTerminal() { return terminal; }
    fabgl::VGA16Controller& getController() { return vgaController; }

private:
    NodeStore &nodeStore;
    MessageStore &msgStore;
    MeshtasticClient &client;

    fabgl::VGA16Controller vgaController;
    fabgl::Terminal terminal;

    ActivePane currentPane;
    int selectedNodeIdx;
    uint8_t selectedChannel;
    bool dmMode;
    bool showNodeDetails;
    bool isDirty;
    uint32_t lastRenderTime;
    int logScrollOffset = 0;
    int logHorizOffset = 0;

    char currentInputText[MAX_TEXT_PAYLOAD_LEN + 1];
    size_t inputCursorPos;

    struct MessageNotification {
        bool active = false;
        uint32_t fromNode = 0;
        char fromName[32] = {0};
        uint8_t channel = 0;
        bool isDirect = false;
        char previewText[40] = {0};
        uint32_t timestamp = 0;
    } notification;

    void drawStatusBar();
    void drawDividers();
    void drawNodesPane();
    void drawMessagesPane();
    void drawInputBar();
    void drawFooter();
    void drawNodeDetailsPopup();
    void drawLogsView();

    void moveCursor(int row, int col);
};
