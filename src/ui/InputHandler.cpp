#include "InputHandler.h"
#include <cstring>

InputHandler::InputHandler(fabgl::PS2Controller &ps2, TerminalUI &ui, MeshtasticClient &cli)
    : ps2Controller(ps2), terminalUI(ui), meshtasticClient(cli),
      inputLen(0), cursorPos(0), historyIndex(-1) {
    memset(inputBuffer, 0, sizeof(inputBuffer));
}

void InputHandler::begin() {
    ps2Controller.begin(fabgl::PS2Preset::KeyboardPort0);
    
    // Attach to FabGL Terminal keyboard handler
    terminalUI.getTerminal().onVirtualKeyItem = [this](fabgl::VirtualKeyItem *vkItem) {
        this->onVirtualKey(vkItem);
    };

    terminalUI.setInputBuffer(inputBuffer, cursorPos);
}

void InputHandler::update() {
    // If Terminal's internal queue doesn't pick up, fallback check directly
    auto keyboard = ps2Controller.keyboard();
    if (!keyboard || !keyboard->isKeyboardAvailable()) return;

    while (keyboard->virtualKeyAvailable()) {
        bool down;
        auto vk = keyboard->getNextVirtualKey(&down);
        if (down) {
            fabgl::VirtualKeyItem item;
            item.vk = vk;
            item.down = 1;
            item.ASCII = keyboard->virtualKeyToASCII(vk);
            item.CTRL = keyboard->isVKDown(fabgl::VK_LCTRL) || keyboard->isVKDown(fabgl::VK_RCTRL);
            item.LALT = keyboard->isVKDown(fabgl::VK_LALT);
            item.RALT = keyboard->isVKDown(fabgl::VK_RALT);
            item.SHIFT = keyboard->isVKDown(fabgl::VK_LSHIFT) || keyboard->isVKDown(fabgl::VK_RSHIFT);
            
            onVirtualKey(&item);
        }
    }
}

void InputHandler::onVirtualKey(fabgl::VirtualKeyItem *vkItem) {
    if (!vkItem || !vkItem->down) return;
    fabgl::VirtualKey vk = vkItem->vk;

    switch (vk) {
        case fabgl::VK_F1:
            terminalUI.toggleNodeDetails();
            break;

        case fabgl::VK_F2:
            if (!terminalUI.isNodeDetailsVisible()) {
                terminalUI.selectNextChannel();
            }
            break;

        case fabgl::VK_F3:
            if (terminalUI.isNodeDetailsVisible()) {
                terminalUI.toggleNodeDetails();
            }
            terminalUI.setPane(ActivePane::MESSAGES);
            break;

        case fabgl::VK_F4:
            if (terminalUI.isNodeDetailsVisible()) {
                terminalUI.toggleNodeDetails();
            }
            terminalUI.toggleDMMode();
            break;

        case fabgl::VK_F5:
            if (terminalUI.isNodeDetailsVisible()) {
                terminalUI.toggleNodeDetails();
            }
            if (terminalUI.getPane() == ActivePane::LOGS) {
                terminalUI.setPane(ActivePane::MESSAGES);
            } else {
                terminalUI.setPane(ActivePane::LOGS);
            }
            break;

        case fabgl::VK_F6:
            terminalUI.jumpToNotification();
            break;

        case fabgl::VK_TAB:
            if (!terminalUI.isNodeDetailsVisible()) {
                terminalUI.togglePane();
            }
            break;

        case fabgl::VK_ESCAPE:
            if (terminalUI.isNodeDetailsVisible()) {
                terminalUI.toggleNodeDetails();
            } else if (terminalUI.hasActiveNotification()) {
                terminalUI.dismissNotification();
            } else if (terminalUI.getPane() == ActivePane::LOGS) {
                terminalUI.setPane(ActivePane::MESSAGES);
            }
            break;

        case fabgl::VK_UP:
            if (terminalUI.getPane() == ActivePane::LOGS) {
                terminalUI.scrollLogUp(1);
            } else if (terminalUI.isNodeDetailsVisible() || terminalUI.getPane() == ActivePane::NODES) {
                terminalUI.selectPrevNode();
            } else if (!commandHistory.empty()) {
                if (historyIndex < (int)commandHistory.size() - 1) {
                    historyIndex++;
                    strncpy(inputBuffer, commandHistory[commandHistory.size() - 1 - historyIndex].c_str(), sizeof(inputBuffer) - 1);
                    inputLen = strlen(inputBuffer);
                    cursorPos = inputLen;
                    terminalUI.setInputBuffer(inputBuffer, cursorPos);
                }
            }
            break;

        case fabgl::VK_DOWN:
            if (terminalUI.getPane() == ActivePane::LOGS) {
                terminalUI.scrollLogDown(1);
            } else if (terminalUI.isNodeDetailsVisible() || terminalUI.getPane() == ActivePane::NODES) {
                terminalUI.selectNextNode();
            } else if (historyIndex > 0) {
                historyIndex--;
                strncpy(inputBuffer, commandHistory[commandHistory.size() - 1 - historyIndex].c_str(), sizeof(inputBuffer) - 1);
                inputLen = strlen(inputBuffer);
                cursorPos = inputLen;
                terminalUI.setInputBuffer(inputBuffer, cursorPos);
            } else if (historyIndex == 0) {
                historyIndex = -1;
                inputBuffer[0] = '\0';
                inputLen = 0;
                cursorPos = 0;
                terminalUI.setInputBuffer(inputBuffer, cursorPos);
            }
            break;

        case fabgl::VK_PAGEUP:
            if (terminalUI.getPane() == ActivePane::LOGS) {
                terminalUI.scrollLogUp(20);
            }
            break;

        case fabgl::VK_PAGEDOWN:
            if (terminalUI.getPane() == ActivePane::LOGS) {
                terminalUI.scrollLogDown(20);
            }
            break;

        case fabgl::VK_LEFT:
            if (terminalUI.getPane() == ActivePane::LOGS) {
                terminalUI.scrollLogLeft(8);
            } else if (terminalUI.isNodeDetailsVisible()) {
                terminalUI.selectPrevNode();
            } else if (cursorPos > 0) {
                cursorPos--;
                terminalUI.setInputBuffer(inputBuffer, cursorPos);
            }
            break;

        case fabgl::VK_RIGHT:
            if (terminalUI.getPane() == ActivePane::LOGS) {
                terminalUI.scrollLogRight(8);
            } else if (terminalUI.isNodeDetailsVisible()) {
                terminalUI.selectNextNode();
            } else if (cursorPos < inputLen) {
                cursorPos++;
                terminalUI.setInputBuffer(inputBuffer, cursorPos);
            }
            break;

        case fabgl::VK_HOME:
            if (terminalUI.getPane() == ActivePane::LOGS) {
                terminalUI.scrollLogToTop();
            } else if (!terminalUI.isNodeDetailsVisible()) {
                cursorPos = 0;
                terminalUI.setInputBuffer(inputBuffer, cursorPos);
            }
            break;

        case fabgl::VK_END:
            if (terminalUI.getPane() == ActivePane::LOGS) {
                terminalUI.resetLogScroll();
            } else if (!terminalUI.isNodeDetailsVisible()) {
                cursorPos = inputLen;
                terminalUI.setInputBuffer(inputBuffer, cursorPos);
            }
            break;

        case fabgl::VK_RETURN:
        case fabgl::VK_KP_ENTER:
            if (terminalUI.getPane() == ActivePane::LOGS) {
                terminalUI.setPane(ActivePane::MESSAGES);
            } else if (terminalUI.isNodeDetailsVisible()) {
                if (terminalUI.hasActiveNotification()) {
                    terminalUI.jumpToNotification();
                } else {
                    terminalUI.toggleNodeDetails();
                }
            } else if (terminalUI.getPane() == ActivePane::NODES) {
                // Pressing Enter on a selected node starts DM and focuses chat
                if (!terminalUI.isDMMode()) {
                    terminalUI.toggleDMMode();
                } else {
                    terminalUI.setPane(ActivePane::MESSAGES);
                }
            } else {
                sendMessage();
            }
            break;

        case fabgl::VK_BACKSPACE:
            if (terminalUI.getPane() != ActivePane::LOGS && !terminalUI.isNodeDetailsVisible() && cursorPos > 0) {
                memmove(&inputBuffer[cursorPos - 1], &inputBuffer[cursorPos], inputLen - cursorPos + 1);
                cursorPos--;
                inputLen--;
                terminalUI.setInputBuffer(inputBuffer, cursorPos);
            }
            break;

        case fabgl::VK_DELETE:
            if (terminalUI.getPane() != ActivePane::LOGS && !terminalUI.isNodeDetailsVisible() && cursorPos < inputLen) {
                memmove(&inputBuffer[cursorPos], &inputBuffer[cursorPos + 1], inputLen - cursorPos);
                inputLen--;
                terminalUI.setInputBuffer(inputBuffer, cursorPos);
            }
            break;

        default: {
            uint8_t ch = vkItem->ASCII;
            if (terminalUI.getPane() != ActivePane::LOGS && !terminalUI.isNodeDetailsVisible() && ch >= 32 && ch <= 126 && inputLen < (sizeof(inputBuffer) - 2)) {
                memmove(&inputBuffer[cursorPos + 1], &inputBuffer[cursorPos], inputLen - cursorPos + 1);
                inputBuffer[cursorPos] = (char)ch;
                cursorPos++;
                inputLen++;
                terminalUI.setInputBuffer(inputBuffer, cursorPos);
            }
            break;
        }
    }
}

void InputHandler::sendMessage() {
    // Strictly do not send if input buffer is empty
    if (inputLen == 0 || inputBuffer[0] == '\0') {
        return;
    }

    uint32_t targetNode = terminalUI.getTargetNodeNum();
    uint8_t channel = terminalUI.getSelectedChannel();

    meshtasticClient.sendTextMessage(inputBuffer, targetNode, channel, true);

    // Save to command history
    commandHistory.push_back(std::string(inputBuffer));
    if (commandHistory.size() > 20) {
        commandHistory.erase(commandHistory.begin());
    }
    historyIndex = -1;

    // Reset buffer completely
    inputBuffer[0] = '\0';
    inputLen = 0;
    cursorPos = 0;
    terminalUI.setInputBuffer(inputBuffer, cursorPos);
}
