/*
 * AP "ESP32-NAV" + WebSocket :8080/ws.
 *
 * Mensajes binarios  → tile JPEG (legacy) decodificado a buffer RGB565.
 * Mensajes de texto  → JSON con "t":"vec" (frame vectorial) o "t":"nav" (paso).
 *
 * Fragmentación: info->index indica el offset del chunk; se ensambla en
 * s_jpeg_buf (binario) o s_text_buf (texto) y se procesa cuando el frame
 * está completo (info->index + len == info->len && info->final).
 */
#include "maps_ws_server.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <cstring>

#define MAPS_AP_SSID   "ESP32-NAV"
#define MAPS_AP_PASS   "esp32nav12"
#define MAPS_WS_PORT   8080
#define MAPS_JPEG_MAX  (120 * 1024)
#define MAPS_TEXT_MAX  (14 * 1024)   /* 14 KB para el JSON vectorial */

static AsyncWebServer    *s_server   = nullptr;
static AsyncWebSocket    *s_ws       = nullptr;
static uint16_t          *s_map_buf  = nullptr;
static maps_ws_on_frame_t s_on_frame = nullptr;
static maps_ws_on_vec_t   s_on_vec   = nullptr;
static maps_ws_on_nav_t   s_on_nav   = nullptr;
static maps_ws_on_gps_t   s_on_gps   = nullptr;
static bool               s_has_client = false;
static uint8_t           *s_jpeg_buf = nullptr;
static char              *s_text_buf = nullptr;
static vec_frame_t       *s_vec_frame = nullptr;

/* ── JPEG output callback ────────────────────────────────────────── */
static bool maps_jpeg_output(int16_t x, int16_t y, uint16_t w, uint16_t h,
                             uint16_t *bitmap) {
  if (!s_map_buf) return 0;
  if (y + h > MAPS_WS_MAP_H || x + w > MAPS_WS_MAP_W) return 1;
  for (uint16_t row = 0; row < h; row++) {
    memcpy(s_map_buf + (y + row) * MAPS_WS_MAP_W + x,
           bitmap + row * w, (size_t)w * 2);
  }
  return 1;
}

/* ── Parser de velocidad GPS ─────────────────────────────────────── */
static void parse_gps_spd(const char *json, size_t len) {
  if (!s_on_gps) return;
  JsonDocument doc;
  if (deserializeJson(doc, json, len) != DeserializationError::Ok) return;
  s_on_gps(doc["spd"] | 0);
}

/* ── Parser de frame vectorial ───────────────────────────────────── */
static void parse_vec_frame(const char *json, size_t len) {
  if (!s_on_vec) return;

  JsonDocument doc;
  if (deserializeJson(doc, json, len) != DeserializationError::Ok) {
    Serial.println("[Maps] vec: error deserializeJson");
    return;
  }

  if (!s_vec_frame) return;
  vec_frame_t &frame = *s_vec_frame;
  memset(&frame, 0, sizeof(frame));

  /* Calles */
  JsonArray roads = doc["roads"];
  for (JsonObject road : roads) {
    if (frame.n_roads >= VEC_MAX_ROAD_SEGS) break;
    vec_road_t &r = frame.roads[frame.n_roads];
    r.w = road["w"] | 1;
    for (JsonArray pt : road["p"].as<JsonArray>()) {
      if (r.n >= VEC_MAX_PTS_PER_SEG) break;
      r.pts[r.n] = { (int16_t)pt[0].as<int>(), (int16_t)pt[1].as<int>() };
      r.n++;
    }
    if (r.n > 0) frame.n_roads++;
  }

  /* Ruta */
  for (JsonArray pt : doc["route"].as<JsonArray>()) {
    if (frame.n_route >= VEC_MAX_ROUTE_PTS) break;
    frame.route[frame.n_route] = { (int16_t)pt[0].as<int>(), (int16_t)pt[1].as<int>() };
    frame.n_route++;
  }

  /* Nombres de calles */
  for (JsonObject lbl : doc["labels"].as<JsonArray>()) {
    if (frame.n_labels >= VEC_MAX_LABELS) break;
    JsonArray p = lbl["p"];
    if (p.size() < 2) continue;
    vec_label_t &l = frame.labels[frame.n_labels];
    l.x = p[0].as<int>();
    l.y = p[1].as<int>();
    strlcpy(l.name, lbl["n"] | "", sizeof(l.name));
    frame.n_labels++;
  }

  /* Posición */
  JsonArray pos = doc["pos"];
  if (pos.size() >= 2) {
    frame.pos_x = pos[0].as<int>();
    frame.pos_y = pos[1].as<int>();
  }
  frame.heading = doc["hdg"] | -1;

  Serial.printf("[Maps] vec: roads=%u route=%u labels=%u pos=(%d,%d)\n",
                frame.n_roads, frame.n_route, frame.n_labels, frame.pos_x, frame.pos_y);
  s_on_vec(frame);
}

/* ── Parser de paso de navegación ────────────────────────────────── */
static void parse_nav_step(const char *json, size_t len) {
  if (!s_on_nav) return;

  JsonDocument doc;
  if (deserializeJson(doc, json, len) != DeserializationError::Ok) return;

  static nav_step_t step;
  strlcpy(step.step, doc["step"] | "", sizeof(step.step));
  strlcpy(step.dist, doc["dist"] | "", sizeof(step.dist));
  strlcpy(step.eta,  doc["eta"]  | "", sizeof(step.eta));

  Serial.printf("[Maps] nav: %s  %s  ETA %s\n", step.step, step.dist, step.eta);
  s_on_nav(step);
}

/* ── WebSocket event handler ─────────────────────────────────────── */
static void on_ws_event(AsyncWebSocket *ws, AsyncWebSocketClient *client,
                        AwsEventType type, void *arg, uint8_t *data,
                        size_t len) {
  (void)ws; (void)client;

  if (type == WS_EVT_CONNECT) {
    Serial.println("[Maps] cliente conectado");
    s_has_client = true;
    return;
  }
  if (type == WS_EVT_DISCONNECT) {
    Serial.println("[Maps] cliente desconectado");
    s_has_client = false;
    return;
  }
  if (type != WS_EVT_DATA || len == 0) return;

  AwsFrameInfo *info = (AwsFrameInfo *)arg;

  /* ── Mensajes binarios (JPEG legacy) ──────────────────────────── */
  if (info->message_opcode == WS_BINARY) {
    if (!s_map_buf || !s_on_frame) return;

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

    if (info->index == 0)
      Serial.printf("[Maps] JPEG iniciando, total: %llu bytes\n", info->len);

    if (info->index + len > MAPS_JPEG_MAX) {
      Serial.printf("[Maps] JPEG demasiado grande (%llu bytes)\n", info->len);
      return;
    }

    memcpy(s_jpeg_buf + info->index, data, len);
    if (info->index + len < info->len || !info->final) return;

    Serial.printf("[Maps] JPEG completo, %llu bytes\n", info->len);
    TJpgDec.setCallback(maps_jpeg_output);
    JRESULT r = TJpgDec.drawJpg(0, 0, s_jpeg_buf, (uint32_t)info->len);
    if (r == JDR_OK) {
      Serial.println("[Maps] JPEG decodificado OK");
      s_on_frame();
    } else {
      Serial.printf("[Maps] JPEG decode error %d\n", (int)r);
    }
    return;
  }

  /* ── Mensajes de texto (JSON vectorial / nav) ─────────────────── */
  if (info->message_opcode == WS_TEXT) {
    if (!s_text_buf) {
      s_text_buf = (char *)heap_caps_malloc(
          MAPS_TEXT_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (!s_text_buf)
        s_text_buf = (char *)heap_caps_malloc(
            MAPS_TEXT_MAX, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      if (!s_text_buf) {
        Serial.println("[Maps] ERROR: sin memoria para text_buf");
        return;
      }
    }

    if (info->index + len >= MAPS_TEXT_MAX) return;
    memcpy(s_text_buf + info->index, data, len);
    if (info->index + len < info->len || !info->final) return;

    /* Null-terminate y parsear */
    size_t total = (size_t)info->len;
    s_text_buf[total] = '\0';

    /* Leer el tipo del mensaje con mínima asignación */
    const char *t_start = strstr(s_text_buf, "\"t\":\"");
    if (!t_start) return;
    t_start += 5;

    if (strncmp(t_start, "vec", 3) == 0)
      parse_vec_frame(s_text_buf, total);
    else if (strncmp(t_start, "nav", 3) == 0)
      parse_nav_step(s_text_buf, total);
    else if (strncmp(t_start, "gps", 3) == 0)
      parse_gps_spd(s_text_buf, total);
  }
}

/* ── maps_ws_start ───────────────────────────────────────────────── */
bool maps_ws_start(uint16_t *map_buf, maps_ws_on_frame_t on_frame,
                   maps_ws_on_vec_t on_vec, maps_ws_on_nav_t on_nav) {
  if (s_server) return true;
  if (!map_buf || !on_frame) return false;

  if (!s_vec_frame) {
    s_vec_frame = (vec_frame_t *)heap_caps_malloc(
        sizeof(vec_frame_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_vec_frame) {
      Serial.println("[Maps] ERROR: sin memoria para vec_frame");
      return false;
    }
  }

  s_map_buf  = map_buf;
  s_on_frame = on_frame;
  s_on_vec   = on_vec;
  s_on_nav   = on_nav;

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(MAPS_AP_SSID, MAPS_AP_PASS, 1, 0, 4)) {
    s_map_buf = nullptr; s_on_frame = nullptr;
    return false;
  }

  s_server = new AsyncWebServer(MAPS_WS_PORT);
  s_ws     = new AsyncWebSocket("/ws");
  s_ws->onEvent(on_ws_event);
  s_server->addHandler(s_ws);
  s_server->begin();

  Serial.printf("[Maps] AP %s OK, ws://192.168.4.1:%d/ws\n",
                MAPS_AP_SSID, MAPS_WS_PORT);
  return true;
}

/* ── maps_ws_set_gps_cb ──────────────────────────────────────────── */
void maps_ws_set_gps_cb(maps_ws_on_gps_t cb) { s_on_gps = cb; }

/* ── maps_ws_stop ────────────────────────────────────────────────── */
void maps_ws_stop(void) {
  if (s_server) { s_server->end(); delete s_server; s_server = nullptr; }
  if (s_ws)     { delete s_ws;     s_ws     = nullptr; }
  if (s_jpeg_buf)  { heap_caps_free(s_jpeg_buf);  s_jpeg_buf  = nullptr; }
  if (s_text_buf)  { heap_caps_free(s_text_buf);  s_text_buf  = nullptr; }
  if (s_vec_frame) { heap_caps_free(s_vec_frame); s_vec_frame = nullptr; }
  s_map_buf  = nullptr;
  s_on_frame = nullptr;
  s_on_vec   = nullptr;
  s_on_nav   = nullptr;
  s_on_gps     = nullptr;
  s_has_client = false;
  WiFi.softAPdisconnect(true);
}

bool maps_ws_is_running(void) { return s_server != nullptr; }
bool maps_ws_has_client(void) { return s_has_client; }
