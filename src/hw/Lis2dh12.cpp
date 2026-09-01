#include "Lis2dh12.h"
#include "board_pins.h"
#include <Arduino.h>
#include <Wire.h>

namespace hw {

namespace {
constexpr uint8_t REG_WHO_AM_I = 0x0F;
constexpr uint8_t WHO_AM_I_VALUE = 0x33;
constexpr uint8_t REG_CTRL1 = 0x20;
constexpr uint8_t REG_OUT_X_L = 0x28;
constexpr uint8_t CTRL1_10HZ_ALL_AXES = 0x27;  // ODR=10 Hz, normal mode, XYZ til
constexpr uint8_t CTRL1_POWER_DOWN = 0x00;
constexpr uint8_t AUTO_INCREMENT = 0x80;

bool writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(LIS2DH12_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool readRegs(uint8_t reg, uint8_t* out, size_t len) {
  Wire.beginTransmission(LIS2DH12_I2C_ADDR);
  Wire.write(reg | AUTO_INCREMENT);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(static_cast<int>(LIS2DH12_I2C_ADDR), static_cast<int>(len)) !=
      static_cast<int>(len))
    return false;
  for (size_t i = 0; i < len; i++) out[i] = Wire.read();
  return true;
}
}  // namespace

bool Lis2dh12::measure(int16_t& axMg, int16_t& ayMg, int16_t& azMg) {
  uint8_t who = 0;
  if (!readRegs(REG_WHO_AM_I, &who, 1) || who != WHO_AM_I_VALUE) return false;
  if (!writeReg(REG_CTRL1, CTRL1_10HZ_ALL_AXES)) return false;
  delay(120);  // mindst én sample-periode ved 10 Hz + opstart

  uint8_t raw[6];
  bool ok = readRegs(REG_OUT_X_L, raw, 6);
  writeReg(REG_CTRL1, CTRL1_POWER_DOWN);  // altid sluk igen
  if (!ok) return false;

  // Normal mode: 10-bit venstrejusteret, 4 mg/digit ved ±2g.
  auto toMg = [](uint8_t lo, uint8_t hi) {
    int16_t v = static_cast<int16_t>((hi << 8) | lo);
    return static_cast<int16_t>((v >> 6) * 4);
  };
  axMg = toMg(raw[0], raw[1]);
  ayMg = toMg(raw[2], raw[3]);
  azMg = toMg(raw[4], raw[5]);
  return true;
}

}  // namespace hw
