#include "i2s_output.h"
#include <Arduino.h>

I2SOutput::I2SOutput(i2s_port_t port,
                     int bclk, int ws, int dout,
                     int dma_bufs, int dma_len)
    : _port(port), _bclk(bclk), _ws(ws), _dout(dout),
      _dma_bufs(dma_bufs), _dma_len(dma_len)
{
    hertz    = 44100;
    channels = 2;
    SetGain(1.0f);
}

I2SOutput::~I2SOutput() { uninstall(); }

void I2SOutput::install(int rate) {
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = (uint32_t)rate,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = _dma_bufs,
        .dma_buf_len          = _dma_len,
        .use_apll             = false,
        .tx_desc_auto_clear   = true,
        .fixed_mclk           = 0,
    };
    i2s_driver_install(_port, &cfg, 0, nullptr);

    i2s_pin_config_t pins = {
        .mck_io_num   = I2S_PIN_NO_CHANGE,
        .bck_io_num   = _bclk,
        .ws_io_num    = _ws,
        .data_out_num = _dout,
        .data_in_num  = I2S_PIN_NO_CHANGE,
    };
    i2s_set_pin(_port, &pins);
    _running = true;
}

void I2SOutput::uninstall() {
    if (_running) {
        i2s_driver_uninstall(_port);
        _running = false;
    }
}

bool I2SOutput::begin() {
    if (!_running) install(hertz);
    return true;
}

bool I2SOutput::stop() {
    uninstall();
    return true;
}

bool I2SOutput::SetRate(int hz) {
    hertz = (uint16_t)hz;
    if (_running) {
        i2s_set_sample_rates(_port, (uint32_t)hz);
    }
    return true;
}

bool I2SOutput::ConsumeSample(int16_t s[2]) {
    MakeSampleStereo16(s);
    int16_t data[2] = { Amplify(s[LEFTCHANNEL]), Amplify(s[RIGHTCHANNEL]) };
    size_t  written = 0;
    i2s_write(_port, data, sizeof(data), &written, portMAX_DELAY);
    return written == sizeof(data);
}
