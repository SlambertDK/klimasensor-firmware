#include "Config.h"
#include <cstring>
#include <cstdio>

namespace core {

namespace {
const char* KEY_MEASURE = "cfg_measure";
const char* KEY_UPLOAD = "cfg_upload";
const char* KEY_NTP = "cfg_ntp";

uint32_t clampInterval(uint32_t v) {
  if (v < ConfigStore::MIN_INTERVAL_S) return ConfigStore::MIN_INTERVAL_S;
  if (v > ConfigStore::MAX_INTERVAL_S) return ConfigStore::MAX_INTERVAL_S;
  return v;
}
}  // namespace

DeviceConfig ConfigStore::load() {
  DeviceConfig cfg;
  cfg.measureIntervalS = clampInterval(kv_.getU32(KEY_MEASURE, cfg.measureIntervalS));
  cfg.uploadIntervalS = clampInterval(kv_.getU32(KEY_UPLOAD, cfg.uploadIntervalS));
  kv_.getStr(KEY_NTP, cfg.ntpServer, sizeof(cfg.ntpServer));
  return cfg;
}

void ConfigStore::save(const DeviceConfig& cfg) {
  kv_.setU32(KEY_MEASURE, cfg.measureIntervalS);
  kv_.setU32(KEY_UPLOAD, cfg.uploadIntervalS);
  kv_.setStr(KEY_NTP, cfg.ntpServer);
}

bool ConfigStore::applyServerConfig(DeviceConfig& cfg, uint32_t measureS,
                                    uint32_t uploadS, const char* ntpServer) {
  bool changed = false;
  if (measureS != 0) {
    uint32_t v = clampInterval(measureS);
    if (v != cfg.measureIntervalS) { cfg.measureIntervalS = v; changed = true; }
  }
  if (uploadS != 0) {
    uint32_t v = clampInterval(uploadS);
    if (v != cfg.uploadIntervalS) { cfg.uploadIntervalS = v; changed = true; }
  }
  if (ntpServer && ntpServer[0] != '\0' &&
      strncmp(ntpServer, cfg.ntpServer, sizeof(cfg.ntpServer)) != 0) {
    snprintf(cfg.ntpServer, sizeof(cfg.ntpServer), "%s", ntpServer);
    changed = true;
  }
  if (changed) save(cfg);
  return changed;
}

}  // namespace core
