// Mock IPAddress
#pragma once
#include <Arduino.h>

class IPAddress {
public:
  uint8_t o[4] = { 0, 0, 0, 0 };
  IPAddress() {}
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) { o[0] = a; o[1] = b; o[2] = c; o[3] = d; }
  IPAddress(uint32_t v) { o[0] = v & 0xFF; o[1] = (v >> 8) & 0xFF; o[2] = (v >> 16) & 0xFF; o[3] = (v >> 24) & 0xFF; }
  operator uint32_t() const { return o[0] | (o[1] << 8) | (o[2] << 16) | ((uint32_t)o[3] << 24); }
  uint8_t operator[](int i) const { return o[i & 3]; }
  uint8_t &operator[](int i) { return o[i & 3]; }
  String toString() const {
    return String(o[0]) + "." + String(o[1]) + "." + String(o[2]) + "." + String(o[3]);
  }
  bool fromString(const String &s) {
    int a, b, c, d;
    if (sscanf(s.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d) == 4) {
      o[0] = a; o[1] = b; o[2] = c; o[3] = d;
      return true;
    }
    return false;
  }
  bool operator==(const IPAddress &x) const { return memcmp(o, x.o, 4) == 0; }
};
typedef IPAddress IPv6Address;
