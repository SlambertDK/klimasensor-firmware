# Klimasensor Firmware Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Batteridrevet ESP32-firmware der måler klima-data (SHTC3/VEML6040/LIS2DH12/SPS30), persisterer i flash uden datatab, og uploader via BG96 HTTP med server-styret konfiguration, NTP-tid og OTA.

**Architecture:** Deep-sleep-cyklus-state-machine. Al kerne-logik (queue, config, payload, tid, cyklus-beslutninger) er hardware-uafhængig og unit-testes native med mocks; drivere og Arduino-adaptere er tynde skaller udenom. Spec: `docs/superpowers/specs/2026-09-01-klimasensor-firmware-design.md`.

**Tech Stack:** PlatformIO, Arduino-core for ESP32, ArduinoJson, LittleFS, NVS (Preferences), Unity (native tests), Python-mockserver.

---

## Filstruktur

```
platformio.ini                      # env:esp32 (arduino) + env:native (tests)
include/board_pins.h                # AL pin-mapping + build-defaults (APN, server-URL, token)
src/core/Record.h/.cpp              # binært måleformat (40 B) + CRC16 — ren C++
src/core/MeasurementQueue.h/.cpp    # ack-baseret kø oven på IQueueStorage — ren C++
src/core/IQueueStorage.h            # interface: read/append/truncate/size/capacity
src/core/Config.h/.cpp              # config-struct, validering, persist via IKvStore — ren C++
src/core/IKvStore.h                 # interface: get/set af u32/string
src/core/TimeKeeper.h/.cpp          # era/offset-logik, backfill af tidsstempler — ren C++
src/core/Uplink.h/.cpp              # JSON-payload-byg + svar-parse (ack/config/ota) — ren C++
src/core/Cycle.h/.cpp               # state machine; afhænger kun af interfaces — ren C++
src/core/ISensors.h                 # interface: readClimate/readLight/readAccel/readPm
src/core/INet.h                     # interface: bringUp/httpPost/ntpTime/otaFetch/shutDown
src/hw/Shtc3.cpp/.h                 # I2C-driver
src/hw/Veml6040.cpp/.h              # I2C-driver + lux-beregning
src/hw/Lis2dh12.cpp/.h              # I2C-driver
src/hw/Sps30.cpp/.h                 # I2C-driver (UART bag SPS30_USE_UART-flag: v2)
src/hw/Bg96.cpp/.h                  # AT-driver: power, attach, QNTP, QHTTP*
src/hw/AtParser.h/.cpp              # AT-svar-parsning, ren C++ (native-testbar)
src/hw/OtaUpdater.cpp/.h            # QHTTP-stream → esp_ota + SHA-256
src/hw/LittleFsQueueStorage.cpp/.h  # IQueueStorage på LittleFS
src/hw/NvsKvStore.cpp/.h            # IKvStore på Preferences/NVS
src/main.cpp                        # setup(): wire alt sammen, kør Cycle, deep sleep
test/test_core/*.cpp                # native unit-tests (Unity)
tools/mockserver.py                 # test-server: ack, config-push, OTA
README.md
```

Nøgle-datatyper (bruges konsistent i alle tasks):

```cpp
// Record.h — 40 bytes serialiseret
struct Record {
  uint8_t  version;      // =1
  uint8_t  flags;        // bit0: tsSynced
  uint16_t era;          // cold-boot-æra
  uint32_t seq;
  int64_t  ts;           // epoch-s hvis synced, ellers relativ uptime-s
  int16_t  tempCx100;    int16_t rhX100;
  uint16_t pm1_0x10, pm2_5x10, pm4_0x10, pm10x10;
  uint32_t luxX100;
  int16_t  axMg, ayMg, azMg;
  // + uint16_t crc16 (CCITT) ved serialisering
};
static const size_t RECORD_SIZE = 40;

// Config.h
struct DeviceConfig {
  uint32_t measureIntervalS = 300;
  uint32_t uploadIntervalS  = 300;
  char     ntpServer[64]    = "pool.ntp.org";
};

// Cycle-beslutninger (ren logik, testbar):
//   skalUploade(nowS, lastUploadS, cfg) ; sovetidS(cfg, cyklusVarighed)
```

## Tasks

### Task 1: Projekt-skelet
**Files:** Create `platformio.ini`, `include/board_pins.h`, `README.md`, `test/test_core/test_smoke.cpp`
- [ ] platformio.ini med `[env:esp32]` (espressif32, arduino, ArduinoJson, board_build.partitions med OTA + 3MB LittleFS) og `[env:native]` (test, ArduinoJson)
- [ ] board_pins.h: I2C SDA/SCL, BG96 UART TX/RX/PWRKEY, SPS30-forsyningsstyring, defaults for APN/SERVER_URL/API_TOKEN — alle som `#ifndef`-overstyrbare
- [ ] Smoke-test + `pio test -e native` grøn → commit

### Task 2: Record (TDD)
**Files:** Create `src/core/Record.h/.cpp`, `test/test_core/test_record.cpp`
- [ ] Tests: roundtrip serialize/deserialize, CRC-fejl afvises, forkert version afvises, størrelse == 40
- [ ] Implementér, kør, commit

### Task 3: MeasurementQueue (TDD)
**Files:** Create `src/core/IQueueStorage.h`, `src/core/MeasurementQueue.h/.cpp`, `test/test_core/test_queue.cpp` (med in-memory-storage-fake)
- [ ] Tests: push/peek-batch FIFO, ack(n) flytter markør, fuld-ack trunkerer, fuld kapacitet ⇒ `isFull()` og push afvises, ack-markør overlever "genstart" (ny instans på samme storage), korrupt record springes over uden at miste efterfølgende
- [ ] Implementér, kør, commit

### Task 4: Config (TDD)
**Files:** Create `src/core/IKvStore.h`, `src/core/Config.h/.cpp`, `test/test_core/test_config.cpp`
- [ ] Tests: defaults uden gemt state, load/save-roundtrip, validering (interval 60–86400 s clampes; tom ntpServer ignoreres)
- [ ] Implementér, kør, commit

### Task 5: TimeKeeper (TDD)
**Files:** Create `src/core/TimeKeeper.h/.cpp`, `test/test_core/test_time.cpp`
- [ ] Tests: usynced ts = uptime + flag; efter sync(epoch) er ts epoch + flag synced; backfill: record fra samme æra korrigeres med offset; record fra fremmed æra uden offset markeres uløst; æra-inkrement ved cold boot
- [ ] Implementér, kør, commit

### Task 6: Uplink payload/parse (TDD)
**Files:** Create `src/core/Uplink.h/.cpp`, `test/test_core/test_uplink.cpp`
- [ ] Tests: payload-JSON matcher spec §7 (token, deviceId, fwVersion, storageFull, measurements-array, skalering af x100/x10-felter, `tsValid:false` ved uløst tid); parse af svar: ackCount, delvis config, ota-blok, tomt/ugyldigt JSON ⇒ ack 0
- [ ] Implementér, kør, commit

### Task 7: Cycle state machine (TDD)
**Files:** Create `src/core/ISensors.h`, `src/core/INet.h`, `src/core/Cycle.h/.cpp`, `test/test_core/test_cycle.cpp` (fake sensors/net/clock)
- [ ] Tests: måling skrives til kø før upload-forsøg; upload kun når forfalden; upload i batches à 50 til kø tom; ack anvendes pr. batch; config fra svar gemmes; net-fejl ⇒ data forbliver i kø, cyklus fortsætter til sleep; fuld kø ⇒ ingen måling men upload forsøges stadig + storageFull i payload; sensor-fejl ⇒ NaN-felter men record skrives; NTP køres når tid usynced
- [ ] Implementér, kør, commit

### Task 8: Sensor-drivere (I2C)
**Files:** Create `src/hw/Shtc3.*`, `src/hw/Veml6040.*`, `src/hw/Lis2dh12.*`, `src/hw/Sps30.*`
- [ ] SHTC3: wake→measure(T først, clock-stretch disabled)→sleep, CRC8-check
- [ ] VEML6040: config 160 ms integration, læs G-kanal, lux = G × 0.25168
- [ ] LIS2DH12: CTRL1 10 Hz lavstrøm, læs OUT_X/Y/Z → mg
- [ ] SPS30: start measurement (float-format), poll data-ready, læs 10 floats, stop; CRC pr. bytepar
- [ ] `pio run -e esp32` kompilerer → commit (hardwaretest udestår, jf. spec §10)

### Task 9: BG96 AT-driver
**Files:** Create `src/hw/AtParser.h/.cpp`, `src/hw/Bg96.*`, `test/test_core/test_atparser.cpp`
- [ ] TDD på AtParser: find OK/ERROR/timeout, parse `+QNTP: 0,"yy/MM/dd,hh:mm:ss+tz"` → epoch, parse `+QHTTPPOST: 0,200,<len>`
- [ ] Bg96: PWRKEY-power-on-sekvens, init (ATE0, CPIN, CGATT-vent, QIACT), ntpTime(), httpPost() (QHTTPCFG/QHTTPURL/QHTTPPOST/QHTTPREAD), powerOff (QPOWD)
- [ ] `pio run -e esp32` kompilerer → commit

### Task 10: OTA
**Files:** Create `src/hw/OtaUpdater.*`
- [ ] QHTTPGET → QHTTPREAD i chunks → esp_ota_write + mbedtls SHA-256; mismatch ⇒ abort; match ⇒ set_boot_partition + restart; kompilér → commit

### Task 11: Adaptere + main
**Files:** Create `src/hw/LittleFsQueueStorage.*`, `src/hw/NvsKvStore.*`, `src/main.cpp`
- [ ] LittleFS-storage (append-fil + capacity), NVS-kvstore
- [ ] main.cpp: setup() wirer alt, kører Cycle::run(), watchdog (90 s worst case + SPS30-opvarmning), esp_deep_sleep med beregnet sovetid; loop() tom
- [ ] `pio run -e esp32` kompilerer → commit

### Task 12: Mockserver + docs
**Files:** Create `tools/mockserver.py`, Modify `README.md`
- [ ] Mockserver: modtag POST, print målinger, svar ackCount=len, config fra `mock_config.json` hvis findes, OTA-blok hvis `mock_ota.json` findes
- [ ] README: byg/flash/test-instruktioner, pin-udfyldning, API-kontrakt-henvisning → commit

### Task 13: Slutverifikation
- [ ] `pio test -e native` alle grønne; `pio run -e esp32` bygger uden warnings af betydning; spec-krav §3.1–3.7 kan hver peges til kode → commit

## Self-review
- Spec-dækning: §3.1→T2/T8, §3.2→T4/T7, §3.3→T6/T9, §3.4→T6/T7, §3.5→T3/T7/T11, §3.6→T5/T9, §3.7→T10 ✓
- `tsValid:false` (uløst æra-tid) er en bevidst kontrakt-tilføjelse ift. spec §7 — dokumenteres i README ✓
- Typer konsistente på tværs (Record/DeviceConfig defineret én gang øverst) ✓
