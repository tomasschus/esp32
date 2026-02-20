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

/* true mientras la pantalla de mapas está activa en portrait */
static volatile bool s_portrait = false;

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

    if (s_portrait) {
      /* El touch driver con TFT_ROT=1 entrega coords landscape:
       *   lx = raw_y  (portrait_y del panel nativo)
       *   ly = 319 - raw_x  (portrait_x del panel nativo, invertido)
       * En portrait LVGL espera (x∈[0,319], y∈[0,479]) = coords nativas.
       * Para recuperarlas:
       *   portrait_x = raw_x = (DISP_VER_RES-1) - ly
       *   portrait_y = raw_y = lx                              */
      uint16_t lx = x, ly = y;
      x = (DISP_VER_RES - 1) - ly;
      y = lx;
    }

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

static disp_rot_t s_current_rot = DISP_ROT_LANDSCAPE;

/*
 * Cambia la orientación del display en runtime usando rotación hardware del
 * canvas GFX (gfx->setRotation) en lugar de rotación software de LVGL.
 *
 * Ventaja: las dimensiones del canvas cambian realmente → fillScreen cubre
 * TODOS los píxeles físicos → sin rastro del contenido anterior.
 * LVGL recibe las nuevas dimensiones vía lv_display_set_resolution y marca
 * todo el display como dirty, garantizando un redibujado completo.
 *
 * Llamar ANTES de lv_screen_load() para que la resolución LVGL esté
 * correcta cuando se renderice la nueva pantalla.
 */
void display_set_rotation(disp_rot_t rot) {
  if (rot == s_current_rot)
    return;
  s_current_rot = rot;

  /* 1. Limpiar canvas físico completo (dimensiones actuales aún válidas) */
  gfx->fillScreen(0x0000);
  gfx->flush();

  lv_display_t *disp = lv_display_get_default();

  if (rot == DISP_ROT_PORTRAIT) {
    /* Canvas GFX → portrait: rotation=0 → width=320, height=480 */
    gfx->setRotation(0);
    /* LVGL lógico 320×480 (marca todo el display dirty) */
    lv_display_set_resolution(disp, 320, 480);
    s_portrait = true;
    Serial.println("[Disp] → portrait 320×480");
  } else {
    /* Canvas GFX → landscape: rotation=1 → width=480, height=320 */
    gfx->setRotation(1);
    /* LVGL lógico 480×320 */
    lv_display_set_resolution(disp, 480, 320);
    s_portrait = false;
    Serial.println("[Disp] → landscape 480×320");
  }
}

disp_rot_t display_get_rotation(void) { return s_current_rot; }
