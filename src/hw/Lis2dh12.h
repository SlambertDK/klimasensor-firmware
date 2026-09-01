// LIS2DH12 — 3-akset accelerometer (I2C, adresse fra board_pins.h).
#pragma once
#include <cstdint>

namespace hw {

class Lis2dh12 {
 public:
  // Tænd (10 Hz normal mode ±2g), vent på sample, læs x/y/z i mg, sluk.
  bool measure(int16_t& axMg, int16_t& ayMg, int16_t& azMg);
};

}  // namespace hw
