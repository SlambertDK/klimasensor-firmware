#include <unity.h>
#include "Uplink.h"
#include <ArduinoJson.h>

using namespace core;

static Record measured() {
  Record r;
  r.flags = FLAG_TS_SYNCED;
  r.ts = 1756713600;
  r.tempCx100 = 2142;
  r.rhX100 = 4320;
  r.pm1_0x10 = 41; r.pm2_5x10 = 68; r.pm4_0x10 = 72; r.pm10x10 = 79;
  r.luxX100 = 31250;
  r.axMg = 10; r.ayMg = -20; r.azMg = 1000;
  return r;
}

void test_payload_matches_contract() {
  Record r = measured();
  bool tsValid[1] = {true};
  std::string json = Uplink::buildPayload("123456789", "AABBCC", "1.0.0", false, &r, tsValid, 1);

  JsonDocument doc;
  TEST_ASSERT_TRUE(deserializeJson(doc, json) == DeserializationError::Ok);
  TEST_ASSERT_EQUAL_STRING("123456789", doc["token"] | "");
  TEST_ASSERT_EQUAL_STRING("AABBCC", doc["deviceId"] | "");
  TEST_ASSERT_EQUAL_STRING("1.0.0", doc["fwVersion"] | "");
  TEST_ASSERT_FALSE(doc["storageFull"] | true);
  JsonObject m = doc["measurements"][0];
  TEST_ASSERT_EQUAL_INT64(1756713600, m["ts"] | 0LL);
  TEST_ASSERT_FALSE(m["tsValid"].is<bool>());  // feltet udelades ved gyldig tid
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 21.42f, m["tempC"] | 0.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 43.20f, m["rh"] | 0.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.8f, m["pm2_5"] | 0.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 312.5f, m["lux"] | 0.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -0.02f, m["accel"]["y"] | 0.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, m["accel"]["z"] | 0.0f);
}

void test_payload_null_fields_on_sensor_failure() {
  Record r = measured();
  r.tempCx100 = INVALID_I16;
  r.rhX100 = INVALID_U16;
  r.pm2_5x10 = INVALID_U16;
  r.luxX100 = INVALID_U32;
  bool tsValid[1] = {true};
  std::string json = Uplink::buildPayload("t", "d", "1.0.0", false, &r, tsValid, 1);
  JsonDocument doc;
  deserializeJson(doc, json);
  JsonObject m = doc["measurements"][0];
  TEST_ASSERT_TRUE(m["tempC"].isNull());
  TEST_ASSERT_TRUE(m["rh"].isNull());
  TEST_ASSERT_TRUE(m["pm2_5"].isNull());
  TEST_ASSERT_TRUE(m["lux"].isNull());
  TEST_ASSERT_FALSE(m["pm10"].isNull());  // fungerende felter bevares
}

void test_payload_marks_invalid_ts() {
  Record r = measured();
  bool tsValid[1] = {false};
  std::string json = Uplink::buildPayload("t", "d", "1.0.0", true, &r, tsValid, 1);
  JsonDocument doc;
  deserializeJson(doc, json);
  TEST_ASSERT_TRUE(doc["storageFull"] | false);
  TEST_ASSERT_FALSE(doc["measurements"][0]["tsValid"] | true);
}

void test_parse_full_response() {
  const char* json = R"({"ackCount":12,
    "config":{"measureIntervalS":600,"uploadIntervalS":3600,"ntpServer":"ntp.x.dk"},
    "ota":{"version":"1.1.0","url":"http://s/fw.bin","sha256":"abc123"}})";
  ServerResponse sr;
  TEST_ASSERT_TRUE(Uplink::parseResponse(json, strlen(json), sr));
  TEST_ASSERT_EQUAL(12, sr.ackCount);
  TEST_ASSERT_EQUAL_UINT32(600, sr.measureIntervalS);
  TEST_ASSERT_EQUAL_UINT32(3600, sr.uploadIntervalS);
  TEST_ASSERT_EQUAL_STRING("ntp.x.dk", sr.ntpServer);
  TEST_ASSERT_TRUE(sr.hasOta());
  TEST_ASSERT_EQUAL_STRING("http://s/fw.bin", sr.otaUrl);
  TEST_ASSERT_EQUAL_STRING("abc123", sr.otaSha256);
}

void test_parse_minimal_response() {
  const char* json = R"({"ackCount":3})";
  ServerResponse sr;
  TEST_ASSERT_TRUE(Uplink::parseResponse(json, strlen(json), sr));
  TEST_ASSERT_EQUAL(3, sr.ackCount);
  TEST_ASSERT_EQUAL_UINT32(0, sr.measureIntervalS);  // 0 = ikke sendt
  TEST_ASSERT_FALSE(sr.hasOta());
}

void test_parse_garbage_fails() {
  ServerResponse sr;
  TEST_ASSERT_FALSE(Uplink::parseResponse("not json", 8, sr));
  TEST_ASSERT_FALSE(Uplink::parseResponse("[1,2]", 5, sr));
  TEST_ASSERT_EQUAL(0, sr.ackCount);
}
