// Mock esp_wifi (couche IDF)
#pragma once
#include <Arduino.h>
inline esp_err_t esp_wifi_connect() { return ESP_OK; }
inline esp_err_t esp_wifi_wps_disable() { return ESP_OK; }
