package com.tschuster.esp32nav.map

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.drawable.BitmapDrawable
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.MotionEvent
import java.io.ByteArrayOutputStream
import java.io.File
import org.osmdroid.config.Configuration
import org.osmdroid.events.MapListener
import org.osmdroid.events.ScrollEvent
import org.osmdroid.events.ZoomEvent
import org.osmdroid.tileprovider.tilesource.TileSourceFactory
import org.osmdroid.util.GeoPoint
import org.osmdroid.views.MapView
import org.osmdroid.views.overlay.Marker
import org.osmdroid.views.overlay.Overlay
import org.osmdroid.views.overlay.Polyline
import org.osmdroid.views.overlay.gestures.RotationGestureOverlay
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
 * - Tap en el mapa: [setOnMapTap] para marcar destino y navegar ahí.
 */
class MapController(context: Context) {

    val mapView: MapView
    private val locationOverlay: MyLocationNewOverlay
    private var routeOverlay: Polyline? = null
    private var destMarker: Marker? = null
    private var following = true
    private var navMode = false

    /** Se invoca con (lat, lon) cuando el usuario hace tap en el mapa para marcar destino. */
    var onMapTap: ((Double, Double) -> Unit)? = null

    private val mainHandler = Handler(Looper.getMainLooper())
    private var reEnableFollowRunnable: Runnable = Runnable {}

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

        // Rotación con dos dedos
        mapView.overlays.add(RotationGestureOverlay(mapView))

        reEnableFollowRunnable = Runnable {
            if (navMode) {
                locationOverlay.enableFollowLocation()
                following = true
                Log.d(TAG, "nav mode: seguimiento reactivado automáticamente")
            }
        }

        // Cuando el usuario mueve el mapa manualmente, pausamos el seguimiento automático.
        // En nav mode se programa la reactivación automática a los 5 s.
        mapView.addMapListener(
                object : MapListener {
                    override fun onScroll(event: ScrollEvent): Boolean {
                        if (following) {
                            locationOverlay.disableFollowLocation()
                            following = false
                            Log.d(TAG, "seguimiento pausado (pan manual)")
                        }
                        if (navMode) {
                            mainHandler.removeCallbacks(reEnableFollowRunnable)
                            mainHandler.postDelayed(reEnableFollowRunnable, 5_000L)
                        }
                        return false
                    }
                    override fun onZoom(event: ZoomEvent): Boolean = false
                }
        )

        // Tap en el mapa → marcar destino (navegar a ese punto).
        mapView.overlays.add(
                @Suppress("DEPRECATION")
                object : Overlay(context) {
                    override fun onSingleTapConfirmed(e: MotionEvent, mapView: MapView): Boolean {
                        val proj = mapView.projection ?: return false
                        val geo = proj.fromPixels(e.x.toInt(), e.y.toInt()) ?: return false
                        onMapTap?.invoke(geo.latitude, geo.longitude)
                        Log.d(TAG, "map tap → %.5f, %.5f".format(geo.latitude, geo.longitude))
                        return true
                    }
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
     * Activa/desactiva el modo navegación. Activado: zoom 17, seguimiento forzado, mapa se rota con
     * el heading del usuario. Desactivado: mapa vuelve a orientación norte-arriba.
     */
    fun setNavMode(enabled: Boolean) {
        navMode = enabled
        mainHandler.removeCallbacks(reEnableFollowRunnable)
        if (enabled) {
            mapView.controller.setZoom(17.0)
            locationOverlay.enableFollowLocation()
            following = true
            Log.d(TAG, "nav mode ON")
        } else {
            mapView.setMapOrientation(0f)
            Log.d(TAG, "nav mode OFF")
        }
    }

    /**
     * Rota el mapa para que el heading del usuario quede hacia arriba. Solo tiene efecto en nav
     * mode.
     */
    fun updateBearing(bearing: Float) {
        if (!navMode) return
        mapView.setMapOrientation(-bearing)
    }

    /** Dibuja (o borra) la ruta como polilínea azul + ícono de destino sobre el mapa Android. */
    fun setRoute(points: List<Pair<Double, Double>>) {
        routeOverlay?.let { mapView.overlays.remove(it) }
        routeOverlay = null
        destMarker?.let { mapView.overlays.remove(it) }
        destMarker = null
        if (points.isNotEmpty()) {
            routeOverlay =
                    Polyline().apply {
                        setPoints(points.map { (lat, lon) -> GeoPoint(lat, lon) })
                        outlinePaint.color = Color.rgb(0x44, 0x88, 0xFF)
                        outlinePaint.strokeWidth = 8f
                    }
            // Ruta detrás, ícono de destino encima de la ruta pero debajo del marcador de usuario
            mapView.overlays.add(0, routeOverlay)
            val dest = points.last()
            destMarker =
                    Marker(mapView).apply {
                        position = GeoPoint(dest.first, dest.second)
                        icon = makeDestIcon()
                        setAnchor(Marker.ANCHOR_CENTER, Marker.ANCHOR_BOTTOM)
                        setInfoWindow(null) // sin popup al tocar
                    }
            mapView.overlays.add(1, destMarker!!)
        }
        mapView.invalidate()
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

    /**
     * Ícono de destino: pin rojo con borde blanco y punta hacia abajo. El anchor del Marker se
     * coloca en el centro-bottom del bitmap (la punta del pin).
     */
    private fun makeDestIcon(): BitmapDrawable {
        val ctx = mapView.context
        val density = ctx.resources.displayMetrics.density
        val r = (13 * density).toInt().coerceAtLeast(13) // radio del círculo
        val tailH = (11 * density).toInt().coerceAtLeast(11) // altura de la punta
        val w = r * 2
        val h = r * 2 + tailH

        val bmp = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bmp)
        val paint = Paint(Paint.ANTI_ALIAS_FLAG)
        val cx = w / 2f
        val cy = r.toFloat()

        // Sombra suave
        paint.color = Color.argb(70, 0, 0, 0)
        canvas.drawCircle(cx + 1.5f, cy + 1.5f, r.toFloat(), paint)

        // Círculo rojo
        paint.color = Color.rgb(0xFF, 0x44, 0x44)
        canvas.drawCircle(cx, cy, r.toFloat(), paint)

        // Anillo blanco
        paint.color = Color.WHITE
        canvas.drawCircle(cx, cy, r * 0.62f, paint)

        // Centro rojo interno
        paint.color = Color.rgb(0xFF, 0x44, 0x44)
        canvas.drawCircle(cx, cy, r * 0.38f, paint)

        // Punta triangular hacia abajo
        val path = Path()
        path.moveTo(cx - r * 0.45f, cy + r * 0.72f)
        path.lineTo(cx + r * 0.45f, cy + r * 0.72f)
        path.lineTo(cx, h.toFloat())
        path.close()
        canvas.drawPath(path, paint)

        return BitmapDrawable(ctx.resources, bmp)
    }
}
