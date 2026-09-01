// Fælles Sensirion-I2C-hjælpere: 16-bit kommandoer og CRC8 (poly 0x31, init 0xFF).
// Bruges af både SHTC3 og SPS30.
#pragma once
#include <Arduino.h>
#include <Wire.h>

namespace hw {

inline uint8_t sensirionCrc8(const uint8_t* data, size_t len) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++)
      crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
  }
  return crc;
}

inline bool sensirionWriteCmd(uint8_t addr, uint16_t cmd) {
  Wire.beginTransmission(addr);
  Wire.write(cmd >> 8);
  Wire.write(cmd & 0xFF);
  return Wire.endTransmission() == 0;
}

// Læs 'words' antal 16-bit ord, hver fulgt af CRC8. false ved IO-/CRC-fejl.
inline bool sensirionReadWords(uint8_t addr, uint16_t* out, size_t words) {
  size_t bytes = words * 3;
  if (Wire.requestFrom(static_cast<int>(addr), static_cast<int>(bytes)) != static_cast<int>(bytes))
    return false;
  for (size_t i = 0; i < words; i++) {
    uint8_t raw[2] = {static_cast<uint8_t>(Wire.read()), static_cast<uint8_t>(Wire.read())};
    uint8_t crc = Wire.read();
    if (sensirionCrc8(raw, 2) != crc) return false;
    out[i] = (static_cast<uint16_t>(raw[0]) << 8) | raw[1];
  }
  return true;
}

}  // namespace hw
