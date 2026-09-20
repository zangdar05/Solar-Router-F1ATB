// Mock système de fichiers : tout en mémoire (map nom -> contenu).
#pragma once
#include <Arduino.h>
#include <map>
#include <string>
#include <vector>
#include <memory>

#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"

// Contenu du "disque" : accessible directement par les tests.
extern std::map<std::string, std::string> mock_fs;
void mock_fs_reset();

namespace fs {

class FileImpl {
public:
  std::string path;
  std::string *content = nullptr;  // pointe dans mock_fs
  size_t pos = 0;
  bool dir = false;
  bool valid = false;
  std::vector<std::string> entries;  // pour un répertoire
  size_t entryIdx = 0;
};

class File : public Stream {
public:
  std::shared_ptr<FileImpl> impl;

  File() : impl(std::make_shared<FileImpl>()) {}
  explicit File(std::shared_ptr<FileImpl> i) : impl(i) {}

  operator bool() const { return impl && impl->valid; }
  bool isDirectory() const { return impl && impl->dir; }
  const char *name() const {
    static std::string n;
    if (!impl) return "";
    n = impl->path;
    if (!n.empty() && n[0] == '/') n = n.substr(1);
    return n.c_str();
  }
  const char *path() const { return impl ? impl->path.c_str() : ""; }
  size_t size() const { return (impl && impl->content) ? impl->content->size() : 0; }
  void close() { if (impl) impl->valid = false; }
  void seek(size_t p) { if (impl) impl->pos = p; }
  size_t position() const { return impl ? impl->pos : 0; }

  int available() override {
    if (!impl || !impl->content) return 0;
    return (int)(impl->content->size() - impl->pos);
  }
  int read() override {
    if (available() <= 0) return -1;
    return (unsigned char)(*impl->content)[impl->pos++];
  }
  int read(uint8_t *buf, size_t n) {
    size_t i = 0;
    while (i < n && available() > 0) buf[i++] = (uint8_t)read();
    return (int)i;
  }
  int peek() override {
    if (available() <= 0) return -1;
    return (unsigned char)(*impl->content)[impl->pos];
  }
  void flush() override {}
  size_t write(uint8_t c) override {
    if (!impl || !impl->content) return 0;
    impl->content->push_back((char)c);
    return 1;
  }
  size_t write(const uint8_t *b, size_t n) override {
    if (!impl || !impl->content) return 0;
    impl->content->append((const char *)b, n);
    return n;
  }
  File openNextFile();
};

}  // namespace fs

using fs::File;

class FSClass {
public:
  bool begin(bool format = false, const char *base = "/littlefs", uint8_t max = 10, const char *part = "spiffs") {
    (void)format; (void)base; (void)max; (void)part;
    return true;
  }
  void end() {}
  bool format() { mock_fs.clear(); return true; }
  bool exists(const String &p) { return mock_fs.count(norm(p)) > 0; }
  bool exists(const char *p) { return exists(String(p)); }
  bool remove(const String &p) { return mock_fs.erase(norm(p)) > 0; }
  bool remove(const char *p) { return remove(String(p)); }
  bool mkdir(const String &) { return true; }
  size_t totalBytes() { return 1048576; }
  size_t usedBytes() {
    size_t n = 0;
    for (auto &f : mock_fs) n += f.second.size();
    return n;
  }
  fs::File open(const String &p, const char *mode = FILE_READ, bool create = false);
  fs::File open(const char *p, const char *mode = FILE_READ, bool create = false) {
    return open(String(p), mode, create);
  }

private:
  static std::string norm(const String &p) {
    std::string s = p.c_str();
    if (s.empty() || s[0] != '/') s = "/" + s;
    return s;
  }
  friend class fs::File;
};
