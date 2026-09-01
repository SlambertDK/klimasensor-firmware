#include "Veml6040.h"
#include <Arduino.h>
#include <Wire.h>

namespace hw {

namespace {
constexpr uint8_t REG_CONF = 0x00;
constexpr uint8_t REG_GREEN = 0x09;
// CONF: IT=010 (160 ms), TRIG=0, AF=0, SD=0 (tændt)
constexpr uint8_t CONF_IT160_ON = 0x20;
constexpr uint8_t CONF_SHUTDOWN = 0x01;
constexpr float LUX_PER_COUNT_160MS = 0.25168f;

bool writeConf(uint8_t addr, uint8_t conf) {
  Wire.beginTransmission(addr);
  Wire.write(REG_CONF);
  Wire.write(conf);  // low byte
  Wire.write(0x00);  // high byte (reserveret)
  return Wire.endTransmission() == 0;
}
}  // namespace

bool Veml6040::measure(uint32_t& luxX100) {
  if (!writeConf(ADDR, CONF_IT160_ON)) return false;
  delay(320);  // to integrationsperioder, så første måling er komplet

  Wire.beginTransmission(ADDR);
  Wire.write(REG_GREEN);
  if (Wire.endTransmission(false) != 0) return false;  // repeated start
  if (Wire.requestFrom(static_cast<int>(ADDR), 2) != 2) return false;
  uint16_t g = Wire.read();
  g |= static_cast<uint16_t>(Wire.read()) << 8;

  writeConf(ADDR, CONF_SHUTDOWN);  // spar strøm mellem målinger
  luxX100 = static_cast<uint32_t>(g * LUX_PER_COUNT_160MS * 100.0f);
  return true;
}

}  // namespace hw
