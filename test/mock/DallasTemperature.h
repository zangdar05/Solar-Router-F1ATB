// Mock DallasTemperature : mesures injectables par les tests
#pragma once
#include <Arduino.h>
#include <OneWire.h>
#define DEVICE_DISCONNECTED_C -127.0f

extern int mock_ds18b20_count;
extern float mock_ds18b20_temp[8];

class DallasTemperature {
public:
  DallasTemperature() {}
  DallasTemperature(OneWire *w) { (void)w; }
  void begin() {}
  void setResolution(uint8_t r) { (void)r; }
  int getDeviceCount() { return mock_ds18b20_count; }
  void requestTemperatures() {}
  float getTempCByIndex(int i) {
    return (i >= 0 && i < 8) ? mock_ds18b20_temp[i] : DEVICE_DISCONNECTED_C;
  }
};
