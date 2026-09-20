// Mock ArduinoOTA
#pragma once
#include <Arduino.h>
class ArduinoOTAMock {
public:
  void setHostname(const char *h) { (void)h; }
  void setPassword(const char *p) { (void)p; }
  void begin() {}
  void handle() {}
  ArduinoOTAMock &onStart(void (*f)()) { (void)f; return *this; }
  ArduinoOTAMock &onEnd(void (*f)()) { (void)f; return *this; }
};
extern ArduinoOTAMock ArduinoOTA;
