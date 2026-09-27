package com.oxypulse.esp32tester.ble

object WellueOxyii {
    const val LEAD: Int = 0xA5
    const val CMD_LIVE_B: Int = 0x04
    const val FLAG_REQ: Int = 0x00
    const val FLAG_RSP: Int = 0x01
    const val LIVE_HEADER_SIZE: Int = 24
    const val RX_MAX: Int = 512

    data class LiveMetrics(
        val spo2: Int = 0,
        val hr: Int = 0,
        val battery: Int = 0,
        val motion: Int = 0,
        val ringState: Int = 0,
        val contact: Boolean = false,
        val valid: Boolean = false,
    )

    fun crc8(data: ByteArray, len: Int = data.size): Int {
        var crc = 0
        for (i in 0 until len) {
            crc = crc xor (data[i].toInt() and 0xFF)
            repeat(8) {
                crc = if ((crc and 0x80) != 0) ((crc shl 1) xor 0x07) and 0xFF else (crc shl 1) and 0xFF
            }
        }
        return crc
    }

    fun buildRequest(cmd: Int, seq: Int): ByteArray {
        val out = ByteArray(8)
        out[0] = LEAD.toByte()
        out[1] = cmd.toByte()
        out[2] = (cmd.inv() and 0xFF).toByte()
        out[3] = FLAG_REQ.toByte()
        out[4] = (seq and 0xFF).toByte()
        out[5] = 0
        out[6] = 0
        out[7] = crc8(out, 7).toByte()
        return out
    }

    fun parseLiveBPayload(payload: ByteArray): LiveMetrics? {
        if (payload.size < LIVE_HEADER_SIZE) return null
        val ringState = payload[5].toInt() and 0xFF
        return LiveMetrics(
            ringState = ringState,
            spo2 = payload[6].toInt() and 0xFF,
            motion = payload[7].toInt() and 0xFF,
            hr = payload[8].toInt() and 0xFF,
            battery = payload[13].toInt() and 0xFF,
            contact = ringState != 0,
            valid = true,
        )
    }

    fun feedNotify(rx: ByteArray, rxLen: IntArray, data: ByteArray): LiveMetrics? {
        var len = rxLen[0]
        if (len + data.size > rx.size) {
            len = 0
        }
        if (len + data.size > rx.size) {
            rxLen[0] = 0
            return null
        }
        System.arraycopy(data, 0, rx, len, data.size)
        len += data.size

        var got: LiveMetrics? = null
        while (len >= 8) {
            var start = 0
            while (start < len && (rx[start].toInt() and 0xFF) != LEAD) {
                start += 1
            }
            if (start > 0) {
                System.arraycopy(rx, start, rx, 0, len - start)
                len -= start
            }
            if (len < 8) break
            val payloadLen = (rx[5].toInt() and 0xFF) or ((rx[6].toInt() and 0xFF) shl 8)
            val frameLen = 8 + payloadLen
            if (frameLen > rx.size) {
                System.arraycopy(rx, 1, rx, 0, len - 1)
                len -= 1
                continue
            }
            if (len < frameLen) break
            val crc = crc8(rx, frameLen - 1)
            val cmd = rx[1].toInt() and 0xFF
            val inv = rx[2].toInt() and 0xFF
            if (crc != (rx[frameLen - 1].toInt() and 0xFF) || inv != (cmd.inv() and 0xFF)) {
                System.arraycopy(rx, 1, rx, 0, len - 1)
                len -= 1
                continue
            }
            val flag = rx[3].toInt() and 0xFF
            if (cmd == CMD_LIVE_B && flag == FLAG_RSP) {
                val payload = rx.copyOfRange(7, 7 + payloadLen)
                val parsed = parseLiveBPayload(payload)
                if (parsed != null) got = parsed
            }
            val remain = len - frameLen
            if (remain > 0) System.arraycopy(rx, frameLen, rx, 0, remain)
            len = remain
        }
        rxLen[0] = len
        return got
    }
}
