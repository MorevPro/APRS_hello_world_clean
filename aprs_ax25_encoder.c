#include "aprs_ax25_encoder.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define APRS_AX25_CONTROL_UI  0x03U
#define APRS_AX25_PID_NO_L3   0xF0U
#define APRS_AX25_FLAG        0x7EU
#define APRS_AX25_ADDRESS_LEN 7U
#define APRS_AX25_MAX_FRAME   255U

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

bool aprs_ax25_position_is_valid(const AprsAx25AddressConfig* config) {
    if(!config || !config->position_lat || !config->position_lon) {
        return false;
    }

    // Check if position is not all zeros/default
    const char* lat = config->position_lat;
    const char* lon = config->position_lon;

    // Reject default/zero coordinates (00.0000N and 000.0000E)
    if(strcmp(lat, "00.0000N") == 0 && strcmp(lon, "000.0000E") == 0) {
        return false;  // Default coordinates, not valid for transmission
    }

    // Simple validation: check length and presence of direction indicators (N/S, E/W)
    if(strlen(lat) > 0 && strlen(lon) > 0) {
        char lat_dir = lat[strlen(lat) - 1];
        char lon_dir = lon[strlen(lon) - 1];

        if((lat_dir == 'N' || lat_dir == 'S') && (lon_dir == 'E' || lon_dir == 'W')) {
            return true;
        }
    }

    return false;
}

size_t aprs_ax25_encode_status_frame(
    const AprsAx25AddressConfig* config,
    uint8_t* out,
    size_t out_size) {
    if(!config || !out) {
        return 0;
    }

    const char* status_text = config->status_text ? config->status_text : "";
    const size_t status_len = strlen(status_text);

    // Limit status to APRS standard
    const size_t max_status_len = 62U;
    const size_t actual_status_len = status_len > max_status_len ? max_status_len : status_len;

    // Calculate path count
    size_t path_count = 0;
    if(config->use_path1) path_count++;
    if(config->use_path2) path_count++;

    const size_t address_count = 2U + path_count; // destination + source + paths
    const size_t required_size =
        (address_count * APRS_AX25_ADDRESS_LEN) + 2U + 1U + actual_status_len + 2U;

    if(required_size > out_size || required_size > APRS_AX25_MAX_FRAME) {
        return 0;
    }

    size_t offset = 0U;

    // Write destination (NEVER last)
    offset += aprs_ax25_write_address(
        &out[offset],
        out_size - offset,
        config->destination_call,
        config->destination_ssid,
        false);

    // Determine if source is last (no paths remain)
    bool source_is_last = !(config->use_path1 || config->use_path2);
    
    // Write source
    offset += aprs_ax25_write_address(
        &out[offset],
        out_size - offset,
        config->source_call,
        config->source_ssid,
        source_is_last);

    // Write path1 if used
    if(config->use_path1) {
        bool path1_is_last = !config->use_path2;
        offset += aprs_ax25_write_address(
            &out[offset],
            out_size - offset,
            config->path1_call,
            config->path1_ssid,
            path1_is_last);
    }

    // Write path2 if used
    if(config->use_path2) {
        offset += aprs_ax25_write_address(
            &out[offset], out_size - offset, config->path2_call, config->path2_ssid, true);
    }

    // Control and PID
    out[offset++] = APRS_AX25_CONTROL_UI;
    out[offset++] = APRS_AX25_PID_NO_L3;

    // Info identifier for status ('>')
    out[offset++] = '>';

    // Copy status text
    memcpy(&out[offset], status_text, actual_status_len);
    offset += actual_status_len;

    // Calculate and write CRC
    uint16_t crc = 0xFFFFU;
    for(size_t i = 0; i < offset; i++) {
        aprs_ax25_crc_update(&crc, out[i]);
    }

    crc ^= 0xFFFFU;
    out[offset++] = (uint8_t)(crc & 0x00FFU);
    out[offset++] = (uint8_t)((crc >> 8U) & 0x00FFU);

    return offset;
}

size_t aprs_ax25_encode_position_frame(
    const AprsAx25AddressConfig* config,
    uint8_t* out,
    size_t out_size) {
    if(!config || !out) {
        return 0;
    }

    if(!aprs_ax25_position_is_valid(config)) {
        return 0;
    }

    // Build position info: !DDMM.hhN/DDDMM.hhE[/BRG/SPD]status
    // For now, simplified: !lat/lon/status
    char position_info[128] = {0};
    const char* status = config->status_text ? config->status_text : "";

    int pos_len = snprintf(
        position_info,
        sizeof(position_info),
        "!%s/%s/%s",
        config->position_lat,
        config->position_lon,
        status);

    if(pos_len <= 0 || pos_len >= (int)sizeof(position_info)) {
        return 0;
    }

    // Calculate path count
    size_t path_count = 0;
    if(config->use_path1) path_count++;
    if(config->use_path2) path_count++;

    const size_t address_count = 2U + path_count; // destination + source + paths
    const size_t required_size =
        (address_count * APRS_AX25_ADDRESS_LEN) + 2U + (size_t)pos_len + 2U;

    if(required_size > out_size || required_size > APRS_AX25_MAX_FRAME) {
        return 0;
    }

    size_t offset = 0U;

    // Write destination (NEVER last)
    offset += aprs_ax25_write_address(
        &out[offset],
        out_size - offset,
        config->destination_call,
        config->destination_ssid,
        false);

    // Determine if source is last (no paths remain)
    bool source_is_last = !(config->use_path1 || config->use_path2);
    
    // Write source
    offset += aprs_ax25_write_address(
        &out[offset],
        out_size - offset,
        config->source_call,
        config->source_ssid,
        source_is_last);

    // Write path1 if used
    if(config->use_path1) {
        bool path1_is_last = !config->use_path2;
        offset += aprs_ax25_write_address(
            &out[offset],
            out_size - offset,
            config->path1_call,
            config->path1_ssid,
            path1_is_last);
    }

    // Write path2 if used
    if(config->use_path2) {
        offset += aprs_ax25_write_address(
            &out[offset], out_size - offset, config->path2_call, config->path2_ssid, true);
    }

    // Control and PID
    out[offset++] = APRS_AX25_CONTROL_UI;
    out[offset++] = APRS_AX25_PID_NO_L3;

    // Copy position info
    memcpy(&out[offset], position_info, (size_t)pos_len);
    offset += (size_t)pos_len;

    // Calculate and write CRC
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

        if(!aprs_ax25_emit_bit(*current_level, out_levels, out_level_capacity, out_level_count)) {
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
