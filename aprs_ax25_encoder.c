#include "aprs_ax25_encoder.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define APRS_AX25_CONTROL_UI  0x03U
#define APRS_AX25_PID_NO_L3   0xF0U
#define APRS_AX25_FLAG        0x7EU
#define APRS_AX25_ADDRESS_LEN 7U

static void aprs_ax25_crc_update(uint16_t* crc, uint8_t data) {
    *crc ^= data;

    for(uint8_t bit = 0; bit < 8; bit++) {
        if(*crc & 0x0001U) {
            *crc = (*crc >> 1U) ^ 0x8408U;
        } else {
            *crc >>= 1U;
        }
    }
}

static size_t aprs_ax25_write_address(
    uint8_t* out,
    size_t out_size,
    const char* callsign,
    uint8_t ssid,
    bool last) {
    if(out_size < APRS_AX25_ADDRESS_LEN) {
        return 0;
    }

    size_t call_len = callsign ? strlen(callsign) : 0;
    if(call_len > 6U) {
        call_len = 6U;
    }

    for(size_t i = 0; i < 6U; i++) {
        char ch = (i < call_len) ? callsign[i] : ' ';
        out[i] = ((uint8_t)toupper((unsigned char)ch)) << 1U;
    }

    out[6] = (uint8_t)(0x60U | ((ssid & 0x0FU) << 1U) | (last ? 0x01U : 0x00U));
    return APRS_AX25_ADDRESS_LEN;
}

size_t aprs_ax25_make_mystatus(
    char* buffer,
    size_t buffer_size,
    uint32_t frequency_hz,
    bool tx_armed) {
    if(!buffer || (buffer_size == 0U)) {
        return 0;
    }

    int written = snprintf(
        buffer,
        buffer_size,
        "Flipper Zero lab beacon %lu.%03lu MHz tx=%s",
        (unsigned long)(frequency_hz / 1000000UL),
        (unsigned long)((frequency_hz / 1000UL) % 1000UL),
        tx_armed ? "armed" : "safe");

    if(written < 0) {
        buffer[0] = '\0';
        return 0;
    }

    if((size_t)written >= buffer_size) {
        buffer[buffer_size - 1U] = '\0';
        return buffer_size - 1U;
    }

    return (size_t)written;
}

size_t aprs_ax25_encode_status_frame(
    const AprsAx25AddressConfig* config,
    const char* status_text,
    uint8_t* out,
    size_t out_size) {
    if(!config || !status_text || !out) {
        return 0;
    }

    const size_t status_len = strlen(status_text);
    const size_t path_count = config->use_path1 ? 1U : 0U;
    const size_t address_count = 2U + path_count;
    const size_t required_size =
        (address_count * APRS_AX25_ADDRESS_LEN) + 2U + 1U + status_len + 2U;

    if(required_size > out_size) {
        return 0;
    }

    size_t offset = 0U;

    offset += aprs_ax25_write_address(
        &out[offset],
        out_size - offset,
        config->destination_call,
        config->destination_ssid,
        false);
    offset += aprs_ax25_write_address(
        &out[offset],
        out_size - offset,
        config->source_call,
        config->source_ssid,
        !config->use_path1);

    if(config->use_path1) {
        offset += aprs_ax25_write_address(
            &out[offset],
            out_size - offset,
            config->path1_call,
            config->path1_ssid,
            true);
    }

    out[offset++] = APRS_AX25_CONTROL_UI;
    out[offset++] = APRS_AX25_PID_NO_L3;
    out[offset++] = '>';
    memcpy(&out[offset], status_text, status_len);
    offset += status_len;

    uint16_t crc = 0xFFFFU;
    for(size_t i = 0; i < offset; i++) {
        aprs_ax25_crc_update(&crc, out[i]);
    }

    crc ^= 0xFFFFU;
    out[offset++] = (uint8_t)(crc & 0x00FFU);
    out[offset++] = (uint8_t)((crc >> 8U) & 0x00FFU);

    return offset;
}

static bool aprs_ax25_emit_bit(
    bool level,
    bool* out_levels,
    size_t out_level_capacity,
    size_t* out_level_count) {
    if(*out_level_count >= out_level_capacity) {
        return false;
    }

    out_levels[*out_level_count] = level;
    (*out_level_count)++;
    return true;
}

static bool aprs_ax25_emit_byte_nrzi(
    uint8_t value,
    bool bit_stuffing,
    bool* current_level,
    uint8_t* consecutive_ones,
    bool* out_levels,
    size_t out_level_capacity,
    size_t* out_level_count) {
    for(uint8_t bit_index = 0; bit_index < 8U; bit_index++) {
        const bool bit = ((value >> bit_index) & 0x01U) != 0U;

        if(!bit) {
            *current_level = !(*current_level);
            *consecutive_ones = 0U;
        } else if(bit_stuffing) {
            (*consecutive_ones)++;
        }

        if(!aprs_ax25_emit_bit(
               *current_level, out_levels, out_level_capacity, out_level_count)) {
            return false;
        }

        if(bit_stuffing && bit && (*consecutive_ones == 5U)) {
            *current_level = !(*current_level);
            if(!aprs_ax25_emit_bit(
                   *current_level, out_levels, out_level_capacity, out_level_count)) {
                return false;
            }
            *consecutive_ones = 0U;
        }
    }

    if(!bit_stuffing) {
        *consecutive_ones = 0U;
    }

    return true;
}

bool aprs_ax25_build_nrzi_stream(
    const uint8_t* frame,
    size_t frame_size,
    uint8_t preamble_flags,
    uint8_t postamble_flags,
    bool* out_levels,
    size_t out_level_capacity,
    size_t* out_level_count) {
    if(!frame || !out_levels || !out_level_count || (frame_size == 0U)) {
        return false;
    }

    bool current_level = true;
    uint8_t consecutive_ones = 0U;
    *out_level_count = 0U;

    for(uint8_t i = 0; i < preamble_flags; i++) {
        if(!aprs_ax25_emit_byte_nrzi(
               APRS_AX25_FLAG,
               false,
               &current_level,
               &consecutive_ones,
               out_levels,
               out_level_capacity,
               out_level_count)) {
            return false;
        }
    }

    for(size_t i = 0; i < frame_size; i++) {
        if(!aprs_ax25_emit_byte_nrzi(
               frame[i],
               true,
               &current_level,
               &consecutive_ones,
               out_levels,
               out_level_capacity,
               out_level_count)) {
            return false;
        }
    }

    for(uint8_t i = 0; i < postamble_flags; i++) {
        if(!aprs_ax25_emit_byte_nrzi(
               APRS_AX25_FLAG,
               false,
               &current_level,
               &consecutive_ones,
               out_levels,
               out_level_capacity,
               out_level_count)) {
            return false;
        }
    }

    return true;
}
