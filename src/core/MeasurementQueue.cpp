#include "MeasurementQueue.h"

namespace core {

namespace {
const char* KEY_ACK = "q_ack";
}

MeasurementQueue::MeasurementQueue(IQueueStorage& storage, IKvStore& kv)
    : storage_(storage), kv_(kv) {
  // Defensivt: peger ack-markøren udenfor lageret (fx efter delvis skrivning
  // afbrudt af strømsvigt), klampes den til lagerets faktiske længde.
  size_t sz = storage_.size();
  if (ackOffset() > sz) setAckOffset(sz);
}

size_t MeasurementQueue::ackOffset() { return kv_.getU32(KEY_ACK, 0); }
void MeasurementQueue::setAckOffset(size_t off) { kv_.setU32(KEY_ACK, static_cast<uint32_t>(off)); }

bool MeasurementQueue::push(const Record& r) {
  if (isFull()) return false;
  uint8_t buf[RECORD_SIZE];
  r.serialize(buf);
  return storage_.append(buf, RECORD_SIZE);
}

size_t MeasurementQueue::peek(Record* out, size_t maxCount) {
  size_t off = ackOffset();
  size_t sz = storage_.size();
  size_t n = 0;
  uint8_t buf[RECORD_SIZE];
  while (n < maxCount && off + RECORD_SIZE <= sz) {
    if (!storage_.read(off, buf, RECORD_SIZE)) break;
    off += RECORD_SIZE;
    if (!Record::deserialize(buf, out[n])) continue;  // korrupt record: spring over
    n++;
  }
  return n;
}

void MeasurementQueue::ack(size_t count) {
  // Ack gælder de records peek() ville levere; korrupte records mellem dem
  // kvitteres implicit med (de kan alligevel aldrig leveres).
  size_t off = ackOffset();
  size_t sz = storage_.size();
  uint8_t buf[RECORD_SIZE];
  Record tmp;
  size_t acked = 0;
  while (acked < count && off + RECORD_SIZE <= sz) {
    if (!storage_.read(off, buf, RECORD_SIZE)) break;
    off += RECORD_SIZE;
    if (Record::deserialize(buf, tmp)) acked++;
  }
  setAckOffset(off);
  // Alt kvitteret: genbrug pladsen. (Trunkering sker kun her, så en record
  // aldrig fjernes før den er kvitteret.)
  if (off >= sz && sz > 0) {
    if (storage_.truncate()) setAckOffset(0);
  }
}

size_t MeasurementQueue::pendingCount() {
  size_t off = ackOffset();
  size_t sz = storage_.size();
  size_t n = 0;
  uint8_t buf[RECORD_SIZE];
  Record tmp;
  while (off + RECORD_SIZE <= sz) {
    if (!storage_.read(off, buf, RECORD_SIZE)) break;
    off += RECORD_SIZE;
    if (Record::deserialize(buf, tmp)) n++;
  }
  return n;
}

bool MeasurementQueue::isFull() {
  return storage_.size() + RECORD_SIZE > storage_.capacity();
}

}  // namespace core
