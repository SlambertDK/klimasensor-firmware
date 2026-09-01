// MeasurementQueue — kvitteringsbaseret FIFO-kø af Records oven på IQueueStorage.
//
// Datatabs-garanti (spec §3.5): en record slettes først, når serveren har
// kvitteret den (ack). Ack-positionen persisteres via IKvStore, så den
// overlever deep sleep, genstart og strømsvigt. Når ALT er kvitteret,
// trunkeres lageret. Er lageret fuldt, afvises nye målinger (isFull()).
#pragma once
#include "IQueueStorage.h"
#include "IKvStore.h"
#include "Record.h"

namespace core {

class MeasurementQueue {
 public:
  static constexpr size_t MAX_BATCH = 50;

  MeasurementQueue(IQueueStorage& storage, IKvStore& kv);

  bool push(const Record& r);            // false hvis fuld eller IO-fejl
  // Læs op til maxCount ukvitterede records i FIFO-rækkefølge; korrupte
  // records springes over. Returnerer antal læste.
  size_t peek(Record* out, size_t maxCount);
  void ack(size_t count);                // kvittér de 'count' første ukvitterede
  size_t pendingCount();                 // antal ukvitterede records
  bool isFull();

 private:
  size_t ackOffset();                    // persisteret byte-offset for ack-markør
  void setAckOffset(size_t off);

  IQueueStorage& storage_;
  IKvStore& kv_;
};

}  // namespace core
