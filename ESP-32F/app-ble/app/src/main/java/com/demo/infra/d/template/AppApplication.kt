package com.george.esp32k.led

import android.app.Application
import com.george.esp32k.led.storage.KVManager

class AppApplication : Application() {

    override fun onCreate() {
        super.onCreate()
        instance = this
        KVManager.init(this)
    }

    companion object {
        lateinit var instance: AppApplication
            private set
    }
}
