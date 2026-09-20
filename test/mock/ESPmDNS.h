// Mock mDNS
#pragma once
#include <Arduino.h>
#include <IPAddress.h>
class MDNSMock {
public:
  bool begin(const char *h) { (void)h; return true; }
  bool begin(const String &h) { (void)h; return true; }
  void addService(const char *a, const char *b, uint16_t p) { (void)a; (void)b; (void)p; }
  IPAddress queryHost(const String &h, uint32_t t = 2000) { (void)h; (void)t; return IPAddress(); }
  int queryService(const char *a, const char *b) { (void)a; (void)b; return 0; }
  void end() {}
};
extern MDNSMock MDNS;
