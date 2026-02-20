/*
 * AP "ESP32-NAV" + WebSocket :8080/ws.
 * Recibe tiles JPEG (480×320) de la app Android y los decodifica a buffer
 * RGB565.
 *
 * La librería ESPAsyncWebServer llama al handler múltiples veces por el mismo
 * mensaje WebSocket (una por cada chunk TCP). info->index indica el offset del
 * chunk dentro del frame; info->len es el tamaño total del frame. Copiamos
 * cada chunk en s_jpeg_buf + info->index y decodificamos cuando
 * info->index + len == info->len (frame completo).
 */
#include "maps_ws_server.h"
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <cstring>

#define MAPS_AP_SSID "ESP32-NAV"
#define MAPS_AP_PASS "esp32nav12"
#define MAPS_WS_PORT 8080
#define MAPS_JPEG_MAX (120 * 1024)

static AsyncWebServer *s_server = nullptr;
static AsyncWebSocket *s_ws = nullptr;
static uint16_t *s_map_buf = nullptr;
static maps_ws_on_frame_t s_on_frame = nullptr;
static uint8_t *s_jpeg_buf = nullptr;

/* Callback de TJpg_Decoder: escribe bloque (x,y,w,h) en s_map_buf (RGB565). */
static bool maps_jpeg_output(int16_t x, int16_t y, uint16_t w, uint16_t h,
                             uint16_t *bitmap) {
  if (!s_map_buf)
    return 0;
  if (y + h > MAPS_WS_MAP_H || x + w > MAPS_WS_MAP_W)
    return 1;
  for (uint16_t row = 0; row < h; row++) {
    memcpy(s_map_buf + (y + row) * MAPS_WS_MAP_W + x, bitmap + row * w,
           (size_t)w * 2);
  }
  return 1;
}

static void on_ws_event(AsyncWebSocket *ws, AsyncWebSocketClient *client,
                        AwsEventType type, void *arg, uint8_t *data,
                        size_t len) {
  (void)ws;
  (void)client;

  if (type == WS_EVT_CONNECT) {
    Serial.println("[Maps] cliente conectado");
    return;
  }
  if (type == WS_EVT_DISCONNECT) {
    Serial.println("[Maps] cliente desconectado");
    return;
  }
  if (type != WS_EVT_DATA)
    return;

  AwsFrameInfo *info = (AwsFrameInfo *)arg;

  /* Solo mensajes binarios. */
  if (info->message_opcode != WS_BINARY)
    return;
  if (!s_map_buf || !s_on_frame || len == 0)
    return;

  /* Alocar buffer la primera vez: PSRAM primero, luego RAM interna. */
  if (!s_jpeg_buf) {
    s_jpeg_buf = (uint8_t *)heap_caps_malloc(
        MAPS_JPEG_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_jpeg_buf)
      s_jpeg_buf = (uint8_t *)heap_caps_malloc(
          MAPS_JPEG_MAX, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!s_jpeg_buf) {
      Serial.println("[Maps] ERROR: sin memoria para jpeg_buf");
      return;
    }
  }

  /* info->index = offset del chunk dentro del frame WebSocket.
   * Copiamos directamente en la posición correcta del buffer. */
  if (info->index == 0)
    Serial.printf("[Maps] tile iniciando, total esperado: %llu bytes\n",
                  info->len);

  if (info->index + len > MAPS_JPEG_MAX) {
    Serial.printf("[Maps] tile demasiado grande (%llu bytes)\n", info->len);
    return;
  }

  memcpy(s_jpeg_buf + info->index, data, len);

  /* El frame está completo cuando hemos recibido todos sus bytes. */
  if (info->index + len < info->len)
    return;

  /* Si el mensaje WebSocket tiene varios frames (WS fragmentation), esperar al
   * final. */
  if (!info->final)
    return;

  Serial.printf("[Maps] tile completo, %llu bytes\n", info->len);

  TJpgDec.setCallback(maps_jpeg_output);
  JRESULT r = TJpgDec.drawJpg(0, 0, s_jpeg_buf, (uint32_t)info->len);
  if (r == JDR_OK) {
    Serial.println("[Maps] tile decodificado OK");
    s_on_frame();
  } else {
    Serial.printf("[Maps] JPEG decode error %d\n", (int)r);
  }
}

bool maps_ws_start(uint16_t *map_buf, maps_ws_on_frame_t on_frame) {
  if (s_server)
    return true;
  if (!map_buf || !on_frame)
    return false;

  s_map_buf = map_buf;
  s_on_frame = on_frame;

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(MAPS_AP_SSID, MAPS_AP_PASS, 1, 0, 4)) {
    s_map_buf = nullptr;
    s_on_frame = nullptr;
    return false;
  }

  s_server = new AsyncWebServer(MAPS_WS_PORT);
  s_ws = new AsyncWebSocket("/ws");
  s_ws->onEvent(on_ws_event);
  s_server->addHandler(s_ws);
  s_server->begin();

  Serial.printf("[Maps] AP %s OK, WebSocket ws://192.168.4.1:%d/ws\n",
                MAPS_AP_SSID, MAPS_WS_PORT);
  return true;
}

void maps_ws_stop(void) {
  if (s_server) {
    s_server->end();
    delete s_server;
    s_server = nullptr;
  }
  if (s_ws) {
    delete s_ws;
    s_ws = nullptr;
  }
  if (s_jpeg_buf) {
    heap_caps_free(s_jpeg_buf);
    s_jpeg_buf = nullptr;
  }
  s_map_buf = nullptr;
  s_on_frame = nullptr;
  WiFi.softAPdisconnect(true);
}

bool maps_ws_is_running(void) { return s_server != nullptr; }
