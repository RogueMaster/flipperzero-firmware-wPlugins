package xyz.i12bp8.tagtinker.android

import android.app.Application
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import java.io.File
import java.util.UUID
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import xyz.i12bp8.tagtinker.android.ble.FlipperBleClient
import xyz.i12bp8.tagtinker.android.data.HistoryStore
import xyz.i12bp8.tagtinker.android.data.isValidBarcode
import xyz.i12bp8.tagtinker.android.data.normalizeBarcode
import xyz.i12bp8.tagtinker.android.data.tagFromBarcode
import xyz.i12bp8.tagtinker.android.model.BlePeripheral
import xyz.i12bp8.tagtinker.android.model.EditorState
import xyz.i12bp8.tagtinker.android.model.PayloadHistoryItem
import xyz.i12bp8.tagtinker.android.model.TagRecord
import xyz.i12bp8.tagtinker.android.model.UploadJob
import xyz.i12bp8.tagtinker.android.model.UploadProgress
import xyz.i12bp8.tagtinker.android.render.ImagePipeline

data class MainUiState(
    val status: String = "Disconnected",
    val connected: Boolean = false,
    val scanning: Boolean = false,
    val devices: List<BlePeripheral> = emptyList(),
    val tag: TagRecord? = null,
    val editor: EditorState = EditorState(),
    val sourceBitmap: Bitmap? = null,
    val previewBitmap: Bitmap? = null,
    val payloadSizeLabel: String = "No image",
    val progress: Float = 0f,
    val logs: List<String> = emptyList(),
    val history: List<PayloadHistoryItem> = emptyList(),
    val manualBarcode: String = "",
)

class MainViewModel(application: Application) : AndroidViewModel(application) {
    private val bleClient = FlipperBleClient(application)
    private val historyStore = HistoryStore(application)
    private val _uiState = MutableStateFlow(MainUiState(history = historyStore.load()))
    val uiState: StateFlow<MainUiState> = _uiState.asStateFlow()
    private var renderJob: Job? = null

    init {
        viewModelScope.launch {
            bleClient.scanResults.collect { results ->
                _uiState.value = _uiState.value.copy(devices = results)
            }
        }
        viewModelScope.launch {
            bleClient.scanStatus.collect { status ->
                if (!status.isNullOrBlank() && !_uiState.value.connected) {
                    _uiState.value = _uiState.value.copy(status = status)
                }
            }
        }
    }

    fun startScan() {
        bleClient.startScan()
        _uiState.value = _uiState.value.copy(scanning = true, status = "Scanning nearby Flippers")
        log("Scanning")
    }

    fun stopScan() {
        bleClient.stopScan()
        _uiState.value = _uiState.value.copy(scanning = false)
    }

    fun setStatus(status: String) {
        _uiState.value = _uiState.value.copy(status = status)
        log(status)
    }

    fun connect(address: String) {
        viewModelScope.launch {
            runCatching {
                bleClient.connect(address)
            }.onSuccess {
                _uiState.value = _uiState.value.copy(
                    connected = true,
                    scanning = false,
                    status = "BLE ready",
                )
                log("BLE ready")
            }.onFailure {
                _uiState.value = _uiState.value.copy(status = it.message ?: "BLE failed")
                log(it.message ?: "BLE failed")
            }
        }
    }

    fun disconnect() {
        viewModelScope.launch {
            bleClient.disconnect()
            _uiState.value = MainUiState(history = historyStore.load())
        }
    }

    fun onManualBarcodeChanged(value: String) {
        _uiState.value = _uiState.value.copy(manualBarcode = value)
    }

    fun acceptBarcode(raw: String) {
        val barcode = normalizeBarcode(raw)
        if (!isValidBarcode(barcode)) {
            log("Invalid barcode")
            return
        }
        _uiState.value = _uiState.value.copy(
            tag = tagFromBarcode(barcode),
            manualBarcode = barcode,
        )
        renderPreview()
        log("Loaded tag $barcode")
    }

    fun loadImage(uri: Uri) {
        viewModelScope.launch {
            val bitmap = withContext(Dispatchers.IO) {
                getApplication<Application>().contentResolver.openInputStream(uri)?.use { input ->
                    BitmapFactory.decodeStream(input)
                }
            } ?: return@launch
            _uiState.value = _uiState.value.copy(sourceBitmap = bitmap)
            renderPreview()
            log("Loaded image")
        }
    }

    fun updateEditor(transform: EditorState) {
        _uiState.value = _uiState.value.copy(editor = transform)
        scheduleRenderPreview()
    }

    fun updateEditor(mutator: (EditorState) -> EditorState) {
        _uiState.value = _uiState.value.copy(editor = mutator(_uiState.value.editor))
        scheduleRenderPreview()
    }

    private fun scheduleRenderPreview() {
        renderJob?.cancel()
        renderJob = viewModelScope.launch {
            delay(72)
            renderPreview()
        }
    }

    private fun renderPreview() {
        val tag = _uiState.value.tag ?: return
        val source = _uiState.value.sourceBitmap ?: run {
            _uiState.value = _uiState.value.copy(previewBitmap = null, payloadSizeLabel = "No image")
            return
        }
        viewModelScope.launch(Dispatchers.Default) {
            val result = ImagePipeline.render(
                source = source,
                width = tag.width,
                height = tag.height,
                accentColor = tag.color,
                editor = _uiState.value.editor,
            )
            withContext(Dispatchers.Main) {
                val cacheFile = File(getApplication<Application>().cacheDir, "latest_payload.bmp")
                cacheFile.writeBytes(result.bmpBytes)
                _uiState.value = _uiState.value.copy(
                    previewBitmap = result.bitmap,
                    payloadSizeLabel = "${result.bmpBytes.size} B",
                )
            }
        }
    }

    fun sendCurrentPayload() {
        val state = _uiState.value
        val tag = state.tag ?: return
        val bmpFile = File(getApplication<Application>().cacheDir, "latest_payload.bmp")
        if (!bmpFile.exists()) {
            log("No payload rendered")
            return
        }
        viewModelScope.launch {
            _uiState.value = _uiState.value.copy(
                progress = 0f,
                status = "Uploading to Flipper",
            )
            runCatching {
                bleClient.upload(
                    UploadJob(
                        jobId = UUID.randomUUID().toString().replace("-", "").take(6).uppercase(),
                        barcode = tag.barcode,
                        width = tag.width,
                        height = tag.height,
                        page = (state.editor.page - 1).coerceAtLeast(0),
                        bmpBytes = bmpFile.readBytes(),
                    ),
                ) { progress ->
                    applyProgress(progress)
                }
            }.onSuccess {
                val stamped = File(
                    getApplication<Application>().filesDir,
                    "payload-${System.currentTimeMillis()}.bmp",
                )
                bmpFile.copyTo(stamped, overwrite = true)
                val updated = listOf(
                    PayloadHistoryItem(
                        id = UUID.randomUUID().toString(),
                        barcode = tag.barcode,
                        name = tag.name,
                        width = tag.width,
                        height = tag.height,
                        color = tag.color,
                        page = state.editor.page,
                        bmpPath = stamped.absolutePath,
                        timestamp = System.currentTimeMillis(),
                    ),
                ) + _uiState.value.history
                historyStore.save(updated)
                _uiState.value = _uiState.value.copy(
                    history = updated.take(24),
                    progress = 1f,
                    status = "Saved on Flipper",
                )
                log("Upload complete")
            }.onFailure {
                _uiState.value = _uiState.value.copy(status = it.message ?: "Upload failed")
                log(it.message ?: "Upload failed")
            }
        }
    }

    fun resendHistory(item: PayloadHistoryItem) {
        acceptBarcode(item.barcode)
        _uiState.value = _uiState.value.copy(
            editor = _uiState.value.editor.copy(page = item.page),
            sourceBitmap = BitmapFactory.decodeFile(item.bmpPath),
        )
        renderPreview()
    }

    private fun applyProgress(progress: UploadProgress) {
        _uiState.value = _uiState.value.copy(
            progress = if (progress.bytesTotal == 0) 0f else progress.bytesDone.toFloat() / progress.bytesTotal,
            status = progress.label,
        )
    }

    private fun log(message: String) {
        _uiState.value = _uiState.value.copy(
            logs = (_uiState.value.logs + "${System.currentTimeMillis()}: $message").takeLast(24),
        )
    }
}
