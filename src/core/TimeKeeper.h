// TimeKeeper — tidsstyring uden RTC-batteri (spec §3.6, §6).
//
// Enheden kender først kalendertid efter NTP-sync via BG96. Indtil da
// tidsstemples målinger med relativ uptime i en "æra" (inkrementeres ved hver
// cold boot, hvor den relative tid nulstilles). Ved sync gemmes offsettet for
// den aktuelle æra, og records fra samme æra kan backfilles til kalendertid.
// ESP32's systemtid overlever deep sleep (RTC-tæller), men ikke strømsvigt —
// derfor æra-begrebet.
#pragma once
#include "IKvStore.h"
#include "Record.h"
#include <cstdint>

namespace core {

class TimeKeeper {
 public:
  explicit TimeKeeper(IKvStore& kv) : kv_(kv) {}

  // Kaldes ved cold boot (ikke ved deep-sleep-opvågnen): ny æra uden offset.
  void beginNewEra();
  uint16_t currentEra();

  bool isSynced();
  // Registrér NTP-resultat: 'epochNow' er kalendertid netop nu, 'uptimeNowS'
  // er den relative tid netop nu. Gemmer æra-offset til backfill.
  void sync(int64_t epochNow, int64_t uptimeNowS);

  // Tidsstempel + flags til en ny måling. 'uptimeNowS' er relativ tid nu.
  void stamp(Record& r, int64_t uptimeNowS);

  // Forsøg at konvertere r.ts til kalendertid. Returnerer true hvis r nu har
  // gyldig kalendertid (var allerede synced, eller blev backfillet fra kendt
  // æra-offset). false: tiden kan ikke afgøres (fremmed æra uden offset).
  bool resolve(Record& r);

 private:
  IKvStore& kv_;
};

}  // namespace core
