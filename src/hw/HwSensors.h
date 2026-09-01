// HwSensors — samler de fire fysiske sensorer bag core::ISensors.
// Fejlende sensorer giver INVALID_*-sentinels; resten af målingen gennemføres.
#pragma once
#include "ISensors.h"
#include "Shtc3.h"
#include "Veml6040.h"
#include "Lis2dh12.h"
#include "Sps30.h"

namespace hw {

class HwSensors : public core::ISensors {
 public:
  void measure(core::Record& r) override {
    if (!shtc3_.measure(r.tempCx100, r.rhX100)) {
      r.tempCx100 = core::INVALID_I16;
      r.rhX100 = core::INVALID_U16;
    }
    if (!veml_.measure(r.luxX100)) r.luxX100 = core::INVALID_U32;
    if (!lis_.measure(r.axMg, r.ayMg, r.azMg)) {
      r.axMg = r.ayMg = r.azMg = core::INVALID_I16;
    }
    Sps30Values pm;
    if (sps30_.measure(pm)) {
      r.pm1_0x10 = pm.pm1_0x10;
      r.pm2_5x10 = pm.pm2_5x10;
      r.pm4_0x10 = pm.pm4_0x10;
      r.pm10x10 = pm.pm10x10;
    } else {
      r.pm1_0x10 = r.pm2_5x10 = r.pm4_0x10 = r.pm10x10 = core::INVALID_U16;
    }
  }

 private:
  Shtc3 shtc3_;
  Veml6040 veml_;
  Lis2dh12 lis_;
  Sps30 sps30_;
};

}  // namespace hw
