// Mock WebServer : mémorise le dernier corps envoyé dans mock_last_http_body.
#pragma once
#include <Arduino.h>
#include <WiFiClient.h>
#include <map>

#define HTTP_GET 1
#define HTTP_POST 2
#define HTTP_ANY 255
typedef int HTTPMethod;

#define CONTENT_LENGTH_UNKNOWN ((size_t)-1)
#define UPLOAD_FILE_START 0
#define UPLOAD_FILE_WRITE 1
#define UPLOAD_FILE_END 2
#define UPLOAD_FILE_ABORTED 3

struct HTTPUpload {
  int status = UPLOAD_FILE_END;
  String filename;
  String name;
  String type;
  size_t totalSize = 0;
  size_t currentSize = 0;
  uint8_t buf[1] = { 0 };
};

// Etat observable par les tests
extern String mock_last_http_body;
extern String mock_last_http_headers;  // en-têtes "Nom: valeur" (une par ligne) ajoutés par sendHeader()
extern int mock_last_http_code;
extern String mock_last_http_type;
extern std::map<std::string, std::string> mock_http_args;
extern std::map<std::string, std::string> mock_http_headers;

class WebServer {
public:
  WebServer(int port = 80) { (void)port; }

  void on(const String &, void (*fn)()) { (void)fn; }
  void on(const String &, HTTPMethod, void (*fn)()) { (void)fn; }
  void on(const String &, HTTPMethod, void (*fn)(), void (*fn2)()) { (void)fn; (void)fn2; }
  void onNotFound(void (*fn)()) { (void)fn; }
  void onFileUpload(void (*fn)()) { (void)fn; }
  void begin(int port = 80) { (void)port; }
  void handleClient() {}
  void close() {}
  void stop() {}

  void send(int code, const char *type, const String &content) {
    mock_last_http_code = code;
    mock_last_http_type = type;
    mock_last_http_body = content;
  }
  void send(int code, const String &type, const String &content) { send(code, type.c_str(), content); }
  void send(int code, const char *type, const char *content) { send(code, type, String(content)); }
  void send_P(int code, const char *type, const char *content) { send(code, type, String(content)); }
  void send_P(int code, const char *type, const char *content, size_t len) {
    send(code, type, String(std::string(content, len)));
  }
  void sendContent(const String &c) { mock_last_http_body += c; }
  void setContentLength(size_t) {}
  void sendHeader(const String &n, const String &v, bool first = false) { (void)first; mock_last_http_headers += n + ": " + v + "\n"; }
  void collectHeaders(const char **h, size_t n) { (void)h; (void)n; }
  template <typename... T>
  void collectHeaders(T... t) { (void)sizeof...(t); }

  String arg(const String &name) {
    auto it = mock_http_args.find(name.c_str());
    return it == mock_http_args.end() ? String("") : String(it->second);
  }
  String arg(int i) {
    int k = 0;
    for (auto &p : mock_http_args) { if (k++ == i) return String(p.second); }
    return String("");
  }
  String argName(int i) {
    int k = 0;
    for (auto &p : mock_http_args) { if (k++ == i) return String(p.first); }
    return String("");
  }
  int args() { return (int)mock_http_args.size(); }
  bool hasArg(const String &name) { return mock_http_args.count(name.c_str()) > 0; }
  bool hasHeader(const String &name) { return mock_http_headers.count(name.c_str()) > 0; }
  String header(const String &name) {
    auto it = mock_http_headers.find(name.c_str());
    return it == mock_http_headers.end() ? String("") : String(it->second);
  }
  String uri() { return String("/"); }
  HTTPMethod method() { return HTTP_GET; }
  HTTPUpload &upload() { static HTTPUpload u; return u; }
  WiFiClient client() { static WiFiClient c; return c; }
};
