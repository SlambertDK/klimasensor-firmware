#include "Sps30.h"
#include "SensirionI2c.h"
#include "board_pins.h"
#include <cstring>

namespace hw {

namespace {
constexpr uint8_t SPS30_ADDR = 0x69;
constexpr uint16_t CMD_START_MEASUREMENT = 0x0010;
constexpr uint16_t CMD_STOP_MEASUREMENT = 0x0104;
constexpr uint16_t CMD_DATA_READY = 0x0202;
constexpr uint16_t CMD_READ_VALUES = 0x0300;
constexpr uint16_t ARG_FORMAT_FLOAT = 0x0300;

bool startMeasurement() {
  uint8_t arg[2] = {ARG_FORMAT_FLOAT >> 8, ARG_FORMAT_FLOAT & 0xFF};
  Wire.beginTransmission(SPS30_ADDR);
  Wire.write(CMD_START_MEASUREMENT >> 8);
  Wire.write(CMD_START_MEASUREMENT & 0xFF);
  Wire.write(arg[0]);
  Wire.write(arg[1]);
  Wire.write(sensirionCrc8(arg, 2));
  return Wire.endTransmission() == 0;
}

float wordsToFloat(uint16_t hi, uint16_t lo) {
  uint32_t bits = (static_cast<uint32_t>(hi) << 16) | lo;  // big-endian IEEE754
  float f;
  memcpy(&f, &bits, sizeof(f));
  return f;
}

uint16_t toX10(float ugm3) {
  if (!(ugm3 >= 0.0f) || ugm3 > 6000.0f) return 0;  // NaN/absurd → 0
  return static_cast<uint16_t>(ugm3 * 10.0f + 0.5f);
}
}  // namespace

bool Sps30::measure(Sps30Values& out) {
  if (PIN_SPS30_PWR >= 0) {
    pinMode(PIN_SPS30_PWR, OUTPUT);
    digitalWrite(PIN_SPS30_PWR, HIGH);
    delay(100);  // forsyning + sensor-boot
  }

  bool ok = false;
  if (startMeasurement()) {
    delay(WARMUP_MS);  // blæser + flow skal stabiliseres

    // Poll data-ready (burde være klar for længst, men vær robust).
    for (int i = 0; i < 10 && !ok; i++) {
      uint16_t ready = 0;
      if (sensirionWriteCmd(ADDR, CMD_DATA_READY) &&
          sensirionReadWords(ADDR, &ready, 1) && ready == 1) {
        ok = true;
        break;
      }
      delay(100);
    }

    if (ok) {
      ok = false;
      // 10 floats à 2 words: PM1.0, PM2.5, PM4.0, PM10 (masse),
      // NC0.5..NC10 (antal), typisk partikelstørrelse.
      uint16_t w[20];
      if (sensirionWriteCmd(ADDR, CMD_READ_VALUES)) {
        delay(3);
        if (sensirionReadWords(ADDR, w, 20)) {
          out.pm1_0x10 = toX10(wordsToFloat(w[0], w[1]));
          out.pm2_5x10 = toX10(wordsToFloat(w[2], w[3]));
          out.pm4_0x10 = toX10(wordsToFloat(w[4], w[5]));
          out.pm10x10 = toX10(wordsToFloat(w[6], w[7]));
          ok = true;
        }
      }
    }
    sensirionWriteCmd(ADDR, CMD_STOP_MEASUREMENT);
  }

  if (PIN_SPS30_PWR >= 0) digitalWrite(PIN_SPS30_PWR, LOW);
  return ok;
}

}  // namespace hw
