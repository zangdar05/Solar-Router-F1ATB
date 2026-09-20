// Mock Arduino/ESP32 minimal pour compilation et test du firmware sur PC.
// Uniquement ce qui est utilisé par le routeur F1ATB.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>
#include <type_traits>
#include <sys/time.h>

// mingw ne fournit pas localtime_r / gmtime_r
#ifndef localtime_r
inline struct tm *localtime_r(const time_t *t, struct tm *out) {
  struct tm *r = localtime(t);
  if (!r) return nullptr;
  *out = *r;
  return out;
}
inline struct tm *gmtime_r(const time_t *t, struct tm *out) {
  struct tm *r = gmtime(t);
  if (!r) return nullptr;
  *out = *r;
  return out;
}
#endif

// ---------------------------------------------------------------------------
// Types et macros ESP32
// ---------------------------------------------------------------------------
#define IRAM_ATTR
#define PROGMEM
#define PGM_P const char *
#define F(x) (x)
#define pgm_read_byte(a) (*(const unsigned char *)(a))
// NB: on ne definit PAS ARDUINO : ArduinoJson activerait PROGMEM/Print/Stream.

typedef uint8_t byte;
typedef bool boolean;

#define HIGH 1
#define LOW 0
#define INPUT 0x01
#define OUTPUT 0x03
#define INPUT_PULLUP 0x05
#define RISING 0x01
#define FALLING 0x02
#define CHANGE 0x03
#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

typedef int esp_err_t;
#define ESP_OK 0
typedef void *TaskHandle_t;
typedef struct hw_timer_s hw_timer_t;
#define portNUM_PROCESSORS 2
#define SOC_GPIO_PIN_COUNT 40

// ---------------------------------------------------------------------------
// String compatible Arduino, construite sur std::string
// ---------------------------------------------------------------------------
class String {
public:
  std::string s;

  String() {}
  String(const char *p) : s(p ? p : "") {}
  String(const std::string &p) : s(p) {}
  String(const String &o) : s(o.s) {}
  String(char c) : s(1, c) {}

  String(unsigned char v, int base = 10) { fromInt((long long)v, base); }
  String(short v, int base = 10) { fromInt((long long)v, base); }
  String(unsigned short v, int base = 10) { fromInt((long long)v, base); }
  String(int v, int base = 10) { fromInt((long long)v, base); }
  String(unsigned int v, int base = 10) { fromUInt((unsigned long long)v, base); }
  String(long v, int base = 10) { fromInt((long long)v, base); }
  String(unsigned long v, int base = 10) { fromUInt((unsigned long long)v, base); }
  String(long long v, int base = 10) { fromInt(v, base); }
  String(unsigned long long v, int base = 10) { fromUInt(v, base); }
  String(float v, int dec = 2) { fromDouble((double)v, dec); }
  String(double v, int dec = 2) { fromDouble(v, dec); }

  String &operator=(const String &o) { s = o.s; return *this; }
  String &operator=(const char *p) { s = p ? p : ""; return *this; }
  String &operator=(char c) { s.assign(1, c); return *this; }

  // --- Accès -----------------------------------------------------------
  unsigned int length() const { return (unsigned int)s.size(); }
  bool isEmpty() const { return s.empty(); }
  const char *c_str() const { return s.c_str(); }
  char charAt(unsigned int i) const { return i < s.size() ? s[i] : 0; }
  void setCharAt(unsigned int i, char c) { if (i < s.size()) s[i] = c; }
  char operator[](int i) const { return (i >= 0 && (size_t)i < s.size()) ? s[i] : 0; }
  char &operator[](int i) { static char dummy = 0; if (i >= 0 && (size_t)i < s.size()) return s[i]; dummy = 0; return dummy; }
  void reserve(unsigned int n) { s.reserve(n); }
  void toCharArray(char *buf, unsigned int size, unsigned int index = 0) const {
    if (!buf || size == 0) return;
    unsigned int n = 0;
    for (unsigned int i = index; i < s.size() && n + 1 < size; i++) buf[n++] = s[i];
    buf[n] = 0;
  }

  // --- Recherche -------------------------------------------------------
  int indexOf(char c, unsigned int from = 0) const {
    if (from > s.size()) return -1;
    size_t p = s.find(c, from);
    return p == std::string::npos ? -1 : (int)p;
  }
  int indexOf(const char *t, unsigned int from = 0) const {
    if (from > s.size()) return -1;
    size_t p = s.find(t, from);
    return p == std::string::npos ? -1 : (int)p;
  }
  int indexOf(const String &t, unsigned int from = 0) const { return indexOf(t.s.c_str(), from); }
  int lastIndexOf(char c) const {
    size_t p = s.rfind(c);
    return p == std::string::npos ? -1 : (int)p;
  }
  int lastIndexOf(char c, unsigned int from) const {
    if (s.empty()) return -1;
    size_t p = s.rfind(c, from);
    return p == std::string::npos ? -1 : (int)p;
  }
  int lastIndexOf(const char *t) const {
    size_t p = s.rfind(t);
    return p == std::string::npos ? -1 : (int)p;
  }
  int lastIndexOf(const String &t) const { return lastIndexOf(t.s.c_str()); }
  bool startsWith(const String &t) const { return s.rfind(t.s, 0) == 0; }
  bool startsWith(const char *t) const { return s.rfind(t, 0) == 0; }
  bool endsWith(const String &t) const {
    return s.size() >= t.s.size() && s.compare(s.size() - t.s.size(), t.s.size(), t.s) == 0;
  }
  bool endsWith(const char *t) const { return endsWith(String(t)); }
  bool equals(const String &t) const { return s == t.s; }
  bool equals(const char *t) const { return s == (t ? t : ""); }
  bool equalsIgnoreCase(const String &t) const {
    if (s.size() != t.s.size()) return false;
    for (size_t i = 0; i < s.size(); i++)
      if (tolower((unsigned char)s[i]) != tolower((unsigned char)t.s[i])) return false;
    return true;
  }

  // --- Extraction / modification ---------------------------------------
  String substring(unsigned int a) const {
    if (a >= s.size()) return String();
    return String(s.substr(a));
  }
  String substring(unsigned int a, unsigned int b) const {
    if (a > b) std::swap(a, b);
    if (a >= s.size()) return String();
    if (b > s.size()) b = (unsigned int)s.size();
    return String(s.substr(a, b - a));
  }
  void trim() {
    size_t b = 0, e = s.size();
    while (b < e && (unsigned char)s[b] <= ' ') b++;
    while (e > b && (unsigned char)s[e - 1] <= ' ') e--;
    s = s.substr(b, e - b);
  }
  void replace(const String &f, const String &r) {
    if (f.s.empty()) return;
    size_t p = 0;
    while ((p = s.find(f.s, p)) != std::string::npos) {
      s.replace(p, f.s.size(), r.s);
      p += r.s.size();
    }
  }
  void replace(char f, char r) { std::replace(s.begin(), s.end(), f, r); }
  void remove(unsigned int index) { if (index < s.size()) s.erase(index); }
  void remove(unsigned int index, unsigned int count) { if (index < s.size()) s.erase(index, count); }
  void toUpperCase() { for (auto &c : s) c = toupper((unsigned char)c); }
  void toLowerCase() { for (auto &c : s) c = tolower((unsigned char)c); }
  void clear() { s.clear(); }

  long toInt() const { return strtol(s.c_str(), nullptr, 10); }
  float toFloat() const { return (float)atof(s.c_str()); }
  double toDouble() const { return atof(s.c_str()); }

  // --- Concaténation ----------------------------------------------------
  bool concat(const char *p) { if (p) s += p; return true; }
  bool concat(const String &o) { s += o.s; return true; }
  bool concat(char c) { s += c; return true; }
  String &operator+=(const String &o) { s += o.s; return *this; }
  String &operator+=(const char *p) { if (p) s += p; return *this; }
  String &operator+=(char c) { s += c; return *this; }
  template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
  String &operator+=(T v) { s += String(v).s; return *this; }

private:
  void fromInt(long long v, int base) {
    char buf[72];
    if (base == 10) snprintf(buf, sizeof(buf), "%lld", v);
    else if (base == 16) snprintf(buf, sizeof(buf), "%llx", (unsigned long long)v);
    else if (base == 8) snprintf(buf, sizeof(buf), "%llo", (unsigned long long)v);
    else binaire(buf, sizeof(buf), (unsigned long long)v);
    s = buf;
  }
  void fromUInt(unsigned long long v, int base) {
    char buf[72];
    if (base == 10) snprintf(buf, sizeof(buf), "%llu", v);
    else if (base == 16) snprintf(buf, sizeof(buf), "%llx", v);
    else if (base == 8) snprintf(buf, sizeof(buf), "%llo", v);
    else binaire(buf, sizeof(buf), v);
    s = buf;
  }
  void binaire(char *buf, size_t n, unsigned long long v) {
    std::string r;
    if (v == 0) r = "0";
    while (v) { r.insert(r.begin(), char('0' + (v & 1))); v >>= 1; }
    snprintf(buf, n, "%s", r.c_str());
  }
  void fromDouble(double v, int dec) {
    char buf[64];
    if (dec < 0) dec = 0;
    snprintf(buf, sizeof(buf), "%.*f", dec, v);
    s = buf;
  }
};

inline String operator+(const String &a, const String &b) { String r(a); r += b; return r; }
inline String operator+(const String &a, const char *b) { String r(a); r += b; return r; }
inline String operator+(const char *a, const String &b) { String r(a); r += b; return r; }
inline String operator+(const String &a, char b) { String r(a); r += b; return r; }
inline String operator+(char a, const String &b) { String r(a); r += b; return r; }
template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
inline String operator+(const String &a, T b) { String r(a); r += String(b); return r; }
template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
inline String operator+(T a, const String &b) { String r = String(a); r += b; return r; }

inline bool operator==(const String &a, const String &b) { return a.s == b.s; }
inline bool operator==(const String &a, const char *b) { return a.s == (b ? b : ""); }
inline bool operator==(const char *a, const String &b) { return b == a; }
inline bool operator!=(const String &a, const String &b) { return !(a == b); }
inline bool operator!=(const String &a, const char *b) { return !(a == b); }
inline bool operator!=(const char *a, const String &b) { return !(b == a); }
inline bool operator<(const String &a, const String &b) { return a.s < b.s; }
inline bool operator>(const String &a, const String &b) { return a.s > b.s; }

typedef String StringSumHelper;

// ---------------------------------------------------------------------------
// Fonctions mathématiques Arduino (templates, pas de macros)
// ---------------------------------------------------------------------------
template <typename T, typename U>
inline typename std::common_type<T, U>::type min(T a, U b) {
  typedef typename std::common_type<T, U>::type R;
  return (R)a < (R)b ? (R)a : (R)b;
}
template <typename T, typename U>
inline typename std::common_type<T, U>::type max(T a, U b) {
  typedef typename std::common_type<T, U>::type R;
  return (R)a > (R)b ? (R)a : (R)b;
}
template <typename T, typename L, typename H>
inline typename std::common_type<T, L, H>::type constrain(T x, L lo, H hi) {
  typedef typename std::common_type<T, L, H>::type R;
  return (R)x < (R)lo ? (R)lo : ((R)x > (R)hi ? (R)hi : (R)x);
}
template <typename T>
inline T sq(T x) { return x * x; }
inline long map(long x, long a, long b, long c, long d) { return (x - a) * (d - c) / (b - a) + c; }
using std::abs;
using std::round;
using std::pow;
using std::sqrt;
using std::sin;
using std::cos;

// ---------------------------------------------------------------------------
// Horloge simulée
// ---------------------------------------------------------------------------
extern unsigned long mock_millis_value;
extern uint64_t mock_micros_value;
void mock_set_millis(unsigned long ms);
void mock_advance_millis(unsigned long ms);
unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
int64_t esp_timer_get_time();
void yield();

// Epoch simulée (settimeofday)
extern time_t mock_epoch;
int mock_settimeofday(const struct timeval *tv, const void *tz);
time_t mock_time(time_t *t);
#define settimeofday(a, b) mock_settimeofday(a, b)
#define time(a) mock_time(a)

// ---------------------------------------------------------------------------
// GPIO simulés : mock_gpio_state[pin] contient la dernière valeur écrite
// ---------------------------------------------------------------------------
#define MOCK_GPIO_MAX 64
extern int mock_gpio_state[MOCK_GPIO_MAX];
extern int mock_gpio_mode[MOCK_GPIO_MAX];
extern int mock_gpio_writes[MOCK_GPIO_MAX];  // nombre d'écritures
extern int mock_analog_value;
void mock_reset_gpio();

void pinMode(int pin, int mode);
void digitalWrite(int pin, int val);
int digitalRead(int pin);
int analogRead(int pin);
void analogReadResolution(int bits);
void analogSetAttenuation(int a);
void ledcWrite(int pin, int duty);
bool ledcAttachChannel(int pin, uint32_t freq, uint8_t resolution, uint8_t channel);
bool ledcAttach(int pin, uint32_t freq, uint8_t resolution);
void attachInterrupt(int pin, void (*isr)(), int mode);
void detachInterrupt(int pin);
int digitalPinToInterrupt(int pin);
void noInterrupts();
void interrupts();

// Timers matériels
hw_timer_t *timerBegin(uint32_t frequency);
void timerAttachInterrupt(hw_timer_t *t, void (*fn)());
void timerAlarm(hw_timer_t *t, uint64_t alarm, bool autoreload, uint64_t count);
void timerEnd(hw_timer_t *t);

// FreeRTOS
typedef int BaseType_t;
BaseType_t xTaskCreatePinnedToCore(void (*fn)(void *), const char *name, uint32_t stack,
                                   void *param, uint32_t prio, TaskHandle_t *handle, int core);
void vTaskDelay(uint32_t t);
#define portTICK_PERIOD_MS 1

// Heap
size_t esp_get_free_internal_heap_size();
size_t esp_get_minimum_free_heap_size();
const char *esp_err_to_name(esp_err_t e);

// ---------------------------------------------------------------------------
// Print / Stream (minimal)
// ---------------------------------------------------------------------------
class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t *buf, size_t size) {
    size_t n = 0;
    for (size_t i = 0; i < size; i++) n += write(buf[i]);
    return n;
  }
  size_t print(const String &v) { return write((const uint8_t *)v.c_str(), v.length()); }
  size_t print(const char *v) { return write((const uint8_t *)v, strlen(v)); }
  size_t print(char v) { return write((uint8_t)v); }
  template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
  size_t print(T v) { return print(String(v)); }
  template <typename T>
  size_t print(T v, int base) { return print(String(v, base)); }
  size_t println() { return print("\r\n"); }
  template <typename T>
  size_t println(T v) { return print(v) + println(); }
  template <typename T>
  size_t println(T v, int base) { return print(v, base) + println(); }
  size_t printf(const char *fmt, ...);
};

class Stream : public Print {
public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
  virtual void flush() {}
  virtual size_t readBytes(char *buf, size_t n) {
    size_t i = 0;
    while (i < n && available() > 0) buf[i++] = (char)read();
    return i;
  }
  String readStringUntil(char term);
  String readString();
  void setTimeout(unsigned long) {}
};

// ---------------------------------------------------------------------------
// Objet ESP
// ---------------------------------------------------------------------------
class ESPClass {
public:
  uint64_t getEfuseMac() { return 0x001122334455ULL; }
  const char *getChipModel() { return "ESP32-D0WDQ6"; }
  int getChipRevision() { return 3; }
  uint32_t getFreeHeap() { return 120000; }
  uint32_t getMinFreeHeap() { return 100000; }
  uint32_t getHeapSize() { return 320000; }
  uint32_t getFlashChipSize() { return 4194304; }
  uint32_t getCpuFreqMHz() { return 240; }
  void restart();
};
extern ESPClass ESP;
extern int mock_restart_count;
