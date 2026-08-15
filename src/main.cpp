#include <Arduino.h>
#include "config.h"
#include "model/NodeStore.h"
#include "model/MessageStore.h"
#include "protocol/MeshtasticClient.h"
#include "ui/TerminalUI.h"
#include "ui/InputHandler.h"

// Hardware Serial port for Meshtastic radio (TX=13, RX=14)
HardwareSerial MeshSerial(MESH_SERIAL_PORT_NUM);

// Global Model & Controllers
NodeStore nodeStore;
MessageStore messageStore;
MeshtasticClient meshtasticClient(nodeStore, messageStore);

fabgl::PS2Controller ps2Controller;
TerminalUI terminalUI(nodeStore, messageStore, meshtasticClient);
InputHandler inputHandler(ps2Controller, terminalUI, meshtasticClient);

// FreeRTOS Comms Task on Core 0
TaskHandle_t commsTaskHandle = nullptr;

void commsTask(void *param) {
    while (true) {
        meshtasticClient.update();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void setup() {
    // USB Debug Serial
    Serial.begin(115200);
    delay(200);
    Serial.println("\n\n========================================");
    Serial.println("   SERIALTASTIC - Meshtastic FabGL VGA   ");
    Serial.println("========================================");

    // Disable Watchdogs that might interfere with VGA generation
    disableCore0WDT();
    disableCore1WDT();

    // Start UART to Meshtastic Radio
    Serial.printf("Initializing UART%d on TX=%d, RX=%d @ %d baud...\n",
                  MESH_SERIAL_PORT_NUM, MESH_SERIAL_TX_PIN, MESH_SERIAL_RX_PIN, MESH_SERIAL_BAUD);
    MeshSerial.begin(MESH_SERIAL_BAUD, SERIAL_8N1, MESH_SERIAL_RX_PIN, MESH_SERIAL_TX_PIN);

    // Initialize Meshtastic Protocol Client
    meshtasticClient.begin(&MeshSerial);
    meshtasticClient.setMessageReceivedCallback([](const ChatMessage &msg) {
        terminalUI.notifyIncomingMessage(msg);
    });

    // Initialize FabGL VGA Display Controller
    Serial.println("Initializing FabGL VGA Controller...");
    terminalUI.begin();

    // Initialize FabGL PS/2 Keyboard Controller
    Serial.println("Initializing PS/2 Keyboard Controller...");
    inputHandler.begin();

    // Create FreeRTOS Comms Task pinned to Core 0
    xTaskCreatePinnedToCore(
        commsTask,
        "CommsTask",
        4096,
        nullptr,
        2, // Priority
        &commsTaskHandle,
        0  // Core 0
    );

    Serial.println("Serialtastic started successfully!");
}

void loop() {
    // UI and Keyboard processing runs on Core 1
    inputHandler.update();
    terminalUI.render();
    vTaskDelay(pdMS_TO_TICKS(10));
}
