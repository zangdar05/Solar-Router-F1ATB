// Mock PubSubClient : journalise chaque publish dans mock_mqtt_published.
#pragma once
#include <Arduino.h>
#include <WiFiClient.h>
#include <vector>
#include <string>

struct MqttMsg {
  std::string topic;
  std::string payload;
  bool retained;
};

extern std::vector<MqttMsg> mock_mqtt_published;
extern std::vector<std::string> mock_mqtt_subscribed;
extern bool mock_mqtt_connected;     // état retourné par connected()
extern bool mock_mqtt_connect_ok;    // résultat de connect()

typedef void (*MQTT_CALLBACK_SIGNATURE_T)(char *, uint8_t *, unsigned int);

class PubSubClient {
public:
  MQTT_CALLBACK_SIGNATURE_T cb = nullptr;
  std::string server;
  uint16_t port = 1883;
  uint16_t bufferSize = 256;

  PubSubClient() {}
  PubSubClient(WiFiClient &c) { (void)c; }

  PubSubClient &setServer(const char *h, uint16_t p) { server = h ? h : ""; port = p; return *this; }
  PubSubClient &setCallback(MQTT_CALLBACK_SIGNATURE_T f) { cb = f; return *this; }
  bool setBufferSize(uint16_t n) { bufferSize = n; return true; }
  bool connected() { return mock_mqtt_connected; }
  void loop() {}
  void disconnect() { mock_mqtt_connected = false; }

  bool connect(const char *id, const char *user = nullptr, const char *pass = nullptr,
               const char *willTopic = nullptr, uint8_t willQos = 0, bool willRetain = false,
               const char *willMessage = nullptr) {
    (void)id; (void)user; (void)pass; (void)willTopic; (void)willQos; (void)willRetain; (void)willMessage;
    if (mock_mqtt_connect_ok) mock_mqtt_connected = true;
    return mock_mqtt_connect_ok;
  }
  bool publish(const char *topic, const char *payload, bool retained = false) {
    mock_mqtt_published.push_back({ topic ? topic : "", payload ? payload : "", retained });
    return true;
  }
  bool publish(const char *topic, const uint8_t *payload, unsigned int len, bool retained = false) {
    mock_mqtt_published.push_back({ topic ? topic : "", std::string((const char *)payload, len), retained });
    return true;
  }
  bool subscribe(const char *topic, uint8_t qos = 0) {
    (void)qos;
    mock_mqtt_subscribed.push_back(topic ? topic : "");
    return true;
  }
  bool unsubscribe(const char *topic) { (void)topic; return true; }

  // Aide au test : simule la réception d'un message sur un topic souscrit
  void mock_deliver(const char *topic, const char *payload) {
    if (!cb) return;
    std::string t(topic), p(payload);
    cb(&t[0], (uint8_t *)p.data(), (unsigned int)p.size());
  }
};
