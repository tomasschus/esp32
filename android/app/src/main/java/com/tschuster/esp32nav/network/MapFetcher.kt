package com.tschuster.esp32nav.network

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.ConnectivityManager
import android.net.Network
import android.util.Log
import java.io.ByteArrayOutputStream
import java.util.concurrent.TimeUnit
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.ln
import kotlin.math.tan
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request

private const val TAG = "ESP32Nav/Map"

/**
 * Descarga el tile de mapa a través de la red CELULAR (mientras el WiFi está conectado al ESP32).
 *
 * Orden: Mapbox (si hay token) → staticmap.openstreetmap.de → tile.openstreetmap.org (1 tile
 * 256×256 escalado a 480×320 en JPEG). Los dos últimos sin API key.
 *
 * ► Obtener token Mapbox gratis en: https://account.mapbox.com Copiarlo en MAPBOX_TOKEN.
 */
class MapFetcher {

    companion object {
        // ▼ Reemplazar con tu token de Mapbox
        const val MAPBOX_TOKEN = "YOUR_MAPBOX_TOKEN_HERE"
        const val DISPLAY_W = 480
        const val DISPLAY_H = 320
        const val ZOOM_NORMAL = 15
        const val ZOOM_NAV = 16
    }

    // Cliente por defecto: rutas por la red de internet activa (celular cuando WiFi es local-only)
    private val defaultClient =
            OkHttpClient.Builder()
                    .connectTimeout(10, TimeUnit.SECONDS)
                    .readTimeout(15, TimeUnit.SECONDS)
                    .build()

    // Cliente con binding explícito a la red celular
    private var cellClient: OkHttpClient? = null
    private var cellNetwork: Network? = null
    private var connectivityManager: ConnectivityManager? = null

    /** Red y context para bindProcessToNetwork durante el fetch (DNS + TCP por celular). */
    fun setCellularNetwork(network: Network, context: Context) {
        Log.i(
                TAG,
                "setCellularNetwork: network=$network handle=${network.getNetworkHandle()} (bind durante fetch para DNS por celular)"
        )
        cellNetwork = network
        connectivityManager =
                context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        cellClient =
                OkHttpClient.Builder()
                        .socketFactory(network.socketFactory)
                        .connectTimeout(10, TimeUnit.SECONDS)
                        .readTimeout(15, TimeUnit.SECONDS)
                        .build()
    }

    // ── Mapbox Static ─────────────────────────────────────────────
    suspend fun fetchMapbox(lat: Double, lon: Double, zoom: Int = ZOOM_NORMAL): ByteArray? =
            withContext(Dispatchers.IO) {
                val client = cellClient ?: defaultClient
                Log.d(
                        TAG,
                        "fetchMapbox: client=${if (cellClient != null) "cellClient" else "defaultClient"} lat=$lat lon=$lon"
                )
                val fLat = "%.5f".format(lat)
                val fLon = "%.5f".format(lon)
                val pin = "pin-s-car+E94560($fLon,$fLat)"
                val url =
                        "https://api.mapbox.com/styles/v1/mapbox/navigation-day-v1/static/" +
                                "$pin/$fLon,$fLat,$zoom/${DISPLAY_W}x${DISPLAY_H}" +
                                "?access_token=$MAPBOX_TOKEN"
                fetch(client, url)
            }

    // ── OpenStreetMap estático (puede fallar DNS en algunas redes) ─
    suspend fun fetchOsm(lat: Double, lon: Double, zoom: Int = ZOOM_NORMAL): ByteArray? =
            withContext(Dispatchers.IO) {
                val client = cellClient ?: defaultClient
                Log.d(
                        TAG,
                        "fetchOsm: client=${if (cellClient != null) "cellClient" else "defaultClient"} lat=$lat lon=$lon"
                )
                val url =
                        "https://staticmap.openstreetmap.de/staticmap.php" +
                                "?center=$lat,$lon&zoom=$zoom" +
                                "&size=${DISPLAY_W}x${DISPLAY_H}" +
                                "&markers=$lat,$lon,lightblue1"
                fetch(client, url)
            }

    /** Convierte lat/lon/zoom a número de tile OSM (Slippy map). */
    private fun latLonZoomToTile(lat: Double, lon: Double, zoom: Int): Pair<Int, Int> {
        val n = 1 shl zoom
        val x = (n * (lon + 180) / 360).toDouble().let { floor(it).toInt().coerceIn(0, n - 1) }
        val latRad = lat * Math.PI / 180.0
        val y =
                (n * (1 - ln(tan(latRad) + 1 / cos(latRad)) / Math.PI) / 2).toDouble().let {
                    floor(it).toInt().coerceIn(0, n - 1)
                }
        return x to y
    }

    /**
     * Un tile 256×256 de tile.openstreetmap.org; se escala a DISPLAY_W×DISPLAY_H y se devuelve como
     * JPEG.
     */
    suspend fun fetchOsmTile(lat: Double, lon: Double, zoom: Int = ZOOM_NORMAL): ByteArray? =
            withContext(Dispatchers.IO) {
                val client = cellClient ?: defaultClient
                val (tx, ty) = latLonZoomToTile(lat, lon, zoom)
                val url = "https://tile.openstreetmap.org/$zoom/$tx/$ty.png"
                Log.d(
                        TAG,
                        "fetchOsmTile: client=${if (cellClient != null) "cellClient" else "default"} tile=$zoom/$tx/$ty"
                )
                val pngBytes = fetch(client, url) ?: return@withContext null
                val bmp =
                        BitmapFactory.decodeByteArray(pngBytes, 0, pngBytes.size)
                                ?: return@withContext null
                val scaled = Bitmap.createScaledBitmap(bmp, DISPLAY_W, DISPLAY_H, true)
                if (scaled != bmp) bmp.recycle()
                val jpeg =
                        ByteArrayOutputStream().use { out ->
                            if (!scaled.compress(Bitmap.CompressFormat.JPEG, 85, out))
                                    return@withContext null
                            out.toByteArray()
                        }
                scaled.recycle()
                Log.d(
                        TAG,
                        "fetchOsmTile OK: ${jpeg.size} bytes (256×256→${DISPLAY_W}×${DISPLAY_H} JPEG)"
                )
                jpeg
            }

    // ── Fetch: Mapbox → OSM estático → OSM tile (escalado a JPEG) ─
    // bindProcessToNetwork(cell) durante el request para que DNS y TCP usen datos móviles
    // (si no, con WiFi=ESP32 el DNS va por WiFi sin internet → UnknownHostException).
    suspend fun fetch(lat: Double, lon: Double, zoom: Int = ZOOM_NORMAL): ByteArray? {
        if (cellClient == null || cellNetwork == null || connectivityManager == null) {
            Log.w(TAG, "fetch: cellClient/cellNetwork/cm null → devolviendo null")
            return null
        }
        val cm = connectivityManager!!
        val net = cellNetwork!!
        Log.d(TAG, "fetch: bindProcessToNetwork(cell) para DNS+TCP por celular, luego fetch")
        cm.bindProcessToNetwork(net)
        return try {
            val result =
                    if (MAPBOX_TOKEN != "YOUR_MAPBOX_TOKEN_HERE") {
                        fetchMapbox(lat, lon, zoom)
                                ?: fetchOsm(lat, lon, zoom) ?: fetchOsmTile(lat, lon, zoom)
                    } else {
                        fetchOsm(lat, lon, zoom) ?: fetchOsmTile(lat, lon, zoom)
                    }
            result
        } finally {
            cm.bindProcessToNetwork(null)
            Log.d(TAG, "fetch: bindProcessToNetwork(null) restaurado")
        }
    }

    private fun fetch(client: OkHttpClient, url: String): ByteArray? =
            try {
                val baseUrl = url.substringBefore('?')
                Log.d(TAG, "fetch(client): request a $baseUrl")
                val request =
                        Request.Builder().url(url).header("User-Agent", "ESP32Nav/1.0").build()
                client.newCall(request).execute().use { resp ->
                    if (resp.isSuccessful) {
                        val bytes = resp.body?.bytes()
                        Log.d(TAG, "fetch OK: ${bytes?.size ?: 0} bytes desde $baseUrl")
                        bytes
                    } else {
                        Log.w(TAG, "fetch HTTP ${resp.code} para $baseUrl")
                        null
                    }
                }
            } catch (e: Exception) {
                Log.e(
                        TAG,
                        "fetch error: ${e.message} type=${e.javaClass.simpleName} cause=${e.cause}"
                )
                e.printStackTrace()
                null
            }
}
