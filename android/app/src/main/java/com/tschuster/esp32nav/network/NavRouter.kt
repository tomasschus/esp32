package com.tschuster.esp32nav.network

import android.net.Network
import java.net.URLEncoder
import java.util.concurrent.TimeUnit
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONArray
import org.json.JSONObject

data class GeocodeSuggestion(
        val label: String,
        val lat: Double,
        val lon: Double,
)

data class NavStep(val instruction: String, val distanceM: Int, val lat: Double, val lon: Double)

data class NavRoute(
        val steps: List<NavStep>,
        val totalDistanceM: Int,
        val totalDurationSec: Int,
        val geometry: List<Pair<Double, Double>> = emptyList()
) {
    val etaMin: Int
        get() = totalDurationSec / 60
}

private const val BASE_URL = "https://maps.tomasschuster.com"
private val JSON_MEDIA_TYPE = "application/json; charset=utf-8".toMediaType()

class NavRouter {

    private val defaultClient =
            OkHttpClient.Builder()
                    .connectTimeout(10, TimeUnit.SECONDS)
                    .readTimeout(15, TimeUnit.SECONDS)
                    .build()

    private var boundClient: OkHttpClient? = null

    fun setInternetNetwork(network: Network) {
        boundClient =
                OkHttpClient.Builder()
                        .socketFactory(network.socketFactory)
                        .connectTimeout(10, TimeUnit.SECONDS)
                        .readTimeout(15, TimeUnit.SECONDS)
                        .build()
    }

    private val client
        get() = boundClient ?: defaultClient

    // ── Buscar sugerencias → lista de hits ───────────────────────
    suspend fun suggest(
            query: String,
            userLat: Double? = null,
            userLon: Double? = null,
    ): List<GeocodeSuggestion> =
            withContext(Dispatchers.IO) {
                if (boundClient == null) return@withContext emptyList()
                val encoded = URLEncoder.encode(query, "UTF-8")
                val latLon =
                        if (userLat != null && userLon != null) "&lat=$userLat&lon=$userLon" else ""
                val request =
                        Request.Builder()
                                .url("$BASE_URL/geocode?q=$encoded$latLon")
                                .header("User-Agent", "ESP32Nav/1.0")
                                .build()
                try {
                    client.newCall(request).execute().use { resp ->
                        if (!resp.isSuccessful) return@withContext emptyList()
                        val hits =
                                JSONObject(resp.body?.string() ?: return@withContext emptyList())
                                        .getJSONArray("hits")
                        (0 until hits.length()).map { i ->
                            val h = hits.getJSONObject(i)
                            GeocodeSuggestion(
                                    label = h.getString("label"),
                                    lat = h.getDouble("lat"),
                                    lon = h.getDouble("lon"),
                            )
                        }
                    }
                } catch (e: Exception) {
                    emptyList()
                }
            }

    // ── Calcular ruta → NavRoute ─────────────────────────────────
    suspend fun route(fromLat: Double, fromLon: Double, toLat: Double, toLon: Double): NavRoute? =
            withContext(Dispatchers.IO) {
                val bodyJson =
                        JSONObject().apply {
                            put("from", JSONArray().put(fromLon).put(fromLat))
                            put("to", JSONArray().put(toLon).put(toLat))
                            put("profile", "car")
                        }
                val request =
                        Request.Builder()
                                .url("$BASE_URL/route")
                                .post(bodyJson.toString().toRequestBody(JSON_MEDIA_TYPE))
                                .build()
                try {
                    client.newCall(request).execute().use { resp ->
                        if (!resp.isSuccessful) return@withContext null
                        val root = JSONObject(resp.body?.string() ?: return@withContext null)

                        val distM = root.getDouble("distance").toInt()
                        // GraphHopper devuelve "time" en milisegundos (283061 ms = 4,7 min)
                        val durS = (root.getLong("time") / 1000L).toInt().coerceAtLeast(0)
                        val multiplier = root.optDouble("points_encoded_multiplier", 1e6)

                        // El backend indica explícitamente si el polyline incluye elevación (3D).
                        // Default true: compatibilidad con backend deployado antes de agregar este
                        // campo.
                        val withElevation = root.optBoolean("elevation", true)
                        val geometry =
                                decodePolyline(root.getString("points"), multiplier, withElevation)

                        val instructionsArr = root.getJSONArray("instructions")
                        val steps = mutableListOf<NavStep>()
                        for (i in 0 until instructionsArr.length()) {
                            val instr = instructionsArr.getJSONObject(i)
                            val ptIdx = instr.getJSONArray("interval").getInt(0)
                            val (lat, lon) =
                                    if (ptIdx < geometry.size) geometry[ptIdx] else 0.0 to 0.0
                            steps.add(
                                    NavStep(
                                            instruction = instr.getString("text"),
                                            distanceM = instr.getDouble("distance").toInt(),
                                            lat = lat,
                                            lon = lon
                                    )
                            )
                        }

                        NavRoute(steps, distM, durS, geometry)
                    }
                } catch (e: Exception) {
                    null
                }
            }

    // ── Decodificar polyline (GraphHopper usa multiplier=1e6) ────
    // withElevation=true cuando GraphHopper devuelve puntos 3D (lat,lon,ele):
    // en ese caso hay que leer y descartar el tercer valor para no corromper los siguientes.
    private fun decodePolyline(
            encoded: String,
            multiplier: Double,
            withElevation: Boolean = false
    ): List<Pair<Double, Double>> {
        val result = mutableListOf<Pair<Double, Double>>()
        var index = 0
        var lat = 0
        var lon = 0
        while (index < encoded.length) {
            var b: Int
            var shift = 0
            var chunk = 0
            do {
                b = encoded[index++].code - 63
                chunk = chunk or ((b and 0x1f) shl shift)
                shift += 5
            } while (b >= 0x20)
            lat += if (chunk and 1 != 0) (chunk shr 1).inv() else chunk shr 1

            shift = 0
            chunk = 0
            do {
                b = encoded[index++].code - 63
                chunk = chunk or ((b and 0x1f) shl shift)
                shift += 5
            } while (b >= 0x20)
            lon += if (chunk and 1 != 0) (chunk shr 1).inv() else chunk shr 1

            // Saltear elevación si está presente (3D polyline)
            if (withElevation) {
                shift = 0
                chunk = 0
                do {
                    b = encoded[index++].code - 63
                    chunk = chunk or ((b and 0x1f) shl shift)
                    shift += 5
                } while (b >= 0x20)
                // valor de elevación descartado
            }

            result.add(lat / multiplier to lon / multiplier)
        }
        return result
    }
}
