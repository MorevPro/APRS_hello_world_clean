#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char* source_call;
    uint8_t source_ssid;
    const char* destination_call;
    uint8_t destination_ssid;
    const char* path1_call;
    uint8_t path1_ssid;
    bool use_path1;
    const char* path2_call;
    uint8_t path2_ssid;
    bool use_path2;
    const char* status_text;
    const char* position_lat;
    const char* position_lon;
    uint16_t bearing_deg;
    uint16_t speed;
} AprsAx25AddressConfig;

size_t aprs_ax25_encode_status_frame(
    const AprsAx25AddressConfig* config,
    uint8_t* out,
    size_t out_size);

size_t aprs_ax25_encode_position_frame(
    const AprsAx25AddressConfig* config,
    uint8_t* out,
    size_t out_size);

/**
 * Returns true if coordinates are valid for position report.
 */
bool aprs_ax25_position_is_valid(const AprsAx25AddressConfig* config);

bool aprs_ax25_build_nrzi_stream(
    const uint8_t* frame,
    size_t frame_size,
    uint8_t preamble_flags,
    uint8_t postamble_flags,
    bool* out_levels,
    size_t out_level_capacity,
    size_t* out_level_count);
