# Klimasensor-firmware — designspecifikation

**Dato:** 2026-09-01
**Status:** Godkendt

## 1. Formål

Firmware til en batteridrevet klimasensor, der måler temperatur, luftfugtighed,
partikler (PM1.0/PM2.5/PM4.0/PM10), lys og acceleration, og sender målingerne
til en server over mobilnettet med et server-konfigurerbart interval. Ingen
målinger må mistes, selv ved langvarige netværksudfald.

## 2. Hardware

| Modul | Funktion | Interface | Noter |
|---|---|---|---|
| ESP32 | Mikrocontroller | — | Arduino-framework via PlatformIO |
| Quectel BG96 | Modem (LTE Cat-M1 / NB-IoT / EGPRS) | UART (AT-kommandoer) | Indbygget HTTP-stak og NTP (`AT+QNTP`) |
| Sensirion SPS30 | Partikler PM1.0/2.5/4.0/10 | I2C 0x69 (default) eller UART (build-flag) | 5V forsyning, ~30 s opvarmning pr. måling, ugentlig auto-fanrens |
| Sensirion SHTC3 | Temperatur + luftfugtighed | I2C 0x70 | Lavstrøms, wake/sleep-kommandoer |
| Vishay VEML6040 | Lys (RGBW) | I2C 0x10 | Lux beregnes fra G-kanalen jf. Vishays applikationsnote |
| ST LIS2DH12 | 3-akset accelerometer | I2C | Rådata (x, y, z) medtages i hver måling; ingen alarmer i v1 |

Bemærk: Specifikationens oprindelige angivelse af LIS2DH12 som "lys sensor" er
afklaret — LIS2DH12 er et accelerometer; lysmåling varetages af VEML6040.

**Ukendt endnu:** pin-mapping og om SPS30 er fortrådet til I2C eller UART.
Hele pin-konfigurationen samles i én fil, `include/board_pins.h`, og
SPS30-interfacet vælges med et build-flag (`SPS30_USE_I2C`, default til).
Rettes ét sted, når hardwaren er ved hånden.

## 3. Funktionelle krav

1. **Måling:** Hver måling omfatter temperatur (°C), relativ fugt (%), PM1.0,
   PM2.5, PM4.0, PM10 (µg/m³), lux samt accelerometer x/y/z.
2. **Intervaller:** Måleinterval og upload-interval er to separate,
   server-konfigurerbare værdier. Default: **300 s** for begge.
3. **Transport:** HTTP POST (uden TLS) via BG96. API-token sendes i payload;
   foreløbig værdi `123456789`.
4. **Konfiguration:** Serverens svar på hver upload kan indeholde ny
   konfiguration, som persisteres i NVS og anvendes fra næste cyklus.
5. **Ingen datatab:** Målinger persisteres i flash **før** upload-forsøg og
   slettes først, når serveren har kvitteret. Ved fuldt lager (måneders udfald)
   **stoppes nye målinger** — eksisterende data røres aldrig.
6. **Tid:** Enheden har intet RTC-batteri. Tid hentes via BG96's NTP
   (`AT+QNTP`, konfigurerbar tidsserver, default `pool.ntp.org`). Målinger
   taget før første tidssync tidsstemples relativt og korrigeres bagudrettet,
   når kalendertid kendes.
7. **OTA:** Firmware kan opdateres over HTTP. Config-svaret kan indeholde
   `{version, url, sha256}`; ny firmware hentes, SHA-256 verificeres før
   aktivering (kompenserer for manglende TLS), og ESP32'ens dual-partition +
   rollback sikrer mod bricking.

## 4. Arkitektur: deep-sleep-cyklus

Enheden ligger i deep sleep og vågner på timer. Hver opvågnen kører en fast
state machine:

```
Vågn (timer)
  → Læs SHTC3 + VEML6040 + LIS2DH12 (hurtige)
  → Tænd SPS30 → ~30 s opvarmning → læs → sluk
  → Skriv måling til flash-kø
  → Upload forfalden?
      ja → Tænd BG96 → netværks-attach
            → NTP-sync hvis tid mangler/er gammel (+ backfill af tidsstempler)
            → HTTP POST alle ukvitterede målinger (batch)
            → Parse svar: ack → slet kvitterede; config → gem i NVS;
              ota → hent, verificér SHA-256, aktivér, genstart
            → Sluk BG96
  → Beregn næste opvågnen → deep sleep
```

Alt state, der skal overleve søvn, ligger i flash (målekø) og NVS (config,
tids-offset, upload-bogholderi). Valgt frem for always-on FreeRTOS (dræber
batteriet) og light-sleep-hybrid (kompleks strømprofil uden tilsvarende
gevinst).

## 5. Moduler

| Modul | Ansvar | Afhængigheder |
|---|---|---|
| `drivers/shtc3` | Wake, mål, sleep; CRC-check | I2C |
| `drivers/veml6040` | Konfigurér integrationstid, læs RGBW, beregn lux | I2C |
| `drivers/lis2dh12` | Init lavstrømstilstand, læs x/y/z | I2C |
| `drivers/sps30` | Start/stop måling, læs PM-værdier, CRC | I2C (default) / UART |
| `drivers/bg96` | AT-driver: power on/off, attach, QNTP, HTTP POST | UART |
| `storage` | Kvitteringsbaseret målekø i LittleFS; fuld-lager-stop | LittleFS |
| `config` | NVS-persisteret config, defaults, server-overstyring | NVS |
| `timekeeping` | NTP-sync via BG96, backfill af præ-sync-tidsstempler | bg96, NVS |
| `uplink` | Byg JSON-payload, POST, parse svar (ack/config/ota) | bg96, storage, config |
| `ota` | Hent firmware, SHA-256-verifikation, esp_ota-aktivering | bg96 |
| `cycle` | State machine der binder det hele sammen | alle |

Sensor- og modemdrivere ligger bag smalle interfaces, så `cycle`, `storage`,
`uplink` og `timekeeping` kan unit-testes native med mocks.

## 6. Datalagring

- **Format:** binære records med fast størrelse (~40 B: sekvensnr.,
  tidsstempel + sync-flag, målefelter) i en append-log i LittleFS.
- **Kvittering:** en persisteret læse-markør; records før markøren er
  kvitteret og genbruges. Markøren flyttes kun ved server-ack (`ackCount`).
- **Kapacitet:** ~3 MB partition ⇒ ca. 75.000 records ≈ 8+ måneder ved
  5-min målinger. Ved fuldt lager: nye målinger stoppes (kravet "må aldrig
  miste data"), og en statusmarkering sendes med, når forbindelsen genopstår.
- **Præ-sync tid:** records skrevet før første NTP-sync gemmes med
  millis-baseret relativ tid + flag; ved første sync beregnes offset, og
  tidsstempler korrigeres ved upload.

## 7. API-kontrakt (defineres af firmwaren, serversiden er under udvikling)

**Request:** `POST /api/v1/measurements` — `Content-Type: application/json`

```json
{
  "token": "123456789",
  "deviceId": "<ESP32 efuse-MAC, hex>",
  "fwVersion": "1.0.0",
  "storageFull": false,
  "measurements": [
    {
      "ts": 1756713600,
      "tempC": 21.4,
      "rh": 43.2,
      "pm1_0": 4.1,
      "pm2_5": 6.8,
      "pm4_0": 7.2,
      "pm10": 7.9,
      "lux": 312.5,
      "accel": { "x": 0.01, "y": -0.02, "z": 1.00 }
    }
  ]
}
```

**Response (200):**

```json
{
  "ackCount": 12,
  "config": {
    "measureIntervalS": 300,
    "uploadIntervalS": 300,
    "ntpServer": "pool.ntp.org"
  },
  "ota": {
    "version": "1.1.0",
    "url": "http://server/fw/klimasensor-1.1.0.bin",
    "sha256": "<hex>"
  }
}
```

`config` og `ota` er valgfrie. `ackCount` angiver hvor mange målinger (i
afsendt rækkefølge) serveren har modtaget. Store bagkataloger sendes i batches
(maks. ~50 målinger pr. request) i løkke, til køen er tom.

## 8. Fejlhåndtering

- **Watchdog** om hele cyklussen — hænger noget, genstartes enheden og
  fortsætter fra persisteret state; ingen data mistes (de er allerede i flash).
- **Modem-fejl:** retries med backoff inden for cyklussen; lykkes upload ikke,
  bliver data i køen og forsøges næste upload-cyklus.
- **Sensor-fejl:** en fejlende sensor markeres med null-felter i payload;
  resten af målingen gennemføres.
- **OTA-fejl:** SHA-256-mismatch eller afbrudt download ⇒ ny firmware
  aktiveres ikke; ESP32-rollback ved boot-fejl på ny partition.

## 9. Strømbudget (opmærksomhedspunkt)

Default 5/5 min betyder 288 modem-opvågninger og 288 × ~30 s blæserkørsel i
døgnet — det kræver et stort batteri. Arkitekturen er bygget, så intervallerne
kan skrues op fra serveren uden firmwareændring; det er den primære
levetidsknap.

## 10. Test

- **Native unit-tests** (PlatformIO `native`-miljø) af `storage`, `config`,
  `uplink`-payload/parse, `timekeeping`-backfill og `cycle` med mockede
  drivere.
- **Mockserver:** lille Python-script der modtager uploads, kvitterer, og kan
  udstede config-ændringer og OTA — hele flowet testes uden den rigtige server.
- **Hardware-in-the-loop:** manuel testprotokol til når enheden er ved hånden
  (pin-mapping i `board_pins.h` udfyldes først).

## 11. Uden for scope i v1

- TLS/HTTPS (token-i-payload er accepteret foreløbigt)
- Tamper-/bevægelsesalarmer fra LIS2DH12 (kun rådata i payload)
- Lokal konfiguration ud over compile-time defaults
- Strømoptimering ud over deep-sleep-arkitekturen (PSM/eDRX-finjustering)
