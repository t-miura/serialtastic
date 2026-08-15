#include "MessageStore.h"

MessageStore::MessageStore() {
    mutex = xSemaphoreCreateMutex();
}

MessageStore::~MessageStore() {
    if (mutex) {
        vSemaphoreDelete(mutex);
    }
}

void MessageStore::addMessage(const ChatMessage &msg) {
    if (!mutex) return;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Prevent exact duplicate packet IDs
        if (msg.id != 0) {
            for (auto &m : messages) {
                if (m.id == msg.id) {
                    xSemaphoreGive(mutex);
                    return;
                }
            }
        }
        if (messages.size() >= MAX_MESSAGES_IN_STORE) {
            messages.pop_front();
        }
        messages.push_back(msg);
        xSemaphoreGive(mutex);
    }
}

void MessageStore::markAcknowledged(uint32_t msgId) {
    if (!mutex || msgId == 0) return;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (auto &msg : messages) {
            if (msg.id == msgId) {
                msg.is_acknowledged = true;
                break;
            }
        }
        xSemaphoreGive(mutex);
    }
}

size_t MessageStore::getMessageCount() {
    if (!mutex) return 0;
    size_t count = 0;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        count = messages.size();
        xSemaphoreGive(mutex);
    }
    return count;
}

bool MessageStore::getMessageByIndex(size_t index, ChatMessage &outMsg) {
    if (!mutex) return false;
    bool found = false;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (index < messages.size()) {
            outMsg = messages[index];
            found = true;
        }
        xSemaphoreGive(mutex);
    }
    return found;
}

void MessageStore::getChannelMessages(uint8_t channel, std::vector<ChatMessage> &outMsgs, size_t maxCount) {
    outMsgs.clear();
    if (!mutex) return;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (auto it = messages.rbegin(); it != messages.rend() && outMsgs.size() < maxCount; ++it) {
            if (!it->is_direct && it->channel == channel) {
                outMsgs.insert(outMsgs.begin(), *it);
            }
        }
        xSemaphoreGive(mutex);
    }
}

void MessageStore::getDMMessages(uint32_t nodeNum, std::vector<ChatMessage> &outMsgs, size_t maxCount) {
    outMsgs.clear();
    if (!mutex) return;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (auto it = messages.rbegin(); it != messages.rend() && outMsgs.size() < maxCount; ++it) {
            if (it->is_direct && (it->from_node == nodeNum || it->to_node == nodeNum)) {
                outMsgs.insert(outMsgs.begin(), *it);
            }
        }
        xSemaphoreGive(mutex);
    }
}

void MessageStore::getAllRecentMessages(std::vector<ChatMessage> &outMsgs, size_t maxCount) {
    outMsgs.clear();
    if (!mutex) return;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (auto it = messages.rbegin(); it != messages.rend() && outMsgs.size() < maxCount; ++it) {
            outMsgs.insert(outMsgs.begin(), *it);
        }
        xSemaphoreGive(mutex);
    }
}

void MessageStore::clear() {
    if (!mutex) return;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        messages.clear();
        xSemaphoreGive(mutex);
    }
}
