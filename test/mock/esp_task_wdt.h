// Mock Watchdog
#pragma once
#include <Arduino.h>
typedef struct {
  uint32_t timeout_ms;
  uint32_t idle_core_mask;
  bool trigger_panic;
} esp_task_wdt_config_t;
inline esp_err_t esp_task_wdt_deinit() { return ESP_OK; }
inline esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t *c) { (void)c; return ESP_OK; }
inline esp_err_t esp_task_wdt_add(void *h) { (void)h; return ESP_OK; }
inline esp_err_t esp_task_wdt_reset() { return ESP_OK; }
