#pragma once

#include <Arduino.h>
#include "fabgl.h"
#include "TerminalUI.h"
#include "protocol/MeshtasticClient.h"
#include <vector>
#include <string>

class InputHandler {
public:
    InputHandler(fabgl::PS2Controller &ps2, TerminalUI &ui, MeshtasticClient &client);

    void begin();
    void update();
    void onVirtualKey(fabgl::VirtualKeyItem *vkItem);

private:
    fabgl::PS2Controller &ps2Controller;
    TerminalUI &terminalUI;
    MeshtasticClient &meshtasticClient;

    char inputBuffer[MAX_TEXT_PAYLOAD_LEN + 1];
    size_t inputLen;
    size_t cursorPos;

    std::vector<std::string> commandHistory;
    int historyIndex;

    void sendMessage();
};
