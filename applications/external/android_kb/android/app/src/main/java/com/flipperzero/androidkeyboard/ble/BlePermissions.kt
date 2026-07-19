package com.flipperzero.androidkeyboard.ble

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.content.ContextCompat

object BlePermissions {

    /** Permissions required to connect / list bonded BLE devices on this API level. */
    fun required(): Array<String> {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            return emptyArray()
        }
        return arrayOf(
            Manifest.permission.BLUETOOTH_CONNECT,
            Manifest.permission.BLUETOOTH_SCAN,
        )
    }

    /** Subset of [required] needed only to read bonded device names/addresses. */
    fun requiredForBondedDevices(): Array<String> {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            return emptyArray()
        }
        return arrayOf(Manifest.permission.BLUETOOTH_CONNECT)
    }

    fun missing(context: Context, permissions: Array<String> = required()): Array<String> {
        return permissions
            .filter {
                ContextCompat.checkSelfPermission(context, it) != PackageManager.PERMISSION_GRANTED
            }
            .toTypedArray()
    }

    fun hasAll(context: Context, permissions: Array<String> = required()): Boolean {
        return missing(context, permissions).isEmpty()
    }
}
