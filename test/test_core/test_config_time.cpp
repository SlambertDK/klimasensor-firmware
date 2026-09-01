#include <unity.h>
#include "Config.h"
#include "TimeKeeper.h"
#include "fakes.h"
#include <cstring>

using namespace core;

void test_config_defaults() {
  MemKv kv;
  ConfigStore cs(kv);
  DeviceConfig cfg = cs.load();
  TEST_ASSERT_EQUAL_UINT32(300, cfg.measureIntervalS);
  TEST_ASSERT_EQUAL_UINT32(300, cfg.uploadIntervalS);
  TEST_ASSERT_EQUAL_STRING("pool.ntp.org", cfg.ntpServer);
}

void test_config_save_load_roundtrip() {
  MemKv kv;
  ConfigStore cs(kv);
  DeviceConfig cfg;
  cfg.measureIntervalS = 600;
  cfg.uploadIntervalS = 3600;
  strcpy(cfg.ntpServer, "ntp.energinet.dk");
  cs.save(cfg);
  DeviceConfig loaded = cs.load();
  TEST_ASSERT_EQUAL_UINT32(600, loaded.measureIntervalS);
  TEST_ASSERT_EQUAL_UINT32(3600, loaded.uploadIntervalS);
  TEST_ASSERT_EQUAL_STRING("ntp.energinet.dk", loaded.ntpServer);
}

void test_config_apply_server_clamps_and_persists() {
  MemKv kv;
  ConfigStore cs(kv);
  DeviceConfig cfg = cs.load();
  TEST_ASSERT_TRUE(cs.applyServerConfig(cfg, 10, 999999, ""));  // 10 < min, 999999 > max
  TEST_ASSERT_EQUAL_UINT32(ConfigStore::MIN_INTERVAL_S, cfg.measureIntervalS);
  TEST_ASSERT_EQUAL_UINT32(ConfigStore::MAX_INTERVAL_S, cfg.uploadIntervalS);
  TEST_ASSERT_EQUAL_STRING("pool.ntp.org", cfg.ntpServer);  // tom streng = uændret
  DeviceConfig reloaded = cs.load();  // ændringer blev persisteret
  TEST_ASSERT_EQUAL_UINT32(ConfigStore::MIN_INTERVAL_S, reloaded.measureIntervalS);
}

void test_config_apply_zero_means_unchanged() {
  MemKv kv;
  ConfigStore cs(kv);
  DeviceConfig cfg = cs.load();
  TEST_ASSERT_FALSE(cs.applyServerConfig(cfg, 0, 0, ""));
  TEST_ASSERT_EQUAL_UINT32(300, cfg.measureIntervalS);
}

void test_time_unsynced_stamp_is_relative() {
  MemKv kv;
  TimeKeeper tk(kv);
  tk.beginNewEra();
  Record r;
  tk.stamp(r, 1234);
  TEST_ASSERT_EQUAL_INT64(1234, r.ts);
  TEST_ASSERT_EQUAL(0, r.flags & FLAG_TS_SYNCED);
  TEST_ASSERT_EQUAL_UINT16(1, r.era);
  TEST_ASSERT_FALSE(tk.isSynced());
}

void test_time_synced_stamp_is_epoch() {
  MemKv kv;
  TimeKeeper tk(kv);
  tk.beginNewEra();
  tk.sync(1756713600, 1000);  // kl. uptime=1000 er epoch 1756713600
  TEST_ASSERT_TRUE(tk.isSynced());
  Record r;
  tk.stamp(r, 1300);
  TEST_ASSERT_EQUAL_INT64(1756713900, r.ts);
  TEST_ASSERT_EQUAL(FLAG_TS_SYNCED, r.flags & FLAG_TS_SYNCED);
}

void test_time_backfill_same_era() {
  MemKv kv;
  TimeKeeper tk(kv);
  tk.beginNewEra();
  Record r;
  tk.stamp(r, 500);  // målt før sync
  TEST_ASSERT_EQUAL(0, r.flags & FLAG_TS_SYNCED);
  tk.sync(1756713600, 1000);
  TEST_ASSERT_TRUE(tk.resolve(r));  // backfill: 500 + (1756713600 - 1000)
  TEST_ASSERT_EQUAL_INT64(1756713100, r.ts);
  TEST_ASSERT_EQUAL(FLAG_TS_SYNCED, r.flags & FLAG_TS_SYNCED);
}

void test_time_foreign_era_unresolvable() {
  MemKv kv;
  TimeKeeper tk(kv);
  tk.beginNewEra();
  Record r;
  tk.stamp(r, 500);   // æra 1, aldrig synced
  tk.beginNewEra();   // strømsvigt → æra 2
  tk.sync(1756713600, 40);
  TEST_ASSERT_FALSE(tk.resolve(r));  // æra 1's offset kendes ikke
  TEST_ASSERT_EQUAL_INT64(500, r.ts);
}

void test_time_era_increments_and_persists() {
  MemKv kv;
  TimeKeeper tk(kv);
  TEST_ASSERT_EQUAL_UINT16(0, tk.currentEra());
  tk.beginNewEra();
  tk.beginNewEra();
  TimeKeeper tk2(kv);
  TEST_ASSERT_EQUAL_UINT16(2, tk2.currentEra());
}
