package com.tschuster.esp32nav.map

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.util.Log
import java.io.ByteArrayOutputStream
import java.io.File
import org.osmdroid.config.Configuration
import org.osmdroid.events.MapListener
import org.osmdroid.events.ScrollEvent
import org.osmdroid.events.ZoomEvent
import org.osmdroid.tileprovider.tilesource.TileSourceFactory
import org.osmdroid.util.GeoPoint
import org.osmdroid.views.MapView
import org.osmdroid.views.overlay.mylocation.GpsMyLocationProvider
import org.osmdroid.views.overlay.mylocation.MyLocationNewOverlay

private const val TAG = "ESP32Nav/Map"

/**
 * Gestiona el MapView de OSMDroid.
 *
 * - Muestra un punto azul en la posición del usuario.
 * - Sigue automáticamente la ubicación (modo seguimiento).
 * - El seguimiento se pausa si el usuario hace pan manual; se reactiva con [enableFollow].
 * - [captureJpeg] dibuja el mapa a un Bitmap 480×320 para enviar al ESP32.
 */
class MapController(context: Context) {

    val mapView: MapView
    private val locationOverlay: MyLocationNewOverlay
    private var following = true

    init {
        Configuration.getInstance().apply {
            load(context, context.getSharedPreferences("osmdroid", Context.MODE_PRIVATE))
            userAgentValue = "ESP32Nav/1.0"
            osmdroidBasePath = context.cacheDir
            osmdroidTileCache = File(context.cacheDir, "osm_tiles")
        }

        mapView =
                MapView(context).apply {
                    setTileSource(TileSourceFactory.MAPNIK)
                    setMultiTouchControls(true)
                    isTilesScaledToDpi = true
                    controller.setZoom(15.0)
                    // Centro inicial: Plaza de Mayo, Buenos Aires.
                    // Se reemplaza automáticamente cuando llega el primer GPS.
                    controller.setCenter(GeoPoint(-34.6083, -58.3712))
                }

        val dot = makeBlueDot(context)
        val dotRadius = dot.width / 2f

        locationOverlay =
                MyLocationNewOverlay(GpsMyLocationProvider(context), mapView).apply {
                    enableMyLocation()
                    enableFollowLocation()
                    setPersonIcon(dot)
                    @Suppress("DEPRECATION") setPersonHotspot(dotRadius, dotRadius)
                    // Mismo punto azul para el modo "con dirección"
                    @Suppress("DEPRECATION") setDirectionArrow(dot, dot)
                }
        mapView.overlays.add(locationOverlay)

        // Cuando el usuario mueve el mapa manualmente, pausamos el seguimiento automático
        mapView.addMapListener(
                object : MapListener {
                    override fun onScroll(event: ScrollEvent): Boolean {
                        if (following) {
                            locationOverlay.disableFollowLocation()
                            following = false
                            Log.d(TAG, "seguimiento pausado (pan manual)")
                        }
                        return false
                    }
                    override fun onZoom(event: ZoomEvent): Boolean = false
                }
        )
    }

    /** Centra el mapa con animación (primera ubicación, botón "ir a mi ubicación"). */
    fun animateTo(lat: Double, lon: Double) {
        mapView.controller.animateTo(GeoPoint(lat, lon))
    }

    /**
     * Reactiva el modo seguimiento y centra el mapa en la última ubicación conocida. Llamar al
     * tocar el botón "ir a mi ubicación".
     */
    fun enableFollow() {
        locationOverlay.enableFollowLocation()
        following = true
        Log.d(TAG, "seguimiento reactivado")
    }

    fun setZoom(zoom: Int) {
        mapView.controller.setZoom(zoom.toDouble())
    }

    /**
     * Captura el estado actual del mapa como JPEG [outW]×[outH] para enviar al ESP32.
     *
     * Estrategia de calidad:
     * 1. Se captura a doble resolución (2× outW, 2× outH) en ARGB_8888 → más detalle y
     * ```
     *     antialiasing en el paso de escala (supersampling).
     * ```
     * 2. Se escala a la resolución objetivo con filtro bilineal (filter=true).
     * 3. Se comprime a JPEG con [quality] alto (92 por defecto).
     *
     * Usar ARGB_8888 (no RGB_565) evita la pérdida de color previa al JPEG. Debe llamarse desde el
     * hilo principal. Devuelve null si el view no está listo.
     */
    fun captureJpeg(outW: Int = 320, outH: Int = 480, quality: Int = 75): ByteArray? {
        val w = mapView.width
        val h = mapView.height
        if (w <= 0 || h <= 0) {
            Log.w(TAG, "captureJpeg: MapView sin layout aún (${w}x${h})")
            return null
        }

        // Supersampling: capturar a 2× y luego reducir → menos aliasing en bordes y texto
        val ssW = (outW * 2).coerceAtMost(w)
        val ssH = (outH * 2).coerceAtMost(h)

        // ARGB_8888 preserva los 24 bits de color antes de la compresión JPEG
        val raw = Bitmap.createBitmap(ssW, ssH, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(raw)
        canvas.scale(ssW.toFloat() / w, ssH.toFloat() / h)
        mapView.draw(canvas)

        val scaled = Bitmap.createScaledBitmap(raw, outW, outH, true)
        raw.recycle()

        return ByteArrayOutputStream()
                .use { out ->
                    val ok = scaled.compress(Bitmap.CompressFormat.JPEG, quality, out)
                    scaled.recycle()
                    if (ok) out.toByteArray() else null
                }
                .also {
                    if (it != null)
                            Log.d(
                                    TAG,
                                    "captureJpeg: ${it.size} bytes (ss=${ssW}x${ssH}→${outW}x${outH} q=$quality)"
                            )
                }
    }

    fun onResume() = mapView.onResume()
    fun onPause() = mapView.onPause()

    // ── Helpers ───────────────────────────────────────────────────────────────

    /**
     * Dibuja un punto azul tipo Google Maps: halo semitransparente + borde blanco + centro azul.
     */
    private fun makeBlueDot(context: Context): Bitmap {
        val density = context.resources.displayMetrics.density
        val size = (22 * density).toInt().coerceAtLeast(22)
        val bmp = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888)
        val c = Canvas(bmp)
        val paint = Paint(Paint.ANTI_ALIAS_FLAG)
        val cx = size / 2f
        val cy = size / 2f

        // Halo azul translúcido (simula la "zona de precisión")
        paint.color = Color.argb(60, 33, 150, 243)
        c.drawCircle(cx, cy, cx, paint)

        // Borde blanco
        paint.color = Color.WHITE
        c.drawCircle(cx, cy, cx * 0.62f, paint)

        // Relleno azul
        paint.color = Color.rgb(33, 150, 243)
        c.drawCircle(cx, cy, cx * 0.45f, paint)

        return bmp
    }
}
