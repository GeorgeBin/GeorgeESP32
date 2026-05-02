package com.george.esp32k.led

import com.george.esp32k.led.protocol.LedCommand
import com.george.esp32k.led.protocol.LedMode
import com.george.esp32k.led.protocol.LedProtocolCodec
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.nio.charset.StandardCharsets

class LedProtocolCodecTest {
    @Test
    fun encodeSetLedUsesEsp32WireShape() {
        val bytes = LedProtocolCodec.encodeSetLed(
            LedCommand(
                seq = 7,
                color = "#ff0000",
                mode = LedMode.SOLID,
                brightness = 80,
                periodMs = 2000,
                onMs = 500,
                offMs = 500,
            )
        )

        val json = bytes.toString(StandardCharsets.UTF_8)
        assertTrue(json.contains("\"seq\":7"))
        assertTrue(json.contains("\"cmd\":\"set_led\""))
        assertTrue(json.contains("\"color\":\"#FF0000\""))
        assertTrue(json.contains("\"mode\":\"solid\""))
        assertTrue(json.contains("\"brightness\":80"))
    }

    @Test
    fun encodeOffOmitsColor() {
        val json = LedProtocolCodec.encodeSetLed(
            LedCommand(
                seq = 8,
                color = "#00FF00",
                mode = LedMode.OFF,
                brightness = 0,
                periodMs = 2000,
                onMs = 500,
                offMs = 500,
            )
        ).toString(StandardCharsets.UTF_8)

        assertTrue(json.contains("\"mode\":\"off\""))
        assertTrue(!json.contains("\"color\""))
    }

    @Test
    fun decodeResponseParsesState() {
        val response = LedProtocolCodec.decodeResponse(
            """
            {"seq":2,"code":0,"msg":"ok","state":{"color":"#112233","mode":"blink","brightness":42,"period_ms":900,"on_ms":100,"off_ms":200,"source":"ble","ble_connected":true,"wifi_connected":true}}
            """.trimIndent().toByteArray(StandardCharsets.UTF_8)
        )

        assertEquals(2, response.seq)
        assertEquals(0, response.code)
        assertEquals("ok", response.msg)
        assertEquals("#112233", response.state?.color)
        assertEquals(LedMode.BLINK, response.state?.mode)
        assertEquals(42, response.state?.brightness)
        assertEquals(true, response.state?.bleConnected)
        assertEquals(true, response.state?.wifiConnected)
    }
}
