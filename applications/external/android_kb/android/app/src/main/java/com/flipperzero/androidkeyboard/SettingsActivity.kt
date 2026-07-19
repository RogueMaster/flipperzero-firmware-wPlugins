package com.flipperzero.androidkeyboard

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.os.Build
import android.os.Bundle
import android.view.View
import android.widget.CheckBox
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import com.flipperzero.androidkeyboard.ble.BlePermissions
import com.flipperzero.androidkeyboard.ble.FlipperBleClient
import com.flipperzero.androidkeyboard.databinding.ActivitySettingsBinding
import com.flipperzero.androidkeyboard.hid.DirectHidClient
import com.flipperzero.androidkeyboard.keyboard.KeyboardLayoutLoader
import com.flipperzero.androidkeyboard.keyboard.LayoutInfo
import com.flipperzero.androidkeyboard.prefs.AppPreferences

class SettingsActivity : AppCompatActivity() {

    private lateinit var binding: ActivitySettingsBinding
    private lateinit var prefs: AppPreferences
    private var devices: List<BluetoothDevice> = emptyList()
    private var catalog: List<LayoutInfo> = emptyList()
    private var selectedDeviceIndex: Int = -1
    private val layoutChecks = mutableListOf<Pair<LayoutInfo, CheckBox>>()

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions(),
    ) { results ->
        if (results.values.all { it }) {
            refreshDevices()
        } else {
            Toast.makeText(this, R.string.ble_permission_required, Toast.LENGTH_SHORT).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivitySettingsBinding.inflate(layoutInflater)
        setContentView(binding.root)

        prefs = AppPreferences(this)
        catalog = KeyboardLayoutLoader.loadCatalog(this)
        bindOutputMode()
        binding.editHidName.setText(prefs.hidDeviceName)
        bindLayoutChecks()
        updateDeviceSectionLabels()
        updateCurrentTargetLabel()
        updateHidNameVisibility()

        binding.groupOutputMode.setOnCheckedChangeListener { _, _ ->
            updateDeviceSectionLabels()
            updateHidNameVisibility()
            refreshDevices()
            updateCurrentTargetLabel()
        }
        binding.btnRefresh.setOnClickListener { ensurePermissionsAndRefresh() }
        binding.btnSave.setOnClickListener { saveAll() }

        ensurePermissionsAndRefresh()
    }

    private fun updateHidNameVisibility() {
        val direct = selectedOutputMode() == OutputMode.DIRECT_BT
        binding.sectionHidName.visibility = if (direct) View.VISIBLE else View.GONE
    }

    private fun bindOutputMode() {
        when (prefs.outputMode) {
            OutputMode.DIRECT_BT -> binding.radioOutputDirect.isChecked = true
            OutputMode.FLIPPER -> binding.radioOutputFlipper.isChecked = true
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            binding.radioOutputDirect.isEnabled = false
            binding.radioOutputDirect.text =
                "${getString(R.string.settings_output_direct)} (${getString(R.string.settings_direct_needs_api28)})"
        }
    }

    private fun selectedOutputMode(): OutputMode {
        return if (binding.radioOutputDirect.isChecked) OutputMode.DIRECT_BT else OutputMode.FLIPPER
    }

    private fun updateDeviceSectionLabels() {
        if (selectedOutputMode() == OutputMode.DIRECT_BT) {
            binding.txtDeviceSectionTitle.setText(R.string.settings_host_list)
            binding.txtDeviceSectionHint.setText(R.string.settings_host_hint)
        } else {
            binding.txtDeviceSectionTitle.setText(R.string.settings_paired_list)
            binding.txtDeviceSectionHint.setText(R.string.settings_mac_hint)
        }
    }

    private fun bindLayoutChecks() {
        binding.listLayouts.removeAllViews()
        layoutChecks.clear()
        val enabled = prefs.enabledLayoutIds(catalog).toSet()
        catalog.forEach { info ->
            val box = CheckBox(this).apply {
                text = info.title
                isChecked = info.id in enabled
                textSize = 16f
            }
            binding.listLayouts.addView(box)
            layoutChecks += info to box
        }
    }

    private fun ensurePermissionsAndRefresh() {
        val needed = BlePermissions.missing(this, BlePermissions.requiredForBondedDevices())
        if (needed.isEmpty()) {
            refreshDevices()
        } else {
            permissionLauncher.launch(needed)
        }
    }

    @SuppressLint("MissingPermission")
    private fun refreshDevices() {
        devices = if (selectedOutputMode() == OutputMode.DIRECT_BT) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                DirectHidClient.bondedHosts(this)
            } else {
                emptyList()
            }
        } else {
            FlipperBleClient.bondedFlipperDevices(this)
        }
        binding.listDevices.removeAllViews()
        selectedDeviceIndex = -1

        if (devices.isEmpty()) {
            val msg = if (selectedOutputMode() == OutputMode.DIRECT_BT) {
                R.string.settings_no_hosts
            } else {
                R.string.settings_no_devices
            }
            Toast.makeText(this, msg, Toast.LENGTH_LONG).show()
            return
        }

        val group = RadioGroup(this).apply {
            orientation = RadioGroup.VERTICAL
        }
        val saved = if (selectedOutputMode() == OutputMode.DIRECT_BT) {
            prefs.hostMac
        } else {
            prefs.flipperMac
        }
        devices.forEachIndexed { index, device ->
            val name = device.name ?: getString(R.string.settings_unknown_device)
            val button = RadioButton(this).apply {
                id = index
                text = "$name\n${device.address}"
                textSize = 15f
            }
            group.addView(button)
            if (saved != null && device.address.equals(saved, ignoreCase = true)) {
                selectedDeviceIndex = index
                button.isChecked = true
            }
        }
        group.setOnCheckedChangeListener { _, checkedId ->
            selectedDeviceIndex = checkedId
        }
        binding.listDevices.addView(group)
    }

    @SuppressLint("MissingPermission")
    private fun saveAll() {
        val enabledIds = layoutChecks
            .filter { (_, box) -> box.isChecked }
            .map { (info, _) -> info.id }

        if (enabledIds.isEmpty()) {
            Toast.makeText(this, R.string.settings_layouts_required, Toast.LENGTH_SHORT).show()
            return
        }
        prefs.setEnabledLayoutIds(enabledIds)

        val mode = selectedOutputMode()
        prefs.outputMode = mode
        if (mode == OutputMode.DIRECT_BT) {
            prefs.hidDeviceName = binding.editHidName.text?.toString().orEmpty()
            binding.editHidName.setText(prefs.hidDeviceName)
        }

        if (selectedDeviceIndex in devices.indices) {
            val address = devices[selectedDeviceIndex].address
            if (mode == OutputMode.DIRECT_BT) {
                prefs.hostMac = address
            } else {
                prefs.flipperMac = address
            }
            updateCurrentTargetLabel()
        } else if (mode == OutputMode.FLIPPER && prefs.flipperMac.isNullOrBlank()) {
            Toast.makeText(this, R.string.settings_select_device, Toast.LENGTH_SHORT).show()
            return
        }
        // Direct BT: host optional — Connect will wait for pairing if unset.

        if (prefs.currentLayoutId !in enabledIds) {
            prefs.currentLayoutId = enabledIds.first()
        }

        Toast.makeText(this, R.string.settings_saved, Toast.LENGTH_SHORT).show()
        finish()
    }

    private fun updateCurrentTargetLabel() {
        val mode = selectedOutputMode()
        val mac = if (mode == OutputMode.DIRECT_BT) prefs.hostMac else prefs.flipperMac
        binding.txtCurrentMac.text = when {
            mac.isNullOrBlank() && mode == OutputMode.DIRECT_BT ->
                getString(R.string.settings_host_none)
            mac.isNullOrBlank() ->
                getString(R.string.settings_mac_none)
            mode == OutputMode.DIRECT_BT ->
                getString(R.string.settings_host_current, mac)
            else ->
                getString(R.string.settings_mac_current, mac)
        }
        binding.txtCurrentMac.visibility = View.VISIBLE
    }
}
