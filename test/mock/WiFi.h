// Mock WiFi : valeurs neutres, aucune connexion réelle.
#pragma once
#include <Arduino.h>
#include <IPAddress.h>
#include <WiFiClient.h>

#define WL_CONNECTED 3
#define WL_IDLE_STATUS 0
#define WL_NO_SSID_AVAIL 1
#define WL_CONNECT_FAILED 4
#define WL_DISCONNECTED 6

#define WIFI_OFF 0
#define WIFI_STA 1
#define WIFI_AP 2
#define WIFI_AP_STA 3
typedef int wifi_mode_t;

#define WIFI_CONNECT_AP_BY_SIGNAL 1
#define WIFI_ALL_CHANNEL_SCAN 1
#define WIFI_AUTH_OPEN 0

typedef int wifi_auth_mode_t;
typedef int WiFiEvent_t;
#define ARDUINO_EVENT_WIFI_STA_GOT_IP 4
#define ARDUINO_EVENT_WPS_ER_SUCCESS 30
#define ARDUINO_EVENT_WIFI_STA_DISCONNECTED 5
typedef union {
  int dummy;
} arduino_event_info_t;

class WiFiClassMock {
public:
  int _mode = WIFI_STA;
  void mode(int m) { _mode = m; }
  int getMode() { return _mode; }
  void disconnect(bool a = false, bool b = false) { (void)a; (void)b; }
  void reconnect() {}
  int status() { return WL_DISCONNECTED; }
  bool begin(const char *s = nullptr, const char *p = nullptr, int ch = 0, const uint8_t *bssid = nullptr) {
    (void)s; (void)p; (void)ch; (void)bssid; return false;
  }
  bool config(IPAddress, IPAddress, IPAddress, IPAddress a = IPAddress(), IPAddress b = IPAddress()) { return true; }
  bool hostname(const String &) { return true; }
  bool setSleep(bool) { return true; }
  void setSortMethod(int) {}
  void setScanMethod(int) {}
  bool enableIPv6(bool en = true) { (void)en; return true; }
  int waitForConnectResult(unsigned long t = 0) { (void)t; return WL_DISCONNECTED; }
  IPAddress localIP() { return IPAddress(192, 168, 1, 50); }
  IPAddress gatewayIP() { return IPAddress(192, 168, 1, 1); }
  IPAddress subnetMask() { return IPAddress(255, 255, 255, 0); }
  IPAddress softAPIP() { return IPAddress(192, 168, 4, 1); }
  IPv6Address globalIPv6() { return IPAddress(); }
  int RSSI(int i = 0) { (void)i; return -60; }
  String SSID(int i = -1) { (void)i; return String("MockAP"); }
  String psk() { return String("MockPsk"); }
  String BSSIDstr(int i = 0) { (void)i; return String("00:11:22:33:44:55"); }
  uint8_t *BSSID(int i = 0) {
    static uint8_t b[6] = { 0, 0x11, 0x22, 0x33, 0x44, 0x55 };
    (void)i; return b;
  }
  int channel(int i = 0) { (void)i; return 1; }
  int encryptionType(int i) { (void)i; return WIFI_AUTH_OPEN; }
  int scanNetworks(bool a = false, bool b = false) { (void)a; (void)b; return 0; }
  void scanDelete() {}
  bool softAP(const char *s = nullptr, const char *p = nullptr) { (void)s; (void)p; return true; }
  void onEvent(void (*cb)(WiFiEvent_t, arduino_event_info_t)) { (void)cb; }
  String macAddress() { return String("AA:BB:CC:DD:EE:FF"); }
  void macAddress(uint8_t *mac) {
    static const uint8_t m[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
    memcpy(mac, m, 6);
  }
};
extern WiFiClassMock WiFi;
