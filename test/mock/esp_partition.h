// Mock partitions
#pragma once
#include <Arduino.h>
#define ESP_PARTITION_TYPE_ANY 0xFF
#define ESP_PARTITION_SUBTYPE_ANY 0xFF
typedef struct {
  const char *label;
  int type;
  int subtype;
  uint32_t address;
  uint32_t size;
} esp_partition_t;
typedef const esp_partition_t **esp_partition_iterator_t;
inline esp_partition_iterator_t esp_partition_find(int t, int s, const char *l) {
  (void)t; (void)s; (void)l; return nullptr;
}
inline const esp_partition_t *esp_partition_get(esp_partition_iterator_t it) { return it ? *it : nullptr; }
inline esp_partition_iterator_t esp_partition_next(esp_partition_iterator_t it) { (void)it; return nullptr; }
inline void esp_partition_iterator_release(esp_partition_iterator_t it) { (void)it; }
