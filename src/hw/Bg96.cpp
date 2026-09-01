#include "Bg96.h"
#include "AtParser.h"
#include "OtaUpdater.h"
#include "board_pins.h"

namespace hw {

namespace {
constexpr uint32_t AT_TIMEOUT_MS = 3000;
constexpr uint32_t ATTACH_TIMEOUT_MS = 90000;
constexpr uint32_t HTTP_TIMEOUT_MS = 60000;

void logLine(const String& s) {
  if (s.length()) Serial.printf("[bg96] %s\n", s.c_str());
}
}  // namespace

bool Bg96::waitFor(const char* token, uint32_t timeoutMs, String* capture) {
  String buf;
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (uart_.available()) {
      char c = static_cast<char>(uart_.read());
      buf += c;
      if (capture) *capture += c;
      if (buf.endsWith(token)) return true;
      if (buf.endsWith("\r\nERROR\r\n") || buf.endsWith("+CME ERROR")) {
        logLine(buf);
        return false;
      }
      if (buf.length() > 4096) buf.remove(0, 2048);  // bounded buffer
    }
    delay(5);
  }
  logLine(buf);
  return false;
}

bool Bg96::sendAt(const char* cmd, const char* expect, uint32_t timeoutMs,
                  String* capture) {
  // Tøm evt. gamle URC'er før ny kommando.
  while (uart_.available()) uart_.read();
  uart_.print(cmd);
  uart_.print("\r\n");
  return waitFor(expect, timeoutMs, capture);
}

bool Bg96::powerOn() {
  uart_.begin(BG96_BAUD, SERIAL_8N1, PIN_BG96_RX, PIN_BG96_TX);
  if (PIN_BG96_PWR >= 0) {
    pinMode(PIN_BG96_PWR, OUTPUT);
    digitalWrite(PIN_BG96_PWR, HIGH);
    delay(100);
  }
  // PWRKEY-puls ≥500 ms (polaritet afhænger af boardets driver-transistor —
  // justér her hvis modemet ikke svarer).
  pinMode(PIN_BG96_PWRKEY, OUTPUT);
  digitalWrite(PIN_BG96_PWRKEY, HIGH);
  delay(600);
  digitalWrite(PIN_BG96_PWRKEY, LOW);

  // Poll AT indtil modemet svarer (boot tager typisk 3-8 s).
  for (int i = 0; i < 30; i++) {
    if (sendAt("AT", "OK", 500)) {
      sendAt("ATE0", "OK", AT_TIMEOUT_MS);  // ekko fra
      return true;
    }
  }
  return false;
}

bool Bg96::attach() {
  if (!sendAt("AT+CPIN?", "READY", 5000)) return false;
  waitFor("OK", AT_TIMEOUT_MS);

  if (strlen(NET_APN) > 0) {
    String cmd = String("AT+QICSGP=1,1,\"") + NET_APN + "\",\"\",\"\",1";
    if (!sendAt(cmd.c_str(), "OK", AT_TIMEOUT_MS)) return false;
  }

  // Vent på netværks-attach.
  uint32_t start = millis();
  while (millis() - start < ATTACH_TIMEOUT_MS) {
    String resp;
    if (sendAt("AT+CGATT?", "OK", AT_TIMEOUT_MS, &resp) &&
        resp.indexOf("+CGATT: 1") >= 0)
      break;
    delay(2000);
    if (millis() - start >= ATTACH_TIMEOUT_MS) return false;
  }

  // Aktivér PDP-kontekst 1 (allerede aktiv giver fejl — tjek først).
  String state;
  sendAt("AT+QIACT?", "OK", AT_TIMEOUT_MS, &state);
  if (state.indexOf("+QIACT: 1,1") >= 0) return true;
  return sendAt("AT+QIACT=1", "OK", 60000);
}

bool Bg96::bringUp() {
  if (!powerOn()) return false;
  poweredOn_ = true;
  return attach();
}

void Bg96::shutDown() {
  if (poweredOn_) {
    sendAt("AT+QPOWD=1", "POWERED DOWN", 15000);  // ordnet nedlukning
    poweredOn_ = false;
  }
  if (PIN_BG96_PWR >= 0) digitalWrite(PIN_BG96_PWR, LOW);
}

bool Bg96::ntpTime(const char* server, int64_t& epochOut) {
  String cmd = String("AT+QNTP=1,\"") + server + "\"";
  String resp;
  if (!sendAt(cmd.c_str(), "OK", AT_TIMEOUT_MS)) return false;
  if (!waitFor("+QNTP:", HTTP_TIMEOUT_MS, &resp)) return false;
  waitFor("\r\n", 2000, &resp);  // resten af URC-linjen
  return core::parseQntp(resp.c_str(), epochOut);
}

bool Bg96::setHttpUrl(const char* url) {
  if (!sendAt("AT+QHTTPCFG=\"contextid\",1", "OK", AT_TIMEOUT_MS)) return false;
  sendAt("AT+QHTTPCFG=\"responseheader\",0", "OK", AT_TIMEOUT_MS);
  String cmd = String("AT+QHTTPURL=") + strlen(url) + ",80";
  if (!sendAt(cmd.c_str(), "CONNECT", AT_TIMEOUT_MS)) return false;
  uart_.print(url);
  return waitFor("OK", AT_TIMEOUT_MS);
}

bool Bg96::httpPost(const char* url, const std::string& payload,
                    std::string& responseOut) {
  if (!setHttpUrl(url)) return false;

  String cmd = String("AT+QHTTPPOST=") + payload.size() + ",60,60";
  if (!sendAt(cmd.c_str(), "CONNECT", HTTP_TIMEOUT_MS)) return false;
  uart_.write(reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

  String urc;
  if (!waitFor("+QHTTPPOST:", HTTP_TIMEOUT_MS, &urc)) return false;
  waitFor("\r\n", 2000, &urc);
  int err = -1, code = 0;
  long len = 0;
  if (!core::parseQhttpResult(urc.c_str(), err, code, len) || err != 0 || code != 200)
    return false;

  // Hent svar-body.
  String body;
  if (!sendAt("AT+QHTTPREAD=60", "CONNECT", HTTP_TIMEOUT_MS)) return false;
  if (!waitFor("\r\nOK\r\n", HTTP_TIMEOUT_MS, &body)) return false;
  int endIdx = body.lastIndexOf("\r\nOK\r\n");
  if (endIdx < 0) return false;
  // body starter evt. med \r\n efter CONNECT.
  int startIdx = 0;
  while (startIdx < endIdx && (body[startIdx] == '\r' || body[startIdx] == '\n')) startIdx++;
  responseOut.assign(body.c_str() + startIdx, endIdx - startIdx);
  return true;
}

bool Bg96::otaUpdate(const char* url, const char* sha256Hex) {
  Serial.printf("[ota] henter %s\n", url);
  if (!setHttpUrl(url)) return false;
  if (!sendAt("AT+QHTTPGET=60", "OK", AT_TIMEOUT_MS)) return false;

  String urc;
  if (!waitFor("+QHTTPGET:", HTTP_TIMEOUT_MS, &urc)) return false;
  waitFor("\r\n", 2000, &urc);
  int err = -1, code = 0;
  long contentLen = 0;
  if (!core::parseQhttpResult(urc.c_str(), err, code, contentLen) || err != 0 ||
      code != 200 || contentLen <= 0)
    return false;

  OtaUpdater ota;
  if (!ota.begin(static_cast<size_t>(contentLen))) return false;

  if (!sendAt("AT+QHTTPREAD=120", "CONNECT", HTTP_TIMEOUT_MS)) { ota.abort(); return false; }
  // Efter CONNECT følger et \r\n og derefter præcis contentLen rå bytes.
  uint8_t chunk[512];
  long remaining = contentLen;
  bool skippedLeadingCrLf = false;
  uint32_t lastData = millis();
  while (remaining > 0 && millis() - lastData < 30000) {
    size_t avail = uart_.available();
    if (avail == 0) { delay(2); continue; }
    if (!skippedLeadingCrLf) {
      while (uart_.available() && (uart_.peek() == '\r' || uart_.peek() == '\n')) uart_.read();
      if (uart_.available() == 0) continue;
      skippedLeadingCrLf = true;
    }
    size_t want = avail < sizeof(chunk) ? avail : sizeof(chunk);
    if (static_cast<long>(want) > remaining) want = static_cast<size_t>(remaining);
    size_t got = uart_.readBytes(chunk, want);
    if (got == 0) continue;
    if (!ota.write(chunk, got)) { ota.abort(); return false; }
    remaining -= static_cast<long>(got);
    lastData = millis();
  }
  waitFor("\r\nOK\r\n", 5000);
  if (remaining > 0) { ota.abort(); return false; }

  if (!ota.finish(sha256Hex)) {
    Serial.println("[ota] SHA-256-mismatch eller aktiveringsfejl - firmware forkastet");
    return false;
  }
  Serial.println("[ota] verificeret - genstarter til ny firmware");
  shutDown();
  ESP.restart();
  return true;  // nås aldrig
}

}  // namespace hw
