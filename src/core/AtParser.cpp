#include "AtParser.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace core {

int64_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

bool parseQntp(const char* line, int64_t& epochOut) {
  const char* p = strstr(line, "+QNTP:");
  if (!p) return false;
  int err = -1, y, mo, d, h, mi, s, tz;
  // Format: +QNTP: 0,"yyyy/MM/dd,hh:mm:ss±zz" (zz = kvarte timer)
  if (sscanf(p, "+QNTP: %d,\"%d/%d/%d,%d:%d:%d%d\"", &err, &y, &mo, &d, &h, &mi, &s, &tz) != 8)
    return false;
  if (err != 0) return false;
  int64_t localEpoch = daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + s;
  epochOut = localEpoch - static_cast<int64_t>(tz) * 900;  // lokal → UTC
  return true;
}

bool parseQhttpResult(const char* line, int& errOut, int& httpCodeOut, long& lengthOut) {
  const char* p = strstr(line, ": ");
  if (!p) return false;
  p += 2;
  char* end;
  errOut = static_cast<int>(strtol(p, &end, 10));
  httpCodeOut = 0;
  lengthOut = 0;
  if (*end == ',') {
    httpCodeOut = static_cast<int>(strtol(end + 1, &end, 10));
    if (*end == ',') lengthOut = strtol(end + 1, nullptr, 10);
  }
  return true;
}

}  // namespace core
