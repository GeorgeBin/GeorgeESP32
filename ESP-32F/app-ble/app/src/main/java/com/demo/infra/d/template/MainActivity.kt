package com.george.esp32k.led

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import com.george.esp32k.led.ble.BleConnectionState
import com.george.esp32k.led.ble.EspLedBleManager
import com.george.esp32k.led.ble.EspLedBleScanner
import com.george.esp32k.led.ble.EspLedDevice
import com.george.esp32k.led.protocol.LedCommand
import com.george.esp32k.led.protocol.LedMode
import com.george.esp32k.led.protocol.LedProtocolCodec
import com.george.esp32k.led.protocol.LedState
import com.george.esp32k.led.ui.theme.InfraD_TemplateTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            InfraD_TemplateTheme {
                GeorgeLedApp()
            }
        }
    }
}

@Composable
private fun GeorgeLedApp() {
    val context = LocalContext.current
    val bluetoothAdapter = remember {
        context.getSystemService(BluetoothManager::class.java)?.adapter
    }
    val scanner = remember { EspLedBleScanner(context.applicationContext, bluetoothAdapter) }
    val manager = remember { EspLedBleManager(context.applicationContext) }
    val devices by scanner.devices.collectAsState()
    val scanning by scanner.scanning.collectAsState()
    val connectionState by manager.connectionState.collectAsState()
    val ledState by manager.ledState.collectAsState()
    val lastMessage by manager.lastMessage.collectAsState()
    val requiredPermissions = remember { requiredBlePermissions() }
    var hasPermissions by remember { mutableStateOf(hasBlePermissions(context)) }
    var selectedDevice by remember { mutableStateOf<EspLedDevice?>(null) }
    var useServiceFilter by remember { mutableStateOf(true) }

    val permissionLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) {
        hasPermissions = hasBlePermissions(context)
        if (hasPermissions) {
            scanner.startScan(useServiceFilter = true)
        }
    }

    DisposableEffect(Unit) {
        onDispose {
            scanner.stopScan()
            manager.disconnectDevice()
        }
    }

    Scaffold(
        modifier = Modifier.fillMaxSize(),
        bottomBar = {
            StatusFooter(
                connectionState = connectionState,
                lastMessage = lastMessage,
            )
        }
    ) { innerPadding ->
        Surface(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding)
                .background(MaterialTheme.colorScheme.background)
        ) {
            when {
                !hasPermissions -> PermissionScreen(
                    permissions = requiredPermissions,
                    onGrantClick = { permissionLauncher.launch(requiredPermissions) },
                )

                bluetoothAdapter == null -> MessageScreen(
                    title = "Bluetooth LE unavailable",
                    message = "This device does not expose a Bluetooth adapter.",
                )

                bluetoothAdapter.isEnabled != true -> MessageScreen(
                    title = "Bluetooth is off",
                    message = "Turn on Bluetooth and return to this app.",
                )

                selectedDevice == null -> ScanScreen(
                    scanning = scanning,
                    devices = devices,
                    useServiceFilter = useServiceFilter,
                    onStartScan = {
                        useServiceFilter = true
                        scanner.startScan(useServiceFilter = true)
                    },
                    onFallbackScan = {
                        useServiceFilter = false
                        scanner.startScan(useServiceFilter = false)
                    },
                    onStopScan = scanner::stopScan,
                    onDeviceClick = { device ->
                        selectedDevice = device
                        scanner.stopScan()
                        manager.connectDevice(device.bluetoothDevice)
                    },
                )

                else -> ControlScreen(
                    device = selectedDevice,
                    connectionState = connectionState,
                    ledState = ledState,
                    onDisconnect = {
                        manager.disconnectDevice()
                        selectedDevice = null
                    },
                    onReadState = manager::readState,
                    onSendCommand = { command ->
                        manager.writeCommand(LedProtocolCodec.encodeSetLed(command))
                    },
                )
            }
        }
    }
}

@Composable
private fun PermissionScreen(
    permissions: Array<String>,
    onGrantClick: () -> Unit,
) {
    PageColumn {
        Text(
            text = "George LED",
            style = MaterialTheme.typography.headlineMedium,
        )
        Text(
            text = "Bluetooth permission is required to scan and connect to the ESP32 LED service.",
            style = MaterialTheme.typography.bodyLarge,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Text(
            text = permissions.joinToString(separator = "\n"),
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Button(onClick = onGrantClick, modifier = Modifier.fillMaxWidth()) {
            Text("Grant permission")
        }
    }
}

@Composable
private fun ScanScreen(
    scanning: Boolean,
    devices: List<EspLedDevice>,
    useServiceFilter: Boolean,
    onStartScan: () -> Unit,
    onFallbackScan: () -> Unit,
    onStopScan: () -> Unit,
    onDeviceClick: (EspLedDevice) -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .statusBarsPadding()
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text(text = "Select LED device", style = MaterialTheme.typography.headlineMedium)
        Text(
            text = when {
                scanning && useServiceFilter -> "Scanning for LED service..."
                scanning -> "Scanning by device name..."
                else -> "Scan stopped"
            },
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = onStartScan, enabled = !scanning) {
                Text("Scan")
            }
            OutlinedButton(onClick = onStopScan, enabled = scanning) {
                Text("Stop")
            }
            OutlinedButton(onClick = onFallbackScan) {
                Text("Name scan")
            }
        }
        LazyColumn(
            modifier = Modifier.fillMaxSize(),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            items(devices, key = { it.address }) { device ->
                DeviceRow(device = device, onClick = { onDeviceClick(device) })
            }
        }
    }
}

@SuppressLint("MissingPermission")
@Composable
private fun ControlScreen(
    device: EspLedDevice?,
    connectionState: BleConnectionState,
    ledState: LedState?,
    onDisconnect: () -> Unit,
    onReadState: () -> Unit,
    onSendCommand: (LedCommand) -> Unit,
) {
    var seq by remember { mutableIntStateOf(1) }
    var mode by remember { mutableStateOf(LedMode.SOLID) }
    var red by remember { mutableIntStateOf(255) }
    var green by remember { mutableIntStateOf(0) }
    var blue by remember { mutableIntStateOf(0) }
    var brightness by remember { mutableIntStateOf(100) }
    var periodMs by remember { mutableStateOf("2000") }
    var onMs by remember { mutableStateOf("500") }
    var offMs by remember { mutableStateOf("500") }

    LaunchedEffect(ledState) {
        val state = ledState ?: return@LaunchedEffect
        val parsed = parseColor(state.color)
        red = parsed.first
        green = parsed.second
        blue = parsed.third
        mode = state.mode
        brightness = state.brightness
        periodMs = state.periodMs.toString()
        onMs = state.onMs.toString()
        offMs = state.offMs.toString()
    }

    PageColumn {
        Text(text = device?.name ?: "LED device", style = MaterialTheme.typography.headlineMedium)
        Text(
            text = "${device?.address.orEmpty()}  ${connectionState.toDisplayText()}",
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
        StateCard(ledState)
        ColorControls(
            red = red,
            green = green,
            blue = blue,
            onRed = { red = it },
            onGreen = { green = it },
            onBlue = { blue = it },
        )
        ModeControls(mode = mode, onModeChange = { mode = it })
        SliderField(
            label = "Brightness",
            value = brightness,
            range = 0f..100f,
            onValueChange = { brightness = it },
        )
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            NumericField("Period ms", periodMs, { periodMs = it }, Modifier.weight(1f))
            NumericField("On ms", onMs, { onMs = it }, Modifier.weight(1f))
            NumericField("Off ms", offMs, { offMs = it }, Modifier.weight(1f))
        }
        Button(
            onClick = {
                val command = LedCommand(
                    seq = seq++,
                    color = "#%02X%02X%02X".format(red, green, blue),
                    mode = mode,
                    brightness = brightness,
                    periodMs = periodMs.toPositiveInt(2000),
                    onMs = onMs.toPositiveInt(500),
                    offMs = offMs.toPositiveInt(500),
                )
                onSendCommand(command)
            },
            enabled = connectionState is BleConnectionState.Ready,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text("Send")
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = onReadState, modifier = Modifier.weight(1f)) {
                Text("Read state")
            }
            OutlinedButton(onClick = onDisconnect, modifier = Modifier.weight(1f)) {
                Text("Disconnect")
            }
        }
    }
}

@Composable
private fun DeviceRow(device: EspLedDevice, onClick: () -> Unit) {
    Card(
        onClick = onClick,
        shape = RoundedCornerShape(8.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer),
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(14.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(text = device.name ?: "(unnamed)", style = MaterialTheme.typography.titleMedium)
                Text(
                    text = device.address,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            Column(horizontalAlignment = Alignment.End) {
                Text(text = "${device.rssi} dBm", style = MaterialTheme.typography.bodyMedium)
                Text(
                    text = if (device.matchesLedService) "LED service" else "Name match",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

@Composable
private fun StateCard(state: LedState?) {
    Card(
        shape = RoundedCornerShape(8.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerHigh),
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Text(text = "Current state", style = MaterialTheme.typography.titleMedium)
            Text(text = "Color: ${state?.color ?: "--"}")
            Text(text = "Mode: ${state?.mode?.label ?: "--"}")
            Text(text = "Brightness: ${state?.brightness ?: "--"}")
            Text(text = "Source: ${state?.source ?: "--"}")
            Text(text = "Wi-Fi: ${if (state?.wifiConnected == true) "connected" else "not connected"}")
        }
    }
}

@Composable
private fun ColorControls(
    red: Int,
    green: Int,
    blue: Int,
    onRed: (Int) -> Unit,
    onGreen: (Int) -> Unit,
    onBlue: (Int) -> Unit,
) {
    val color = Color(red, green, blue)
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Box(
                modifier = Modifier
                    .size(40.dp)
                    .clip(RoundedCornerShape(8.dp))
                    .background(color),
            )
            Text(text = "#%02X%02X%02X".format(red, green, blue), style = MaterialTheme.typography.titleMedium)
        }
        SliderField("Red", red, 0f..255f, onRed)
        SliderField("Green", green, 0f..255f, onGreen)
        SliderField("Blue", blue, 0f..255f, onBlue)
    }
}

@Composable
private fun ModeControls(mode: LedMode, onModeChange: (LedMode) -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        LedMode.entries.forEach { item ->
            FilterChip(
                selected = mode == item,
                onClick = { onModeChange(item) },
                label = { Text(item.label) },
            )
        }
    }
}

@Composable
private fun SliderField(
    label: String,
    value: Int,
    range: ClosedFloatingPointRange<Float>,
    onValueChange: (Int) -> Unit,
) {
    Column {
        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text(label)
            Text(value.toString())
        }
        Slider(
            value = value.toFloat(),
            onValueChange = { onValueChange(it.toInt()) },
            valueRange = range,
        )
    }
}

@Composable
private fun NumericField(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    OutlinedTextField(
        value = value,
        onValueChange = { text -> onValueChange(text.filter(Char::isDigit).take(5)) },
        label = { Text(label) },
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
        singleLine = true,
        modifier = modifier,
    )
}

@Composable
private fun StatusFooter(
    connectionState: BleConnectionState,
    lastMessage: String?,
) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        tonalElevation = 6.dp,
        shadowElevation = 6.dp,
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .navigationBarsPadding()
                .padding(horizontal = 16.dp, vertical = 8.dp),
        ) {
            Text(text = connectionState.toDisplayText(), style = MaterialTheme.typography.labelLarge)
            Text(
                text = lastMessage ?: "Ready",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
    }
}

@Composable
private fun MessageScreen(title: String, message: String) {
    PageColumn {
        Text(text = title, style = MaterialTheme.typography.headlineMedium)
        Text(text = message, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}

@Composable
private fun PageColumn(content: @Composable ColumnScope.() -> Unit) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .statusBarsPadding()
            .padding(horizontal = 16.dp, vertical = 12.dp)
            .verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(14.dp),
        content = content,
    )
}

private fun requiredBlePermissions(): Array<String> {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
    } else {
        arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
    }
}

private fun hasBlePermissions(context: Context): Boolean {
    return requiredBlePermissions().all { permission ->
        ContextCompat.checkSelfPermission(context, permission) == PackageManager.PERMISSION_GRANTED
    }
}

private fun BleConnectionState.toDisplayText(): String = when (this) {
    BleConnectionState.Idle -> "Idle"
    BleConnectionState.Connecting -> "Connecting"
    BleConnectionState.Connected -> "Discovering services"
    BleConnectionState.Ready -> "Connected"
    BleConnectionState.Disconnecting -> "Disconnecting"
    is BleConnectionState.Disconnected -> reason ?: "Disconnected"
    is BleConnectionState.Error -> message
}

private fun String.toPositiveInt(defaultValue: Int): Int {
    return toIntOrNull()?.takeIf { it > 0 } ?: defaultValue
}

private fun parseColor(color: String): Triple<Int, Int, Int> {
    return try {
        val hex = color.removePrefix("#")
        Triple(
            hex.substring(0, 2).toInt(16),
            hex.substring(2, 4).toInt(16),
            hex.substring(4, 6).toInt(16),
        )
    } catch (_: Exception) {
        Triple(255, 255, 255)
    }
}
