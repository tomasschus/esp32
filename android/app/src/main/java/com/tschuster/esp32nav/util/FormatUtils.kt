package com.tschuster.esp32nav.util

/** Formatea minutos totales como "1 h 25 min" o "45 min". */
fun formatEtaMinutes(etaMin: Int): String =
        when {
            etaMin >= 60 ->
                    if (etaMin % 60 == 0) "${etaMin / 60} h"
                    else "${etaMin / 60} h ${etaMin % 60} min"
            else -> "$etaMin min"
        }
