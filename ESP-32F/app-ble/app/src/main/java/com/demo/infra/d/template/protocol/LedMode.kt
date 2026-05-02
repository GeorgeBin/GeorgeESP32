package com.george.esp32k.led.protocol

enum class LedMode(val wireName: String, val label: String) {
    OFF("off", "Off"),
    SOLID("solid", "Solid"),
    BREATH("breath", "Breath"),
    BLINK("blink", "Blink");

    companion object {
        fun fromWireName(value: String?): LedMode {
            return entries.firstOrNull { it.wireName == value } ?: SOLID
        }
    }
}
