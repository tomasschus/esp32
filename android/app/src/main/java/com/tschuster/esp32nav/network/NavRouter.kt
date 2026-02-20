package com.tschuster.esp32nav.network

import android.net.Network
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONArray
import org.json.JSONObject
import java.net.URLEncoder
import java.util.concurrent.TimeUnit

data class NavStep(
    val instruction: String,
    val distanceM: Int,
    val lat: Double,
    val lon: Double
)

data class NavRoute(
    val steps: List<NavStep>,
    val totalDistanceM: Int,
    val totalDurationSec: Int
) {
    val etaMin: Int get() = totalDurationSec / 60
}

/**
 * Geocodificación: Nominatim (OSM) — gratis, sin API key.
 * Routing:        OSRM público — gratis, sin API key.
 *
 * Ambas APIs usan internet (red celular, no WiFi del ESP32).
 */
class NavRouter {

    private val defaultClient = OkHttpClient.Builder()
        .connectTimeout(10, TimeUnit.SECONDS)
        .readTimeout(15, TimeUnit.SECONDS)
        .build()

    private var boundClient: OkHttpClient? = null

    /** Vincular a la red de internet (datos móviles) para evitar rutar por el WiFi del ESP32. */
    fun setInternetNetwork(network: Network) {
        boundClient = OkHttpClient.Builder()
            .socketFactory(network.socketFactory)
            .connectTimeout(10, TimeUnit.SECONDS)
            .readTimeout(15, TimeUnit.SECONDS)
            .build()
    }

    private val client get() = boundClient ?: defaultClient

    // ── Geocodificar dirección → (lat, lon) ──────────────────────
    suspend fun geocode(query: String): Pair<Double, Double>? = withContext(Dispatchers.IO) {
        if (boundClient == null) return@withContext null
        val encoded = URLEncoder.encode(query, "UTF-8")
        val url = "https://nominatim.openstreetmap.org/search" +
                  "?q=$encoded&format=json&limit=1&addressdetails=0"
        val request = Request.Builder().url(url)
            .header("User-Agent", "ESP32Nav/1.0")
            .build()
        try {
            client.newCall(request).execute().use { resp ->
                if (!resp.isSuccessful) return@withContext null
                val arr = JSONArray(resp.body?.string() ?: return@withContext null)
                if (arr.length() == 0) return@withContext null
                val obj = arr.getJSONObject(0)
                obj.getDouble("lat") to obj.getDouble("lon")
            }
        } catch (e: Exception) { null }
    }

    // ── Calcular ruta → NavRoute ─────────────────────────────────
    suspend fun route(
        fromLat: Double, fromLon: Double,
        toLat: Double,   toLon: Double
    ): NavRoute? = withContext(Dispatchers.IO) {
        val url = "http://router.project-osrm.org/route/v1/driving/" +
                  "${"%.5f".format(fromLon)},${"%.5f".format(fromLat)};" +
                  "${"%.5f".format(toLon)},${"%.5f".format(toLat)}" +
                  "?steps=true&overview=false"
        val request = Request.Builder().url(url).build()
        try {
            client.newCall(request).execute().use { resp ->
                if (!resp.isSuccessful) return@withContext null
                val root  = JSONObject(resp.body?.string() ?: return@withContext null)
                if (root.getString("code") != "Ok") return@withContext null
                val route = root.getJSONArray("routes").getJSONObject(0)
                val leg   = route.getJSONArray("legs").getJSONObject(0)

                val distM = leg.getDouble("distance").toInt()
                val durS  = leg.getDouble("duration").toInt()

                val steps = mutableListOf<NavStep>()
                val stepsArr = leg.getJSONArray("steps")
                for (i in 0 until stepsArr.length()) {
                    val step     = stepsArr.getJSONObject(i)
                    val maneuver = step.getJSONObject("maneuver")
                    val loc      = maneuver.getJSONArray("location")
                    val street   = step.optString("name", "")
                    steps.add(
                        NavStep(
                            instruction = buildInstruction(maneuver, street),
                            distanceM   = step.getDouble("distance").toInt(),
                            lon         = loc.getDouble(0),
                            lat         = loc.getDouble(1)
                        )
                    )
                }
                NavRoute(steps, distM, durS)
            }
        } catch (e: Exception) { null }
    }

    // ── Construir instrucción en castellano ──────────────────────
    private fun buildInstruction(maneuver: JSONObject, street: String): String {
        val type     = maneuver.optString("type", "")
        val modifier = maneuver.optString("modifier", "")
        val via      = if (street.isNotBlank()) " por $street" else ""

        return when (type) {
            "depart"  -> "Salir$via"
            "arrive"  -> "Llegaste al destino"
            "turn"    -> when (modifier) {
                "left"        -> "Girar a la izquierda$via"
                "right"       -> "Girar a la derecha$via"
                "sharp left"  -> "Giro brusco a la izquierda$via"
                "sharp right" -> "Giro brusco a la derecha$via"
                "slight left" -> "Girar levemente a la izquierda$via"
                "slight right"-> "Girar levemente a la derecha$via"
                "uturn"       -> "Dar la vuelta"
                else          -> "Continuar$via"
            }
            "new name"   -> "Continuar$via"
            "merge"      -> "Incorporarse$via"
            "on ramp"    -> "Tomar la rampa$via"
            "off ramp"   -> "Salir por la rampa$via"
            "fork"       -> "En el cruce, ${dirSpan(modifier)}$via"
            "end of road"-> "Al final de la calle, ${dirSpan(modifier)}"
            "roundabout", "rotary" -> {
                val exit = maneuver.optInt("exit", 0)
                "Entrar a la rotonda, tomar la ${ordinal(exit)} salida$via"
            }
            else -> "Continuar$via"
        }
    }

    private fun dirSpan(modifier: String) = when (modifier) {
        "left"  -> "ir a la izquierda"
        "right" -> "ir a la derecha"
        "slight left"  -> "ir levemente a la izquierda"
        "slight right" -> "ir levemente a la derecha"
        else -> "continuar"
    }

    private fun ordinal(n: Int) = when (n) {
        1 -> "primera"; 2 -> "segunda"; 3 -> "tercera"
        4 -> "cuarta";  5 -> "quinta";  else -> "${n}a"
    }
}
