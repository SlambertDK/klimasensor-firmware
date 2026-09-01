// Record — én måling i det binære lagringsformat (40 bytes, little-endian, CRC16-CCITT).
// Ren C++ uden Arduino-afhængigheder, så formatet kan unit-testes native.
#pragma once
#include <cstdint>
#include <cstddef>

namespace core {

constexpr size_t RECORD_SIZE = 40;
constexpr uint8_t RECORD_VERSION = 1;

enum RecordFlags : uint8_t {
  FLAG_TS_SYNCED = 0x01,  // ts er kalendertid (epoch-s); ellers relativ uptime-s
};

// Sentinel-værdier for fejlende sensorer (spec §8: null-felter i payload).
constexpr int16_t INVALID_I16 = INT16_MIN;
constexpr uint16_t INVALID_U16 = 0xFFFF;
constexpr uint32_t INVALID_U32 = 0xFFFFFFFF;

struct Record {
  uint8_t version = RECORD_VERSION;
  uint8_t flags = 0;
  uint16_t era = 0;      // cold-boot-æra; bruges til backfill af præ-sync-tid
  uint32_t seq = 0;
  int64_t ts = 0;
  int16_t tempCx100 = 0;
  uint16_t rhX100 = 0;
  uint16_t pm1_0x10 = 0, pm2_5x10 = 0, pm4_0x10 = 0, pm10x10 = 0;
  uint32_t luxX100 = 0;
  int16_t axMg = 0, ayMg = 0, azMg = 0;

  // Serialiserer til præcis RECORD_SIZE bytes (inkl. afsluttende CRC16).
  void serialize(uint8_t out[RECORD_SIZE]) const;
  // false ved CRC-fejl eller ukendt version.
  static bool deserialize(const uint8_t in[RECORD_SIZE], Record& out);
};

uint16_t crc16ccitt(const uint8_t* data, size_t len);

}  // namespace core
