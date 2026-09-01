#include "Cycle.h"

namespace core {

namespace {
// Upload-/sync-bogholderi gemmes med æra, så uptime-sammenligninger aldrig
// krydser en cold-boot-grænse (hvor uptime er nulstillet).
const char* KEY_LAST_UPLOAD = "cy_lastup";
const char* KEY_LAST_UPLOAD_ERA = "cy_lastup_era";
const char* KEY_LAST_SYNC = "cy_lastsync";
const char* KEY_LAST_SYNC_ERA = "cy_lastsync_era";
const char* KEY_SEQ = "cy_seq";
}  // namespace

CycleResult Cycle::run(int64_t uptimeNowS) {
  CycleResult res;
  DeviceConfig cfg = configStore_.load();

  // 1) Mål og persistér FØR ethvert upload-forsøg (spec §3.5: data i flash først).
  //    Fuld kø ⇒ ingen ny måling — eksisterende data røres aldrig.
  if (!queue_.isFull()) {
    Record r;
    sensors_.measure(r);
    timeKeeper_.stamp(r, uptimeNowS);
    uint32_t seq = kv_.getU32(KEY_SEQ, 0);
    r.seq = seq;
    if (queue_.push(r)) {
      kv_.setU32(KEY_SEQ, seq + 1);
      res.measured = true;
    }
  }

  // 2) Upload hvis forfalden og der er noget at sende.
  if (queue_.pendingCount() > 0 && uploadDue(cfg, uptimeNowS)) {
    res.uploadAttempted = true;
    if (net_.bringUp()) {
      maybeNtpSync(cfg, uptimeNowS);
      doUploadSession(cfg, uptimeNowS, res);
      net_.shutDown();
    }
  }

  // Genindlæs config: serveren kan netop have ændret intervallerne.
  res.sleepSeconds = configStore_.load().measureIntervalS;
  return res;
}

bool Cycle::uploadDue(const DeviceConfig& cfg, int64_t uptimeNowS) {
  uint32_t lastEra = kv_.getU32(KEY_LAST_UPLOAD_ERA, 0xFFFFFFFF);
  if (lastEra != timeKeeper_.currentEra()) return true;  // aldrig uploadet i denne æra
  int64_t last = kv_.getI64(KEY_LAST_UPLOAD, 0);
  return uptimeNowS - last >= static_cast<int64_t>(cfg.uploadIntervalS);
}

void Cycle::maybeNtpSync(const DeviceConfig& cfg, int64_t uptimeNowS) {
  bool due = !timeKeeper_.isSynced();
  if (!due) {
    uint32_t syncEra = kv_.getU32(KEY_LAST_SYNC_ERA, 0xFFFFFFFF);
    int64_t lastSync = kv_.getI64(KEY_LAST_SYNC, 0);
    due = syncEra != timeKeeper_.currentEra() ||
          uptimeNowS - lastSync >= NTP_RESYNC_INTERVAL_S;
  }
  if (!due) return;
  int64_t epoch;
  if (net_.ntpTime(cfg.ntpServer, epoch)) {
    timeKeeper_.sync(epoch, uptimeNowS);
    kv_.setI64(KEY_LAST_SYNC, uptimeNowS);
    kv_.setU32(KEY_LAST_SYNC_ERA, timeKeeper_.currentEra());
  }
}

void Cycle::doUploadSession(const DeviceConfig& cfg, int64_t uptimeNowS,
                            CycleResult& res) {
  DeviceConfig liveCfg = cfg;
  Record batch[MeasurementQueue::MAX_BATCH];
  bool tsValid[MeasurementQueue::MAX_BATCH];
  bool anySuccess = false;

  while (true) {
    size_t n = queue_.peek(batch, MeasurementQueue::MAX_BATCH);
    if (n == 0) break;
    for (size_t i = 0; i < n; i++) tsValid[i] = timeKeeper_.resolve(batch[i]);

    std::string payload = Uplink::buildPayload(
        id_.token, id_.deviceId, id_.fwVersion, queue_.isFull(), batch, tsValid, n);
    std::string response;
    if (!net_.httpPost(id_.serverUrl, payload, response)) break;

    ServerResponse sr;
    if (!Uplink::parseResponse(response.c_str(), response.size(), sr)) break;

    size_t acked = sr.ackCount < n ? sr.ackCount : n;
    if (acked > 0) {
      queue_.ack(acked);
      res.recordsAcked += acked;
      anySuccess = true;
    }

    configStore_.applyServerConfig(liveCfg, sr.measureIntervalS,
                                   sr.uploadIntervalS, sr.ntpServer);

    if (sr.hasOta()) {
      // otaUpdate genstarter enheden ved succes; ved fejl fortsætter vi blot.
      net_.otaUpdate(sr.otaUrl, sr.otaSha256);
    }

    if (acked < n) break;  // serveren tog ikke hele batchen — prøv igen næste cyklus
  }

  if (anySuccess) {
    res.uploadSucceeded = true;
    kv_.setI64(KEY_LAST_UPLOAD, uptimeNowS);
    kv_.setU32(KEY_LAST_UPLOAD_ERA, timeKeeper_.currentEra());
  }
}

}  // namespace core
