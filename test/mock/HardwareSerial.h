// Mock HardwareSerial : file d'entrée injectable par les tests (mock_feed)
#pragma once
#include <Arduino.h>
#include <deque>

#define SERIAL_5N1 0x8000010
#define SERIAL_8N1 0x800001c
#define SERIAL_7E1 0x800001a
#define SERIAL_8E1 0x800001e
#define SERIAL_8N2 0x800003c

class HardwareSerial : public Stream {
public:
  std::deque<char> rx;   // octets à lire par le firmware
  std::string tx;        // tout ce que le firmware a écrit
  bool started = false;
  unsigned long baud = 0;
  int cfg = 0;
  int pinRx = -1, pinTx = -1;

  HardwareSerial(int n = 0) { (void)n; }

  void begin(unsigned long b, int c = SERIAL_8N1, int rxPin = -1, int txPin = -1) {
    started = true; baud = b; cfg = c; pinRx = rxPin; pinTx = txPin;
  }
  void end() { started = false; }
  void setRxBufferSize(size_t) {}
  void setTxBufferSize(size_t) {}
  void setTimeout(unsigned long) {}

  int available() override { return (int)rx.size(); }
  int availableForWrite() { return 128; }
  int read() override {
    if (rx.empty()) return -1;
    char c = rx.front();
    rx.pop_front();
    return (unsigned char)c;
  }
  int peek() override { return rx.empty() ? -1 : (unsigned char)rx.front(); }
  void flush() override {}

  size_t write(uint8_t c) override { tx += (char)c; return 1; }
  size_t write(const uint8_t *b, size_t n) override { tx.append((const char *)b, n); return n; }
  size_t write(const char *str) { return write((const uint8_t *)str, strlen(str)); }

  // --- API de test ------------------------------------------------------
  void mock_feed(const std::string &data) { for (char c : data) rx.push_back(c); }
  void mock_clear() { rx.clear(); tx.clear(); }
};

extern HardwareSerial Serial;
