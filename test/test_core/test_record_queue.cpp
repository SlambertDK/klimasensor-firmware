#include <unity.h>
#include "Record.h"
#include "MeasurementQueue.h"
#include "fakes.h"

using namespace core;

static Record sample(uint32_t seq) {
  Record r;
  r.flags = FLAG_TS_SYNCED;
  r.era = 3;
  r.seq = seq;
  r.ts = 1756713600 + seq;
  r.tempCx100 = 2142;
  r.rhX100 = 4320;
  r.pm1_0x10 = 41; r.pm2_5x10 = 68; r.pm4_0x10 = 72; r.pm10x10 = 79;
  r.luxX100 = 31250;
  r.axMg = 10; r.ayMg = -20; r.azMg = 1000;
  return r;
}

void test_record_roundtrip() {
  Record in = sample(7), out;
  uint8_t buf[RECORD_SIZE];
  in.serialize(buf);
  TEST_ASSERT_TRUE(Record::deserialize(buf, out));
  TEST_ASSERT_EQUAL_UINT32(7, out.seq);
  TEST_ASSERT_EQUAL_INT16(2142, out.tempCx100);
  TEST_ASSERT_EQUAL_UINT16(4320, out.rhX100);
  TEST_ASSERT_EQUAL_UINT16(68, out.pm2_5x10);
  TEST_ASSERT_EQUAL_UINT32(31250, out.luxX100);
  TEST_ASSERT_EQUAL_INT16(-20, out.ayMg);
  TEST_ASSERT_EQUAL_INT64(1756713607, out.ts);
  TEST_ASSERT_EQUAL_UINT16(3, out.era);
  TEST_ASSERT_EQUAL_UINT8(FLAG_TS_SYNCED, out.flags);
}

void test_record_crc_rejects_corruption() {
  Record in = sample(1), out;
  uint8_t buf[RECORD_SIZE];
  in.serialize(buf);
  buf[10] ^= 0xFF;
  TEST_ASSERT_FALSE(Record::deserialize(buf, out));
}

void test_record_rejects_wrong_version() {
  Record in = sample(1), out;
  in.version = 99;
  uint8_t buf[RECORD_SIZE];
  in.serialize(buf);  // CRC er gyldig, men versionen ukendt
  TEST_ASSERT_FALSE(Record::deserialize(buf, out));
}

void test_queue_fifo_and_ack() {
  MemStorage st;
  MemKv kv;
  MeasurementQueue q(st, kv);
  for (uint32_t i = 0; i < 5; i++) TEST_ASSERT_TRUE(q.push(sample(i)));
  TEST_ASSERT_EQUAL(5, q.pendingCount());

  Record out[10];
  TEST_ASSERT_EQUAL(5, q.peek(out, 10));
  TEST_ASSERT_EQUAL_UINT32(0, out[0].seq);
  TEST_ASSERT_EQUAL_UINT32(4, out[4].seq);

  q.ack(2);
  TEST_ASSERT_EQUAL(3, q.pendingCount());
  TEST_ASSERT_EQUAL(3, q.peek(out, 10));
  TEST_ASSERT_EQUAL_UINT32(2, out[0].seq);  // FIFO fortsætter efter ack
}

void test_queue_truncates_when_fully_acked() {
  MemStorage st;
  MemKv kv;
  MeasurementQueue q(st, kv);
  for (uint32_t i = 0; i < 3; i++) q.push(sample(i));
  q.ack(3);
  TEST_ASSERT_EQUAL(0, q.pendingCount());
  TEST_ASSERT_EQUAL(0, st.size());  // lager genbrugt
  q.push(sample(99));
  Record out[1];
  TEST_ASSERT_EQUAL(1, q.peek(out, 1));
  TEST_ASSERT_EQUAL_UINT32(99, out[0].seq);
}

void test_queue_full_rejects_push() {
  MemStorage st(RECORD_SIZE * 2);  // plads til præcis 2 records
  MemKv kv;
  MeasurementQueue q(st, kv);
  TEST_ASSERT_FALSE(q.isFull());
  TEST_ASSERT_TRUE(q.push(sample(0)));
  TEST_ASSERT_TRUE(q.push(sample(1)));
  TEST_ASSERT_TRUE(q.isFull());
  TEST_ASSERT_FALSE(q.push(sample(2)));   // afvist — ingen data overskrives
  TEST_ASSERT_EQUAL(2, q.pendingCount()); // eksisterende data urørt
}

void test_queue_ack_survives_restart() {
  MemStorage st;
  MemKv kv;
  {
    MeasurementQueue q(st, kv);
    for (uint32_t i = 0; i < 4; i++) q.push(sample(i));
    q.ack(2);
  }
  MeasurementQueue q2(st, kv);  // "genstart": samme storage + kv
  Record out[10];
  TEST_ASSERT_EQUAL(2, q2.peek(out, 10));
  TEST_ASSERT_EQUAL_UINT32(2, out[0].seq);
}

void test_queue_skips_corrupt_record() {
  MemStorage st;
  MemKv kv;
  MeasurementQueue q(st, kv);
  q.push(sample(0));
  q.push(sample(1));
  q.push(sample(2));
  st.buf_[RECORD_SIZE + 5] ^= 0xFF;  // korruptér record #1
  Record out[10];
  TEST_ASSERT_EQUAL(2, q.peek(out, 10));
  TEST_ASSERT_EQUAL_UINT32(0, out[0].seq);
  TEST_ASSERT_EQUAL_UINT32(2, out[1].seq);  // efterfølgende data ikke mistet
  q.ack(2);
  TEST_ASSERT_EQUAL(0, q.pendingCount());
}
