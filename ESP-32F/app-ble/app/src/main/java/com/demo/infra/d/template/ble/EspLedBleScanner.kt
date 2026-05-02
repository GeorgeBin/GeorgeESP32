package com.george.esp32k.led.ble

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.content.ContextCompat
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

class EspLedBleScanner(
    private val context: Context,
    private val bluetoothAdapter: BluetoothAdapter?,
) {
    private val _devices = MutableStateFlow<List<EspLedDevice>>(emptyList())
    val devices: StateFlow<List<EspLedDevice>> = _devices.asStateFlow()

    private val _scanning = MutableStateFlow(false)
    val scanning: StateFlow<Boolean> = _scanning.asStateFlow()

    private val scanner
        get() = bluetoothAdapter?.bluetoothLeScanner

    private var serviceFilterMode = true

    private val callback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            addResult(result)
        }

        override fun onBatchScanResults(results: MutableList<ScanResult>) {
            results.forEach(::addResult)
        }

        override fun onScanFailed(errorCode: Int) {
            _scanning.value = false
        }
    }

    @SuppressLint("MissingPermission")
    fun startScan(useServiceFilter: Boolean = true) {
        if (!hasScanPermission() || bluetoothAdapter?.isEnabled != true || scanner == null) {
            _scanning.value = false
            return
        }

        stopScan()
        serviceFilterMode = useServiceFilter
        val filters = if (useServiceFilter) {
            listOf(ScanFilter.Builder().setServiceUuid(BleConstants.LED_SERVICE_PARCEL_UUID).build())
        } else {
            emptyList()
        }
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        _devices.value = emptyList()
        _scanning.value = true
        scanner?.startScan(filters, settings, callback)
    }

    @SuppressLint("MissingPermission")
    fun stopScan() {
        if (hasScanPermission()) {
            scanner?.stopScan(callback)
        }
        _scanning.value = false
    }

    @SuppressLint("MissingPermission")
    private fun addResult(result: ScanResult) {
        val scanRecord = result.scanRecord
        val matchesService = scanRecord?.serviceUuids?.contains(BleConstants.LED_SERVICE_PARCEL_UUID) == true
        val name = scanRecord?.deviceName ?: result.device.name

        if (!matchesService && !serviceFilterMode && name != BleConstants.DEVICE_NAME) {
            return
        }

        val device = EspLedDevice(
            name = name,
            address = result.device.address,
            rssi = result.rssi,
            matchesLedService = matchesService,
            bluetoothDevice = result.device,
        )
        _devices.value = (_devices.value.filterNot { it.address == device.address } + device)
            .sortedByDescending { it.rssi }
    }

    private fun hasScanPermission(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_SCAN) ==
                PackageManager.PERMISSION_GRANTED
        } else {
            ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_FINE_LOCATION) ==
                PackageManager.PERMISSION_GRANTED
        }
    }
}
