package com.george.esp32k.led.protocol

data class LedCommand(
    val seq: Int,
    val color: String,
    val mode: LedMode,
    val brightness: Int,
    val periodMs: Int,
    val onMs: Int,
    val offMs: Int,
)
