// board_pins.h — AL hardware-afhængig konfiguration samles her.
// Pin-numrene er PLACEHOLDERS: udfyld dem, når hardwaren/skemaet er ved hånden.
// Alt kan overstyres fra platformio.ini med -D<NAVN>=<værdi>.
//
// *** ADVARSEL (ESP32-PICO-D4 / pico32) ***
// GPIO 6, 7, 8, 11, 16 og 17 er forbundet til modulets INDBYGGEDE FLASH og må
// aldrig bruges som almindelige GPIO'er — det giver flash-korruption, crashes
// og mystiske partitionsfejl. Alle defaults herunder holder sig fra dem.
#pragma once

// ---------- I2C-bus (SHTC3 0x70, VEML6040 0x10, LIS2DH12, SPS30 0x69) ----------
#ifndef PIN_I2C_SDA
#define PIN_I2C_SDA 21
#endif
#ifndef PIN_I2C_SCL
#define PIN_I2C_SCL 22
#endif
#ifndef I2C_FREQ_HZ
#define I2C_FREQ_HZ 100000  // SPS30 kræver standard mode (100 kHz) + clock stretching
#endif

// LIS2DH12: SA0-pin bestemmer adressen (0x18 eller 0x19)
#ifndef LIS2DH12_I2C_ADDR
#define LIS2DH12_I2C_ADDR 0x19
#endif

// ---------- SPS30 ----------
// Default: I2C på bussen ovenfor. Er den fortrådet til UART: byg med -DSPS30_USE_UART
// og udfyld UART-pins (v1 implementerer kun I2C — UART-fortrådning kræver driver-udvidelse).
#ifndef PIN_SPS30_PWR
#define PIN_SPS30_PWR -1  // GPIO der tænder SPS30's 5V-forsyning (-1 = altid tændt)
#endif

// ---------- BG96-modem ----------
#ifndef PIN_BG96_TX
#define PIN_BG96_TX 25    // ESP32 TX -> BG96 RX (IKKE 16/17 på PICO-D4!)
#endif
#ifndef PIN_BG96_RX
#define PIN_BG96_RX 26    // ESP32 RX <- BG96 TX (IKKE 16/17 på PICO-D4!)
#endif
#ifndef PIN_BG96_PWRKEY
#define PIN_BG96_PWRKEY 4
#endif
#ifndef PIN_BG96_PWR
#define PIN_BG96_PWR -1   // GPIO der styrer modemets forsyning (-1 = altid tændt)
#endif
#ifndef BG96_BAUD
#define BG96_BAUD 115200
#endif

// ---------- Netværk / server ----------
#ifndef NET_APN
#define NET_APN ""        // tom = brug SIM-kortets default-APN
#endif
#ifndef SERVER_URL
#define SERVER_URL "http://example.energinet.dk/api/v1/measurements"
#endif
#ifndef API_TOKEN
#define API_TOKEN "123456789"  // foreløbig token, jf. spec
#endif

// ---------- Cyklus ----------
#ifndef CYCLE_WATCHDOG_S
#define CYCLE_WATCHDOG_S 240  // hård grænse pr. opvågnen (SPS30-opvarmning + netværk + OTA-check)
#endif
