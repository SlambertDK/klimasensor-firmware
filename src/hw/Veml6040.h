// VEML6040 — RGBW-lyssensor (I2C 0x10). Lux beregnes fra G-kanalen
// (Vishay application note: 0.25168 lux/count ved 160 ms integrationstid).
#pragma once
#include <cstdint>

namespace hw {

class Veml6040 {
 public:
  bool measure(uint32_t& luxX100);

 private:
  static constexpr uint8_t ADDR = 0x10;
};

}  // namespace hw
