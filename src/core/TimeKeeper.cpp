#include "TimeKeeper.h"
#include <cstdio>

namespace core {

namespace {
const char* KEY_ERA = "tk_era";
// Pr.-æra-offset gemmes under nøglen "tk_off_<era>"; kun de seneste æraer er
// interessante, og NVS-slid er ubetydeligt (én skrivning pr. cold boot).
void offsetKey(char* out, size_t len, uint16_t era) {
  snprintf(out, len, "tk_off_%u", static_cast<unsigned>(era));
}
const int64_t NO_OFFSET = INT64_MIN;
}  // namespace

void TimeKeeper::beginNewEra() {
  kv_.setU32(KEY_ERA, kv_.getU32(KEY_ERA, 0) + 1);
}

uint16_t TimeKeeper::currentEra() {
  return static_cast<uint16_t>(kv_.getU32(KEY_ERA, 0));
}

bool TimeKeeper::isSynced() {
  char key[24];
  offsetKey(key, sizeof(key), currentEra());
  return kv_.getI64(key, NO_OFFSET) != NO_OFFSET;
}

void TimeKeeper::sync(int64_t epochNow, int64_t uptimeNowS) {
  char key[24];
  offsetKey(key, sizeof(key), currentEra());
  kv_.setI64(key, epochNow - uptimeNowS);
}

void TimeKeeper::stamp(Record& r, int64_t uptimeNowS) {
  r.era = currentEra();
  char key[24];
  offsetKey(key, sizeof(key), r.era);
  int64_t off = kv_.getI64(key, NO_OFFSET);
  if (off != NO_OFFSET) {
    r.ts = uptimeNowS + off;
    r.flags |= FLAG_TS_SYNCED;
  } else {
    r.ts = uptimeNowS;
    r.flags &= ~FLAG_TS_SYNCED;
  }
}

bool TimeKeeper::resolve(Record& r) {
  if (r.flags & FLAG_TS_SYNCED) return true;
  char key[24];
  offsetKey(key, sizeof(key), r.era);
  int64_t off = kv_.getI64(key, NO_OFFSET);
  if (off == NO_OFFSET) return false;
  r.ts += off;
  r.flags |= FLAG_TS_SYNCED;
  return true;
}

}  // namespace core
