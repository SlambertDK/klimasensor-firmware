// ISensors — samlet sensor-interface til Cycle (mockes i tests).
// Fejlende sensorer udfylder felterne med INVALID_*-sentinels (spec §8).
#pragma once
#include "Record.h"

namespace core {

class ISensors {
 public:
  virtual ~ISensors() = default;
  // Læs SHTC3 + VEML6040 + LIS2DH12 (hurtige) og SPS30 (inkl. ~30 s
  // opvarmning og sluk bagefter) og udfyld målefelterne i r.
  virtual void measure(Record& r) = 0;
};

}  // namespace core
