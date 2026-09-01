// OtaUpdater — streamer firmware ind i næste OTA-partition med løbende
// SHA-256. Uden TLS er checksummen den eneste integritetsgaranti (spec §3.7):
// finish() aktiverer KUN den nye partition ved eksakt SHA-256-match.
#pragma once
#include <cstdint>
#include <cstddef>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>

namespace hw {

class OtaUpdater {
 public:
  bool begin(size_t imageSize);
  bool write(const uint8_t* data, size_t len);
  bool finish(const char* expectedSha256Hex);  // verificér + set boot partition
  void abort();

 private:
  esp_ota_handle_t handle_ = 0;
  const esp_partition_t* partition_ = nullptr;
  mbedtls_sha256_context sha_;
  bool active_ = false;
};

}  // namespace hw
