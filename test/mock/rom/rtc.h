// Mock raison de reset
#pragma once
#include <Arduino.h>
typedef enum {
  NO_MEAN = 0,
  POWERON_RESET = 1,
  SW_RESET = 3,
  OWDT_RESET = 4,
  DEEPSLEEP_RESET = 5,
  SDIO_RESET = 6,
  TG0WDT_SYS_RESET = 7,
  TG1WDT_SYS_RESET = 8,
  RTCWDT_SYS_RESET = 9,
  INTRUSION_RESET = 10,
  TGWDT_CPU_RESET = 11,
  SW_CPU_RESET = 12,
  RTCWDT_CPU_RESET = 13,
  EXT_CPU_RESET = 14,
  RTCWDT_BROWN_OUT_RESET = 15,
  RTCWDT_RTC_RESET = 16
} RESET_REASON;
inline RESET_REASON rtc_get_reset_reason(int core) { (void)core; return POWERON_RESET; }
