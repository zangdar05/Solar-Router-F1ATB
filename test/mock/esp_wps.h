// Mock WPS
#pragma once
#include <Arduino.h>
#define WPS_TYPE_PBC 1
typedef struct {
  int wps_type;
} esp_wps_config_t;
#define WPS_CONFIG_INIT_DEFAULT(type) { type }
inline esp_err_t esp_wifi_wps_enable(const esp_wps_config_t *c) { (void)c; return ESP_OK; }
inline esp_err_t esp_wifi_wps_start(int t) { (void)t; return ESP_OK; }
