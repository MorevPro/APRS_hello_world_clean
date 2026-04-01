#include "aprs_hello_world_clean_app.h"

#include "aprs_ax25_encoder.h"

#include <furi.h>
#include <furi_hal_power.h>
#include <furi_hal_region.h>
#include <furi_hal_subghz.h>
#include <gui/elements.h>
#include <input/input.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <lib/toolbox/level_duration.h>
#include <notification/notification_messages.h>
#include <stdio.h>
#include <string.h>

#define TAG "APRS432"
#define APRS_FREQUENCY_HZ          432500000UL
#define APRS_GFSK_BIT_DURATION_US  104U
#define APRS_PREAMBLE_FLAGS        32U
#define APRS_POSTAMBLE_FLAGS       3U
#define APP_TICK_PERIOD_MS         200U

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

static const AprsAx25AddressConfig aprs_address = {
    .source_call = "MYCALL",
    .source_ssid = 1U,
    .destination_call = "APRS",
    .destination_ssid = 0U,
    .path1_call = "WIDE2",
    .path1_ssid = 1U,
    .use_path1 = true,
};

static void aprs_set_status(APRSHelloWorldCleanApp* app, const char* text) {
    snprintf(app->last_status, sizeof(app->last_status), "%s", text);
}

static void aprs_hex_preview(
    const uint8_t* data,
    size_t data_size,
    char* out,
    size_t out_size,
    size_t max_bytes) {
    if(!out || (out_size == 0U)) {
        return;
    }

    out[0] = '\0';

    if(!data || (data_size == 0U)) {
        return;
    }

    size_t preview_size = data_size;
    if(preview_size > max_bytes) {
        preview_size = max_bytes;
    }

    size_t offset = 0U;
    for(size_t i = 0; i < preview_size; i++) {
        int written = snprintf(
            out + offset, out_size - offset, "%02X%s", data[i], (i + 1U < preview_size) ? " " : "");
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

    if((preview_size < data_size) && (offset + 4U < out_size)) {
        snprintf(out + offset, out_size - offset, " ...");
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
    furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_gfsk_9_99kb_async_regs);
    app->frequency_hz = furi_hal_subghz_set_frequency_and_path(app->frequency_hz);
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

    if(app->tx_level_index >= app->tx_level_count) {
        return level_duration_reset();
    }

    const bool level = app->tx_levels[app->tx_level_index++];
    return level_duration_make(level, APRS_GFSK_BIT_DURATION_US);
}

static bool aprs_refresh_frame(APRSHelloWorldCleanApp* app) {
    if(aprs_ax25_make_mystatus(
           app->status_text,
           sizeof(app->status_text),
           app->frequency_hz,
           app->tx_armed) == 0U) {
        aprs_set_status(app, "status text failed");
        return false;
    }

    app->frame_size = aprs_ax25_encode_status_frame(
        &aprs_address, app->status_text, app->frame_buffer, sizeof(app->frame_buffer));
    if(app->frame_size == 0U) {
        aprs_set_status(app, "AX25 frame failed");
        return false;
    }

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
        return false;
    }

    app->tx_bit_count = (uint32_t)app->tx_level_count;
    aprs_hex_preview(
        app->frame_buffer, app->frame_size, app->last_tx_hex, sizeof(app->last_tx_hex), 8U);

    FURI_LOG_I(
        TAG,
        "AX25 status: %s | frame=%u bytes | preview=%s",
        app->status_text,
        (unsigned int)app->frame_size,
        app->last_tx_hex);

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
    furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_gfsk_9_99kb_async_regs);
    app->frequency_hz = furi_hal_subghz_set_frequency_and_path(app->frequency_hz);
    app->tx_allowed_in_region = furi_hal_region_is_frequency_allowed(app->frequency_hz);

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
    app->tx_last_start_ok = furi_hal_subghz_start_async_tx(aprs_radio_tx_callback, app);

    if(!app->tx_last_start_ok) {
        furi_hal_power_suppress_charge_exit();
        aprs_set_status(app, "TX start failed");
        FURI_LOG_E(TAG, "TX start failed at %lu Hz", (unsigned long)app->frequency_hz);
        notification_message(app->notifications, &sequence_tx_error);
        aprs_radio_start_rx(app);
        return false;
    }

    notification_message(app->notifications, &sequence_tx_start);
    app->tx_running = true;
    app->tx_packets++;
    aprs_set_status(app, "TX in progress");
    FURI_LOG_I(
        TAG,
        "TX started: freq=%lu frame=%u bits=%lu preview=%s",
        (unsigned long)app->frequency_hz,
        (unsigned int)app->frame_size,
        (unsigned long)app->tx_bit_count,
        app->last_tx_hex);
    return true;
}

static void aprs_radio_poll(APRSHelloWorldCleanApp* app) {
    if(!app->radio_ready) {
        return;
    }

    app->last_rssi = furi_hal_subghz_get_rssi();
    app->last_lqi = furi_hal_subghz_get_lqi();

    if(app->tx_running && furi_hal_subghz_is_async_tx_complete()) {
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
    app->frequency_hz = furi_hal_subghz_set_frequency_and_path(app->frequency_hz);
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

static void aprs_hello_world_clean_app_draw_callback(Canvas* canvas, void* ctx) {
    APRSHelloWorldCleanApp* app = ctx;
    char line[64];

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    if(!app) {
        canvas_draw_str(canvas, 2, 10, "No app context");
        return;
    }

    snprintf(
        line,
        sizeof(line),
        "APRS 432.500 %s",
        app->mode == AprsRadioModeLabTx ? "LABTX" : "RX");
    aprs_draw_status_line(canvas, 8, line);

    snprintf(
        line,
        sizeof(line),
        "Arm:%s TX:%s %s",
        app->tx_armed ? "yes" : "no",
        app->tx_running ? "busy" : "idle",
        app->tx_allowed_in_region ? app->region_name : "LOCK");
    aprs_draw_status_line(canvas, 16, line);

    snprintf(line, sizeof(line), "RSSI:%3d LQI:%3u", (int)app->last_rssi, app->last_lqi);
    aprs_draw_status_line(canvas, 24, line);

    snprintf(
        line,
        sizeof(line),
        "Edges:%lu %c%luus",
        (unsigned long)app->rx_edge_count,
        app->last_edge_level ? 'H' : 'L',
        (unsigned long)app->last_edge_duration_us);
    aprs_draw_status_line(canvas, 32, line);

    snprintf(
        line,
        sizeof(line),
        "Frame:%uB Bits:%lu",
        (unsigned int)app->frame_size,
        (unsigned long)app->tx_bit_count);
    aprs_draw_status_line(canvas, 40, line);

    aprs_draw_status_line(canvas, 48, app->last_status);
    aprs_draw_status_line(canvas, 56, app->last_tx_hex);
    aprs_draw_status_line(canvas, 63, "L arm R mode hold OK tx");
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

static void aprs_handle_press(APRSHelloWorldCleanApp* app, InputKey key) {
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
    case InputKeyUp:
        aprs_refresh_frame(app);
        FURI_LOG_I(TAG, "Frame regenerated manually");
        break;
    case InputKeyDown:
        app->rx_edge_count = 0U;
        app->last_edge_duration_us = 0U;
        app->last_edge_level = false;
        aprs_set_status(app, "RX counters reset");
        FURI_LOG_I(TAG, "RX counters reset");
        break;
    default:
        break;
    }
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
            running = false;
            continue;
        }

        if(event.input.key == InputKeyOk && event.input.type == InputTypeLong) {
            aprs_radio_start_tx(app);
            continue;
        }

        if(event.input.type == InputTypePress) {
            aprs_handle_press(app, event.input.key);
        }
    }

    aprs_hello_world_clean_app_free(app);
    return 0;
}
