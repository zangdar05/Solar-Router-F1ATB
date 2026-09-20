// Mock Ethernet ESP32
#pragma once
#include <Arduino.h>
#include <IPAddress.h>
#define ETH_PHY_LAN8720 0
#define LinkON 1
#define LinkOFF 0
class EMACDriver {
public:
  EMACDriver(int phy = 0, int a = 0, int b = 0, int c = 0) { (void)phy; (void)a; (void)b; (void)c; }
};
class EthernetMock {
public:
  void init(EMACDriver &d) { (void)d; }
  int begin() { return 0; }
  int begin(IPAddress ip, IPAddress dns, IPAddress gw, IPAddress sn) { (void)ip; (void)dns; (void)gw; (void)sn; return 0; }
  int linkStatus() { return LinkOFF; }
  IPAddress localIP() { return IPAddress(); }
  IPAddress gatewayIP() { return IPAddress(); }
  IPAddress subnetMask() { return IPAddress(); }
  void hostname(const String &h) { (void)h; }
  String macAddress() { return String("AA:BB:CC:DD:EE:00"); }
  void macAddress(uint8_t *mac) { memset(mac, 0, 6); }
};
extern EthernetMock Ethernet;
