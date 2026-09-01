// INet — netværksinterface til Cycle: BG96 på target, fake i tests.
#pragma once
#include <cstdint>
#include <string>

namespace core {

class INet {
 public:
  virtual ~INet() = default;
  virtual bool bringUp() = 0;  // power on + attach + PDP-kontekst
  // NTP-opslag; true + epoch ved succes.
  virtual bool ntpTime(const char* server, int64_t& epochOut) = 0;
  // POST payload til url; true ved HTTP 200 + svar-body i responseOut.
  virtual bool httpPost(const char* url, const std::string& payload,
                        std::string& responseOut) = 0;
  // Hent og aktivér ny firmware (sha256-verificeret). Genstarter ved succes.
  virtual bool otaUpdate(const char* url, const char* sha256Hex) = 0;
  virtual void shutDown() = 0;
};

}  // namespace core
