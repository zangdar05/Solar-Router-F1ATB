// Mock Update (OTA)
#pragma once
#include <Arduino.h>
#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF
#define U_FLASH 0
class UpdateMock {
public:
  bool begin(size_t s = UPDATE_SIZE_UNKNOWN, int cmd = U_FLASH) { (void)s; (void)cmd; return true; }
  size_t write(uint8_t *d, size_t n) { (void)d; return n; }
  bool end(bool evenIfRemaining = false) { (void)evenIfRemaining; return true; }
  bool hasError() { return false; }
  void printError(Print &p) { (void)p; }
  void printError(Stream &p) { (void)p; }
};
extern UpdateMock Update;
