package com.george.esp32k.led.protocol

data class LedState(
    val color: String = "#FFFFFF",
    val mode: LedMode = LedMode.SOLID,
    val brightness: Int = 100,
    val periodMs: Int = 2000,
    val onMs: Int = 500,
    val offMs: Int = 500,
    val source: String = "none",
    val bleConnected: Boolean = false,
    val wifiConnected: Boolean = false,
)
