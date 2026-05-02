package com.george.esp32k.led.protocol

import org.json.JSONObject
import java.nio.charset.StandardCharsets

object LedProtocolCodec {
    fun encodeSetLed(command: LedCommand): ByteArray {
        val root = JSONObject()
            .put("seq", command.seq)
            .put("cmd", "set_led")
            .put("mode", command.mode.wireName)
            .put("brightness", command.brightness.coerceIn(0, 100))
            .put("period_ms", command.periodMs.coerceAtLeast(1))
            .put("on_ms", command.onMs.coerceAtLeast(1))
            .put("off_ms", command.offMs.coerceAtLeast(1))

        if (command.mode != LedMode.OFF) {
            root.put("color", normalizeColor(command.color))
        }

        return root.toString().toByteArray(StandardCharsets.UTF_8)
    }

    fun encodeGetState(seq: Int): ByteArray {
        return JSONObject()
            .put("seq", seq)
            .put("cmd", "get_state")
            .toString()
            .toByteArray(StandardCharsets.UTF_8)
    }

    fun decodeResponse(bytes: ByteArray): LedResponse {
        val root = JSONObject(bytes.toString(StandardCharsets.UTF_8))
        val stateJson = root.optJSONObject("state")
        return LedResponse(
            seq = root.optInt("seq", 0),
            code = root.optInt("code", -1),
            msg = root.optString("msg", "unknown"),
            state = stateJson?.let(::decodeState),
        )
    }

    fun normalizeColor(color: String): String {
        val text = color.trim()
        val hex = if (text.startsWith("#")) text.drop(1) else text
        require(Regex("[0-9a-fA-F]{6}").matches(hex)) { "Invalid color: $color" }
        return "#${hex.uppercase()}"
    }

    private fun decodeState(json: JSONObject): LedState {
        return LedState(
            color = json.optString("color", "#FFFFFF"),
            mode = LedMode.fromWireName(json.optString("mode", "solid")),
            brightness = json.optInt("brightness", 100),
            periodMs = json.optInt("period_ms", 2000),
            onMs = json.optInt("on_ms", 500),
            offMs = json.optInt("off_ms", 500),
            source = json.optString("source", "none"),
            bleConnected = json.optBoolean("ble_connected", false),
            wifiConnected = json.optBoolean("wifi_connected", false),
        )
    }
}
