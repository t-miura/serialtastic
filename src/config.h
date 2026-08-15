#pragma once

#include <Arduino.h>

// Serial link to Meshtastic Node
#define MESH_SERIAL_TX_PIN      13
#define MESH_SERIAL_RX_PIN      14
#define MESH_SERIAL_BAUD        115200
#define MESH_SERIAL_PORT_NUM    2  // UART2

// Keepalive & Timing
#define MESH_HEARTBEAT_INTERVAL_MS  10000
#define MESH_SYNC_TIMEOUT_MS        15000

// Screen / VGA Resolution Defaults
#define VGA_SCREEN_COLUMNS      80
#define VGA_SCREEN_ROWS         30

// Storage Limits
#define MAX_NODES_IN_STORE      100
#define MAX_MESSAGES_IN_STORE   150
#define MAX_CHANNELS            8

// Message payload size
#define MAX_TEXT_PAYLOAD_LEN    200
