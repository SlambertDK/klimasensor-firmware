#include <unity.h>
#include "Cycle.h"
#include "fakes.h"

using namespace core;

namespace {
struct Rig {
  MemStorage storage;
  MemKv kv;
  FakeSensors sensors;
  FakeNet net;
  MeasurementQueue queue{storage, kv};
  ConfigStore config{kv};
  TimeKeeper time{kv};
  CycleIdentity id{"123456789", "AABBCC", "1.0.0", "http://srv/api/v1/measurements"};
  Cycle cycle{sensors, net, queue, config, time, kv, id};

  Rig(size_t cap = 1024 * 1024) : storage(cap) { time.beginNewEra(); }
};
}  // namespace

void test_cycle_measures_and_uploads() {
  Rig rig;
  rig.net.responses = {R"({"ackCount":1})"};
  CycleResult res = rig.cycle.run(100);
  TEST_ASSERT_TRUE(res.measured);
  TEST_ASSERT_TRUE(res.uploadAttempted);
  TEST_ASSERT_TRUE(res.uploadSucceeded);
  TEST_ASSERT_EQUAL(1, res.recordsAcked);
  TEST_ASSERT_EQUAL(0, rig.queue.pendingCount());
  TEST_ASSERT_EQUAL(1, rig.net.shutDownCalls);
  TEST_ASSERT_EQUAL_UINT32(300, res.sleepSeconds);
}

void test_cycle_data_survives_net_failure() {
  Rig rig;
  rig.net.bringUpOk = false;
  CycleResult res = rig.cycle.run(100);
  TEST_ASSERT_TRUE(res.measured);
  TEST_ASSERT_TRUE(res.uploadAttempted);
  TEST_ASSERT_FALSE(res.uploadSucceeded);
  TEST_ASSERT_EQUAL(1, rig.queue.pendingCount());  // data ligger sikkert i køen

  // Forbindelsen kommer igen: begge målinger afleveres.
  rig.net.bringUpOk = true;
  rig.net.responses = {R"({"ackCount":2})"};
  res = rig.cycle.run(400);
  TEST_ASSERT_TRUE(res.uploadSucceeded);
  TEST_ASSERT_EQUAL(2, res.recordsAcked);
  TEST_ASSERT_EQUAL(0, rig.queue.pendingCount());
}

void test_cycle_upload_only_when_due() {
  Rig rig;
  rig.net.responses = {R"({"ackCount":1})"};
  rig.cycle.run(100);  // første upload (aldrig uploadet før → forfalden)
  CycleResult res = rig.cycle.run(200);  // 100 s senere, interval er 300 s
  TEST_ASSERT_TRUE(res.measured);
  TEST_ASSERT_FALSE(res.uploadAttempted);
  TEST_ASSERT_EQUAL(1, rig.queue.pendingCount());

  rig.net.responses = {R"({"ackCount":1})"};
  res = rig.cycle.run(401);  // >300 s efter sidste upload
  TEST_ASSERT_TRUE(res.uploadAttempted);
}

void test_cycle_batches_until_queue_empty() {
  Rig rig;
  Record r;
  for (uint32_t i = 0; i < 60; i++) {  // 60 > MAX_BATCH(50) ⇒ to batches
    rig.time.stamp(r, i);
    r.seq = i;
    rig.queue.push(r);
  }
  rig.net.responses = {R"({"ackCount":50})", R"({"ackCount":11})"};
  CycleResult res = rig.cycle.run(100);
  TEST_ASSERT_EQUAL(2 , rig.net.posts.size());
  TEST_ASSERT_EQUAL(61, res.recordsAcked);  // 60 + cyklussens egen måling
  TEST_ASSERT_EQUAL(0, rig.queue.pendingCount());
}

void test_cycle_applies_server_config() {
  Rig rig;
  rig.net.responses = {R"({"ackCount":1,"config":{"measureIntervalS":600,"uploadIntervalS":1800}})"};
  CycleResult res = rig.cycle.run(100);
  TEST_ASSERT_EQUAL_UINT32(600, res.sleepSeconds);  // ny config gælder straks
  DeviceConfig cfg = rig.config.load();
  TEST_ASSERT_EQUAL_UINT32(600, cfg.measureIntervalS);
  TEST_ASSERT_EQUAL_UINT32(1800, cfg.uploadIntervalS);
}

void test_cycle_full_queue_stops_measuring_but_uploads() {
  Rig rig(RECORD_SIZE * 2);
  Record r;
  rig.time.stamp(r, 1);
  rig.queue.push(r);
  rig.queue.push(r);  // fuld
  rig.net.responses = {R"({"ackCount":0})"};  // serveren kvitterer intet
  CycleResult res = rig.cycle.run(100);
  TEST_ASSERT_FALSE(res.measured);  // ingen ny måling — data røres aldrig
  TEST_ASSERT_TRUE(res.uploadAttempted);
  TEST_ASSERT_EQUAL(2, rig.queue.pendingCount());
  TEST_ASSERT_TRUE(rig.net.posts[0].find("\"storageFull\":true") != std::string::npos);
}

void test_cycle_ntp_syncs_once_then_backfills() {
  Rig rig;
  rig.net.ntpEpoch = 1756713600;
  rig.net.responses = {R"({"ackCount":1})"};
  rig.cycle.run(100);  // måling ved uptime 100, sync ved upload
  TEST_ASSERT_EQUAL(1, rig.net.ntpCalls);
  TEST_ASSERT_TRUE(rig.time.isSynced());
  // Målingen blev taget FØR sync men uploadet med backfillet epoch-tid:
  TEST_ASSERT_TRUE(rig.net.posts[0].find("\"ts\":1756713600") != std::string::npos);
  TEST_ASSERT_TRUE(rig.net.posts[0].find("tsValid") == std::string::npos);

  rig.net.responses = {R"({"ackCount":1})"};
  rig.cycle.run(500);
  TEST_ASSERT_EQUAL(1, rig.net.ntpCalls);  // ingen re-sync inden for 24 t
}

void test_cycle_ota_triggered_from_response() {
  Rig rig;
  rig.net.responses = {R"({"ackCount":1,"ota":{"version":"1.1.0","url":"http://s/fw.bin","sha256":"ab"}})"};
  rig.cycle.run(100);
  TEST_ASSERT_EQUAL(1, rig.net.otaUrls.size());
  TEST_ASSERT_EQUAL_STRING("http://s/fw.bin", rig.net.otaUrls[0].c_str());
}

void test_cycle_sensor_failure_still_records() {
  Rig rig;
  rig.sensors.next.tempCx100 = INVALID_I16;
  rig.sensors.next.luxX100 = INVALID_U32;
  rig.net.responses = {R"({"ackCount":1})"};
  CycleResult res = rig.cycle.run(100);
  TEST_ASSERT_TRUE(res.measured);
  TEST_ASSERT_TRUE(rig.net.posts[0].find("\"tempC\":null") != std::string::npos);
  TEST_ASSERT_TRUE(rig.net.posts[0].find("\"lux\":null") != std::string::npos);
}
