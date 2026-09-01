// AtParser — parsning af BG96 AT-svar. Ren C++ (native-testbar).
#pragma once
#include <cstdint>

namespace core {

// "+QNTP: 0,\"2026/09/01,08:14:32+08\"" → epoch-sekunder (UTC).
// Tidszonefeltet er kvarte timer (BG96-konvention). false ved fejl/err!=0.
bool parseQntp(const char* line, int64_t& epochOut);

// "+QHTTPPOST: 0,200,5" / "+QHTTPGET: 0,200,124" → err og httpCode (og evt. længde).
bool parseQhttpResult(const char* line, int& errOut, int& httpCodeOut, long& lengthOut);

// Gregoriansk dato → dage siden epoch (Howard Hinnant's days_from_civil).
int64_t daysFromCivil(int y, int m, int d);

}  // namespace core
