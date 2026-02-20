/*
 * UI + WiFi + audio. Importante: audio_mgr_init() debe ir ANTES de
 * gfx->begin(), así el canvas del display se reserva después y no se corrompe
 * (evita pantalla verde).
 */
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

#include "AXS15231B_touch.h"
#include "audio_mgr.h"
#include "dispcfg.h"
#include "display_access.h"
#include "pincfg.h"
#include "ui/ui.h"
#include "wifi_manager.h"

static Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    TFT_CS, TFT_SCK, TFT_SDA0, TFT_SDA1, TFT_SDA2, TFT_SDA3);

static Arduino_GFX *panel =
    new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, TFT_RES_W, TFT_RES_H);

static Arduino_Canvas *gfx =
    new Arduino_Canvas(TFT_RES_W, TFT_RES_H, panel, 0, 0, TFT_ROT);

static AXS15231B_Touch touch(Touch_SCL, Touch_SDA, Touch_INT, Touch_ADDR,
                             TFT_ROT);

static volatile bool s_display_dirty = false;

static uint32_t lvgl_tick_cb(void) { return millis(); }

static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area,
                          uint8_t *px_map) {
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map,
                          lv_area_get_width(area), lv_area_get_height(area));
  s_display_dirty = true;
  lv_disp_flush_ready(disp);
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  uint16_t x, y;
  if (touch.touched()) {
    touch.readData(&x, &y);
    data->point.x = (lv_coord_t)x;
    data->point.y = (lv_coord_t)y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void setup() {
#ifdef ARDUINO_USB_CDC_ON_BOOT
  delay(2000);
#endif
  Serial.begin(115200);
  Serial.println("CopilotLauncher");

  /* Audio primero: SD + I2S reservan heap; luego display usa lo que queda */
  audio_mgr_init();

  if (!gfx->begin(40000000UL)) {
    Serial.println("ERROR: display");
    while (1)
      ;
  }
  gfx->fillScreen(BLACK);
  gfx->flush();

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  if (!touch.begin()) {
    Serial.println("ERROR: touch");
    while (1)
      ;
  }
  touch.enOffsetCorrection(true);
  touch.setOffsets(Touch_X_MIN, Touch_X_MAX, TFT_RES_W - 1, Touch_Y_MIN,
                   Touch_Y_MAX, TFT_RES_H - 1);

  lv_init();
  lv_tick_set_cb(lvgl_tick_cb);

  /* Buffer LVGL: RAM interna (audio ya se inició antes, no pisa este buffer) */
  uint32_t buf_px = (uint32_t)gfx->width() * gfx->height() / 10;
  lv_color_t *buf = (lv_color_t *)heap_caps_malloc(
      buf_px * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!buf) {
    Serial.println("ERROR: buffer LVGL");
    while (1)
      ;
  }

  lv_display_t *disp = lv_display_create(gfx->width(), gfx->height());
  lv_display_set_flush_cb(disp, disp_flush_cb);
  lv_display_set_buffers(disp, buf, NULL, buf_px * sizeof(lv_color_t),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_display(indev, disp);
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_read_cb);
  lv_indev_set_scroll_limit(indev, 8);
  lv_indev_set_scroll_throw(indev, 100);

  wifi_mgr_init();

  ui_init();

  gfx->fillScreen(0x0000);
  gfx->flush();

  Serial.println("Listo.");
}

#define DISP_FLUSH_INTERVAL_MS 50

void loop() {
  wifi_mgr_update();
  audio_mgr_update();
  lv_task_handler();

  static uint32_t last_flush_ms = 0;
  uint32_t now = millis();
  if (s_display_dirty ||
      (now - last_flush_ms) >= (uint32_t)DISP_FLUSH_INTERVAL_MS) {
    s_display_dirty = false;
    last_flush_ms = now;
    gfx->flush();
  }
}

Arduino_Canvas *get_canvas() { return gfx; }
AXS15231B_Touch *get_touch() { return &touch; }
