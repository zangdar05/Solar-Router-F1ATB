// Mock WiFiClient : connect() échoue par défaut, réponse injectable par les tests.
#pragma once
#include <Arduino.h>
#include <IPAddress.h>
#include <deque>

// Pilotage global depuis les tests
extern bool mock_client_connect_ok;      // false par défaut : aucun réseau
extern std::string mock_client_response; // contenu servi en lecture après connect()
extern std::string mock_client_request;  // dernière requête écrite par le firmware

class WiFiClient : public Stream {
public:
  std::deque<char> rx;
  bool _connected = false;

  WiFiClient() {}
  virtual ~WiFiClient() {}

  int connect(const char *host, uint16_t port, int32_t timeout = 0) {
    (void)host; (void)port; (void)timeout;
    if (!mock_client_connect_ok) return 0;
    _connected = true;
    rx.clear();
    for (char c : mock_client_response) rx.push_back(c);
    return 1;
  }
  int connect(IPAddress ip, uint16_t port, int32_t timeout = 0) { return connect("ip", port, timeout); }
  uint8_t connected() { return _connected ? 1 : 0; }
  operator bool() const { return _connected; }
  void stop() { _connected = false; rx.clear(); }
  void setNoDelay(bool) {}
  void setTimeout(unsigned long) {}
  void setInsecure() {}
  void setCACert(const char *) {}
  IPAddress remoteIP() { return IPAddress(); }

  int available() override { return (int)rx.size(); }
  int read() override {
    if (rx.empty()) return -1;
    char c = rx.front(); rx.pop_front();
    return (unsigned char)c;
  }
  int read(uint8_t *buf, size_t n) {
    size_t i = 0;
    while (i < n && !rx.empty()) { buf[i++] = (uint8_t)rx.front(); rx.pop_front(); }
    return (int)i;
  }
  int peek() override { return rx.empty() ? -1 : (unsigned char)rx.front(); }
  void flush() override {}
  size_t write(uint8_t c) override { mock_client_request += (char)c; return 1; }
  size_t write(const uint8_t *b, size_t n) override { mock_client_request.append((const char *)b, n); return n; }
};

typedef WiFiClient NetworkClient;
typedef WiFiClient EthernetClient;

class WiFiClientSecure : public WiFiClient {};
typedef WiFiClientSecure NetworkClientSecure;

class WiFiServer {
public:
  uint16_t port = 0;
  WiFiServer(uint16_t p = 80) : port(p) {}
  void begin(uint16_t p = 0) { if (p) port = p; }
  void setNoDelay(bool) {}
  bool hasClient() { return false; }
  WiFiClient accept() { return WiFiClient(); }
  WiFiClient available() { return WiFiClient(); }
  void stop() {}
};
typedef WiFiServer NetworkServer;
