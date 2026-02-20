package com.tschuster.esp32nav

import android.app.Application
import com.tschuster.esp32nav.network.NetworkManager

class ESP32NavApp : Application() {
    lateinit var networkManager: NetworkManager
        private set

    override fun onCreate() {
        super.onCreate()
        networkManager = NetworkManager(this)
    }
}
