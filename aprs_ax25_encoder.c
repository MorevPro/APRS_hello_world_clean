#include "aprs_ax25_encoder.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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

static bool aprs_ax25_is_aprs_lat(const char* lat) {
    if(!lat || strlen(lat) != 8U) {
        return false;
    }

    return isdigit((unsigned char)lat[0]) && isdigit((unsigned char)lat[1]) &&
           isdigit((unsigned char)lat[2]) && isdigit((unsigned char)lat[3]) &&
           lat[4] == '.' && isdigit((unsigned char)lat[5]) &&
           isdigit((unsigned char)lat[6]) && (lat[7] == 'N' || lat[7] == 'S');
}

static bool aprs_ax25_is_aprs_lon(const char* lon) {
    if(!lon || strlen(lon) != 9U) {
        return false;
    }

    return isdigit((unsigned char)lon[0]) && isdigit((unsigned char)lon[1]) &&
           isdigit((unsigned char)lon[2]) && isdigit((unsigned char)lon[3]) &&
           isdigit((unsigned char)lon[4]) && lon[5] == '.' &&
           isdigit((unsigned char)lon[6]) && isdigit((unsigned char)lon[7]) &&
           (lon[8] == 'E' || lon[8] == 'W');
}

static bool aprs_ax25_parse_decimal_coordinate(
    const char* text,
    bool is_latitude,
    char* out,
    size_t out_size) {
    if(!text || !out || out_size == 0U) {
        return false;
    }

    while(*text == ' ') {
        text++;
    }

    size_t len = strlen(text);
    if(len < 2U) {
        return false;
    }

    char hemi = text[len - 1U];
    bool hemi_ok = is_latitude ? (hemi == 'N' || hemi == 'S') : (hemi == 'E' || hemi == 'W');
    if(!hemi_ok) {
        return false;
    }

    char numeric[24] = {0};
    size_t numeric_len = len - 1U;
    if(numeric_len >= sizeof(numeric)) {
        return false;
    }

    memcpy(numeric, text, numeric_len);
    numeric[numeric_len] = '\0';

    char* end = NULL;
    float degrees_value = strtof(numeric, &end);
    if(end == numeric || *end != '\0') {
        return false;
    }

    if(degrees_value < 0.0f) {
        degrees_value = -degrees_value;
    }

    float max_value = is_latitude ? 90.0f : 180.0f;
    if(degrees_value >= max_value) {
        return false;
    }

    uint16_t degrees = (uint16_t)degrees_value;
    float minutes_full = (degrees_value - (float)degrees) * 60.0f;
    uint16_t minutes = (uint16_t)minutes_full;
    uint16_t hundredths = (uint16_t)(((minutes_full - (float)minutes) * 100.0f) + 0.5f);

    if(hundredths >= 100U) {
        hundredths = 0U;
        minutes++;
    }

    if(minutes >= 60U) {
        minutes = 0U;
        degrees++;
    }

    int written = 0;
    if(is_latitude) {
        written = snprintf(out, out_size, "%02u%02u.%02u%c", degrees, minutes, hundredths, hemi);
    } else {
        written = snprintf(out, out_size, "%03u%02u.%02u%c", degrees, minutes, hundredths, hemi);
    }

    return written > 0 && (size_t)written < out_size;
}

static bool aprs_ax25_format_position(
    const AprsAx25AddressConfig* config,
    char* lat_out,
    size_t lat_out_size,
    char* lon_out,
    size_t lon_out_size) {
    if(!config || !config->position_lat || !config->position_lon) {
        return false;
    }

    const char* lat = config->position_lat;
    const char* lon = config->position_lon;

    while(*lat == ' ') {
        lat++;
    }
    while(*lon == ' ') {
        lon++;
    }

    if(strcmp(lat, "00.0000N") == 0 && strcmp(lon, "000.0000E") == 0) {
        return false;
    }

    if(aprs_ax25_is_aprs_lat(lat) && aprs_ax25_is_aprs_lon(lon)) {
        if(snprintf(lat_out, lat_out_size, "%s", lat) <= 0) {
            return false;
        }
        if(snprintf(lon_out, lon_out_size, "%s", lon) <= 0) {
            return false;
        }
        return true;
    }

    return aprs_ax25_parse_decimal_coordinate(lat, true, lat_out, lat_out_size) &&
           aprs_ax25_parse_decimal_coordinate(lon, false, lon_out, lon_out_size);
}

bool aprs_ax25_position_is_valid(const AprsAx25AddressConfig* config) {
    char aprs_lat[9] = {0};
    char aprs_lon[10] = {0};

    return aprs_ax25_format_position(
        config, aprs_lat, sizeof(aprs_lat), aprs_lon, sizeof(aprs_lon));
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

    char aprs_lat[9] = {0};
    char aprs_lon[10] = {0};

    if(!aprs_ax25_format_position(config, aprs_lat, sizeof(aprs_lat), aprs_lon, sizeof(aprs_lon))) {
        return 0;
    }

    char position_info[128] = {0};
    const char* status = config->status_text ? config->status_text : "";

    int pos_len = snprintf(
        position_info,
        sizeof(position_info),
        "!%s/%s-%s",
        aprs_lat,
        aprs_lon,
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
