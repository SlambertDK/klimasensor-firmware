// SHTC3 — temperatur + luftfugtighed (I2C 0x70). Datasheet: Sensirion SHTC3 v4.
#pragma once
#include <cstdint>

namespace hw {

class Shtc3 {
 public:
  // Wake → mål (T først, normal mode, uden clock stretching) → sleep.
  // false ved fejl; ellers udfyldes tempCx100/rhX100.
  bool measure(int16_t& tempCx100, uint16_t& rhX100);

 private:
  static constexpr uint8_t ADDR = 0x70;
};

}  // namespace hw
