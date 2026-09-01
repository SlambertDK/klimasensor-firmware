#include <unity.h>
#include "AtParser.h"
#include <cstring>

using namespace core;

void test_qntp_parses_utc_epoch() {
  int64_t epoch = 0;
  // 2026-09-01 08:14:32 lokal, +08 kvarte timer = UTC+2 ⇒ UTC 06:14:32
  TEST_ASSERT_TRUE(parseQntp("+QNTP: 0,\"2026/09/01,08:14:32+08\"", epoch));
  // 2026-09-01T06:14:32Z = 1788243272
  TEST_ASSERT_EQUAL_INT64(1788243272LL, epoch);
}

void test_qntp_zero_offset_and_error() {
  int64_t epoch = 0;
  TEST_ASSERT_TRUE(parseQntp("+QNTP: 0,\"1970/01/01,00:00:00+00\"", epoch));
  TEST_ASSERT_EQUAL_INT64(0, epoch);
  TEST_ASSERT_FALSE(parseQntp("+QNTP: 5,\"2026/09/01,08:14:32+08\"", epoch));  // err != 0
  TEST_ASSERT_FALSE(parseQntp("garbage", epoch));
}

void test_qhttp_result_parses() {
  int err, code;
  long len;
  TEST_ASSERT_TRUE(parseQhttpResult("+QHTTPPOST: 0,200,5", err, code, len));
  TEST_ASSERT_EQUAL(0, err);
  TEST_ASSERT_EQUAL(200, code);
  TEST_ASSERT_EQUAL(5, len);
  TEST_ASSERT_TRUE(parseQhttpResult("+QHTTPGET: 0,404,0", err, code, len));
  TEST_ASSERT_EQUAL(404, code);
  TEST_ASSERT_TRUE(parseQhttpResult("+QHTTPPOST: 703", err, code, len));  // fejl uden kode
  TEST_ASSERT_EQUAL(703, err);
  TEST_ASSERT_EQUAL(0, code);
}
