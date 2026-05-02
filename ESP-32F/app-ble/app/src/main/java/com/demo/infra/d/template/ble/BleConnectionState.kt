package com.george.esp32k.led.ble

sealed class BleConnectionState {
    data object Idle : BleConnectionState()
    data object Connecting : BleConnectionState()
    data object Connected : BleConnectionState()
    data object Ready : BleConnectionState()
    data object Disconnecting : BleConnectionState()
    data class Disconnected(val reason: String? = null) : BleConnectionState()
    data class Error(val message: String) : BleConnectionState()
}
