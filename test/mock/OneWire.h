// Mock OneWire
#pragma once
#include <Arduino.h>
class OneWire {
public:
  int pin = -1;
  OneWire() {}
  OneWire(int p) : pin(p) {}
  void begin(int p) { pin = p; }
  uint8_t reset() { return 0; }
  void select(const uint8_t *) {}
  void skip() {}
  void write(uint8_t, uint8_t p = 0) { (void)p; }
  uint8_t read() { return 0; }
  bool search(uint8_t *, bool s = true) { (void)s; return false; }
  void reset_search() {}
};
