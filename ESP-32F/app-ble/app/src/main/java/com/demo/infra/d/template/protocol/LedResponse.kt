package com.george.esp32k.led.protocol

data class LedResponse(
    val seq: Int,
    val code: Int,
    val msg: String,
    val state: LedState?,
)
