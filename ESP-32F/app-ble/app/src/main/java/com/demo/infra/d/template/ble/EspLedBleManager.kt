package com.george.esp32k.led.ble

import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.content.Context
import com.george.esp32k.led.protocol.LedProtocolCodec
import com.george.esp32k.led.protocol.LedResponse
import com.george.esp32k.led.protocol.LedState
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import no.nordicsemi.android.ble.BleManager
import no.nordicsemi.android.ble.callback.DataReceivedCallback
import no.nordicsemi.android.ble.data.Data
import no.nordicsemi.android.ble.observer.ConnectionObserver

class EspLedBleManager(context: Context) : BleManager(context), ConnectionObserver {
    private var commandCharacteristic: BluetoothGattCharacteristic? = null
    private var stateCharacteristic: BluetoothGattCharacteristic? = null
    private var infoCharacteristic: BluetoothGattCharacteristic? = null

    private val _connectionState = MutableStateFlow<BleConnectionState>(BleConnectionState.Idle)
    val connectionState: StateFlow<BleConnectionState> = _connectionState.asStateFlow()

    private val _ledState = MutableStateFlow<LedState?>(null)
    val ledState: StateFlow<LedState?> = _ledState.asStateFlow()

    private val _lastResponse = MutableStateFlow<LedResponse?>(null)
    val lastResponse: StateFlow<LedResponse?> = _lastResponse.asStateFlow()

    private val _lastMessage = MutableStateFlow<String?>(null)
    val lastMessage: StateFlow<String?> = _lastMessage.asStateFlow()

    private val stateCallback = DataReceivedCallback { _, data ->
        handleResponse(data)
    }

    init {
        setConnectionObserver(this)
    }

    fun connectDevice(device: BluetoothDevice) {
        _connectionState.value = BleConnectionState.Connecting
        connect(device)
            .useAutoConnect(false)
            .retry(2, 200)
            .enqueue()
    }

    fun disconnectDevice() {
        _connectionState.value = BleConnectionState.Disconnecting
        disconnect().enqueue()
    }

    fun writeCommand(payload: ByteArray) {
        val characteristic = commandCharacteristic
        if (characteristic == null) {
            _lastMessage.value = "Command characteristic not ready"
            return
        }

        writeCharacteristic(characteristic, payload, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
            .done { _lastMessage.value = "Command sent" }
            .fail { _, status -> _lastMessage.value = "Write failed: $status" }
            .enqueue()
    }

    fun readState() {
        val characteristic = stateCharacteristic
        if (characteristic == null) {
            _lastMessage.value = "State characteristic not ready"
            return
        }

        readCharacteristic(characteristic)
            .with(stateCallback)
            .fail { _, status -> _lastMessage.value = "Read state failed: $status" }
            .enqueue()
    }

    override fun isRequiredServiceSupported(gatt: BluetoothGatt): Boolean {
        val service = gatt.getService(BleConstants.LED_SERVICE_UUID) ?: return false
        commandCharacteristic = service.getCharacteristic(BleConstants.LED_COMMAND_UUID)
        stateCharacteristic = service.getCharacteristic(BleConstants.LED_STATE_UUID)
        infoCharacteristic = service.getCharacteristic(BleConstants.DEVICE_INFO_UUID)

        val command = commandCharacteristic
        val state = stateCharacteristic
        val canWrite = command?.properties?.and(
            BluetoothGattCharacteristic.PROPERTY_WRITE or
                BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE
        ) != 0
        val canNotify = state?.properties?.and(BluetoothGattCharacteristic.PROPERTY_NOTIFY) != 0
        val canRead = state?.properties?.and(BluetoothGattCharacteristic.PROPERTY_READ) != 0

        return command != null && state != null && canWrite && (canNotify || canRead)
    }

    override fun initialize() {
        stateCharacteristic?.let { characteristic ->
            setNotificationCallback(characteristic).with(stateCallback)
            requestMtu(256).enqueue()
            enableNotifications(characteristic)
                .done {
                    _lastMessage.value = "State notifications enabled"
                    readState()
                }
                .fail { _, status -> _lastMessage.value = "Notify failed: $status" }
                .enqueue()
        }
    }

    override fun onServicesInvalidated() {
        commandCharacteristic = null
        stateCharacteristic = null
        infoCharacteristic = null
    }

    override fun onDeviceConnecting(device: BluetoothDevice) {
        _connectionState.value = BleConnectionState.Connecting
    }

    override fun onDeviceConnected(device: BluetoothDevice) {
        _connectionState.value = BleConnectionState.Connected
    }

    override fun onDeviceFailedToConnect(device: BluetoothDevice, reason: Int) {
        _connectionState.value = BleConnectionState.Error("Connect failed: $reason")
    }

    override fun onDeviceReady(device: BluetoothDevice) {
        _connectionState.value = BleConnectionState.Ready
    }

    override fun onDeviceDisconnecting(device: BluetoothDevice) {
        _connectionState.value = BleConnectionState.Disconnecting
    }

    override fun onDeviceDisconnected(device: BluetoothDevice, reason: Int) {
        _connectionState.value = BleConnectionState.Disconnected("Disconnected: $reason")
    }

    private fun handleResponse(data: Data) {
        val value = data.value ?: return
        try {
            val response = LedProtocolCodec.decodeResponse(value)
            _lastResponse.value = response
            _lastMessage.value = if (response.code == 0) response.msg else "${response.code}: ${response.msg}"
            response.state?.let { _ledState.value = it }
        } catch (error: Exception) {
            _lastMessage.value = "Invalid state: ${error.message}"
        }
    }
}
