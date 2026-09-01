#include "Shtc3.h"
#include "SensirionI2c.h"

namespace hw {

namespace {
constexpr uint16_t CMD_WAKEUP = 0x3517;
constexpr uint16_t CMD_SLEEP = 0xB098;
constexpr uint16_t CMD_MEASURE_T_FIRST = 0x7866;  // normal mode, clock stretching fra
}

bool Shtc3::measure(int16_t& tempCx100, uint16_t& rhX100) {
  if (!sensirionWriteCmd(ADDR, CMD_WAKEUP)) return false;
  delayMicroseconds(240);  // opvågningstid jf. datasheet

  if (!sensirionWriteCmd(ADDR, CMD_MEASURE_T_FIRST)) return false;
  delay(15);  // maks. målevarighed i normal mode er 12.1 ms

  uint16_t words[2];
  bool ok = sensirionReadWords(ADDR, words, 2);
  sensirionWriteCmd(ADDR, CMD_SLEEP);  // altid tilbage i sleep, også ved fejl
  if (!ok) return false;

  // T[°C] = -45 + 175 * raw / 2^16 ; RH[%] = 100 * raw / 2^16
  tempCx100 = static_cast<int16_t>(-4500 + (17500LL * words[0]) / 65536);
  rhX100 = static_cast<uint16_t>((10000LL * words[1]) / 65536);
  return true;
}

}  // namespace hw
