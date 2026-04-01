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
} AprsAx25AddressConfig;

size_t aprs_ax25_make_mystatus(
    char* buffer,
    size_t buffer_size,
    uint32_t frequency_hz,
    bool tx_armed);

size_t aprs_ax25_encode_status_frame(
    const AprsAx25AddressConfig* config,
    const char* status_text,
    uint8_t* out,
    size_t out_size);

bool aprs_ax25_build_nrzi_stream(
    const uint8_t* frame,
    size_t frame_size,
    uint8_t preamble_flags,
    uint8_t postamble_flags,
    bool* out_levels,
    size_t out_level_capacity,
    size_t* out_level_count);
