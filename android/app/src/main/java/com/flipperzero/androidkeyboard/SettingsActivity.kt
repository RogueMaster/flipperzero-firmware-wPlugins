package com.flipperzero.androidkeyboard

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.os.Build
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.View
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import com.flipperzero.androidkeyboard.ble.BlePermissions
import com.flipperzero.androidkeyboard.ble.FlipperBleClient
import com.flipperzero.androidkeyboard.databinding.ActivitySettingsBinding
import com.flipperzero.androidkeyboard.hid.DirectHidClient
import com.flipperzero.androidkeyboard.keyboard.KeyboardLayoutLoader
import com.flipperzero.androidkeyboard.keyboard.LanguageInfo
import com.flipperzero.androidkeyboard.keyboard.TemplateInfo
import com.flipperzero.androidkeyboard.keyboard.composedLayoutId
import com.flipperzero.androidkeyboard.prefs.AppPreferences
import java.util.Locale

class SettingsActivity : AppCompatActivity() {

    private lateinit var binding: ActivitySettingsBinding
    private lateinit var prefs: AppPreferences
    private var devices: List<BluetoothDevice> = emptyList()
    private var templates: List<TemplateInfo> = emptyList()
    private var allLanguages: List<LanguageInfo> = emptyList()
    private var selectedDeviceIndex: Int = -1
    private val enabledLanguageIds = linkedSetOf<String>()
    private var languageAdapter: LanguagePickAdapter? = null

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
        templates = KeyboardLayoutLoader.loadTemplates(this)
        allLanguages = KeyboardLayoutLoader.loadSelectableLanguages(this)
        enabledLanguageIds.clear()
        enabledLanguageIds.addAll(prefs.enabledLanguageIds())
        bindOutputMode()
        binding.editHidName.setText(prefs.hidDeviceName)
        bindTemplates()
        bindLanguages()
        binding.checkDualLabels.isChecked = prefs.showDualLanguageLabels
        updateDeviceSectionLabels()
        updateCurrentTargetLabel()
        updateHidNameVisibility()
        updateLanguageSectionForTemplate()

        binding.groupOutputMode.setOnCheckedChangeListener { _, _ ->
            updateDeviceSectionLabels()
            updateHidNameVisibility()
            refreshDevices()
            updateCurrentTargetLabel()
        }
        binding.groupTemplates.setOnCheckedChangeListener { _, _ ->
            updateLanguageSectionForTemplate()
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

    private fun selectedTemplate(): TemplateInfo? {
        val checkedId = binding.groupTemplates.checkedRadioButtonId
        return templates.getOrNull(checkedId)
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

    private fun bindTemplates() {
        binding.groupTemplates.removeAllViews()
        val current = prefs.templateId
        templates.forEachIndexed { index, info ->
            val button = RadioButton(this).apply {
                id = index
                text = info.title
                textSize = 15f
            }
            binding.groupTemplates.addView(button)
            if (info.id == current) {
                button.isChecked = true
            }
        }
        if (binding.groupTemplates.checkedRadioButtonId < 0 && templates.isNotEmpty()) {
            binding.groupTemplates.check(0)
        }
    }

    private fun bindLanguages() {
        if (allLanguages.isEmpty()) {
            Toast.makeText(this, R.string.settings_languages_none, Toast.LENGTH_LONG).show()
            return
        }
        val userDir = KeyboardLayoutLoader.userLanguagesDir(this).absolutePath
        binding.txtLanguagesHint.text = buildString {
            append(getString(R.string.settings_languages_hint))
            append('\n')
            append(getString(R.string.settings_languages_custom_dir, userDir))
        }

        val adapter = LanguagePickAdapter(
            enabledIds = enabledLanguageIds,
            onEnabledChanged = {
                // Keep enabled languages pinned to the top of the list.
                applyLanguageFilter(binding.editLanguageSearch.text?.toString().orEmpty())
            },
        )
        languageAdapter = adapter
        binding.listLanguages.layoutManager = LinearLayoutManager(this)
        binding.listLanguages.adapter = adapter
        binding.editLanguageSearch.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) = Unit
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) = Unit
            override fun afterTextChanged(s: Editable?) {
                applyLanguageFilter(s?.toString().orEmpty())
            }
        })
        applyLanguageFilter(binding.editLanguageSearch.text?.toString().orEmpty())
    }

    private fun applyLanguageFilter(query: String) {
        val q = query.trim().lowercase(Locale.ROOT)
        val filtered = if (q.isEmpty()) {
            allLanguages
        } else {
            allLanguages.filter { info ->
                info.title.lowercase(Locale.ROOT).contains(q) ||
                    info.id.contains(q) ||
                    info.locales.any { it.lowercase(Locale.ROOT).contains(q) }
            }
        }
        languageAdapter?.submit(sortLanguagesForPicker(filtered))
        updateLanguagesCount(filtered.size)
    }

    /** Enabled first (prefs order), then label packs, then title. */
    private fun sortLanguagesForPicker(list: List<LanguageInfo>): List<LanguageInfo> {
        val enabledOrder = enabledLanguageIds.mapIndexed { index, id -> id to index }.toMap()
        return list.sortedWith(
            compareBy<LanguageInfo> { it.id !in enabledLanguageIds }
                .thenBy { enabledOrder[it.id] ?: Int.MAX_VALUE }
                .thenBy { !it.hasLabelPack }
                .thenBy { it.title.lowercase(Locale.ROOT) },
        )
    }

    private fun updateLanguagesCount(filteredSize: Int = languageAdapter?.itemCount ?: 0) {
        binding.txtLanguagesCount.text = getString(
            R.string.settings_languages_count,
            filteredSize,
            allLanguages.size,
            enabledLanguageIds.size,
        )
    }

    private fun updateLanguageSectionForTemplate() {
        val template = selectedTemplate() ?: return
        val usesLang = KeyboardLayoutLoader.templateUsesLanguages(this, template)
        binding.listLanguages.visibility = if (usesLang) View.VISIBLE else View.GONE
        binding.editLanguageSearch.visibility = if (usesLang) View.VISIBLE else View.GONE
        binding.txtLanguagesCount.visibility = if (usesLang) View.VISIBLE else View.GONE
        binding.checkDualLabels.visibility = if (usesLang) View.VISIBLE else View.GONE
        binding.txtDualLabelsHint.visibility = if (usesLang) View.VISIBLE else View.GONE
        if (!usesLang) {
            binding.txtLanguagesHint.setText(R.string.settings_languages_not_used)
        }
        binding.editLanguageSearch.isEnabled = usesLang
        binding.checkDualLabels.isEnabled = usesLang
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
        val template = selectedTemplate()
        if (template == null) {
            Toast.makeText(this, R.string.settings_select_device, Toast.LENGTH_SHORT).show()
            return
        }
        prefs.templateId = template.id

        val usesLang = KeyboardLayoutLoader.templateUsesLanguages(this, template)
        val enabledLangIds = if (usesLang) {
            enabledLanguageIds.toList()
        } else {
            emptyList()
        }
        if (usesLang && enabledLangIds.isEmpty()) {
            Toast.makeText(this, R.string.settings_languages_required, Toast.LENGTH_SHORT).show()
            return
        }
        prefs.setEnabledLanguageIds(enabledLangIds)
        prefs.showDualLanguageLabels = usesLang && binding.checkDualLabels.isChecked

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

        val layouts = KeyboardLayoutLoader.buildEnabledLayouts(this, template.id, enabledLangIds)
        val current = prefs.currentLayoutId
        if (current == null || layouts.none { it.id == current }) {
            prefs.currentLayoutId = layouts.firstOrNull()?.id
                ?: composedLayoutId(template.id, enabledLangIds.firstOrNull())
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
