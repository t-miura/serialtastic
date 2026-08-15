#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>
#include <deque>
#include "Types.h"
#include "config.h"

class MessageStore {
public:
    MessageStore();
    ~MessageStore();

    void addMessage(const ChatMessage &msg);
    void markAcknowledged(uint32_t msgId);
    size_t getMessageCount();
    bool getMessageByIndex(size_t index, ChatMessage &outMsg);
    
    // Filtered retrieves
    void getChannelMessages(uint8_t channel, std::vector<ChatMessage> &outMsgs, size_t maxCount = 50);
    void getDMMessages(uint32_t nodeNum, std::vector<ChatMessage> &outMsgs, size_t maxCount = 50);
    void getAllRecentMessages(std::vector<ChatMessage> &outMsgs, size_t maxCount = 50);
    
    void clear();

private:
    std::deque<ChatMessage> messages;
    SemaphoreHandle_t mutex;
};
