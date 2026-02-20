/**
 * Stub header: driver/i2s_std.h (IDF 5.x API)
 *
 * This file exists solely to allow ESP8266Audio library files (AudioOutputI2S.cpp,
 * AudioOutputI2SNoDAC.cpp) to compile against IDF 4.4.x. We use our own I2SOutput
 * class (driver/i2s.h) for actual audio output, so these symbols are never called.
 */
#pragma once

#include <esp_err.h>
#include <hal/gpio_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Handle ──────────────────────────────────────────────────────────────── */
typedef void *i2s_chan_handle_t;

/* ── Clock source ─────────────────────────────────────────────────────────── */
typedef enum {
    I2S_CLK_SRC_DEFAULT = 0,
    I2S_CLK_SRC_APLL    = 1,
} i2s_clock_src_t;

/* ── Role / slot / data-bit types (minimal stubs) ─────────────────────────── */
typedef enum { I2S_ROLE_MASTER = 0 } i2s_role_t;
typedef enum { I2S_SLOT_MODE_STEREO = 2 } i2s_slot_mode_t;
typedef enum {
    I2S_DATA_BIT_WIDTH_8BIT  =  8,
    I2S_DATA_BIT_WIDTH_16BIT = 16,
    I2S_DATA_BIT_WIDTH_24BIT = 24,
    I2S_DATA_BIT_WIDTH_32BIT = 32,
} i2s_data_bit_width_t;
typedef int i2s_port_t;
#define I2S_NUM_AUTO  (-1)
#define I2S_GPIO_UNUSED GPIO_NUM_NC

/* ── Channel config ───────────────────────────────────────────────────────── */
typedef struct {
    i2s_port_t id;
    i2s_role_t role;
    uint32_t   dma_desc_num;
    uint32_t   dma_frame_num;
    bool       auto_clear;
} i2s_chan_config_t;

#define I2S_CHANNEL_DEFAULT_CONFIG(port, role) \
    { (port), (role), 6, 240, false }

/* ── STD clock config ─────────────────────────────────────────────────────── */
typedef struct {
    uint32_t         sample_rate_hz;
    i2s_clock_src_t  clk_src;
    uint32_t         mclk_multiple;
} i2s_std_clk_config_t;

#define I2S_STD_CLK_DEFAULT_CONFIG(rate) \
    { (rate), I2S_CLK_SRC_DEFAULT, 256 }

/* ── STD slot config ──────────────────────────────────────────────────────── */
typedef struct {
    i2s_data_bit_width_t data_bit_width;
    i2s_slot_mode_t      slot_mode;
    bool                 bit_shift;
    uint32_t             slot_mask;
    uint32_t             ws_width;
    bool                 ws_pol;
    bool                 msb_right;
} i2s_std_slot_config_t;

#define I2S_STD_MSB_SLOT_DEFAULT_CONFIG(bits, mode) \
    { (bits), (mode), true, 3, 0, false, false }

/* ── GPIO config ──────────────────────────────────────────────────────────── */
typedef struct {
    bool mclk_inv;
    bool bclk_inv;
    bool ws_inv;
} i2s_std_gpio_invert_t;

typedef struct {
    gpio_num_t            mclk;
    gpio_num_t            bclk;
    gpio_num_t            ws;
    gpio_num_t            dout;
    gpio_num_t            din;
    i2s_std_gpio_invert_t invert_flags;
} i2s_std_gpio_config_t;

/* ── Full STD config ──────────────────────────────────────────────────────── */
typedef struct {
    i2s_std_clk_config_t  clk_cfg;
    i2s_std_slot_config_t slot_cfg;
    i2s_std_gpio_config_t gpio_cfg;
} i2s_std_config_t;

/* ── Stub functions (never called — we use I2SOutput / driver/i2s.h) ──────── */
static inline esp_err_t i2s_new_channel(const i2s_chan_config_t *, i2s_chan_handle_t *tx, i2s_chan_handle_t *rx)
    { (void)tx; (void)rx; return ESP_OK; }
static inline esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t, const i2s_std_config_t *)
    { return ESP_OK; }
static inline esp_err_t i2s_channel_enable(i2s_chan_handle_t)
    { return ESP_OK; }
static inline esp_err_t i2s_channel_disable(i2s_chan_handle_t)
    { return ESP_OK; }
static inline esp_err_t i2s_del_channel(i2s_chan_handle_t)
    { return ESP_OK; }
static inline esp_err_t i2s_channel_reconfig_std_clock(i2s_chan_handle_t, const i2s_std_clk_config_t *)
    { return ESP_OK; }
static inline esp_err_t i2s_channel_write(i2s_chan_handle_t, const void *, size_t, size_t *bytes_written, uint32_t)
    { if (bytes_written) *bytes_written = 0; return ESP_OK; }
static inline esp_err_t i2s_channel_preload_data(i2s_chan_handle_t, void *, size_t, size_t *bytes_loaded)
    { if (bytes_loaded) *bytes_loaded = 0; return ESP_OK; }

#ifdef __cplusplus
}
#endif
