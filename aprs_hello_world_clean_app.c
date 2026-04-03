#include "aprs_hello_world_clean_app.h"

#include <furi.h>
#include <furi_hal_power.h>
#include <furi_hal_region.h>
#include <furi_hal_subghz.h>
#include <gui/elements.h>
#include <input/input.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <lib/toolbox/level_duration.h>
#include <notification/notification_messages.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define TAG "APRS432"
#define APRS_FREQUENCY_HZ          432500000UL
#define APRS_AFSK_TONE_MARK_HZ     1200U
#define APRS_AFSK_TONE_SPACE_HZ    2400U
#define APRS_AFSK_MARK_HALF_US     (1000000UL / (APRS_AFSK_TONE_MARK_HZ * 2UL))
#define APRS_AFSK_SPACE_HALF_US    (1000000UL / (APRS_AFSK_TONE_SPACE_HZ * 2UL))
#define APRS_AFSK_BIT_DURATION_US  833U
#define APRS_PREAMBLE_FLAGS        100U
#define APRS_POSTAMBLE_FLAGS       3U
#define APP_TICK_PERIOD_MS         200U

// Forward declarations
static void aprs_set_status(APRSHelloWorldCleanApp* app, const char* text);

static bool aprs_waveform_emit_level(
    APRSHelloWorldCleanApp* app,
    bool level,
    uint32_t duration_us) {
    if(app->tx_waveform_count >= APRS_HELLO_WORLD_TX_WAVEFORM_CAPACITY) {
        return false;
    }

    app->tx_waveform[app->tx_waveform_count++] = level_duration_make(level, duration_us);
    app->tx_waveform_duration_us += duration_us;
    return true;
}

static bool aprs_waveform_emit_tone(APRSHelloWorldCleanApp* app, bool mark_tone) {
    const uint32_t half_period_us = mark_tone ? APRS_AFSK_MARK_HALF_US : APRS_AFSK_SPACE_HALF_US;
    const uint8_t half_cycles = mark_tone ? 2U : 4U;

    for(uint8_t i = 0; i < half_cycles; i++) {
        app->last_edge_level = !app->last_edge_level;
        if(!aprs_waveform_emit_level(app, app->last_edge_level, half_period_us)) {
            return false;
        }
    }

    return true;
}

static bool aprs_build_afsk_waveform(APRSHelloWorldCleanApp* app) {
    if(!app) {
        return false;
    }

    app->tx_waveform_count = 0U;
    app->tx_waveform_index = 0U;
    app->tx_waveform_duration_us = 0U;
    app->last_edge_level = false;

    for(size_t i = 0; i < app->tx_level_count; i++) {
        const bool mark_tone = app->tx_levels[i];
        if(!aprs_waveform_emit_tone(app, mark_tone)) {
            return false;
        }
    }

    return app->tx_waveform_count > 0U;
}

static const NotificationSequence sequence_tx_start = {
    &message_red_255,
    NULL,
};

static const NotificationSequence sequence_tx_stop = {
    &message_red_0,
    NULL,
};

static const NotificationSequence sequence_rx_activity = {
    &message_blue_255,
    &message_delay_50,
    &message_blue_0,
    NULL,
};

static const NotificationSequence sequence_tx_error = {
    &message_red_255,
    &message_delay_50,
    &message_red_0,
    NULL,
};

static void aprs_sync_address_config(APRSHelloWorldCleanApp* app) {
    furi_assert(app);
    app->address_config.source_call = app->cfg_source_call;
    app->address_config.source_ssid = app->cfg_source_ssid;
    app->address_config.destination_call = app->cfg_dest_call;
    app->address_config.destination_ssid = app->cfg_dest_ssid;
    app->address_config.path1_call = app->cfg_path1_call;
    app->address_config.path1_ssid = app->cfg_path1_ssid;
    app->address_config.use_path1 = app->cfg_use_path1;
    app->address_config.path2_call = app->cfg_path2_call;
    app->address_config.path2_ssid = app->cfg_path2_ssid;
    app->address_config.use_path2 = app->cfg_use_path2;
    app->address_config.status_text = app->cfg_status_text;
    app->address_config.position_lat = app->cfg_lat;
    app->address_config.position_lon = app->cfg_lon;
    app->address_config.bearing_deg = app->cfg_bearing_deg;
    app->address_config.speed = app->cfg_speed;
}

static void aprs_save_config(APRSHelloWorldCleanApp* app) {
    furi_assert(app);
    AprsConfig config = {0};
    
    snprintf(config.cfg_source_call, sizeof(config.cfg_source_call), "%s", app->cfg_source_call);
    config.cfg_source_ssid = app->cfg_source_ssid;
    snprintf(config.cfg_dest_call, sizeof(config.cfg_dest_call), "%s", app->cfg_dest_call);
    config.cfg_dest_ssid = app->cfg_dest_ssid;
    snprintf(config.cfg_path1_call, sizeof(config.cfg_path1_call), "%s", app->cfg_path1_call);
    config.cfg_path1_ssid = app->cfg_path1_ssid;
    config.cfg_use_path1 = app->cfg_use_path1;
    config.cfg_use_path2 = app->cfg_use_path2;
    snprintf(config.cfg_path2_call, sizeof(config.cfg_path2_call), "%s", app->cfg_path2_call);
    config.cfg_path2_ssid = app->cfg_path2_ssid;
    snprintf(config.cfg_lat, sizeof(config.cfg_lat), "%s", app->cfg_lat);
    snprintf(config.cfg_lon, sizeof(config.cfg_lon), "%s", app->cfg_lon);
    config.cfg_bearing_deg = app->cfg_bearing_deg;
    config.cfg_speed = app->cfg_speed;
    snprintf(config.cfg_status_text, sizeof(config.cfg_status_text), "%s", app->cfg_status_text);
    
    if(aprs_config_save(&config)) {
        aprs_set_status(app, "config saved");
        FURI_LOG_I(TAG, "Configuration saved");
    } else {
        aprs_set_status(app, "save failed");
        FURI_LOG_E(TAG, "Failed to save configuration");
    }
}

static bool aprs_cfg_mycall_valid(const char* s) {
    if(!s) {
        return false;
    }

    size_t end = 6U;
    while((end > 0U) && (s[end - 1U] == ' ')) {
        end--;
    }

    if((end == 0U) || (end > 6U)) {
        return false;
    }

    for(size_t i = 0; i < end; i++) {
        if(!isalnum((unsigned char)s[i])) {
            return false;
        }
    }

    return true;
}

static void aprs_cycle_callsign_char(char* ch, bool up) {
    static const char set[] = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const size_t n = sizeof(set) - 1U;

    size_t idx = 0U;
    for(size_t i = 0; i < n; i++) {
        if(set[i] == *ch) {
            idx = i;
            break;
        }
    }

    if(up) {
        idx = (idx + 1U) % n;
    } else {
        idx = (idx + n - 1U) % n;
    }

    *ch = set[idx];
}

static void aprs_cycle_pos_char(char* ch, bool up) {
    static const char set[] = "0123456789.NSEW- ";
    const size_t n = sizeof(set) - 1U;

    size_t idx = 0U;
    for(size_t i = 0; i < n; i++) {
        if(set[i] == *ch) {
            idx = i;
            break;
        }
    }

    if(up) {
        idx = (idx + 1U) % n;
    } else {
        idx = (idx + n - 1U) % n;
    }

    *ch = set[idx];
}

static void aprs_cycle_printable_char(char* ch, bool up) {
    uint8_t c = (uint8_t)*ch;
    if(c < 32U) {
        c = 32U;
    }
    if(c > 126U) {
        c = 126U;
    }

    if(up) {
        if(c >= 126U) {
            c = 32U;
        } else {
            c++;
        }
    } else {
        if(c <= 32U) {
            c = 126U;
        } else {
            c--;
        }
    }

    *ch = (char)c;
}

static void aprs_settings_menu_clamp_scroll(APRSHelloWorldCleanApp* app) {
    if(app->settings_menu_index >= APRS_SETTINGS_MENU_ITEMS) {
        app->settings_menu_index = APRS_SETTINGS_MENU_ITEMS - 1U;
    }

    const uint8_t visible = 4U;
    if(app->settings_menu_scroll > app->settings_menu_index) {
        app->settings_menu_scroll = app->settings_menu_index;
    }

    if(app->settings_menu_index >= app->settings_menu_scroll + visible) {
        app->settings_menu_scroll = (uint8_t)(app->settings_menu_index + 1U - visible);
    }
}

static void aprs_set_status(APRSHelloWorldCleanApp* app, const char* text) {
    snprintf(app->last_status, sizeof(app->last_status), "%s", text);
}

static void aprs_hex_full_dump(
    const uint8_t* data,
    size_t data_size,
    char* out,
    size_t out_size) {
    if(!out || (out_size == 0U)) {
        return;
    }

    out[0] = '\0';

    if(!data || (data_size == 0U)) {
        return;
    }

    size_t offset = 0U;
    for(size_t i = 0; i < data_size; i++) {
        int written = snprintf(
            out + offset, out_size - offset, "%02X", data[i]);
        if(written < 0) {
            break;
        }

        size_t step = (size_t)written;
        if(step >= (out_size - offset)) {
            offset = out_size - 1U;
            break;
        }

        offset += step;
    }
}

static void aprs_radio_stop_rx(APRSHelloWorldCleanApp* app) {
    if(app->radio_ready && app->rx_running) {
        furi_hal_subghz_stop_async_rx();
        furi_hal_subghz_idle();
        app->rx_running = false;
    }
}

static void aprs_radio_capture_callback(bool level, uint32_t duration, void* context) {
    APRSHelloWorldCleanApp* app = context;

    app->last_edge_level = level;
    app->last_edge_duration_us = duration;
    app->rx_edge_count++;
}

static bool aprs_radio_start_rx(APRSHelloWorldCleanApp* app) {
    if(!app->radio_ready) {
        return false;
    }

    furi_hal_subghz_idle();
    furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_2fsk_dev2_38khz_async_regs);
    app->frequency_hz = furi_hal_subghz_set_frequency_and_path(APRS_FREQUENCY_HZ);
    furi_hal_subghz_start_async_rx(aprs_radio_capture_callback, app);
    furi_hal_subghz_rx();
    app->rx_running = true;
    app->last_rssi = furi_hal_subghz_get_rssi();
    app->last_lqi = furi_hal_subghz_get_lqi();
    FURI_LOG_I(TAG, "RX started at %lu Hz", (unsigned long)app->frequency_hz);
    return true;
}

static LevelDuration aprs_radio_tx_callback(void* context) {
    APRSHelloWorldCleanApp* app = context;

    if(app->tx_waveform_index >= app->tx_waveform_count) {
        return level_duration_reset();
    }

    return app->tx_waveform[app->tx_waveform_index++];
}

static bool aprs_refresh_frame(APRSHelloWorldCleanApp* app) {
    aprs_sync_address_config(app);

    FURI_LOG_I(TAG, "=== FRAME GENERATION STARTED ===");
    FURI_LOG_I(
        TAG,
        "Source: %s-%u",
        app->address_config.source_call,
        app->address_config.source_ssid);
    FURI_LOG_I(
        TAG,
        "Dest: %s-%u",
        app->address_config.destination_call,
        app->address_config.destination_ssid);
    FURI_LOG_I(
        TAG,
        "Path1: %s-%u (use=%d)",
        app->address_config.path1_call,
        app->address_config.path1_ssid,
        app->address_config.use_path1);
    FURI_LOG_I(
        TAG,
        "Path2: %s-%u (use=%d)",
        app->address_config.path2_call,
        app->address_config.path2_ssid,
        app->address_config.use_path2);
    FURI_LOG_I(
        TAG,
        "Status: '%s'",
        app->address_config.status_text ? app->address_config.status_text : "(null)");
    FURI_LOG_I(
        TAG,
        "Lat: '%s', Lon: '%s'",
        app->address_config.position_lat,
        app->address_config.position_lon);

    if(aprs_ax25_position_is_valid(&app->address_config)) {
        FURI_LOG_I(TAG, "Position is VALID, encoding position frame");
        app->frame_size = aprs_ax25_encode_position_frame(
            &app->address_config, app->frame_buffer, sizeof(app->frame_buffer));

        if(app->frame_size > 0) {
            snprintf(app->status_text, sizeof(app->status_text), "Position");
            FURI_LOG_I(TAG, "Position frame size: %u bytes", (unsigned int)app->frame_size);
        } else {
            aprs_set_status(app, "Position frame too large");
            FURI_LOG_E(TAG, "Position frame encoding FAILED - size = 0");
            return false;
        }
    } else {
        FURI_LOG_I(TAG, "Position is INVALID, encoding status frame");
        app->frame_size = aprs_ax25_encode_status_frame(
            &app->address_config, app->frame_buffer, sizeof(app->frame_buffer));

        if(app->frame_size == 0U) {
            aprs_set_status(app, "Status frame failed");
            FURI_LOG_E(TAG, "Status frame encoding FAILED");
            return false;
        }

        FURI_LOG_I(TAG, "Status frame size: %u bytes", (unsigned int)app->frame_size);
        snprintf(app->status_text, sizeof(app->status_text), "Status");
    }

    if(app->frame_size > 255U) {
        aprs_set_status(app, "Frame too large (>255)");
        FURI_LOG_E(
            TAG,
            "Frame size exceeds AX.25 maximum: %u > 255",
            (unsigned int)app->frame_size);
        return false;
    }

    FURI_LOG_I(TAG, "Building NRZI stream...");
    FURI_LOG_I(TAG, "  Preamble flags: %u", APRS_PREAMBLE_FLAGS);
    FURI_LOG_I(TAG, "  Postamble flags: %u", APRS_POSTAMBLE_FLAGS);
    FURI_LOG_I(TAG, "  TX level capacity: %u", APRS_HELLO_WORLD_TX_LEVEL_CAPACITY);

    if(!aprs_ax25_build_nrzi_stream(
           app->frame_buffer,
           app->frame_size,
           APRS_PREAMBLE_FLAGS,
           APRS_POSTAMBLE_FLAGS,
           app->tx_levels,
           APRS_HELLO_WORLD_TX_LEVEL_CAPACITY,
           &app->tx_level_count)) {
        app->tx_level_count = 0U;
        aprs_set_status(app, "NRZI stream failed");
        FURI_LOG_E(TAG, "NRZI encoding FAILED");
        return false;
    }

    app->tx_bit_count = (uint32_t)app->tx_level_count;
    FURI_LOG_I(TAG, "NRZI stream generated: %lu bits total", (unsigned long)app->tx_bit_count);
    FURI_LOG_I(TAG, "  Expected preamble bits: %u", APRS_PREAMBLE_FLAGS * 8U);
    FURI_LOG_I(TAG, "  Expected frame bits: ~%u", (unsigned int)(app->frame_size * 8U));
    FURI_LOG_I(TAG, "  Expected postamble bits: %u", APRS_POSTAMBLE_FLAGS * 8U);
    FURI_LOG_I(
        TAG,
        "  Total expected: ~%lu bits",
        (unsigned long)(APRS_PREAMBLE_FLAGS * 8U + app->frame_size * 8U + APRS_POSTAMBLE_FLAGS * 8U));

    if(app->tx_level_count > 0U) {
        char bits_buffer[101] = {0};
        size_t bits_to_log = (app->tx_level_count < 100U) ? app->tx_level_count : 100U;
        for(size_t i = 0; i < bits_to_log; i++) {
            bits_buffer[i] = app->tx_levels[i] ? '1' : '0';
        }
        bits_buffer[bits_to_log] = '\0';
        FURI_LOG_I(TAG, "First 100 NRZI bits: %s", bits_buffer);
        FURI_LOG_I(
            TAG,
            "AFSK half periods: mark=%lu us space=%lu us",
            (unsigned long)APRS_AFSK_MARK_HALF_US,
            (unsigned long)APRS_AFSK_SPACE_HALF_US);
    }

    if(!aprs_build_afsk_waveform(app)) {
        app->tx_waveform_count = 0U;
        app->tx_waveform_index = 0U;
        app->tx_waveform_duration_us = 0U;
        aprs_set_status(app, "AFSK waveform failed");
        FURI_LOG_E(TAG, "AFSK waveform generation FAILED");
        return false;
    }

    FURI_LOG_I(
        TAG,
        "AFSK waveform generated: segments=%lu duration=%lu us (~%lu ms)",
        (unsigned long)app->tx_waveform_count,
        (unsigned long)app->tx_waveform_duration_us,
        (unsigned long)(app->tx_waveform_duration_us / 1000U));

    aprs_hex_full_dump(
        app->frame_buffer, app->frame_size, app->last_tx_hex, sizeof(app->last_tx_hex));

    FURI_LOG_I(
        TAG,
        "AX25 %s: %s | frame=%u bytes | bits=%lu | full_hex=%s",
        app->status_text,
        app->address_config.status_text ? app->address_config.status_text : "(empty)",
        (unsigned int)app->frame_size,
        (unsigned long)app->tx_bit_count,
        app->last_tx_hex);
    FURI_LOG_I(TAG, "=== FRAME GENERATION COMPLETE ===");

    aprs_set_status(app, "frame ready");
    return true;
}

static bool aprs_radio_start_tx(APRSHelloWorldCleanApp* app) {
    if(!app->radio_ready) {
        aprs_set_status(app, "radio not ready");
        FURI_LOG_E(TAG, "TX request rejected: radio not ready");
        return false;
    }

    if(app->tx_running) {
        aprs_set_status(app, "TX already running");
        FURI_LOG_W(TAG, "TX request rejected: already running");
        return false;
    }

    if(app->mode != AprsRadioModeLabTx) {
        aprs_set_status(app, "switch to LAB TX");
        FURI_LOG_W(TAG, "TX request rejected: current mode is RX");
        return false;
    }

    if(!app->tx_armed) {
        aprs_set_status(app, "arm TX first");
        FURI_LOG_W(TAG, "TX request rejected: tx not armed");
        return false;
    }

    if(!aprs_refresh_frame(app)) {
        FURI_LOG_E(TAG, "TX request rejected: frame refresh failed");
        return false;
    }

    aprs_radio_stop_rx(app);

    furi_hal_subghz_idle();
    furi_hal_subghz_reset();
    furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_2fsk_dev2_38khz_async_regs);
    app->frequency_hz = furi_hal_subghz_set_frequency_and_path(APRS_FREQUENCY_HZ);
    app->tx_allowed_in_region = furi_hal_region_is_frequency_allowed(app->frequency_hz);

    FURI_LOG_I(TAG, "=== TX START REQUEST ===");
    FURI_LOG_I(TAG, "TX Configuration:");
    FURI_LOG_I(TAG, "  Frequency: %lu Hz", (unsigned long)app->frequency_hz);
    FURI_LOG_I(TAG, "  Frame size: %u bytes", (unsigned int)app->frame_size);
    FURI_LOG_I(TAG, "  TX bits ready: %lu", (unsigned long)app->tx_bit_count);
    FURI_LOG_I(TAG, "  Waveform segments: %lu", (unsigned long)app->tx_waveform_count);
    FURI_LOG_I(TAG, "  TX waveform capacity: %u", APRS_HELLO_WORLD_TX_WAVEFORM_CAPACITY);
    FURI_LOG_I(
        TAG,
        "  AFSK half-periods: mark=%lu us space=%lu us",
        (unsigned long)APRS_AFSK_MARK_HALF_US,
        (unsigned long)APRS_AFSK_SPACE_HALF_US);
    FURI_LOG_I(
        TAG,
        "  Expected TX time: ~%lu ms",
        (unsigned long)(app->tx_waveform_duration_us / 1000U));

    if(!app->tx_allowed_in_region || !furi_hal_subghz_tx()) {
        app->tx_last_start_ok = false;
        aprs_set_status(app, "TX restricted in region");
        FURI_LOG_E(
            TAG,
            "TX restricted at %lu Hz for region %s",
            (unsigned long)app->frequency_hz,
            app->region_name);
        notification_message(app->notifications, &sequence_tx_error);
        aprs_radio_start_rx(app);
        return false;
    }

    furi_hal_power_suppress_charge_enter();
    app->tx_level_index = 0U;
    app->tx_waveform_index = 0U;
    app->last_edge_level = false;
    app->tx_last_start_ok = furi_hal_subghz_start_async_tx(aprs_radio_tx_callback, app);

    if(!app->tx_last_start_ok) {
        furi_hal_power_suppress_charge_exit();
        aprs_set_status(app, "TX start failed");
        FURI_LOG_E(TAG, "furi_hal_subghz_start_async_tx() FAILED!");
        FURI_LOG_E(TAG, "  Radio may not be properly initialized");
        notification_message(app->notifications, &sequence_tx_error);
        aprs_radio_start_rx(app);
        return false;
    }

    notification_message(app->notifications, &sequence_tx_start);
    app->tx_running = true;
    app->tx_packets++;
    aprs_set_status(app, "TX in progress");
    FURI_LOG_I(TAG, "=== TX STARTED ===");
    FURI_LOG_I(
        TAG,
        "TX started: freq=%lu frame=%u bits=%lu full_hex=%s",
        (unsigned long)app->frequency_hz,
        (unsigned int)app->frame_size,
        (unsigned long)app->tx_bit_count,
        app->last_tx_hex);
    FURI_LOG_I(TAG, "TX in progress - packets sent: %lu", (unsigned long)app->tx_packets);
    return true;
}

static void aprs_radio_poll(APRSHelloWorldCleanApp* app) {
    if(!app->radio_ready) {
        return;
    }

    app->last_rssi = furi_hal_subghz_get_rssi();
    app->last_lqi = furi_hal_subghz_get_lqi();

    if(app->tx_running && furi_hal_subghz_is_async_tx_complete()) {
        FURI_LOG_I(TAG, "=== TX COMPLETE ===");
        FURI_LOG_I(
            TAG,
            "TX finished successfully: waveform sent=%lu / %lu, bits=%lu",
            (unsigned long)app->tx_waveform_index,
            (unsigned long)app->tx_waveform_count,
            (unsigned long)app->tx_bit_count);

        if(app->tx_waveform_index < app->tx_waveform_count) {
            FURI_LOG_W(
                TAG,
                "TX incomplete: missing waveform segments=%lu",
                (unsigned long)(app->tx_waveform_count - app->tx_waveform_index));
        }

        furi_hal_subghz_stop_async_tx();
        furi_hal_power_suppress_charge_exit();
        furi_hal_subghz_idle();
        notification_message(app->notifications, &sequence_tx_stop);
        notification_message(app->notifications, &sequence_rx_activity);
        app->tx_running = false;
        aprs_set_status(app, "TX complete");
        FURI_LOG_I(
            TAG,
            "TX complete: packets=%lu last_rssi=%d last_lqi=%u",
            (unsigned long)app->tx_packets,
            (int)app->last_rssi,
            app->last_lqi);
        aprs_radio_start_rx(app);
    }
}

static bool aprs_radio_init(APRSHelloWorldCleanApp* app) {
    app->frequency_hz = APRS_FREQUENCY_HZ;
    app->tx_allowed_in_region = furi_hal_region_is_frequency_allowed(app->frequency_hz);
    snprintf(app->region_name, sizeof(app->region_name), "%s", furi_hal_region_get_name());

    if(!furi_hal_subghz_is_frequency_valid(app->frequency_hz)) {
        aprs_set_status(app, "freq invalid");
        return false;
    }

    furi_hal_subghz_reset();
    app->radio_ready = true;
    app->frequency_hz = furi_hal_subghz_set_frequency_and_path(APRS_FREQUENCY_HZ);
    aprs_set_status(app, app->tx_allowed_in_region ? "RX ready, TX allowed" :
                                                  "RX ready, TX restricted");
    FURI_LOG_I(
        TAG,
        "Radio init: region=%s freq=%lu tx_allowed=%s",
        app->region_name,
        (unsigned long)app->frequency_hz,
        app->tx_allowed_in_region ? "yes" : "no");
    return aprs_radio_start_rx(app);
}

static void aprs_radio_deinit(APRSHelloWorldCleanApp* app) {
    if(!app->radio_ready) {
        return;
    }

    aprs_radio_stop_rx(app);

    if(app->tx_running) {
        furi_hal_subghz_stop_async_tx();
        furi_hal_power_suppress_charge_exit();
        app->tx_running = false;
    }

    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();
    app->radio_ready = false;
}

static void aprs_hello_world_clean_app_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;

    APRSHelloWorldCleanEvent event = {.type = EventTypeInput, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void aprs_draw_status_line(Canvas* canvas, uint8_t y, const char* text) {
    canvas_draw_str(canvas, 2, y, text);
}

static const char* const aprs_settings_menu_labels[APRS_SETTINGS_MENU_ITEMS] = {
    "MYCALL",
    "SSID",
    "Position",
    "Bearing",
    "Speed",
    "Status text",
    "Path 1",
    "Path 2",
};

static void aprs_draw_view_main(APRSHelloWorldCleanApp* app, Canvas* canvas, uint8_t base_y) {
    char line[64];

    snprintf(
        line,
        sizeof(line),
        "APRS 432.500 %s",
        app->mode == AprsRadioModeLabTx ? "LABTX" : "RX");
    aprs_draw_status_line(canvas, base_y, line);

    snprintf(
        line,
        sizeof(line),
        "Arm:%s TX:%s %s",
        app->tx_armed ? "yes" : "no",
        app->tx_running ? "busy" : "idle",
        app->tx_allowed_in_region ? app->region_name : "LOCK");
    aprs_draw_status_line(canvas, (uint8_t)(base_y + 8U), line);

    snprintf(line, sizeof(line), "RSSI:%3d LQI:%3u", (int)app->last_rssi, app->last_lqi);
    aprs_draw_status_line(canvas, (uint8_t)(base_y + 16U), line);

    snprintf(
        line,
        sizeof(line),
        "Edges:%lu %c%luus",
        (unsigned long)app->rx_edge_count,
        app->last_edge_level ? 'H' : 'L',
        (unsigned long)app->last_edge_duration_us);
    aprs_draw_status_line(canvas, (uint8_t)(base_y + 24U), line);

    snprintf(
        line,
        sizeof(line),
        "Frame:%uB Bits:%lu",
        (unsigned int)app->frame_size,
        (unsigned long)app->tx_bit_count);
    aprs_draw_status_line(canvas, (uint8_t)(base_y + 32U), line);

    aprs_draw_status_line(canvas, (uint8_t)(base_y + 40U), app->last_status);
    aprs_draw_status_line(canvas, (uint8_t)(base_y + 48U), app->last_tx_hex);
    aprs_draw_status_line(canvas, 63, "3xUp Short=cfg OK=refr Long=TX");
}

static void aprs_draw_view_settings_menu(APRSHelloWorldCleanApp* app, Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "APRS settings");
    canvas_set_font(canvas, FontSecondary);

    aprs_settings_menu_clamp_scroll(app);

    const uint8_t visible = 4U;
    uint8_t y = 24;
    for(uint8_t row = 0; row < visible; row++) {
        uint8_t idx = (uint8_t)(app->settings_menu_scroll + row);
        if(idx >= APRS_SETTINGS_MENU_ITEMS) {
            break;
        }

        char line[32];
        if(idx == app->settings_menu_index) {
            snprintf(line, sizeof(line), ">%s", aprs_settings_menu_labels[idx]);
        } else {
            snprintf(line, sizeof(line), " %s", aprs_settings_menu_labels[idx]);
        }

        aprs_draw_status_line(canvas, y, line);
        y = (uint8_t)(y + 8U);
    }

    aprs_draw_status_line(canvas, 63, "OK open Back exit");
}

static void aprs_draw_view_mycall(APRSHelloWorldCleanApp* app, Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "MYCALL");
    canvas_set_font(canvas, FontSecondary);

    char line[32];
    snprintf(line, sizeof(line), "Call: %.6s", app->cfg_source_call);
    aprs_draw_status_line(canvas, 22, line);

    if(!aprs_cfg_mycall_valid(app->cfg_source_call)) {
        aprs_draw_status_line(canvas, 40, "Invalid: A-Z 0-9");
    }

    aprs_draw_status_line(canvas, 52, "LR mv UD ch  OK fld");
    aprs_draw_status_line(canvas, 63, "Back menu");
}

static void aprs_draw_view_ssid(APRSHelloWorldCleanApp* app, Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "SSID");
    canvas_set_font(canvas, FontSecondary);

    char line[32];
    snprintf(line, sizeof(line), "Source SSID: %u", (unsigned)app->cfg_source_ssid);
    aprs_draw_status_line(canvas, 28, line);
    aprs_draw_status_line(canvas, 44, "0-15  Up/Down");
    aprs_draw_status_line(canvas, 63, "Back menu");
}

static void aprs_draw_view_position(APRSHelloWorldCleanApp* app, Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Position");
    canvas_set_font(canvas, FontSecondary);

    char line[32];
    snprintf(line, sizeof(line), "Lat:%s", app->cfg_lat);
    aprs_draw_status_line(canvas, 22, line);
    snprintf(line, sizeof(line), "Lon:%s", app->cfg_lon);
    aprs_draw_status_line(canvas, 30, line);

    snprintf(
        line,
        sizeof(line),
        "Edit %s",
        app->pos_edit_line == 0U ? "latitude" : "longitude");
    aprs_draw_status_line(canvas, 42, line);
    aprs_draw_status_line(canvas, 63, "Back menu");
}

static void aprs_draw_view_bearing(APRSHelloWorldCleanApp* app, Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Bearing");
    canvas_set_font(canvas, FontSecondary);

    char line[32];
    snprintf(line, sizeof(line), "Bearing: %u deg", (unsigned)app->cfg_bearing_deg);
    aprs_draw_status_line(canvas, 30, line);
    aprs_draw_status_line(canvas, 46, "0-359  Up/Down");
    aprs_draw_status_line(canvas, 63, "Back menu");
}

static void aprs_draw_view_speed(APRSHelloWorldCleanApp* app, Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Speed");
    canvas_set_font(canvas, FontSecondary);

    char line[32];
    snprintf(line, sizeof(line), "Speed: %u", (unsigned)app->cfg_speed);
    aprs_draw_status_line(canvas, 28, line);
    aprs_draw_status_line(canvas, 44, "0-999  Up/Down");
    aprs_draw_status_line(canvas, 63, "Back menu");
}

static void aprs_draw_view_status_text(APRSHelloWorldCleanApp* app, Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Status text");
    canvas_set_font(canvas, FontSecondary);

    size_t len = strlen(app->cfg_status_text);
    char line[40];
    snprintf(line, sizeof(line), "Len: %u/%u", (unsigned)len, (unsigned)APRS_CFG_STATUS_MAX);
    aprs_draw_status_line(canvas, 22, line);

    snprintf(line, sizeof(line), "%.18s", app->cfg_status_text);
    aprs_draw_status_line(canvas, 32, line);
    snprintf(line, sizeof(line), "%.18s", app->cfg_status_text + 18);
    aprs_draw_status_line(canvas, 40, line);
    snprintf(line, sizeof(line), "%.18s", app->cfg_status_text + 36);
    aprs_draw_status_line(canvas, 48, line);

    aprs_draw_status_line(canvas, 56, "LR mv UD char");
    aprs_draw_status_line(canvas, 63, "Back menu");
}

static void aprs_draw_view_path1(APRSHelloWorldCleanApp* app, Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Path 1");
    canvas_set_font(canvas, FontSecondary);

    char line[32];
    snprintf(line, sizeof(line), "Call: %.6s", app->cfg_path1_call);
    aprs_draw_status_line(canvas, 22, line);
    snprintf(line, sizeof(line), "SSID: %u", (unsigned)app->cfg_path1_ssid);
    aprs_draw_status_line(canvas, 30, line);
    snprintf(
        line,
        sizeof(line),
        "Field: %s",
        app->path1_edit_ssid ? "SSID" : "call");
    aprs_draw_status_line(canvas, 40, line);
    aprs_draw_status_line(canvas, 52, "OK switch field");
    aprs_draw_status_line(canvas, 63, "Back menu");
}

static void aprs_draw_view_path2(APRSHelloWorldCleanApp* app, Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Path 2");
    canvas_set_font(canvas, FontSecondary);

    const char* focus_label = "use";
    if(app->path2_focus == 1U) {
        focus_label = "call";
    } else if(app->path2_focus == 2U) {
        focus_label = "SSID";
    }

    char line[32];
    snprintf(line, sizeof(line), "Focus: %s", focus_label);
    aprs_draw_status_line(canvas, 22, line);
    snprintf(line, sizeof(line), "Use: %s", app->cfg_use_path2 ? "yes" : "no");
    aprs_draw_status_line(canvas, 30, line);
    snprintf(line, sizeof(line), "Call: %.6s", app->cfg_path2_call);
    aprs_draw_status_line(canvas, 38, line);
    snprintf(line, sizeof(line), "SSID: %u", (unsigned)app->cfg_path2_ssid);
    aprs_draw_status_line(canvas, 46, line);
    aprs_draw_status_line(canvas, 56, "OK next field");
    aprs_draw_status_line(canvas, 63, "Back menu");
}

static void aprs_hello_world_clean_app_draw_callback(Canvas* canvas, void* ctx) {
    APRSHelloWorldCleanApp* app = ctx;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    if(!app) {
        canvas_draw_str(canvas, 2, 10, "No app context");
        return;
    }

    uint32_t now = furi_get_tick();
    uint8_t main_base_y = 8U;
    if((app->ui_toast[0] != '\0') && (now < app->ui_toast_until)) {
        aprs_draw_status_line(canvas, 8, app->ui_toast);
        main_base_y = 18U;
    }

    switch(app->ui_view) {
    case AprsUiViewMain:
        aprs_draw_view_main(app, canvas, main_base_y);
        break;
    case AprsUiViewSettingsMenu:
        aprs_draw_view_settings_menu(app, canvas);
        break;
    case AprsUiViewSettingsMycall:
        aprs_draw_view_mycall(app, canvas);
        break;
    case AprsUiViewSettingsSsid:
        aprs_draw_view_ssid(app, canvas);
        break;
    case AprsUiViewSettingsPosition:
        aprs_draw_view_position(app, canvas);
        break;
    case AprsUiViewSettingsBearing:
        aprs_draw_view_bearing(app, canvas);
        break;
    case AprsUiViewSettingsSpeed:
        aprs_draw_view_speed(app, canvas);
        break;
    case AprsUiViewSettingsStatusText:
        aprs_draw_view_status_text(app, canvas);
        break;
    case AprsUiViewSettingsPath1:
        aprs_draw_view_path1(app, canvas);
        break;
    case AprsUiViewSettingsPath2:
        aprs_draw_view_path2(app, canvas);
        break;
    default:
        aprs_draw_status_line(canvas, 10, "Unknown view");
        break;
    }
}

static void timer_callback(void* context) {
    FuriMessageQueue* event_queue = context;
    furi_assert(event_queue);

    APRSHelloWorldCleanEvent event = {.type = EventTypeTick};
    furi_message_queue_put(event_queue, &event, 0U);
}

static APRSHelloWorldCleanApp* aprs_hello_world_clean_app_alloc(void) {
    APRSHelloWorldCleanApp* app = malloc(sizeof(APRSHelloWorldCleanApp));

    memset(app, 0, sizeof(APRSHelloWorldCleanApp));

    app->view_port = view_port_alloc();
    app->event_queue = furi_message_queue_alloc(8U, sizeof(APRSHelloWorldCleanEvent));
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->gui = furi_record_open(RECORD_GUI);
    app->timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, app->event_queue);

    app->mode = AprsRadioModeRxMonitor;
    app->frequency_hz = APRS_FREQUENCY_HZ;
    app->ui_view = AprsUiViewMain;

    // Load configuration from storage
    AprsConfig config = aprs_config_load();
    snprintf(app->cfg_source_call, sizeof(app->cfg_source_call), "%s", config.cfg_source_call);
    app->cfg_source_ssid = config.cfg_source_ssid;
    snprintf(app->cfg_dest_call, sizeof(app->cfg_dest_call), "%s", config.cfg_dest_call);
    app->cfg_dest_ssid = config.cfg_dest_ssid;
    snprintf(app->cfg_path1_call, sizeof(app->cfg_path1_call), "%s", config.cfg_path1_call);
    app->cfg_path1_ssid = config.cfg_path1_ssid;
    app->cfg_use_path1 = config.cfg_use_path1;
    app->cfg_use_path2 = config.cfg_use_path2;
    snprintf(app->cfg_path2_call, sizeof(app->cfg_path2_call), "%s", config.cfg_path2_call);
    app->cfg_path2_ssid = config.cfg_path2_ssid;
    snprintf(app->cfg_lat, sizeof(app->cfg_lat), "%s", config.cfg_lat);
    snprintf(app->cfg_lon, sizeof(app->cfg_lon), "%s", config.cfg_lon);
    app->cfg_bearing_deg = config.cfg_bearing_deg;
    app->cfg_speed = config.cfg_speed;
    snprintf(app->cfg_status_text, sizeof(app->cfg_status_text), "%s", config.cfg_status_text);

    FURI_LOG_I(TAG, "=== Config loaded ===");
    FURI_LOG_I(TAG, "Source: %s-%u", app->cfg_source_call, app->cfg_source_ssid);
    FURI_LOG_I(TAG, "Dest: %s-%u", app->cfg_dest_call, app->cfg_dest_ssid);
    FURI_LOG_I(TAG, "Path1: %s-%u (use=%d)", app->cfg_path1_call, app->cfg_path1_ssid, app->cfg_use_path1);
    FURI_LOG_I(TAG, "Path2: %s-%u (use=%d)", app->cfg_path2_call, app->cfg_path2_ssid, app->cfg_use_path2);
    FURI_LOG_I(TAG, "Status: '%s'", app->cfg_status_text);
    FURI_LOG_I(TAG, "Lat: '%s', Lon: '%s'", app->cfg_lat, app->cfg_lon);

    app->mycall_edit_cursor = 0U;
    app->pos_edit_line = 0U;
    app->pos_edit_cursor = 0U;
    app->status_edit_cursor = 0U;
    app->path1_edit_cursor = 0U;
    app->path2_edit_cursor = 0U;
    app->path1_edit_ssid = false;
    app->path2_focus = 0U;

    aprs_sync_address_config(app);

    view_port_draw_callback_set(app->view_port, aprs_hello_world_clean_app_draw_callback, app);
    view_port_input_callback_set(
        app->view_port, aprs_hello_world_clean_app_input_callback, app->event_queue);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    aprs_set_status(app, "booting");
    aprs_refresh_frame(app);
    aprs_radio_init(app);

    return app;
}

static void aprs_hello_world_clean_app_free(APRSHelloWorldCleanApp* app) {
    furi_assert(app);

    aprs_radio_deinit(app);

    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);

    furi_timer_free(app->timer);
    furi_message_queue_free(app->event_queue);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

static void aprs_handle_input_main(APRSHelloWorldCleanApp* app, InputKey key) {
    switch(key) {
    case InputKeyLeft:
        app->tx_armed = !app->tx_armed;
        aprs_refresh_frame(app);
        aprs_set_status(app, app->tx_armed ? "LAB TX armed" : "LAB TX safe");
        FURI_LOG_I(TAG, "TX armed=%s", app->tx_armed ? "yes" : "no");
        break;
    case InputKeyRight:
        app->mode = (app->mode == AprsRadioModeRxMonitor) ? AprsRadioModeLabTx :
                                                             AprsRadioModeRxMonitor;
        aprs_set_status(app, app->mode == AprsRadioModeLabTx ? "mode LAB TX" : "mode RX");
        FURI_LOG_I(TAG, "Mode changed to %s", app->mode == AprsRadioModeLabTx ? "LAB TX" : "RX");
        break;
    case InputKeyDown:
        app->rx_edge_count = 0U;
        app->last_edge_duration_us = 0U;
        app->last_edge_level = false;
        aprs_set_status(app, "RX counters reset");
        FURI_LOG_I(TAG, "RX counters reset");
        break;
    case InputKeyOk:
        aprs_refresh_frame(app);
        aprs_set_status(app, "frame refreshed");
        FURI_LOG_I(TAG, "Frame refreshed (OK)");
        break;
    default:
        break;
    }
}

static void aprs_handle_input_settings(APRSHelloWorldCleanApp* app, InputKey key) {
    switch(app->ui_view) {
    case AprsUiViewSettingsMenu: {
        if(key == InputKeyUp) {
            if(app->settings_menu_index > 0U) {
                app->settings_menu_index--;
            }
        } else if(key == InputKeyDown) {
            if(app->settings_menu_index + 1U < APRS_SETTINGS_MENU_ITEMS) {
                app->settings_menu_index++;
            }
        } else if(key == InputKeyOk) {
            switch(app->settings_menu_index) {
            case 0U:
                app->ui_view = AprsUiViewSettingsMycall;
                app->mycall_edit_cursor = 0U;
                break;
            case 1U:
                app->ui_view = AprsUiViewSettingsSsid;
                break;
            case 2U:
                app->ui_view = AprsUiViewSettingsPosition;
                app->pos_edit_line = 0U;
                app->pos_edit_cursor = 0U;
                break;
            case 3U:
                app->ui_view = AprsUiViewSettingsBearing;
                break;
            case 4U:
                app->ui_view = AprsUiViewSettingsSpeed;
                break;
            case 5U:
                app->ui_view = AprsUiViewSettingsStatusText;
                app->status_edit_cursor = 0U;
                break;
            case 6U:
                app->ui_view = AprsUiViewSettingsPath1;
                app->path1_edit_cursor = 0U;
                app->path1_edit_ssid = false;
                break;
            case 7U:
                app->ui_view = AprsUiViewSettingsPath2;
                app->path2_edit_cursor = 0U;
                app->path2_focus = 0U;
                break;
            default:
                break;
            }
        }
        break;
    }
    case AprsUiViewSettingsMycall:
        if(key == InputKeyLeft) {
            if(app->mycall_edit_cursor > 0U) {
                app->mycall_edit_cursor--;
            }
        } else if(key == InputKeyRight) {
            if(app->mycall_edit_cursor < 5U) {
                app->mycall_edit_cursor++;
            }
        } else if(key == InputKeyUp) {
            aprs_cycle_callsign_char(&app->cfg_source_call[app->mycall_edit_cursor], true);
        } else if(key == InputKeyDown) {
            aprs_cycle_callsign_char(&app->cfg_source_call[app->mycall_edit_cursor], false);
        }
        app->cfg_source_call[6] = '\0';
        break;
    case AprsUiViewSettingsSsid:
        if(key == InputKeyUp) {
            if(app->cfg_source_ssid < 15U) {
                app->cfg_source_ssid++;
            }
        } else if(key == InputKeyDown) {
            if(app->cfg_source_ssid > 0U) {
                app->cfg_source_ssid--;
            }
        }
        break;
    case AprsUiViewSettingsPosition: {
        char* line = (app->pos_edit_line == 0U) ? app->cfg_lat : app->cfg_lon;
        const size_t max_idx = APRS_CFG_POS_STR_LEN - 2U;

        if(key == InputKeyOk) {
            app->pos_edit_line ^= 1U;
            app->pos_edit_cursor = 0U;
        } else if(key == InputKeyLeft) {
            if(app->pos_edit_cursor > 0U) {
                app->pos_edit_cursor--;
            }
        } else if(key == InputKeyRight) {
            if(app->pos_edit_cursor < max_idx) {
                app->pos_edit_cursor++;
            }
        } else if(key == InputKeyUp) {
            aprs_cycle_pos_char(&line[app->pos_edit_cursor], true);
        } else if(key == InputKeyDown) {
            aprs_cycle_pos_char(&line[app->pos_edit_cursor], false);
        }

        line[APRS_CFG_POS_STR_LEN - 1U] = '\0';
        break;
    }
    case AprsUiViewSettingsBearing:
        if(key == InputKeyUp) {
            app->cfg_bearing_deg = (uint16_t)((app->cfg_bearing_deg + 1U) % 360U);
        } else if(key == InputKeyDown) {
            app->cfg_bearing_deg = (uint16_t)((app->cfg_bearing_deg + 359U) % 360U);
        }
        break;
    case AprsUiViewSettingsSpeed:
        if(key == InputKeyUp) {
            if(app->cfg_speed < 999U) {
                app->cfg_speed++;
            }
        } else if(key == InputKeyDown) {
            if(app->cfg_speed > 0U) {
                app->cfg_speed--;
            }
        }
        break;
    case AprsUiViewSettingsStatusText:
        if(key == InputKeyLeft) {
            if(app->status_edit_cursor > 0U) {
                app->status_edit_cursor--;
            }
        } else if(key == InputKeyRight) {
            if(app->status_edit_cursor < (APRS_CFG_STATUS_MAX - 1U)) {
                app->status_edit_cursor++;
            }
        } else if(key == InputKeyUp) {
            aprs_cycle_printable_char(&app->cfg_status_text[app->status_edit_cursor], true);
        } else if(key == InputKeyDown) {
            aprs_cycle_printable_char(&app->cfg_status_text[app->status_edit_cursor], false);
        }
        app->cfg_status_text[APRS_CFG_STATUS_MAX] = '\0';
        break;
    case AprsUiViewSettingsPath1:
        if(key == InputKeyOk) {
            app->path1_edit_ssid = !app->path1_edit_ssid;
        } else if(key == InputKeyLeft) {
            if(!app->path1_edit_ssid && (app->path1_edit_cursor > 0U)) {
                app->path1_edit_cursor--;
            }
        } else if(key == InputKeyRight) {
            if(!app->path1_edit_ssid && (app->path1_edit_cursor < 5U)) {
                app->path1_edit_cursor++;
            }
        } else if(key == InputKeyUp) {
            if(app->path1_edit_ssid) {
                if(app->cfg_path1_ssid < 15U) {
                    app->cfg_path1_ssid++;
                }
            } else {
                aprs_cycle_callsign_char(&app->cfg_path1_call[app->path1_edit_cursor], true);
            }
        } else if(key == InputKeyDown) {
            if(app->path1_edit_ssid) {
                if(app->cfg_path1_ssid > 0U) {
                    app->cfg_path1_ssid--;
                }
            } else {
                aprs_cycle_callsign_char(&app->cfg_path1_call[app->path1_edit_cursor], false);
            }
        }
        app->cfg_path1_call[6] = '\0';
        break;
    case AprsUiViewSettingsPath2:
        if(key == InputKeyOk) {
            app->path2_focus = (uint8_t)((app->path2_focus + 1U) % 3U);
        } else if(key == InputKeyLeft) {
            if((app->path2_focus == 1U) && (app->path2_edit_cursor > 0U)) {
                app->path2_edit_cursor--;
            }
        } else if(key == InputKeyRight) {
            if((app->path2_focus == 1U) && (app->path2_edit_cursor < 5U)) {
                app->path2_edit_cursor++;
            }
        } else if(key == InputKeyUp) {
            if(app->path2_focus == 0U) {
                app->cfg_use_path2 = !app->cfg_use_path2;
            } else if(app->path2_focus == 1U) {
                aprs_cycle_callsign_char(&app->cfg_path2_call[app->path2_edit_cursor], true);
            } else if(app->cfg_path2_ssid < 15U) {
                app->cfg_path2_ssid++;
            }
        } else if(key == InputKeyDown) {
            if(app->path2_focus == 0U) {
                app->cfg_use_path2 = !app->cfg_use_path2;
            } else if(app->path2_focus == 1U) {
                aprs_cycle_callsign_char(&app->cfg_path2_call[app->path2_edit_cursor], false);
            } else if(app->cfg_path2_ssid > 0U) {
                app->cfg_path2_ssid--;
            }
        }
        app->cfg_path2_call[6] = '\0';
        break;
    default:
        break;
    }
}

static void aprs_handle_input(APRSHelloWorldCleanApp* app, const InputEvent* event) {
    if(!app || !event) {
        return;
    }

    if(app->ui_view == AprsUiViewMain) {
        if((event->type == InputTypePress) && (event->key != InputKeyUp)) {
            app->triple_up_count = 0U;
        }

        if((event->type == InputTypeShort) && (event->key == InputKeyUp)) {
            uint32_t now = furi_get_tick();
            if((now - app->triple_up_last_tick) > (uint32_t)APRS_TRIPLE_UP_WINDOW_MS) {
                app->triple_up_count = 1U;
            } else {
                app->triple_up_count++;
            }

            app->triple_up_last_tick = now;

            if(app->triple_up_count >= 3U) {
                app->triple_up_count = 0U;
                app->ui_view = AprsUiViewSettingsMenu;
                app->settings_menu_index = 0U;
                app->settings_menu_scroll = 0U;
                snprintf(app->ui_toast, sizeof(app->ui_toast), "Settings");
                app->ui_toast_until = now + 900U;
                FURI_LOG_I(TAG, "Open settings (triple Up Short)");
            } else {
                FURI_LOG_I(
                    TAG,
                    "Settings: Up Short step %u/3 (next within %ums)",
                    (unsigned)app->triple_up_count,
                    (unsigned)APRS_TRIPLE_UP_WINDOW_MS);
            }

            return;
        }

        if(event->type != InputTypePress) {
            return;
        }

        aprs_handle_input_main(app, event->key);
        return;
    }

    if(event->type != InputTypePress) {
        return;
    }

    aprs_handle_input_settings(app, event->key);
}

int32_t aprs_hello_world_clean_app(void* p) {
    UNUSED(p);

    APRSHelloWorldCleanApp* app = aprs_hello_world_clean_app_alloc();
    APRSHelloWorldCleanEvent event;
    bool running = true;

    furi_timer_start(app->timer, furi_ms_to_ticks(APP_TICK_PERIOD_MS));

    while(running) {
        if(furi_message_queue_get(app->event_queue, &event, FuriWaitForever) != FuriStatusOk) {
            continue;
        }

        if(event.type == EventTypeTick) {
            aprs_radio_poll(app);
            view_port_update(app->view_port);
            continue;
        }

        if(event.type != EventTypeInput) {
            continue;
        }

        if(event.input.key == InputKeyBack && event.input.type == InputTypePress) {
            if(app->ui_view != AprsUiViewMain) {
                if(app->ui_view == AprsUiViewSettingsMenu) {
                    app->ui_view = AprsUiViewMain;
                    app->ui_toast[0] = '\0';
                    aprs_save_config(app);
                    aprs_refresh_frame(app);
                } else {
                    app->ui_view = AprsUiViewSettingsMenu;
                }
                view_port_update(app->view_port);
                continue;
            }

            running = false;
            continue;
        }

        if(event.input.key == InputKeyOk && event.input.type == InputTypeLong) {
            if(app->ui_view == AprsUiViewMain) {
                aprs_radio_start_tx(app);
            }
            view_port_update(app->view_port);
            continue;
        }

        const bool main_triple_up_short = (app->ui_view == AprsUiViewMain) &&
                                          (event.input.type == InputTypeShort) &&
                                          (event.input.key == InputKeyUp);

        if((event.input.type == InputTypePress) || main_triple_up_short) {
            aprs_handle_input(app, &event.input);
            view_port_update(app->view_port);
        }
    }

    aprs_hello_world_clean_app_free(app);
    return 0;
}
