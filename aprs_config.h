#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APRS_CFG_CALL_LEN        7U
#define APRS_CFG_STATUS_MAX      62U
#define APRS_CFG_POS_STR_LEN     16U
#define APRS_CONFIG_FILE_PATH    "/ext/aprs/config.cfg"
#define APRS_CONFIG_VERSION      1U

typedef struct {
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
    uint16_t cfg_bearing_deg;
    uint16_t cfg_speed;
    char cfg_status_text[APRS_CFG_STATUS_MAX + 1U];
} AprsConfig;

/**
 * Load APRS configuration from storage.
 * Returns default config if file doesn't exist.
 */
AprsConfig aprs_config_load(void);

/**
 * Save APRS configuration to storage.
 */
bool aprs_config_save(const AprsConfig* config);

/**
 * Get default configuration.
 */
AprsConfig aprs_config_default(void);
