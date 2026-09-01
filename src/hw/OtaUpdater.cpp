#include "OtaUpdater.h"
#include <cstring>
#include <cstdio>

namespace hw {

bool OtaUpdater::begin(size_t imageSize) {
  partition_ = esp_ota_get_next_update_partition(nullptr);
  if (!partition_ || imageSize > partition_->size) return false;
  if (esp_ota_begin(partition_, imageSize, &handle_) != ESP_OK) return false;
  mbedtls_sha256_init(&sha_);
  mbedtls_sha256_starts(&sha_, 0 /* SHA-256, ikke SHA-224 */);
  active_ = true;
  return true;
}

bool OtaUpdater::write(const uint8_t* data, size_t len) {
  if (!active_) return false;
  if (esp_ota_write(handle_, data, len) != ESP_OK) return false;
  mbedtls_sha256_update(&sha_, data, len);
  return true;
}

bool OtaUpdater::finish(const char* expectedSha256Hex) {
  if (!active_) return false;
  active_ = false;

  uint8_t digest[32];
  mbedtls_sha256_finish(&sha_, digest);
  mbedtls_sha256_free(&sha_);

  char hex[65];
  for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", digest[i]);

  bool shaOk = expectedSha256Hex && strlen(expectedSha256Hex) == 64;
  for (int i = 0; shaOk && i < 64; i++) {
    char e = expectedSha256Hex[i];
    if (e >= 'A' && e <= 'F') e += 32;  // case-insensitiv sammenligning
    if (e != hex[i]) shaOk = false;
  }

  if (!shaOk) {
    esp_ota_abort(handle_);
    return false;
  }
  if (esp_ota_end(handle_) != ESP_OK) return false;
  return esp_ota_set_boot_partition(partition_) == ESP_OK;
}

void OtaUpdater::abort() {
  if (!active_) return;
  active_ = false;
  mbedtls_sha256_free(&sha_);
  esp_ota_abort(handle_);
}

}  // namespace hw
