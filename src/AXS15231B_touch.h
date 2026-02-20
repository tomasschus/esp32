#pragma once

#include <Arduino.h>
#include <Wire.h>

/**
 * Driver I2C para el controlador táctil del JC3248W535EN.
 * Usa interrupción en flanco descendente para detectar toques.
 */
class AXS15231B_Touch {
public:
    AXS15231B_Touch(uint8_t scl, uint8_t sda, uint8_t int_pin,
                    uint8_t addr, uint8_t rotation)
        : scl(scl), sda(sda), int_pin(int_pin),
          addr(addr), rotation(rotation) {}

    bool begin();
    bool touched();
    void readData(uint16_t *x, uint16_t *y);
    void setRotation(uint8_t rot);
    void enOffsetCorrection(bool en);
    void setOffsets(uint16_t x_real_min, uint16_t x_real_max, uint16_t x_ideal_max,
                    uint16_t y_real_min, uint16_t y_real_max, uint16_t y_ideal_max);

    uint8_t scl, sda, int_pin, addr, rotation;

protected:
    volatile bool touch_int = false;

private:
    bool    en_offset_correction = false;
    uint16_t point_X = 0, point_Y = 0;
    uint16_t x_real_min, x_real_max, y_real_min, y_real_max;
    uint16_t x_ideal_max, y_ideal_max;

    bool update();
    void correctOffset(uint16_t *x, uint16_t *y);
    static void isrTouched();
    static AXS15231B_Touch *instance;
};

/* Macros para extraer campos del paquete I2C de 8 bytes */
#define AXS_GET_GESTURE_TYPE(buf)  (buf[0])
#define AXS_GET_FINGER_NUMBER(buf) (buf[1])
#define AXS_GET_EVENT(buf)         ((buf[2] >> 0) & 0x03)
#define AXS_GET_POINT_X(buf)       (((uint16_t)(buf[2] & 0x0F) << 8) + (uint16_t)buf[3])
#define AXS_GET_POINT_Y(buf)       (((uint16_t)(buf[4] & 0x0F) << 8) + (uint16_t)buf[5])

#ifndef ISR_PREFIX
  #if defined(ESP32)
    #define ISR_PREFIX IRAM_ATTR
  #else
    #define ISR_PREFIX
  #endif
#endif
