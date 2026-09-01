// Cycle — én opvågnen: mål → persistér → (upload + config + OTA) → beregn søvn.
// Ren logik uden hardware-afhængigheder; alt IO går via interfaces (spec §4).
#pragma once
#include "ISensors.h"
#include "INet.h"
#include "MeasurementQueue.h"
#include "Config.h"
#include "TimeKeeper.h"
#include "Uplink.h"

namespace core {

struct CycleIdentity {
  const char* token;
  const char* deviceId;
  const char* fwVersion;
  const char* serverUrl;
};

struct CycleResult {
  uint32_t sleepSeconds = 300;
  bool measured = false;
  bool uploadAttempted = false;
  bool uploadSucceeded = false;
  size_t recordsAcked = 0;
};

class Cycle {
 public:
  static constexpr int64_t NTP_RESYNC_INTERVAL_S = 86400;  // dagligt mod RTC-drift

  Cycle(ISensors& sensors, INet& net, MeasurementQueue& queue,
        ConfigStore& configStore, TimeKeeper& timeKeeper, IKvStore& kv,
        const CycleIdentity& id)
      : sensors_(sensors), net_(net), queue_(queue), configStore_(configStore),
        timeKeeper_(timeKeeper), kv_(kv), id_(id) {}

  // uptimeNowS: relativ tid nu (overlever deep sleep, nulstilles ved cold boot).
  CycleResult run(int64_t uptimeNowS);

 private:
  bool uploadDue(const DeviceConfig& cfg, int64_t uptimeNowS);
  void doUploadSession(const DeviceConfig& cfg, int64_t uptimeNowS, CycleResult& res);
  void maybeNtpSync(const DeviceConfig& cfg, int64_t uptimeNowS);

  ISensors& sensors_;
  INet& net_;
  MeasurementQueue& queue_;
  ConfigStore& configStore_;
  TimeKeeper& timeKeeper_;
  IKvStore& kv_;
  CycleIdentity id_;
};

}  // namespace core
