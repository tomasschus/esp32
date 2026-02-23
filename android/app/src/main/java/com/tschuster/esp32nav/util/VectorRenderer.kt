package com.tschuster.esp32nav.util

import kotlin.math.*

/**
 * Proyección Mercator + simplificación de polilíneas + construcción del frame JSON
 * que se envía al ESP32 por WebSocket.
 *
 * Protocolo JSON:
 * {
 *   "t": "vec",
 *   "roads": [{"p":[[x,y],...], "w":1}, ...],
 *   "route": [[x,y], ...],
 *   "pos": [x, y],
 *   "hdg": 90
 * }
 * Coordenadas en píxeles de pantalla (0-319, 0-479), ya proyectadas aquí.
 */
object VectorRenderer {

    const val SCREEN_W = 320
    const val SCREEN_H = 480
    /** Posición Y del marcador de usuario en píxeles (3/4 de pantalla hacia abajo). */
    const val POS_Y = SCREEN_H * 3 / 4  // 360

    data class RoadSegment(val pixels: List<Pair<Int, Int>>, val width: Int, val name: String = "")

    data class StreetLabel(val x: Int, val y: Int, val name: String)

    // ── Proyección Web Mercator ───────────────────────────────────────────────
    /**
     * Convierte lat/lon a pixel de pantalla relativo al centro [centerLat, centerLon] en [zoom].
     * El GPS siempre mapea a (SCREEN_W/2, POS_Y) = (160, 360).
     */
    fun latLonToPixel(
        lat: Double, lon: Double,
        centerLat: Double, centerLon: Double,
        zoom: Double
    ): Pair<Int, Int> {
        val scale = 256.0 * 2.0.pow(zoom) / (2.0 * PI)

        fun mercY(lat: Double): Double {
            val rad = lat * PI / 180.0
            return -ln(tan(PI / 4.0 + rad / 2.0))
        }

        val dx = (lon - centerLon) * PI / 180.0 * scale
        val dy = (mercY(lat) - mercY(centerLat)) * scale

        return Pair(
            (SCREEN_W / 2.0 + dx).roundToInt(),
            (POS_Y + dy).roundToInt()
        )
    }

    // ── Simplificación Ramer–Douglas–Peucker ─────────────────────────────────
    fun simplify(points: List<Pair<Int, Int>>, epsilon: Double): List<Pair<Int, Int>> {
        if (points.size <= 2) return points

        var maxDist = 0.0
        var maxIdx = 0
        val first = points.first()
        val last = points.last()

        for (i in 1 until points.size - 1) {
            val d = perpendicularDist(points[i], first, last)
            if (d > maxDist) {
                maxDist = d
                maxIdx = i
            }
        }

        return if (maxDist > epsilon) {
            val left  = simplify(points.subList(0, maxIdx + 1), epsilon)
            val right = simplify(points.subList(maxIdx, points.size), epsilon)
            left.dropLast(1) + right
        } else {
            listOf(first, last)
        }
    }

    private fun perpendicularDist(p: Pair<Int, Int>, a: Pair<Int, Int>, b: Pair<Int, Int>): Double {
        val dx = (b.first - a.first).toDouble()
        val dy = (b.second - a.second).toDouble()
        if (dx == 0.0 && dy == 0.0) {
            return sqrt((p.first - a.first).toDouble().pow(2) + (p.second - a.second).toDouble().pow(2))
        }
        val t = ((p.first - a.first) * dx + (p.second - a.second) * dy) / (dx * dx + dy * dy)
        val nearX = a.first + t * dx
        val nearY = a.second + t * dy
        return sqrt((p.first - nearX).pow(2) + (p.second - nearY).pow(2))
    }

    // ── Rotación heading-up ───────────────────────────────────────────────────
    /**
     * Rota [points] alrededor de ([cx], [cy]) por -[bearingDeg] grados.
     * Con bearing=90 (yendo al este) el mapa rota -90° → el este queda arriba.
     */
    fun rotatePoints(
        points: List<Pair<Int, Int>>,
        bearingDeg: Float,
        cx: Int,
        cy: Int
    ): List<Pair<Int, Int>> {
        if (points.isEmpty()) return points
        val rad = -bearingDeg * PI / 180.0
        val cosA = cos(rad)
        val sinA = sin(rad)
        return points.map { (x, y) ->
            val dx = x - cx
            val dy = y - cy
            Pair(
                (cx + dx * cosA - dy * sinA).roundToInt(),
                (cy + dx * sinA + dy * cosA).roundToInt()
            )
        }
    }

    // ── Constructor de frame JSON ─────────────────────────────────────────────
    fun buildFrame(
        roads: List<RoadSegment>,
        route: List<Pair<Int, Int>>,
        labels: List<StreetLabel>,
        posX: Int,
        posY: Int,
        heading: Int
    ): String {
        val sb = StringBuilder(6144)
        sb.append("{\"t\":\"vec\",\"roads\":[")
        roads.forEachIndexed { i, road ->
            if (i > 0) sb.append(',')
            sb.append("{\"p\":[")
            road.pixels.forEachIndexed { j, pt ->
                if (j > 0) sb.append(',')
                sb.append('[').append(pt.first).append(',').append(pt.second).append(']')
            }
            sb.append("],\"w\":").append(road.width).append('}')
        }
        sb.append("],\"route\":[")
        route.forEachIndexed { i, pt ->
            if (i > 0) sb.append(',')
            sb.append('[').append(pt.first).append(',').append(pt.second).append(']')
        }
        sb.append("],\"labels\":[")
        labels.forEachIndexed { i, lbl ->
            if (i > 0) sb.append(',')
            // Truncar a 20 chars y escapar comillas por si acaso
            val safe = lbl.name.take(20).replace("\"", "'")
            sb.append("{\"p\":[").append(lbl.x).append(',').append(lbl.y)
            sb.append("],\"n\":\"").append(safe).append("\"}")
        }
        sb.append("],\"pos\":[").append(posX).append(',').append(posY)
        sb.append("],\"hdg\":").append(heading).append('}')
        return sb.toString()
    }

    private fun Double.roundToInt(): Int = kotlin.math.round(this).toInt()
}
