// IQueueStorage — byte-lager bag MeasurementQueue (LittleFS på target, RAM i tests).
#pragma once
#include <cstdint>
#include <cstddef>

namespace core {

class IQueueStorage {
 public:
  virtual ~IQueueStorage() = default;
  virtual bool append(const uint8_t* data, size_t len) = 0;
  // Læs len bytes fra offset; false hvis udenfor gyldigt område eller IO-fejl.
  virtual bool read(size_t offset, uint8_t* out, size_t len) = 0;
  virtual size_t size() = 0;       // nuværende datalængde i bytes
  virtual size_t capacity() = 0;   // maks. datalængde i bytes
  virtual bool truncate() = 0;     // slet alt indhold (kaldes kun når alt er kvitteret)
};

}  // namespace core
