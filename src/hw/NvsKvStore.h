// NvsKvStore — IKvStore på ESP32's NVS via Preferences.
// NB: NVS-nøgler er maks. 15 tegn — alle nøgler i core/ overholder det.
#pragma once
#include "IKvStore.h"
#include <Preferences.h>

namespace hw {

class NvsKvStore : public core::IKvStore {
 public:
  NvsKvStore() { prefs_.begin("klima", false); }
  ~NvsKvStore() { prefs_.end(); }

  uint32_t getU32(const char* key, uint32_t def) override {
    return prefs_.getUInt(key, def);
  }
  void setU32(const char* key, uint32_t v) override { prefs_.putUInt(key, v); }
  int64_t getI64(const char* key, int64_t def) override {
    return prefs_.getLong64(key, def);
  }
  void setI64(const char* key, int64_t v) override { prefs_.putLong64(key, v); }
  bool getStr(const char* key, char* out, size_t maxLen) override {
    if (!prefs_.isKey(key)) return false;
    prefs_.getString(key, out, maxLen);
    return true;
  }
  void setStr(const char* key, const char* v) override { prefs_.putString(key, v); }

 private:
  Preferences prefs_;
};

}  // namespace hw
