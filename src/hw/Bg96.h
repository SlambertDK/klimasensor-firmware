// Bg96 — Quectel BG96-modem bag core::INet. AT-kommandoer over UART.
// Referencer: Quectel BG96 AT Commands Manual + BG96 HTTP(S) AT Commands Manual.
#pragma once
#include "INet.h"
#include <Arduino.h>

namespace hw {

class Bg96 : public core::INet {
 public:
  explicit Bg96(HardwareSerial& uart) : uart_(uart) {}

  bool bringUp() override;
  bool ntpTime(const char* server, int64_t& epochOut) override;
  bool httpPost(const char* url, const std::string& payload,
                std::string& responseOut) override;
  bool otaUpdate(const char* url, const char* sha256Hex) override;
  void shutDown() override;

 private:
  bool powerOn();
  bool attach();
  // Send kommando og vent på 'expect' (typisk "OK"). Linjer der modtages
  // undervejs appendes til 'capture' hvis angivet.
  bool sendAt(const char* cmd, const char* expect, uint32_t timeoutMs,
              String* capture = nullptr);
  bool waitFor(const char* token, uint32_t timeoutMs, String* capture = nullptr);
  bool setHttpUrl(const char* url);

  HardwareSerial& uart_;
  bool poweredOn_ = false;
};

}  // namespace hw
