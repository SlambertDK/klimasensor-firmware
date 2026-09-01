#include <unity.h>

// test_record_queue.cpp
void test_record_roundtrip();
void test_record_crc_rejects_corruption();
void test_record_rejects_wrong_version();
void test_queue_fifo_and_ack();
void test_queue_truncates_when_fully_acked();
void test_queue_full_rejects_push();
void test_queue_ack_survives_restart();
void test_queue_skips_corrupt_record();
// test_config_time.cpp
void test_config_defaults();
void test_config_save_load_roundtrip();
void test_config_apply_server_clamps_and_persists();
void test_config_apply_zero_means_unchanged();
void test_time_unsynced_stamp_is_relative();
void test_time_synced_stamp_is_epoch();
void test_time_backfill_same_era();
void test_time_foreign_era_unresolvable();
void test_time_era_increments_and_persists();
// test_uplink.cpp
void test_payload_matches_contract();
void test_payload_null_fields_on_sensor_failure();
void test_payload_marks_invalid_ts();
void test_parse_full_response();
void test_parse_minimal_response();
void test_parse_garbage_fails();
// test_cycle.cpp
void test_cycle_measures_and_uploads();
void test_cycle_data_survives_net_failure();
void test_cycle_upload_only_when_due();
void test_cycle_batches_until_queue_empty();
void test_cycle_applies_server_config();
void test_cycle_full_queue_stops_measuring_but_uploads();
void test_cycle_ntp_syncs_once_then_backfills();
void test_cycle_ota_triggered_from_response();
void test_cycle_sensor_failure_still_records();

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_record_roundtrip);
  RUN_TEST(test_record_crc_rejects_corruption);
  RUN_TEST(test_record_rejects_wrong_version);
  RUN_TEST(test_queue_fifo_and_ack);
  RUN_TEST(test_queue_truncates_when_fully_acked);
  RUN_TEST(test_queue_full_rejects_push);
  RUN_TEST(test_queue_ack_survives_restart);
  RUN_TEST(test_queue_skips_corrupt_record);
  RUN_TEST(test_config_defaults);
  RUN_TEST(test_config_save_load_roundtrip);
  RUN_TEST(test_config_apply_server_clamps_and_persists);
  RUN_TEST(test_config_apply_zero_means_unchanged);
  RUN_TEST(test_time_unsynced_stamp_is_relative);
  RUN_TEST(test_time_synced_stamp_is_epoch);
  RUN_TEST(test_time_backfill_same_era);
  RUN_TEST(test_time_foreign_era_unresolvable);
  RUN_TEST(test_time_era_increments_and_persists);
  RUN_TEST(test_payload_matches_contract);
  RUN_TEST(test_payload_null_fields_on_sensor_failure);
  RUN_TEST(test_payload_marks_invalid_ts);
  RUN_TEST(test_parse_full_response);
  RUN_TEST(test_parse_minimal_response);
  RUN_TEST(test_parse_garbage_fails);
  RUN_TEST(test_cycle_measures_and_uploads);
  RUN_TEST(test_cycle_data_survives_net_failure);
  RUN_TEST(test_cycle_upload_only_when_due);
  RUN_TEST(test_cycle_batches_until_queue_empty);
  RUN_TEST(test_cycle_applies_server_config);
  RUN_TEST(test_cycle_full_queue_stops_measuring_but_uploads);
  RUN_TEST(test_cycle_ntp_syncs_once_then_backfills);
  RUN_TEST(test_cycle_ota_triggered_from_response);
  RUN_TEST(test_cycle_sensor_failure_still_records);
  return UNITY_END();
}
