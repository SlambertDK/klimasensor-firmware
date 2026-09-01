// Uplink — bygger upload-payload og parser serverens svar (API-kontrakt, spec §7).
// Ren streng-ind/streng-ud-logik; selve HTTP-transporten sker i INet/Bg96.
#pragma once
#include "Record.h"
#include <cstdint>
#include <cstddef>
#include <string>

namespace core {

struct ServerResponse {
  size_t ackCount = 0;
  // Config-felter: 0/tom = ikke sendt af serveren.
  uint32_t measureIntervalS = 0;
  uint32_t uploadIntervalS = 0;
  char ntpServer[64] = "";
  // OTA: tom url = ingen opdatering.
  char otaVersion[32] = "";
  char otaUrl[192] = "";
  char otaSha256[65] = "";
  bool hasOta() const { return otaUrl[0] != '\0'; }
};

class Uplink {
 public:
  // records[i] skal være tids-resolvet på forhånd; tsValid[i]=false giver
  // "tsValid": false på målingen (uløselig præ-sync-tid, jf. README).
  static std::string buildPayload(const char* token, const char* deviceId,
                                  const char* fwVersion, bool storageFull,
                                  const Record* records, const bool* tsValid,
                                  size_t count);

  // false ved uparsebart svar (behandles som ack 0, intet config/ota).
  static bool parseResponse(const char* json, size_t len, ServerResponse& out);
};

}  // namespace core
