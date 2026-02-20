#include "AXS15231B_touch.h"

AXS15231B_Touch *AXS15231B_Touch::instance = nullptr;

bool AXS15231B_Touch::begin() {
    instance = this;
    attachInterrupt(digitalPinToInterrupt(int_pin), isrTouched, FALLING);
    return Wire.begin(sda, scl);
}

ISR_PREFIX
void AXS15231B_Touch::isrTouched() {
    if (instance) instance->touch_int = true;
}

void AXS15231B_Touch::setRotation(uint8_t rot) {
    rotation = rot;
}

bool AXS15231B_Touch::touched() {
    return update();
}

void AXS15231B_Touch::readData(uint16_t *x, uint16_t *y) {
    *x = point_X;
    *y = point_Y;
}

void AXS15231B_Touch::enOffsetCorrection(bool en) {
    en_offset_correction = en;
}

void AXS15231B_Touch::setOffsets(uint16_t x_real_min, uint16_t x_real_max,
                                  uint16_t x_ideal_max,
                                  uint16_t y_real_min, uint16_t y_real_max,
                                  uint16_t y_ideal_max) {
    this->x_real_min  = x_real_min;
    this->x_real_max  = x_real_max;
    this->y_real_min  = y_real_min;
    this->y_real_max  = y_real_max;
    this->x_ideal_max = x_ideal_max;
    this->y_ideal_max = y_ideal_max;
}

void AXS15231B_Touch::correctOffset(uint16_t *x, uint16_t *y) {
    *x = map(*x, x_real_min, x_real_max, 0, x_ideal_max);
    *y = map(*y, y_real_min, y_real_max, 0, y_ideal_max);
}

bool AXS15231B_Touch::update() {
    if (!touch_int) return false;
    touch_int = false;

    uint8_t buf[8] = {0};
    static const uint8_t cmd[8] = {0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08};

    Wire.beginTransmission(addr);
    Wire.write(cmd, sizeof(cmd));
    Wire.endTransmission();

    Wire.requestFrom(addr, (uint8_t)sizeof(buf));
    for (int i = 0; i < (int)sizeof(buf) && Wire.available(); i++) {
        buf[i] = Wire.read();
    }

    uint16_t raw_x = AXS_GET_POINT_X(buf);
    uint16_t raw_y = AXS_GET_POINT_Y(buf);

    if (point_X || point_Y) {
        raw_x = constrain(raw_x, x_real_min, x_real_max);
        raw_y = constrain(raw_y, y_real_min, y_real_max);
    }

    uint16_t x_max, y_max;
    if (en_offset_correction) {
        correctOffset(&raw_x, &raw_y);
        x_max = x_ideal_max;
        y_max = y_ideal_max;
    } else {
        x_max = x_real_max;
        y_max = y_real_max;
    }

    switch (rotation) {
        case 0: point_X = raw_x;          point_Y = raw_y;          break;
        case 1: point_X = raw_y;          point_Y = x_max - raw_x;  break;
        case 2: point_X = x_max - raw_x;  point_Y = y_max - raw_y;  break;
        case 3: point_X = y_max - raw_y;  point_Y = raw_x;           break;
        default: break;
    }

    return true;
}
