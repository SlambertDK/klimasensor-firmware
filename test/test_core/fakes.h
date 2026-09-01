// In-memory-fakes af hardware-interfaces til native unit-tests.
#pragma once
#include "IQueueStorage.h"
#include "IKvStore.h"
#include "ISensors.h"
#include "INet.h"
#include <vector>
#include <map>
#include <string>
#include <cstring>

class MemStorage : public core::IQueueStorage {
 public:
  explicit MemStorage(size_t cap = 1024 * 1024) : cap_(cap) {}
  bool append(const uint8_t* data, size_t len) override {
    if (buf_.size() + len > cap_) return false;
    buf_.insert(buf_.end(), data, data + len);
    return true;
  }
  bool read(size_t offset, uint8_t* out, size_t len) override {
    if (offset + len > buf_.size()) return false;
    memcpy(out, buf_.data() + offset, len);
    return true;
  }
  size_t size() override { return buf_.size(); }
  size_t capacity() override { return cap_; }
  bool truncate() override { buf_.clear(); return true; }

  std::vector<uint8_t> buf_;
  size_t cap_;
};

class MemKv : public core::IKvStore {
 public:
  uint32_t getU32(const char* key, uint32_t def) override {
    auto it = u32_.find(key);
    return it == u32_.end() ? def : it->second;
  }
  void setU32(const char* key, uint32_t v) override { u32_[key] = v; }
  int64_t getI64(const char* key, int64_t def) override {
    auto it = i64_.find(key);
    return it == i64_.end() ? def : it->second;
  }
  void setI64(const char* key, int64_t v) override { i64_[key] = v; }
  bool getStr(const char* key, char* out, size_t maxLen) override {
    auto it = str_.find(key);
    if (it == str_.end()) return false;
    snprintf(out, maxLen, "%s", it->second.c_str());
    return true;
  }
  void setStr(const char* key, const char* v) override { str_[key] = v; }

  std::map<std::string, uint32_t> u32_;
  std::map<std::string, int64_t> i64_;
  std::map<std::string, std::string> str_;
};

class FakeSensors : public core::ISensors {
 public:
  void measure(core::Record& r) override {
    r.tempCx100 = next.tempCx100;
    r.rhX100 = next.rhX100;
    r.pm1_0x10 = next.pm1_0x10;
    r.pm2_5x10 = next.pm2_5x10;
    r.pm4_0x10 = next.pm4_0x10;
    r.pm10x10 = next.pm10x10;
    r.luxX100 = next.luxX100;
    r.axMg = next.axMg;
    r.ayMg = next.ayMg;
    r.azMg = next.azMg;
    measureCalls++;
  }
  core::Record next;
  int measureCalls = 0;
};

class FakeNet : public core::INet {
 public:
  bool bringUp() override { bringUpCalls++; return bringUpOk; }
  bool ntpTime(const char* server, int64_t& epochOut) override {
    ntpCalls++;
    lastNtpServer = server;
    epochOut = ntpEpoch;
    return ntpOk;
  }
  bool httpPost(const char* url, const std::string& payload,
                std::string& responseOut) override {
    posts.push_back(payload);
    lastUrl = url;
    if (responses.empty()) return false;
    responseOut = responses.front();
    responses.erase(responses.begin());
    return true;
  }
  bool otaUpdate(const char* url, const char* sha256Hex) override {
    otaUrls.push_back(url);
    return false;  // i tests "fejler" OTA, så cyklussen fortsætter
  }
  void shutDown() override { shutDownCalls++; }

  bool bringUpOk = true;
  bool ntpOk = true;
  int64_t ntpEpoch = 1756713600;  // 2025-09-01T08:00:00Z
  int bringUpCalls = 0, ntpCalls = 0, shutDownCalls = 0;
  std::string lastNtpServer, lastUrl;
  std::vector<std::string> posts;
  std::vector<std::string> responses;
  std::vector<std::string> otaUrls;
};
