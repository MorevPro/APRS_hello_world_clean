#pragma once

#include "aprs_ax25_encoder.h"
#include "aprs_config.h"

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <lib/toolbox/level_duration.h>
#include <notification/notification.h>
#include <stdbool.h>
#include <stdint.h>

#define APRS_HELLO_WORLD_FRAME_BUFFER_SIZE 256U
#define APRS_HELLO_WORLD_TX_LEVEL_CAPACITY 2048U
#define APRS_HELLO_WORLD_TX_WAVEFORM_CAPACITY 6144U

#define APRS_CFG_CALL_LEN        7U
#define APRS_CFG_STATUS_MAX      62U
#define APRS_CFG_POS_STR_LEN     16U
/** Max interval between consecutive short-clicks on Up to count as one gesture (ms). */
#define APRS_TRIPLE_UP_WINDOW_MS 900U
#define APRS_SETTINGS_MENU_ITEMS 8U

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

typedef enum {
    AprsUiViewMain,
    AprsUiViewSettingsMenu,
    AprsUiViewSettingsMycall,
    AprsUiViewSettingsSsid,
    AprsUiViewSettingsPosition,
    AprsUiViewSettingsBearing,
    AprsUiViewSettingsSpeed,
    AprsUiViewSettingsStatusText,
    AprsUiViewSettingsPath1,
    AprsUiViewSettingsPath2,
} AprsUiView;

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

    AprsUiView ui_view;
    uint8_t settings_menu_index;
    uint8_t settings_menu_scroll;
    uint32_t triple_up_last_tick;
    uint8_t triple_up_count;
    uint32_t ui_toast_until;
    char ui_toast[28];

    char cfg_source_call[APRS_CFG_CALL_LEN];
    uint8_t cfg_source_ssid;
    char cfg_dest_call[APRS_CFG_CALL_LEN];
    uint8_t cfg_dest_ssid;
    char cfg_path1_call[APRS_CFG_CALL_LEN];
    uint8_t cfg_path1_ssid;
    bool cfg_use_path1;
    bool cfg_use_path2;
    char cfg_path2_call[APRS_CFG_CALL_LEN];
    uint8_t cfg_path2_ssid;

    char cfg_lat[APRS_CFG_POS_STR_LEN];
    char cfg_lon[APRS_CFG_POS_STR_LEN];
    uint8_t pos_edit_line;
    uint8_t pos_edit_cursor;

    uint16_t cfg_bearing_deg;
    uint16_t cfg_speed;

    char cfg_status_text[APRS_CFG_STATUS_MAX + 1U];
    uint8_t status_edit_cursor;

    uint8_t mycall_edit_cursor;
    uint8_t path1_edit_cursor;
    uint8_t path2_edit_cursor;
    bool path1_edit_ssid;
    uint8_t path2_focus;

    AprsAx25AddressConfig address_config;

    uint32_t tx_packets;
    uint32_t tx_bit_count;
    uint32_t tx_waveform_duration_us;

    size_t frame_size;
    size_t tx_level_count;
    size_t tx_level_index;
    size_t tx_waveform_count;
    size_t tx_waveform_index;

    uint8_t frame_buffer[APRS_HELLO_WORLD_FRAME_BUFFER_SIZE];
    bool tx_levels[APRS_HELLO_WORLD_TX_LEVEL_CAPACITY];
    LevelDuration tx_waveform[APRS_HELLO_WORLD_TX_WAVEFORM_CAPACITY];

    char status_text[96];
    char last_tx_hex[512];
    char last_status[48];
    char region_name[8];
} APRSHelloWorldCleanApp;
