// SPS30 — partikelsensor (I2C 0x69). Datasheet: Sensirion SPS30 v2.0.
// UART-fortrådning understøttes ikke i v1 (build-fejl ved -DSPS30_USE_UART).
#pragma once
#include <cstdint>

#ifdef SPS30_USE_UART
#error "SPS30 UART-interface er ikke implementeret i v1 - se spec §2"
#endif

namespace hw {

struct Sps30Values {
  uint16_t pm1_0x10, pm2_5x10, pm4_0x10, pm10x10;  // µg/m³ × 10
};

class Sps30 {
 public:
  // Tænd evt. forsyning, start måling, vent opvarmning, læs, stop og sluk.
  // Blokerer i ~WARMUP_MS. false ved fejl.
  bool measure(Sps30Values& out);

  static constexpr uint32_t WARMUP_MS = 30000;  // stabile værdier jf. spec §4

 private:
  static constexpr uint8_t ADDR = 0x69;
};

}  // namespace hw
