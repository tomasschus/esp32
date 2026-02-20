#pragma once

/* ── Pantalla QSPI (AXS15231B) ─────────────────────────────── */
#define TFT_BL   1    // Backlight (PWM)
#define TFT_CS   45   // Chip Select
#define TFT_SCK  47   // Clock
#define TFT_SDA0 21   // Data 0
#define TFT_SDA1 48   // Data 1
#define TFT_SDA2 40   // Data 2
#define TFT_SDA3 39   // Data 3

/* ── Touch I2C (AXS15231B touch) ───────────────────────────── */
#define Touch_SDA  4    // I2C Data
#define Touch_SCL  8    // I2C Clock
#define Touch_INT  3    // Interrupción
#define Touch_ADDR 0x3B // Dirección I2C

/* ── SD Card SPI (SPI2 / HSPI) ─────────────────────────────── */
#define SD_CS    10   // Chip Select
#define SD_MOSI  11   // Master Out Slave In
#define SD_SCK   12   // Clock
#define SD_MISO  13   // Master In Slave Out

/* ── Audio I2S (NS4168 amplifier) ──────────────────────────── */
#define AUDIO_BCLK  42   // Bit Clock
#define AUDIO_WS     2   // Word Select / LR Clock
#define AUDIO_DOUT  41   // Serial Data Out
