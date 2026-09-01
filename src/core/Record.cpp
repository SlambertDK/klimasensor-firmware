#include "Record.h"
#include <cstring>

namespace core {

uint16_t crc16ccitt(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int b = 0; b < 8; b++)
      crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
  }
  return crc;
}

namespace {
void putU16(uint8_t*& p, uint16_t v) { *p++ = v & 0xFF; *p++ = v >> 8; }
void putU32(uint8_t*& p, uint32_t v) { for (int i = 0; i < 4; i++) *p++ = (v >> (8 * i)) & 0xFF; }
void putI64(uint8_t*& p, int64_t v) { for (int i = 0; i < 8; i++) *p++ = (static_cast<uint64_t>(v) >> (8 * i)) & 0xFF; }
uint16_t getU16(const uint8_t*& p) { uint16_t v = p[0] | (p[1] << 8); p += 2; return v; }
uint32_t getU32(const uint8_t*& p) { uint32_t v = 0; for (int i = 0; i < 4; i++) v |= static_cast<uint32_t>(p[i]) << (8 * i); p += 4; return v; }
int64_t getI64(const uint8_t*& p) { uint64_t v = 0; for (int i = 0; i < 8; i++) v |= static_cast<uint64_t>(p[i]) << (8 * i); p += 8; return static_cast<int64_t>(v); }
}  // namespace

void Record::serialize(uint8_t out[RECORD_SIZE]) const {
  uint8_t* p = out;
  *p++ = version;
  *p++ = flags;
  putU16(p, era);
  putU32(p, seq);
  putI64(p, ts);
  putU16(p, static_cast<uint16_t>(tempCx100));
  putU16(p, rhX100);
  putU16(p, pm1_0x10);
  putU16(p, pm2_5x10);
  putU16(p, pm4_0x10);
  putU16(p, pm10x10);
  putU32(p, luxX100);
  putU16(p, static_cast<uint16_t>(axMg));
  putU16(p, static_cast<uint16_t>(ayMg));
  putU16(p, static_cast<uint16_t>(azMg));
  putU16(p, crc16ccitt(out, RECORD_SIZE - 2));
}

bool Record::deserialize(const uint8_t in[RECORD_SIZE], Record& out) {
  const uint8_t* p = in;
  uint16_t crc = in[RECORD_SIZE - 2] | (in[RECORD_SIZE - 1] << 8);
  if (crc != crc16ccitt(in, RECORD_SIZE - 2)) return false;
  out.version = *p++;
  if (out.version != RECORD_VERSION) return false;
  out.flags = *p++;
  out.era = getU16(p);
  out.seq = getU32(p);
  out.ts = getI64(p);
  out.tempCx100 = static_cast<int16_t>(getU16(p));
  out.rhX100 = getU16(p);
  out.pm1_0x10 = getU16(p);
  out.pm2_5x10 = getU16(p);
  out.pm4_0x10 = getU16(p);
  out.pm10x10 = getU16(p);
  out.luxX100 = getU32(p);
  out.axMg = static_cast<int16_t>(getU16(p));
  out.ayMg = static_cast<int16_t>(getU16(p));
  out.azMg = static_cast<int16_t>(getU16(p));
  return true;
}

}  // namespace core
