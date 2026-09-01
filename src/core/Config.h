// Config — enhedskonfiguration med NVS-persistens og server-overstyring (spec §3.2/§3.4).
#pragma once
#include "IKvStore.h"
#include <cstdint>

namespace core {

struct DeviceConfig {
  uint32_t measureIntervalS = 300;
  uint32_t uploadIntervalS = 300;
  char ntpServer[64] = "pool.ntp.org";
};

class ConfigStore {
 public:
  static constexpr uint32_t MIN_INTERVAL_S = 60;
  static constexpr uint32_t MAX_INTERVAL_S = 86400;

  explicit ConfigStore(IKvStore& kv) : kv_(kv) {}

  DeviceConfig load();
  void save(const DeviceConfig& cfg);

  // Anvend felter fra serveren (0/tom = feltet blev ikke sendt). Intervaller
  // clampes til [MIN, MAX]. Returnerer true hvis noget blev ændret (og gemt).
  bool applyServerConfig(DeviceConfig& cfg, uint32_t measureS, uint32_t uploadS,
                         const char* ntpServer);

 private:
  IKvStore& kv_;
};

}  // namespace core
