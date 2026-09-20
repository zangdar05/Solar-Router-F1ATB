// Mock SNTP
#pragma once
#include <Arduino.h>
inline void sntp_set_sync_interval(uint32_t i) { (void)i; }
inline uint32_t sntp_get_sync_interval() { return 10800000; }
inline void esp_sntp_servermode_dhcp(bool b) { (void)b; }
inline void sntp_set_time_sync_notification_cb(void (*cb)(struct timeval *)) { (void)cb; }
inline void configTzTime(const char *tz, const char *s1, const char *s2 = nullptr, const char *s3 = nullptr) {
  (void)tz; (void)s1; (void)s2; (void)s3;
}
