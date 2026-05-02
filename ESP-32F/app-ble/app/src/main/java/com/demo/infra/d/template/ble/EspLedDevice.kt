package com.george.esp32k.led.ble

import android.bluetooth.BluetoothDevice

data class EspLedDevice(
    val name: String?,
    val address: String,
    val rssi: Int,
    val matchesLedService: Boolean,
    val bluetoothDevice: BluetoothDevice,
)
