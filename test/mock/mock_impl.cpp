// Définitions des objets et fonctions mockés (inclus une seule fois par firmware_host.cpp).
#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <FS.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <EthernetESP32.h>
#include <Wire.h>
#include <DallasTemperature.h>
#include <esp_stubs.h>
#include <cstdarg>

// --- Horloge ----------------------------------------------------------------
unsigned long mock_millis_value = 0;
uint64_t mock_micros_value = 0;
time_t mock_epoch = 0;

void mock_set_millis(unsigned long ms) { mock_millis_value = ms; mock_micros_value = (uint64_t)ms * 1000ULL; }
void mock_advance_millis(unsigned long ms) { mock_millis_value += ms; mock_micros_value += (uint64_t)ms * 1000ULL; }
unsigned long millis() { return mock_millis_value; }
unsigned long micros() { return (unsigned long)mock_micros_value; }
void delay(unsigned long ms) { mock_advance_millis(ms); }
void delayMicroseconds(unsigned int us) { mock_micros_value += us; }
int64_t esp_timer_get_time() { return (int64_t)mock_micros_value; }
void yield() {}

int mock_settimeofday(const struct timeval *tv, const void *tz) {
  (void)tz;
  if (tv) mock_epoch = (time_t)tv->tv_sec;
  return 0;
}
time_t mock_time(time_t *t) {
  if (t) *t = mock_epoch;
  return mock_epoch;
}

// --- GPIO -------------------------------------------------------------------
int mock_gpio_state[MOCK_GPIO_MAX];
int mock_gpio_mode[MOCK_GPIO_MAX];
int mock_gpio_writes[MOCK_GPIO_MAX];
int mock_analog_value = 2048;

void mock_reset_gpio() {
  for (int i = 0; i < MOCK_GPIO_MAX; i++) {
    mock_gpio_state[i] = -1;
    mock_gpio_mode[i] = -1;
    mock_gpio_writes[i] = 0;
  }
}
void pinMode(int pin, int mode) { if (pin >= 0 && pin < MOCK_GPIO_MAX) mock_gpio_mode[pin] = mode; }
void digitalWrite(int pin, int val) {
  if (pin >= 0 && pin < MOCK_GPIO_MAX) { mock_gpio_state[pin] = val; mock_gpio_writes[pin]++; }
}
int digitalRead(int pin) { return (pin >= 0 && pin < MOCK_GPIO_MAX && mock_gpio_state[pin] > 0) ? HIGH : LOW; }
int analogRead(int pin) { (void)pin; return mock_analog_value; }
void analogReadResolution(int) {}
void analogSetAttenuation(int) {}
void ledcWrite(int pin, int duty) {
  if (pin >= 0 && pin < MOCK_GPIO_MAX) { mock_gpio_state[pin] = duty; mock_gpio_writes[pin]++; }
}
bool ledcAttachChannel(int pin, uint32_t freq, uint8_t resolution, uint8_t channel) {
  (void)freq; (void)resolution; (void)channel;
  if (pin >= 0 && pin < MOCK_GPIO_MAX) mock_gpio_mode[pin] = OUTPUT;
  return true;
}
bool ledcAttach(int pin, uint32_t freq, uint8_t resolution) {
  return ledcAttachChannel(pin, freq, resolution, 0);
}
void attachInterrupt(int, void (*)(), int) {}
void detachInterrupt(int) {}
int digitalPinToInterrupt(int pin) { return pin; }
void noInterrupts() {}
void interrupts() {}

struct hw_timer_s { int dummy; };
static hw_timer_s mock_timers[4];
static int mock_timer_idx = 0;
hw_timer_t *timerBegin(uint32_t) { return &mock_timers[(mock_timer_idx++) & 3]; }
void timerAttachInterrupt(hw_timer_t *, void (*)()) {}
void timerAlarm(hw_timer_t *, uint64_t, bool, uint64_t) {}
void timerEnd(hw_timer_t *) {}

BaseType_t xTaskCreatePinnedToCore(void (*)(void *), const char *, uint32_t, void *,
                                   uint32_t, TaskHandle_t *handle, int) {
  if (handle) *handle = (TaskHandle_t)1;
  return 1;
}
void vTaskDelay(uint32_t) {}

size_t esp_get_free_internal_heap_size() { return 150000; }
size_t esp_get_minimum_free_heap_size() { return 120000; }
const char *esp_err_to_name(esp_err_t) { return "ESP_OK"; }

// --- Print / Stream ---------------------------------------------------------
size_t Print::printf(const char *fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n < 0) return 0;
  return write((const uint8_t *)buf, (size_t)(n < (int)sizeof(buf) ? n : (int)sizeof(buf) - 1));
}
String Stream::readStringUntil(char term) {
  String r;
  while (available() > 0) {
    int c = read();
    if (c < 0 || (char)c == term) break;
    r += (char)c;
  }
  return r;
}
String Stream::readString() {
  String r;
  while (available() > 0) {
    int c = read();
    if (c < 0) break;
    r += (char)c;
  }
  return r;
}

// --- Objets globaux ---------------------------------------------------------
ESPClass ESP;
int mock_restart_count = 0;
void ESPClass::restart() { mock_restart_count++; }

HardwareSerial Serial(0);
WiFiClassMock WiFi;
MDNSMock MDNS;
ArduinoOTAMock ArduinoOTA;
UpdateMock Update;
EthernetMock Ethernet;
TwoWire Wire;

bool mock_client_connect_ok = false;
std::string mock_client_response;
std::string mock_client_request;

String mock_last_http_headers;
String mock_last_http_body;
int mock_last_http_code = 0;
String mock_last_http_type;
std::map<std::string, std::string> mock_http_args;
std::map<std::string, std::string> mock_http_headers;

std::vector<MqttMsg> mock_mqtt_published;
std::vector<std::string> mock_mqtt_subscribed;
bool mock_mqtt_connected = false;
bool mock_mqtt_connect_ok = true;

int mock_ds18b20_count = 0;
float mock_ds18b20_temp[8] = { -127, -127, -127, -127, -127, -127, -127, -127 };

// --- Système de fichiers mémoire -------------------------------------------
std::map<std::string, std::string> mock_fs;
FSClass LittleFS;
FSClass SPIFFS;
void mock_fs_reset() { mock_fs.clear(); }

fs::File FSClass::open(const String &p, const char *mode, bool create) {
  (void)create;
  std::string path = norm(p);
  auto impl = std::make_shared<fs::FileImpl>();
  impl->path = path;

  if (path == "/") {  // ouverture du répertoire racine
    impl->dir = true;
    impl->valid = true;
    for (auto &f : mock_fs) impl->entries.push_back(f.first);
    return fs::File(impl);
  }
  std::string m = mode ? mode : "r";
  if (m[0] == 'w') {
    mock_fs[path] = "";
  } else if (m[0] == 'a') {
    if (!mock_fs.count(path)) mock_fs[path] = "";
  } else {
    if (!mock_fs.count(path)) return fs::File();  // invalide
  }
  impl->content = &mock_fs[path];
  impl->valid = true;
  impl->pos = (m[0] == 'a') ? impl->content->size() : 0;
  return fs::File(impl);
}

fs::File fs::File::openNextFile() {
  if (!impl || !impl->dir) return File();
  while (impl->entryIdx < impl->entries.size()) {
    std::string name = impl->entries[impl->entryIdx++];
    auto it = mock_fs.find(name);
    if (it == mock_fs.end()) continue;
    auto ni = std::make_shared<FileImpl>();
    ni->path = name;
    ni->content = &it->second;
    ni->valid = true;
    return File(ni);
  }
  return File();
}

// --- Bouchons écran ---------------------------------------------------------
int8_t NumPage = 0;
int8_t NbrPage = 7;
bool ScreenOn = true;
bool ReDraw = false;
static EcranFactice ecranFactice;
EcranFactice *lcd = &ecranFactice;
std::vector<std::string> mock_scroll;

void Ecran_Init(byte) {}
void Ecran_Loop() {}
void TraceMessages() {}
void GoPage(int) {}
void SetCouleurs() {}
void Init_LED_OLED(void) {}
void Gestion_LEDs() {}
void PrintScroll(String m) { mock_scroll.push_back(m.c_str()); }
