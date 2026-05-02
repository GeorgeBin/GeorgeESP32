package com.george.esp32k.led.ble

import android.os.ParcelUuid
import java.util.UUID

object BleConstants {
    const val DEVICE_NAME = "George_LED_Device"

    val LED_SERVICE_UUID: UUID = UUID.fromString("0000A001-0000-1000-8000-00805F9B34FB")
    val LED_COMMAND_UUID: UUID = UUID.fromString("0000A002-0000-1000-8000-00805F9B34FB")
    val LED_STATE_UUID: UUID = UUID.fromString("0000A003-0000-1000-8000-00805F9B34FB")
    val DEVICE_INFO_UUID: UUID = UUID.fromString("0000A004-0000-1000-8000-00805F9B34FB")

    val LED_SERVICE_PARCEL_UUID: ParcelUuid = ParcelUuid(LED_SERVICE_UUID)
}
