#include "wifi_manager.h"

#include <WiFi.h>
#include <Arduino.h>
#include <cstring>

#define CONNECT_TIMEOUT_MS 10000
#define MAX_NETWORKS       20
#define SSID_MAX_LEN       33

static wifi_mgr_state_t s_state                         = WIFI_MGR_IDLE;
static int              s_net_count                     = 0;
static uint32_t         s_connect_ts                    = 0;
static char             s_ssids[MAX_NETWORKS][SSID_MAX_LEN];
static int              s_rssis[MAX_NETWORKS];
static int              s_channels[MAX_NETWORKS];
static int              s_encryptions[MAX_NETWORKS];

void wifi_mgr_init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    s_state = WIFI_MGR_IDLE;
}

void wifi_mgr_update() {
    switch (s_state) {
        case WIFI_MGR_SCANNING: {
            int16_t n = WiFi.scanComplete();
            if (n >= 0) {
                s_net_count = (n < MAX_NETWORKS) ? (int)n : MAX_NETWORKS;
                for (int i = 0; i < s_net_count; i++) {
                    strncpy(s_ssids[i], WiFi.SSID(i).c_str(), SSID_MAX_LEN - 1);
                    s_ssids[i][SSID_MAX_LEN - 1] = '\0';
                    s_rssis[i]       = WiFi.RSSI(i);
                    s_channels[i]    = WiFi.channel(i);
                    s_encryptions[i] = (int)WiFi.encryptionType(i);
                }
                WiFi.scanDelete();
                s_state = WIFI_MGR_SCAN_DONE;
            }
            break;
        }
        case WIFI_MGR_CONNECTING: {
            wl_status_t ws = WiFi.status();
            if (ws == WL_CONNECTED) {
                s_state = WIFI_MGR_CONNECTED;
            } else if (ws == WL_CONNECT_FAILED || ws == WL_NO_SSID_AVAIL ||
                       (millis() - s_connect_ts) > CONNECT_TIMEOUT_MS) {
                WiFi.disconnect(true);
                s_state = WIFI_MGR_FAILED;
            }
            break;
        }
        default:
            break;
    }
}

void wifi_mgr_start_scan() {
    WiFi.disconnect(true);
    WiFi.scanNetworks(/*async=*/true);
    s_net_count = 0;
    s_state = WIFI_MGR_SCANNING;
}

wifi_mgr_state_t wifi_mgr_get_state() {
    return s_state;
}

int wifi_mgr_get_network_count() {
    return s_net_count;
}

const char* wifi_mgr_get_ssid(int i) {
    if (i < 0 || i >= s_net_count) return "";
    return s_ssids[i];
}

int wifi_mgr_get_rssi(int i) {
    if (i < 0 || i >= s_net_count) return -100;
    return s_rssis[i];
}

void wifi_mgr_connect(const char* ssid, const char* pass) {
    WiFi.begin(ssid, pass);
    s_connect_ts = millis();
    s_state = WIFI_MGR_CONNECTING;
}

const char* wifi_mgr_get_ip() {
    static char buf[20];
    WiFi.localIP().toString().toCharArray(buf, sizeof(buf));
    return buf;
}

void wifi_mgr_disconnect() {
    WiFi.disconnect(true);
    s_state = WIFI_MGR_IDLE;
}

int wifi_mgr_get_channel(int i) {
    if (i < 0 || i >= s_net_count) return 0;
    return s_channels[i];
}

const char* wifi_mgr_get_encryption(int i) {
    if (i < 0 || i >= s_net_count) return "";
    switch (s_encryptions[i]) {
        case 0:  return "Abierta";
        case 1:  return "WEP";
        case 2:  return "WPA";
        case 3:  return "WPA2";
        case 4:  return "WPA/2";
        case 5:  return "WPA2-E";
        case 6:  return "WPA3";
        default: return "?";
    }
}
