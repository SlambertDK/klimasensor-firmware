#include "Uplink.h"
#include <ArduinoJson.h>
#include <cstdio>

namespace core {

namespace {
// Skalerede heltal → JSON-tal eller null ved sensor-fejl-sentinel.
void addScaled(JsonObject obj, const char* key, int32_t raw, int32_t invalid, float div) {
  if (raw == invalid) obj[key] = nullptr;
  else obj[key] = static_cast<float>(raw) / div;
}
}  // namespace

std::string Uplink::buildPayload(const char* token, const char* deviceId,
                                 const char* fwVersion, bool storageFull,
                                 const Record* records, const bool* tsValid,
                                 size_t count) {
  JsonDocument doc;
  doc["token"] = token;
  doc["deviceId"] = deviceId;
  doc["fwVersion"] = fwVersion;
  doc["storageFull"] = storageFull;
  JsonArray arr = doc["measurements"].to<JsonArray>();
  for (size_t i = 0; i < count; i++) {
    const Record& r = records[i];
    JsonObject m = arr.add<JsonObject>();
    m["ts"] = r.ts;
    if (!tsValid[i]) m["tsValid"] = false;
    addScaled(m, "tempC", r.tempCx100, INVALID_I16, 100.0f);
    addScaled(m, "rh", r.rhX100, INVALID_U16, 100.0f);
    addScaled(m, "pm1_0", r.pm1_0x10, INVALID_U16, 10.0f);
    addScaled(m, "pm2_5", r.pm2_5x10, INVALID_U16, 10.0f);
    addScaled(m, "pm4_0", r.pm4_0x10, INVALID_U16, 10.0f);
    addScaled(m, "pm10", r.pm10x10, INVALID_U16, 10.0f);
    if (r.luxX100 == INVALID_U32) m["lux"] = nullptr;
    else m["lux"] = static_cast<float>(r.luxX100) / 100.0f;
    JsonObject a = m["accel"].to<JsonObject>();
    addScaled(a, "x", r.axMg, INVALID_I16, 1000.0f);
    addScaled(a, "y", r.ayMg, INVALID_I16, 1000.0f);
    addScaled(a, "z", r.azMg, INVALID_I16, 1000.0f);
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

bool Uplink::parseResponse(const char* json, size_t len, ServerResponse& out) {
  out = ServerResponse{};
  JsonDocument doc;
  if (deserializeJson(doc, json, len) != DeserializationError::Ok) return false;
  if (!doc.is<JsonObject>()) return false;
  out.ackCount = doc["ackCount"] | 0;
  JsonObject cfg = doc["config"];
  if (!cfg.isNull()) {
    out.measureIntervalS = cfg["measureIntervalS"] | 0;
    out.uploadIntervalS = cfg["uploadIntervalS"] | 0;
    const char* ntp = cfg["ntpServer"] | "";
    snprintf(out.ntpServer, sizeof(out.ntpServer), "%s", ntp);
  }
  JsonObject ota = doc["ota"];
  if (!ota.isNull()) {
    snprintf(out.otaVersion, sizeof(out.otaVersion), "%s", ota["version"] | "");
    snprintf(out.otaUrl, sizeof(out.otaUrl), "%s", ota["url"] | "");
    snprintf(out.otaSha256, sizeof(out.otaSha256), "%s", ota["sha256"] | "");
  }
  return true;
}

}  // namespace core
