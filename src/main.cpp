// Klimasensor-firmware — entry point.
// Arkitektur: deep-sleep-cyklus (spec §4). setup() kører én hel cyklus og
// lægger enheden i deep sleep; loop() nås aldrig.
#include <Arduino.h>
#include <Wire.h>
#include <LittleFS.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>
#include <sys/time.h>

#include "board_pins.h"
#include "Cycle.h"
#include "hw/HwSensors.h"
#include "hw/Bg96.h"
#include "hw/LittleFsQueueStorage.h"
#include "hw/NvsKvStore.h"

namespace {

// Relativ tid der overlever deep sleep (ESP32's systemur kører videre på
// RTC-tælleren under søvn) men nulstilles ved cold boot/strømsvigt.
int64_t uptimeSeconds() {
  timeval tv;
  gettimeofday(&tv, nullptr);
  return tv.tv_sec;
}

String deviceIdHex() {
  uint64_t mac = ESP.getEfuseMac();
  char buf[13];
  snprintf(buf, sizeof(buf), "%012llX", static_cast<unsigned long long>(mac));
  return String(buf);
}

void goToSleep(uint32_t seconds) {
  Serial.printf("[main] deep sleep i %u s\n", seconds);
  Serial.flush();
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);
  esp_deep_sleep_start();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.printf("\n[main] klimasensor fw %s\n", FW_VERSION);

  // Watchdog: hænger cyklussen (modem/sensor der aldrig svarer), genstartes
  // enheden. Data er sikre — de persisteres før upload-forsøg.
  esp_task_wdt_init(CYCLE_WATCHDOG_S, true);
  esp_task_wdt_add(nullptr);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
  Wire.setTimeOut(200);  // SPS30 bruger clock stretching — vær tålmodig

  if (!LittleFS.begin(true /* formatér ved første boot */)) {
    Serial.println("[main] LittleFS-fejl - genstarter om 60 s");
    goToSleep(60);
  }

  hw::NvsKvStore kv;
  core::TimeKeeper timeKeeper(kv);

  // Cold boot (reset/strømsvigt) nulstiller det relative ur → ny tids-æra.
  // Timer-opvågnen fra deep sleep bevarer uret og dermed æraen.
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER) {
    timeKeeper.beginNewEra();
    Serial.printf("[main] cold boot - tids-aera %u\n", timeKeeper.currentEra());
  }

  hw::LittleFsQueueStorage storage;
  core::MeasurementQueue queue(storage, kv);
  core::ConfigStore configStore(kv);
  hw::HwSensors sensors;
  HardwareSerial modemUart(1);
  hw::Bg96 net(modemUart);

  String deviceId = deviceIdHex();
  core::CycleIdentity id{API_TOKEN, deviceId.c_str(), FW_VERSION, SERVER_URL};
  core::Cycle cycle(sensors, net, queue, configStore, timeKeeper, kv, id);

  core::CycleResult res = cycle.run(uptimeSeconds());
  Serial.printf("[main] maalt=%d upload=%d/%d acked=%u pending=%u\n",
                res.measured, res.uploadAttempted, res.uploadSucceeded,
                static_cast<unsigned>(res.recordsAcked),
                static_cast<unsigned>(queue.pendingCount()));

  esp_task_wdt_delete(nullptr);
  goToSleep(res.sleepSeconds);
}

void loop() {
  // Nås aldrig: setup() ender altid i deep sleep.
}
