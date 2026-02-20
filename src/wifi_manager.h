#pragma once

typedef enum {
    WIFI_MGR_IDLE,
    WIFI_MGR_SCANNING,
    WIFI_MGR_SCAN_DONE,
    WIFI_MGR_CONNECTING,
    WIFI_MGR_CONNECTED,
    WIFI_MGR_FAILED
} wifi_mgr_state_t;

/** Inicializar: poner WiFi en modo estación. Llamar desde setup(). */
void wifi_mgr_init();

/** Procesar la máquina de estados. Llamar en cada iteración de loop(). */
void wifi_mgr_update();

/** Disparar un escaneo asincrónico. */
void wifi_mgr_start_scan();

/** Estado actual. */
wifi_mgr_state_t wifi_mgr_get_state();

/** Cantidad de redes encontradas (válido en SCAN_DONE en adelante). */
int wifi_mgr_get_network_count();

/** SSID de la red i-ésima. */
const char* wifi_mgr_get_ssid(int i);

/** RSSI (dBm) de la red i-ésima. */
int wifi_mgr_get_rssi(int i);

/** Iniciar conexión con el SSID y contraseña indicados. */
void wifi_mgr_connect(const char* ssid, const char* pass);

/** Dirección IP asignada (válida en CONNECTED). */
const char* wifi_mgr_get_ip();

/** Desconectar y volver a IDLE. */
void wifi_mgr_disconnect();

/** Canal (1-14) de la red i-ésima. */
int wifi_mgr_get_channel(int i);

/** Tipo de encriptación como string ("WPA2", "Abierta", etc.). */
const char* wifi_mgr_get_encryption(int i);
