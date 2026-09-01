# Klimasensor-firmware

[![CI](https://github.com/SlambertDK/klimasensor-firmware/actions/workflows/ci.yml/badge.svg)](https://github.com/SlambertDK/klimasensor-firmware/actions/workflows/ci.yml)

Batteridrevet ESP32-firmware der måler **temperatur, luftfugtighed, partikler
(PM1.0/PM2.5/PM4.0/PM10), lys og acceleration** og sender data til en server
over mobilnettet (Quectel BG96) med server-konfigurerbart interval, offline-
buffering uden datatab, NTP-tid og OTA-opdatering.

Design: [docs/superpowers/specs/2026-09-01-klimasensor-firmware-design.md](docs/superpowers/specs/2026-09-01-klimasensor-firmware-design.md)

## Hardware

| Modul | Interface | Adresse |
|---|---|---|
| Sensirion SHTC3 (temp/fugt) | I2C | 0x70 |
| Vishay VEML6040 (lys) | I2C | 0x10 |
| ST LIS2DH12 (accelerometer) | I2C | 0x18/0x19 |
| Sensirion SPS30 (partikler) | I2C | 0x69 |
| Quectel BG96 (modem) | UART | — |

**Inden første flash:** udfyld pin-numrene i
[include/board_pins.h](include/board_pins.h) (I2C, BG96-UART/PWRKEY,
SPS30-forsyning) — de nuværende værdier er placeholders. Server-URL, APN og
API-token sættes samme sted eller via `-D`-flag i `platformio.ini`.

## Byg og flash

```bash
pio run -e esp32              # byg
pio run -e esp32 -t upload    # flash via kabel
pio device monitor            # seriel log (115200 baud)
```

Unit-tests (kører på udviklingsmaskinen, ingen hardware nødvendig):

```bash
pio test -e native
```

## Sådan virker den

Enheden ligger i deep sleep og vågner på timer. Hver opvågnen: læs sensorer
(SPS30 kræver ~30 s blæser-opvarmning) → skriv målingen til flash-køen → hvis
upload er forfalden: tænd BG96, NTP-sync ved behov, POST alle ukvitterede
målinger i batches à 50, anvend evt. ny konfiguration/OTA fra svaret → sov.

- **Datatab:** målinger slettes først ved server-kvittering (`ackCount`).
  Løber flashen fuld (måneders udfald), stoppes nye målinger — intet
  eksisterende slettes, og `storageFull: true` sendes med i næste upload.
- **Tid:** intet RTC-batteri. Målinger taget før første NTP-sync backfilles,
  når tiden kendes. Kan tiden ikke afgøres (strømsvigt før nogensinde sync),
  sendes målingen med `"tsValid": false` og relativt tidsstempel.
- **Konfiguration:** serverens svar kan indeholde
  `config.measureIntervalS` / `config.uploadIntervalS` (clampes til 60–86400 s)
  og `config.ntpServer`. Gemmes i NVS og gælder fra samme cyklus.
- **OTA:** svarets `ota`-blok (`version`, `url`, `sha256`) starter en
  HTTP-download til den inaktive app-partition. Aktiveres kun ved eksakt
  SHA-256-match (transporten er uden TLS). `sha256sum firmware.bin` på
  serversiden giver værdien.

API-kontrakten er beskrevet i spec'ens §7 og kan afprøves mod
[tools/mockserver.py](tools/mockserver.py):

```bash
python3 tools/mockserver.py 8080
```

## Flash-layout

[partitions.csv](partitions.csv): dual OTA-app (2 × 1472 KB) + LittleFS
(1088 KB ≈ 27.000 målinger ≈ 3 måneders buffer ved 5-min interval) på 4 MB
flash. Har boardet 8/16 MB, kan `littlefs`-partitionen forstørres tilsvarende
(flere måneders buffer).

## Struktur

- `src/core/` — hardware-uafhængig logik (kø, config, tid, payload,
  cyklus-state-machine). 35 native unit-tests i `test/test_core/`.
- `src/hw/` — drivere (SHTC3, VEML6040, LIS2DH12, SPS30, BG96, OTA) og
  flash-adaptere.
- `src/main.cpp` — wiring + deep-sleep-loop.

## Kendte begrænsninger (v1)

- SPS30 kun via I2C; UART-fortrådning giver bevidst build-fejl.
- HTTP uden TLS (accepteret i spec); OTA kompenserer med SHA-256.
- Hardware-in-the-loop-test udestår, til enheden er ved hånden — se
  testprotokollen i spec'ens §10.
