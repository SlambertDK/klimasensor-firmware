// IKvStore — nøgle/værdi-persistens (NVS på target, RAM i tests).
#pragma once
#include <cstdint>
#include <cstring>

namespace core {

class IKvStore {
 public:
  virtual ~IKvStore() = default;
  virtual uint32_t getU32(const char* key, uint32_t def) = 0;
  virtual void setU32(const char* key, uint32_t value) = 0;
  virtual int64_t getI64(const char* key, int64_t def) = 0;
  virtual void setI64(const char* key, int64_t value) = 0;
  // Kopierer strengen til out (maks. maxLen inkl. NUL). false hvis nøglen mangler.
  virtual bool getStr(const char* key, char* out, size_t maxLen) = 0;
  virtual void setStr(const char* key, const char* value) = 0;
};

}  // namespace core
