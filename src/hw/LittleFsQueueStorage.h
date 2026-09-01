// LittleFsQueueStorage — IQueueStorage på ESP32's LittleFS-partition.
// Målekøen er én append-fil; kapaciteten er partitionens frie plads minus
// en sikkerhedsmargin til filsystem-metadata.
#pragma once
#include "IQueueStorage.h"
#include <LittleFS.h>

namespace hw {

class LittleFsQueueStorage : public core::IQueueStorage {
 public:
  static constexpr const char* PATH = "/queue.bin";
  static constexpr size_t FS_MARGIN = 32 * 1024;

  bool append(const uint8_t* data, size_t len) override {
    File f = LittleFS.open(PATH, "a");
    if (!f) return false;
    size_t written = f.write(data, len);
    f.close();
    return written == len;
  }

  bool read(size_t offset, uint8_t* out, size_t len) override {
    File f = LittleFS.open(PATH, "r");
    if (!f) return false;
    bool ok = f.seek(offset) && f.read(out, len) == len;
    f.close();
    return ok;
  }

  size_t size() override {
    File f = LittleFS.open(PATH, "r");
    if (!f) return 0;
    size_t s = f.size();
    f.close();
    return s;
  }

  size_t capacity() override {
    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();
    size_t freeBytes = total > used ? total - used : 0;
    size_t cap = size() + (freeBytes > FS_MARGIN ? freeBytes - FS_MARGIN : 0);
    return cap;
  }

  bool truncate() override {
    return LittleFS.remove(PATH) || !LittleFS.exists(PATH);
  }
};

}  // namespace hw
