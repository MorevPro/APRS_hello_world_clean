#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>

#define APRS_HELLO_WORLD_FRAME_BUFFER_SIZE 256U
#define APRS_HELLO_WORLD_TX_LEVEL_CAPACITY 2048U

typedef enum {
    EventTypeTick,
    EventTypeInput,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} APRSHelloWorldCleanEvent;

typedef enum {
    AprsRadioModeRxMonitor,
    AprsRadioModeLabTx,
} AprsRadioMode;

typedef struct APRSHelloWorldCleanApp {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    FuriTimer* timer;
    NotificationApp* notifications;

    uint32_t frequency_hz;
    float last_rssi;
    uint8_t last_lqi;
    bool tx_allowed_in_region;

    volatile uint32_t rx_edge_count;
    volatile uint32_t last_edge_duration_us;
    volatile bool last_edge_level;

    bool radio_ready;
    bool rx_running;
    bool tx_running;
    bool tx_armed;
    bool tx_last_start_ok;

    AprsRadioMode mode;

    uint32_t tx_packets;
    uint32_t tx_bit_count;

    size_t frame_size;
    size_t tx_level_count;
    size_t tx_level_index;

    uint8_t frame_buffer[APRS_HELLO_WORLD_FRAME_BUFFER_SIZE];
    bool tx_levels[APRS_HELLO_WORLD_TX_LEVEL_CAPACITY];

    char status_text[96];
    char last_tx_hex[96];
    char last_status[48];
    char region_name[8];
} APRSHelloWorldCleanApp;
