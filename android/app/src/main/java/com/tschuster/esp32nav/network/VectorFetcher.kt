package com.tschuster.esp32nav.network

import android.location.Location
import android.net.Network
import android.util.Log
import com.tschuster.esp32nav.util.VectorRenderer
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONObject
import java.net.URLEncoder
import java.util.Locale
import java.util.concurrent.TimeUnit

private const val TAG = "ESP32Nav/VectorFetcher"

/**
 * Obtiene calles cercanas de la API Overpass (OpenStreetMap).
 *
 * - Cachea los datos en lat/lon para reproyectar en cada frame.
 * - Solo re-consulta cuando el GPS se mueve >80 m desde la última consulta.
 * - [fetchAndCache] debe llamarse desde un corutina IO (no bloquea el hilo principal).
 * - [getCachedRoads] es sincrónico y devuelve los datos del caché reproyectados.
 */
class VectorFetcher {

    private data class RawRoadSegment(val latLons: List<Pair<Double, Double>>, val width: Int)

    private val defaultClient = OkHttpClient.Builder()
        .connectTimeout(10, TimeUnit.SECONDS)
        .readTimeout(15, TimeUnit.SECONDS)
        .build()

    private var boundClient: OkHttpClient? = null

    @Volatile private var cachedRawRoads: List<RawRoadSegment> = emptyList()
    @Volatile private var lastQueryLat = Double.NaN
    @Volatile private var lastQueryLon = Double.NaN

    fun setInternetNetwork(network: Network) {
        boundClient = OkHttpClient.Builder()
            .socketFactory(network.socketFactory)
            .connectTimeout(10, TimeUnit.SECONDS)
            .readTimeout(15, TimeUnit.SECONDS)
            .build()
    }

    /** Devuelve true si el GPS se alejó >200 m de la última consulta. */
    fun needsRefresh(lat: Double, lon: Double): Boolean {
        if (lastQueryLat.isNaN()) return true
        val dist = FloatArray(1)
        Location.distanceBetween(lat, lon, lastQueryLat, lastQueryLon, dist)
        return dist[0] > 150f
    }

    /** Consulta Overpass y actualiza el caché. Llamar desde Dispatchers.IO. */
    suspend fun fetchAndCache(lat: Double, lon: Double) = withContext(Dispatchers.IO) {
        val client = boundClient ?: defaultClient
        Log.d(TAG, "fetchAndCache lat=${String.format(Locale.US, "%.4f", lat)} " +
                "lon=${String.format(Locale.US, "%.4f", lon)} " +
                "(${if (boundClient != null) "red celular" else "red default"})")
        val query = "[out:json][timeout:10];" +
            "(way[\"highway\"~\"^(motorway|trunk|primary|secondary|tertiary|residential|service)$\"]" +
            "(around:500,${String.format(Locale.US, "%.5f", lat)}," +
            "${String.format(Locale.US, "%.5f", lon)}););" +
            "out geom qt;"
        val url = "https://overpass-api.de/api/interpreter?data=${URLEncoder.encode(query, "UTF-8")}"
        try {
            val req = Request.Builder().url(url).header("User-Agent", "ESP32Nav/1.0").build()
            client.newCall(req).execute().use { resp ->
                if (!resp.isSuccessful) {
                    Log.e(TAG, "fetchAndCache HTTP ${resp.code} ${resp.message}")
                    return@withContext
                }
                val body = resp.body?.string()
                if (body == null) {
                    Log.e(TAG, "fetchAndCache: body null")
                    return@withContext
                }
                val roads = parseOverpass(body)
                cachedRawRoads = roads
                lastQueryLat = lat
                lastQueryLon = lon
                Log.d(TAG, "Overpass: ${roads.size} segmentos cacheados")
            }
        } catch (e: Exception) {
            Log.e(TAG, "fetchAndCache error: ${e.message}")
        }
    }

    /**
     * Reproyecta el caché a píxeles de pantalla para el [centerLat]/[centerLon] y [zoom] actuales.
     * Sincrónico, sin red. Llama desde el hilo principal (map loop).
     */
    fun getCachedRoads(
        zoom: Double,
        centerLat: Double,
        centerLon: Double
    ): List<VectorRenderer.RoadSegment> {
        return cachedRawRoads.mapNotNull { raw ->
            val pixels = raw.latLons.map { (lat, lon) ->
                VectorRenderer.latLonToPixel(lat, lon, centerLat, centerLon, zoom)
            }
            // Descartar segmentos completamente fuera de pantalla (margen de 60 px)
            val onScreen = pixels.any { (x, y) -> x in -60..380 && y in -60..540 }
            if (!onScreen) return@mapNotNull null
            val simplified = VectorRenderer.simplify(pixels, 1.5)
            if (simplified.size < 2) null
            else VectorRenderer.RoadSegment(simplified, raw.width)
        }
    }

    // ── Parser Overpass ───────────────────────────────────────────────────────
    private fun parseOverpass(json: String): List<RawRoadSegment> {
        val result = mutableListOf<RawRoadSegment>()
        try {
            val elements = JSONObject(json).getJSONArray("elements")
            for (i in 0 until elements.length()) {
                val el = elements.getJSONObject(i)
                if (el.getString("type") != "way") continue
                val geom = el.optJSONArray("geometry") ?: continue
                val highway = el.optJSONObject("tags")?.optString("highway", "") ?: continue
                val width = highwayWidth(highway)
                val latLons = mutableListOf<Pair<Double, Double>>()
                for (j in 0 until geom.length()) {
                    val node = geom.getJSONObject(j)
                    latLons.add(node.getDouble("lat") to node.getDouble("lon"))
                }
                if (latLons.size >= 2) result.add(RawRoadSegment(latLons, width))
            }
        } catch (e: Exception) {
            Log.e(TAG, "parseOverpass error: ${e.message}")
        }
        return result
    }

    private fun highwayWidth(highway: String) = when (highway) {
        "motorway", "trunk"          -> 3
        "primary", "secondary"       -> 2
        else                         -> 1
    }
}
