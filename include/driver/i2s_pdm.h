/**
 * Stub header: driver/i2s_pdm.h (IDF 5.x API)
 *
 * Exists only to allow AudioOutputPDM.cpp to compile against IDF 4.4.x.
 * PDM output is never used; these symbols are never called.
 */
#pragma once

#include "i2s_std.h"   /* re-use handle + esp_err_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Minimal PDM TX clock / slot / gpio stubs */
typedef struct {
    uint32_t        sample_rate_hz;
    i2s_clock_src_t clk_src;
    uint32_t        mclk_multiple;
    uint32_t        up_sample_fp;
    uint32_t        up_sample_fs;
} i2s_pdm_tx_clk_config_t;

typedef enum {
    I2S_PDM_DATA_FMT_PCM = 0,
    I2S_PDM_DATA_FMT_COUNT,
} i2s_pdm_dsr_sample_rate_t;
/* Alias used by AudioOutputPDM.cpp */
#define I2S_PDM_DATA_FMT_PCM I2S_PDM_DATA_FMT_PCM

typedef struct {
    i2s_data_bit_width_t        data_bit_width;
    i2s_slot_mode_t             slot_mode;
    uint32_t                    sd_prescale;
    uint32_t                    sd_scale;
    uint32_t                    hp_scale;
    uint32_t                    lp_scale;
    uint32_t                    sinc_scale;
    i2s_pdm_dsr_sample_rate_t   data_fmt;
} i2s_pdm_tx_slot_config_t;

typedef struct {
    gpio_num_t clk;
    gpio_num_t dout;
    struct { bool clk_inv; } invert_flags;
} i2s_pdm_tx_gpio_config_t;

typedef struct {
    i2s_pdm_tx_clk_config_t  clk_cfg;
    i2s_pdm_tx_slot_config_t slot_cfg;
    i2s_pdm_tx_gpio_config_t gpio_cfg;
} i2s_pdm_tx_config_t;

#define I2S_PDM_TX_CLK_DEFAULT_CONFIG(rate) \
    { (rate), I2S_CLK_SRC_DEFAULT, 256, 960, (rate) }

#define I2S_PDM_TX_SLOT_DEFAULT_CONFIG(bits, mode) \
    { (bits), (mode), 0, 0, 0, 0, 0 }

static inline esp_err_t i2s_channel_init_pdm_tx_mode(i2s_chan_handle_t, const i2s_pdm_tx_config_t *)
    { return ESP_OK; }
static inline esp_err_t i2s_channel_reconfig_pdm_tx_clock(i2s_chan_handle_t, const i2s_pdm_tx_clk_config_t *)
    { return ESP_OK; }

/* SOC_I2S_SUPPORTS_PDM_TX guard — defined so AudioOutputPDM.h compiles */
#ifndef SOC_I2S_SUPPORTS_PDM_TX
#define SOC_I2S_SUPPORTS_PDM_TX 1
#endif

#ifdef __cplusplus
}
#endif
