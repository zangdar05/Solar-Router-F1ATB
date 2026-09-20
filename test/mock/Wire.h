// Mock Wire (I2C) : suffisant pour compiler les en-têtes tactiles.
#pragma once
#include <Arduino.h>
class TwoWire {
public:
  TwoWire(int n = 0) { (void)n; }
  bool begin(int sda = -1, int scl = -1, uint32_t freq = 0) { (void)sda; (void)scl; (void)freq; return true; }
  void beginTransmission(uint8_t a) { (void)a; }
  uint8_t endTransmission(bool stop = true) { (void)stop; return 1; }
  size_t write(uint8_t) { return 1; }
  size_t write(const uint8_t *, size_t n) { return n; }
  uint8_t requestFrom(uint8_t a, size_t n, bool stop = true) { (void)a; (void)n; (void)stop; return 0; }
  int available() { return 0; }
  int read() { return -1; }
  void setClock(uint32_t) {}
};
extern TwoWire Wire;
