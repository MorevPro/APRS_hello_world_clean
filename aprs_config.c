#include "aprs_config.h"

#include <furi.h>
#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

#define TAG "APRS_CFG"

AprsConfig aprs_config_default(void) {
    AprsConfig config = {0};

    snprintf(config.cfg_source_call, sizeof(config.cfg_source_call), "MYCALL");
    config.cfg_source_ssid = 1U;

    snprintf(config.cfg_dest_call, sizeof(config.cfg_dest_call), "APRS");
    config.cfg_dest_ssid = 0U;

    snprintf(config.cfg_path1_call, sizeof(config.cfg_path1_call), "WIDE2");
    config.cfg_path1_ssid = 2U;
    config.cfg_use_path1 = true;

    config.cfg_use_path2 = false;
    snprintf(config.cfg_path2_call, sizeof(config.cfg_path2_call), "WIDE1");
    config.cfg_path2_ssid = 1U;

    snprintf(config.cfg_lat, sizeof(config.cfg_lat), "00.0000N");
    snprintf(config.cfg_lon, sizeof(config.cfg_lon), "000.0000E");

    config.cfg_bearing_deg = 0U;
    config.cfg_speed = 0U;

    snprintf(
        config.cfg_status_text,
        sizeof(config.cfg_status_text),
        "Flipper APRS cfg");

    return config;
}

bool aprs_config_save(const AprsConfig* config) {
    if(!config) {
        FURI_LOG_E(TAG, "Cannot save: config is NULL");
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        FURI_LOG_E(TAG, "Cannot open storage");
        return false;
    }

    // Ensure directory exists
    storage_common_mkdir(storage, "/ext/aprs");

    // Open file for writing
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, APRS_CONFIG_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(TAG, "Cannot open config file for writing: %s", APRS_CONFIG_FILE_PATH);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Write version
    uint8_t version = APRS_CONFIG_VERSION;
    if(storage_file_write(file, &version, 1) != 1) {
        FURI_LOG_E(TAG, "Failed to write version");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Write config struct
    if(storage_file_write(file, (const uint8_t*)config, sizeof(AprsConfig)) != sizeof(AprsConfig)) {
        FURI_LOG_E(TAG, "Failed to write config data");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    FURI_LOG_I(TAG, "Config saved successfully");
    return true;
}

AprsConfig aprs_config_load(void) {
    AprsConfig config = aprs_config_default();

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        FURI_LOG_W(TAG, "Cannot open storage, using defaults");
        furi_record_close(RECORD_STORAGE);
        return config;
    }

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, APRS_CONFIG_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_I(TAG, "Config file not found at '%s', using defaults", APRS_CONFIG_FILE_PATH);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return config;
    }

    FURI_LOG_I(TAG, "Config file found, reading...");

    // Read and verify version
    uint8_t version = 0;
    if(storage_file_read(file, &version, 1) != 1) {
        FURI_LOG_E(TAG, "Failed to read version");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return config;
    }

    if(version != APRS_CONFIG_VERSION) {
        FURI_LOG_W(TAG, "Config version mismatch: %u, expected %u", version, APRS_CONFIG_VERSION);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return config;
    }

    // Read config struct
    if(storage_file_read(file, (uint8_t*)&config, sizeof(AprsConfig)) != sizeof(AprsConfig)) {
        FURI_LOG_E(TAG, "Failed to read config data");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return aprs_config_default();
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    FURI_LOG_I(TAG, "Config loaded successfully from '%s'", APRS_CONFIG_FILE_PATH);
    FURI_LOG_I(TAG, "  Source: %s-%u", config.cfg_source_call, config.cfg_source_ssid);
    FURI_LOG_I(TAG, "  Status: '%s'", config.cfg_status_text);
    return config;
}
