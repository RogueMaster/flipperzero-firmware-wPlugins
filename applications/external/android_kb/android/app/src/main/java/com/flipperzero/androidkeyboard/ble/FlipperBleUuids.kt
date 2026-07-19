package com.flipperzero.androidkeyboard.ble

import java.util.UUID

object FlipperBleUuids {
    val SERIAL_SERVICE: UUID = UUID.fromString("8fe5b3d5-2e7f-4a98-2a48-7acc60fe0000")
    val TX_CHARACTERISTIC: UUID = UUID.fromString("19ed82ae-ed21-4c9d-4145-228e62fe0000")
    val RX_CHARACTERISTIC: UUID = UUID.fromString("19ed82ae-ed21-4c9d-4145-228e61fe0000")
}
