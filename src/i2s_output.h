#pragma once
/*
 * Minimal I2S audio output for ESP32-S3 using the classic driver/i2s.h API
 * (ESP-IDF 4.x / Arduino ESP32 2.x).
 * Extends ESP8266Audio's AudioOutput base class so it works as a drop-in
 * replacement for AudioOutputI2S.
 */
#include <AudioOutput.h>
#include <driver/i2s.h>

class I2SOutput : public AudioOutput {
public:
    I2SOutput(i2s_port_t port,
              int bclk, int ws, int dout,
              int dma_bufs = 8, int dma_len = 128);
    ~I2SOutput();

    bool begin()                        override;
    bool ConsumeSample(int16_t s[2])    override;
    bool SetRate(int hz)                override;
    bool stop()                         override;

private:
    void install(int rate);
    void uninstall();

    i2s_port_t _port;
    int        _bclk, _ws, _dout;
    int        _dma_bufs, _dma_len;
    bool       _running = false;
};
