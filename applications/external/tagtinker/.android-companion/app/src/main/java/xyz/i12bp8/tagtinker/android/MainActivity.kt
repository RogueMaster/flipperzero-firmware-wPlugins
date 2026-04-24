package xyz.i12bp8.tagtinker.android

import android.Manifest
import android.app.Activity
import android.bluetooth.BluetoothDevice
import android.util.Log
import android.companion.AssociationRequest
import android.companion.BluetoothLeDeviceFilter
import android.companion.CompanionDeviceManager
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.os.Build
import android.os.Bundle
import android.util.Size
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.IntentSenderRequest
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.camera.core.CameraSelector
import androidx.camera.core.ExperimentalGetImage
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.ImageProxy
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.camera.view.PreviewView
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectTransformGestures
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Bluetooth
import androidx.compose.material.icons.rounded.Collections
import androidx.compose.material.icons.rounded.History
import androidx.compose.material.icons.rounded.Image
import androidx.compose.material.icons.rounded.Send
import androidx.compose.material.icons.rounded.Sync
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalLifecycleOwner
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.content.ContextCompat
import java.util.regex.Pattern
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.google.mlkit.vision.barcode.BarcodeScannerOptions
import com.google.mlkit.vision.barcode.BarcodeScanning
import com.google.mlkit.vision.barcode.common.Barcode
import com.google.mlkit.vision.common.InputImage
import java.text.DateFormat
import java.util.Date
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.OptIn
import xyz.i12bp8.tagtinker.android.model.BlePeripheral
import xyz.i12bp8.tagtinker.android.model.DitherMode
import xyz.i12bp8.tagtinker.android.model.EditorState
import xyz.i12bp8.tagtinker.android.model.FitMode
import xyz.i12bp8.tagtinker.android.model.PayloadHistoryItem

class MainActivity : ComponentActivity() {
    private val viewModel by viewModels<MainViewModel>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            TagTinkerTheme {
                AppRoot(viewModel = viewModel)
            }
        }
    }
}

@Composable
private fun TagTinkerTheme(content: @Composable () -> Unit) {
    val colors = darkColorScheme(
        primary = Color(0xFF5D8DFF),
        onPrimary = Color(0xFF04101F),
        secondary = Color(0xFF8B78FF),
        background = Color(0xFF040507),
        surface = Color(0xFF0B0E13),
        surfaceVariant = Color(0xFF121724),
        onSurface = Color(0xFFF4F7FB),
        onSurfaceVariant = Color(0xFFA2AEC3),
        outline = Color(0xFF22304A),
    )
    MaterialTheme(colorScheme = colors, content = content)
}

@Composable
private fun AppRoot(viewModel: MainViewModel) {
    val uiState by viewModel.uiState.collectAsStateWithLifecycle()
    val context = LocalContext.current
    val activity = context as? ComponentActivity

    var currentTab by rememberSaveable { mutableStateOf("studio") }
    var disclaimerAccepted by rememberSaveable { mutableStateOf(false) }

    val companionDeviceLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.StartIntentSenderForResult(),
    ) { result ->
        val data = result.data
        Log.d("TagTinkerBle", "Companion picker result: ${result.resultCode}, data=$data")
        data?.extras?.keySet()?.forEach { key ->
            Log.d("TagTinkerBle", "Extra: $key = ${data.extras?.get(key)}")
        }

        val device = extractCompanionBluetoothDevice(data)
        if(result.resultCode == Activity.RESULT_OK && device?.address != null) {
            viewModel.setStatus("Connecting to ${device.name ?: "device"}")
            viewModel.connect(device.address)
        } else {
            viewModel.setStatus("No device selected (code=${result.resultCode})")
        }
    }

    val bluetoothPermissionLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions(),
    ) { result ->
        val granted = result.values.all { it }
        if (granted) {
            if(activity != null) {
                launchCompanionPicker(activity, viewModel, companionDeviceLauncher)
            }
        }
    }

    val cameraPermissionLauncher =
        rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { }

    val imagePicker = rememberLauncherForActivityResult(
        ActivityResultContracts.PickVisualMedia(),
    ) { uri ->
        uri?.let(viewModel::loadImage)
    }

    Surface(color = MaterialTheme.colorScheme.background) {
        if (!disclaimerAccepted) {
            DisclaimerGate(
                onAccept = { disclaimerAccepted = true },
            )
            return@Surface
        }

        LaunchedEffect(disclaimerAccepted, currentTab, activity) {
            if(disclaimerAccepted && currentTab == "studio" && activity != null && !hasCameraPermission(activity)) {
                cameraPermissionLauncher.launch(Manifest.permission.CAMERA)
            }
        }

        Scaffold(
            modifier = Modifier.fillMaxSize(),
            topBar = {
                TopBar(
                    status = uiState.status,
                    connected = uiState.connected,
                    onConnect = {
                        if (activity != null && hasBluetoothPermissions(activity)) {
                            launchCompanionPicker(activity, viewModel, companionDeviceLauncher)
                        } else {
                            bluetoothPermissionLauncher.launch(bluetoothPermissions())
                        }
                    },
                    onDisconnect = viewModel::disconnect,
                )
            },
            bottomBar = {
                NavigationBar(
                    containerColor = Color(0xFF090B10),
                    tonalElevation = 0.dp,
                    modifier = Modifier.navigationBarsPadding(),
                ) {
                    NavigationBarItem(
                        selected = currentTab == "studio",
                        onClick = { currentTab = "studio" },
                        icon = { Icon(Icons.Rounded.Collections, contentDescription = null) },
                        label = { Text("Studio") },
                    )
                    NavigationBarItem(
                        selected = currentTab == "recents",
                        onClick = { currentTab = "recents" },
                        icon = { Icon(Icons.Rounded.History, contentDescription = null) },
                        label = { Text("Recents") },
                    )
                }
            },
            containerColor = MaterialTheme.colorScheme.background,
        ) { padding ->
            when (currentTab) {
                "recents" -> RecentsScreen(
                    items = uiState.history,
                    onUse = {
                        viewModel.resendHistory(it)
                        currentTab = "studio"
                    },
                    modifier = Modifier.padding(padding),
                )

                else -> StudioScreen(
                    uiState = uiState,
                    cameraReady = activity != null && hasCameraPermission(activity),
                    onBarcode = viewModel::acceptBarcode,
                    onPickImage = {
                        imagePicker.launch(PickVisualMediaRequest(ActivityResultContracts.PickVisualMedia.ImageOnly))
                    },
                    onEditorChange = viewModel::updateEditor,
                    onSend = viewModel::sendCurrentPayload,
                    modifier = Modifier.padding(padding),
                )
            }
        }

    }
}

@Composable
private fun DisclaimerGate(onAccept: () -> Unit) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xFF040507))
            .padding(24.dp),
        contentAlignment = Alignment.Center,
    ) {
        Card(
            shape = RoundedCornerShape(28.dp),
            colors = cardColors(),
            modifier = Modifier.fillMaxWidth(),
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(14.dp),
                modifier = Modifier.padding(22.dp),
            ) {
                Text("TagTinker", style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.SemiBold)
                Text(
                    "Use this for badges, artwork, lab experiments, IoT reverse engineering, and security curiosity on hardware you own or are allowed to test.",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Text(
                    "Do not use it for unauthorized retail, fraud, or illegal activity.",
                    color = Color(0xFFFF8B94),
                )
                Button(
                    onClick = onAccept,
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(18.dp),
                ) {
                    Text("I understand")
                }
            }
        }
    }
}

@Composable
private fun TopBar(
    status: String,
    connected: Boolean,
    onConnect: () -> Unit,
    onDisconnect: () -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .statusBarsPadding()
            .padding(horizontal = 18.dp, vertical = 10.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column {
            Text("TagTinker", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.SemiBold)
            Text(
                if (connected) status else "Phone Sync over BLE",
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                style = MaterialTheme.typography.bodySmall,
            )
        }
        if (connected) {
            OutlinedButton(onClick = onDisconnect, shape = RoundedCornerShape(16.dp)) {
                Text("Disconnect")
            }
        } else {
            Button(onClick = onConnect, shape = RoundedCornerShape(16.dp)) {
                Icon(Icons.Rounded.Bluetooth, contentDescription = null)
                Spacer(Modifier.size(8.dp))
                Text("Connect")
            }
        }
    }
}

@Composable
private fun StudioScreen(
    uiState: MainUiState,
    cameraReady: Boolean,
    onBarcode: (String) -> Unit,
    onPickImage: () -> Unit,
    onEditorChange: (EditorState) -> Unit,
    onSend: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val canSend = uiState.connected && uiState.tag != null && uiState.previewBitmap != null
    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 16.dp, vertical = 8.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        PanelCard {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Text("Tag", style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.SemiBold)
                CompactBarcodeScanner(
                    enabled = cameraReady,
                    onBarcode = onBarcode,
                )
                uiState.tag?.let { tag ->
                    Text(
                        "${tag.name}  ${tag.width}x${tag.height}  ${tag.color}",
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                } ?: Text(
                    if(cameraReady) "Point the camera at the tag barcode" else "Allow camera access to scan a tag",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    style = MaterialTheme.typography.bodySmall,
                )
            }
        }

        PanelCard {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Column {
                        Text("Image", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
                        Text(uiState.payloadSizeLabel, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                    OutlinedButton(onClick = onPickImage, shape = RoundedCornerShape(18.dp)) {
                        Icon(Icons.Rounded.Image, contentDescription = null)
                        Spacer(Modifier.size(8.dp))
                        Text("Pick")
                    }
                }
                PreviewPane(
                    bitmap = uiState.previewBitmap,
                    tagWidth = uiState.tag?.width,
                    tagHeight = uiState.tag?.height,
                    editor = uiState.editor,
                    onGesture = onEditorChange,
                )
            }
        }

        if (uiState.tag != null && uiState.sourceBitmap != null) {
            EditorPanel(editor = uiState.editor, allowColor = uiState.tag.color != "mono", onChange = onEditorChange)
        }

        PanelCard {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Text("Transfer", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
                LinearProgressIndicator(
                    progress = { uiState.progress },
                    modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(99.dp)),
                    trackColor = MaterialTheme.colorScheme.surfaceVariant,
                )
                Text(uiState.status, color = MaterialTheme.colorScheme.onSurfaceVariant)
                Button(
                    onClick = onSend,
                    enabled = canSend,
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(18.dp),
                ) {
                    Icon(Icons.Rounded.Send, contentDescription = null)
                    Spacer(Modifier.size(8.dp))
                    Text("Send to Flipper")
                }
            }
        }
    }
}

@Composable
private fun PreviewPane(
    bitmap: Bitmap?,
    tagWidth: Int?,
    tagHeight: Int?,
    editor: EditorState,
    onGesture: (EditorState) -> Unit,
) {
    val aspect = remember(tagWidth, tagHeight) {
        if (tagWidth != null && tagHeight != null && tagHeight > 0) tagWidth.toFloat() / tagHeight else 296f / 128f
    }
    var viewportSize by remember { mutableStateOf(IntSize.Zero) }
    var gestureBase by remember(bitmap) { mutableStateOf(editor) }
    var gestureScale by remember(bitmap) { mutableFloatStateOf(1f) }
    var gestureOffset by remember(bitmap) { mutableStateOf(Offset.Zero) }
    val latestGestureBase by rememberUpdatedState(gestureBase)

    LaunchedEffect(bitmap) {
        gestureBase = editor
        gestureScale = 1f
        gestureOffset = Offset.Zero
    }

    LaunchedEffect(gestureScale, gestureOffset, viewportSize, bitmap) {
        if(bitmap == null || viewportSize.width == 0 || viewportSize.height == 0) return@LaunchedEffect
        if(gestureScale == 1f && gestureOffset == Offset.Zero) return@LaunchedEffect
        onGesture(
            latestGestureBase.copy(
                scale = (latestGestureBase.scale * gestureScale).coerceIn(0.6f, 2.6f),
                offsetX = (latestGestureBase.offsetX + gestureOffset.x / (viewportSize.width * 0.5f)).coerceIn(-1.25f, 1.25f),
                offsetY = (latestGestureBase.offsetY + gestureOffset.y / (viewportSize.height * 0.5f)).coerceIn(-1.25f, 1.25f),
            ),
        )
    }

    Box(
        modifier = Modifier
            .fillMaxWidth()
            .aspectRatio(aspect)
            .onSizeChanged { viewportSize = it }
            .clip(RoundedCornerShape(24.dp))
            .background(Color.White)
            .border(1.dp, Color(0xFF1C2434), RoundedCornerShape(24.dp))
            .pointerInput(bitmap) {
                detectTransformGestures { _, pan, zoom, _ ->
                    if(bitmap == null) return@detectTransformGestures
                    gestureScale = (gestureScale * zoom).coerceIn(0.6f / gestureBase.scale, 2.6f / gestureBase.scale)
                    gestureOffset = Offset(
                        x = gestureOffset.x + pan.x,
                        y = gestureOffset.y + pan.y,
                    )
                }
            },
        contentAlignment = Alignment.Center,
    ) {
        if (bitmap == null) {
            Canvas(modifier = Modifier.fillMaxSize()) {
                drawRect(color = Color(0xFFF7F9FC))
                drawCircle(color = Color(0xFFCAD2E2), radius = size.minDimension * 0.12f, style = Stroke(width = 4f))
            }
            Text("Waiting for image", color = Color(0xFF667085))
        } else {
            Image(
                bitmap = bitmap.asImageBitmap(),
                contentDescription = null,
                contentScale = when(editor.fitMode) {
                    FitMode.Fill -> ContentScale.Crop
                    FitMode.Stretch -> ContentScale.FillBounds
                    FitMode.Fit -> ContentScale.Fit
                },
                modifier = Modifier
                    .fillMaxSize()
                    .graphicsLayer {
                        scaleX = gestureScale
                        scaleY = gestureScale
                        translationX = gestureOffset.x
                        translationY = gestureOffset.y
                    },
            )
            Text(
                "Pinch and drag",
                color = Color(0xFF667085),
                style = MaterialTheme.typography.labelSmall,
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .padding(bottom = 10.dp)
                    .background(Color(0xE8F8FAFF), RoundedCornerShape(99.dp))
                    .padding(horizontal = 10.dp, vertical = 4.dp),
            )
        }
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun EditorPanel(editor: EditorState, allowColor: Boolean, onChange: (EditorState) -> Unit) {
    PanelCard {
        Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Text("Adjust", style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.SemiBold)
            FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                FitMode.entries.forEach { mode ->
                    FilterChip(
                        selected = editor.fitMode == mode,
                        onClick = { onChange(editor.copy(fitMode = mode)) },
                        label = { Text(mode.name, style = MaterialTheme.typography.labelMedium) },
                        modifier = Modifier.height(32.dp),
                    )
                }
            }
            FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                DitherMode.entries.forEach { mode ->
                    FilterChip(
                        selected = editor.ditherMode == mode,
                        onClick = { onChange(editor.copy(ditherMode = mode)) },
                        label = { Text(mode.name, style = MaterialTheme.typography.labelMedium) },
                        modifier = Modifier.height(32.dp),
                    )
                }
            }
            LabeledSlider("Brightness", editor.brightness.toFloat(), -80f..80f) { onChange(editor.copy(brightness = it.toInt())) }
            LabeledSlider("Contrast", editor.contrast.toFloat(), -60f..90f) { onChange(editor.copy(contrast = it.toInt())) }
            LabeledSlider("Detail", editor.detail.toFloat(), 10f..90f) { onChange(editor.copy(detail = it.toInt())) }
            Column(modifier = Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Text("Page", style = MaterialTheme.typography.labelLarge)
                FlowRow(
                    horizontalArrangement = Arrangement.spacedBy(6.dp),
                    verticalArrangement = Arrangement.spacedBy(6.dp),
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    (1..8).forEach { page ->
                        FilterChip(
                            selected = editor.page == page,
                            onClick = { onChange(editor.copy(page = page)) },
                            label = { Text(page.toString(), style = MaterialTheme.typography.labelMedium) },
                            modifier = Modifier.height(32.dp),
                        )
                    }
                }
            }
            if (allowColor) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text("Use color", style = MaterialTheme.typography.labelLarge)
                    Switch(
                        checked = editor.useColor,
                        onCheckedChange = { onChange(editor.copy(useColor = it)) },
                    )
                }
            }
        }
    }
}

@Composable
private fun LabeledSlider(label: String, value: Float, range: ClosedFloatingPointRange<Float>, onChange: (Float) -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text(label, style = MaterialTheme.typography.labelLarge)
            Text(
                when {
                    range.endInclusive - range.start <= 3f -> String.format("%.2f", value)
                    else -> value.toInt().toString()
                },
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                style = MaterialTheme.typography.labelMedium,
            )
        }
        Slider(
            value = value,
            onValueChange = onChange,
            valueRange = range,
            modifier = Modifier.height(26.dp),
        )
    }
}

@Composable
private fun RecentsScreen(items: List<PayloadHistoryItem>, onUse: (PayloadHistoryItem) -> Unit, modifier: Modifier = Modifier) {
    if (items.isEmpty()) {
        Box(modifier = modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            Text("No recent payloads yet", color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        return
    }
    LazyColumn(
        modifier = modifier.fillMaxSize(),
        verticalArrangement = Arrangement.spacedBy(10.dp),
        contentPadding = androidx.compose.foundation.layout.PaddingValues(16.dp),
    ) {
        items(items, key = { it.id }) { item ->
            PanelCard {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clickable { onUse(item) },
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        Text(item.name, fontWeight = FontWeight.SemiBold)
                        Text(item.barcode, color = MaterialTheme.colorScheme.onSurfaceVariant)
                        Text(
                            "${item.width}x${item.height}  page ${item.page}  ${DateFormat.getDateTimeInstance().format(Date(item.timestamp))}",
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            style = MaterialTheme.typography.bodySmall,
                        )
                    }
                    Button(onClick = { onUse(item) }, shape = RoundedCornerShape(16.dp)) {
                        Icon(Icons.Rounded.Sync, contentDescription = null)
                        Spacer(Modifier.size(6.dp))
                        Text("Use")
                    }
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun DeviceSheet(
    devices: List<BlePeripheral>,
    scanning: Boolean,
    onDismiss: () -> Unit,
    onSelect: (BlePeripheral) -> Unit,
) {
    ModalBottomSheet(
        onDismissRequest = onDismiss,
        containerColor = Color(0xFF0C1017),
        dragHandle = null,
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .navigationBarsPadding()
                .padding(18.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Text("Choose Flipper", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.SemiBold)
            Text(
                if (scanning) "Select the device running Phone Sync" else "Scan stopped",
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            if (devices.isEmpty()) {
                PanelCard {
                    Text("Scanning...", color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            } else {
                devices.forEach { device ->
                    PanelCard {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clickable { onSelect(device) },
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                                Text(device.name.ifBlank { "Unnamed BLE device" }, fontWeight = FontWeight.SemiBold)
                                Text(device.address, color = MaterialTheme.colorScheme.onSurfaceVariant)
                            }
                            Text("${device.rssi} dBm", color = MaterialTheme.colorScheme.primary)
                        }
                    }
                }
            }
        }
    }
}

@OptIn(ExperimentalGetImage::class)
@Composable
private fun CompactBarcodeScanner(enabled: Boolean, onBarcode: (String) -> Unit) {
    if(!enabled) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(112.dp)
                .clip(RoundedCornerShape(20.dp))
                .background(Color(0xFF111722)),
            contentAlignment = Alignment.Center,
        ) {
            Text("Camera unavailable", color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        return
    }

    val context = LocalContext.current
    val lifecycleOwner = LocalLifecycleOwner.current
    val cameraProviderFuture = remember { ProcessCameraProvider.getInstance(context) }
    val executor = remember { Executors.newSingleThreadExecutor() }
    val scanner = remember {
        BarcodeScanning.getClient(
            BarcodeScannerOptions.Builder()
                .setBarcodeFormats(
                    Barcode.FORMAT_EAN_13,
                    Barcode.FORMAT_EAN_8,
                    Barcode.FORMAT_UPC_A,
                    Barcode.FORMAT_UPC_E,
                    Barcode.FORMAT_CODE_128,
                    Barcode.FORMAT_CODE_39,
                    Barcode.FORMAT_CODE_93,
                    Barcode.FORMAT_ITF,
                    Barcode.FORMAT_CODABAR,
                    Barcode.FORMAT_QR_CODE,
                )
                .build(),
        )
    }
    val lastBarcode = remember { mutableStateOf("") }
    val stableHits = remember { mutableIntStateOf(0) }

    DisposableEffect(Unit) {
        onDispose {
            executor.shutdown()
            scanner.close()
        }
    }

    Box(
        modifier = Modifier
            .fillMaxWidth()
            .height(112.dp)
            .clip(RoundedCornerShape(20.dp))
            .background(Color.Black),
    ) {
        AndroidView(
            factory = { ctx ->
                PreviewView(ctx).apply {
                    scaleType = PreviewView.ScaleType.FILL_CENTER
                    implementationMode = PreviewView.ImplementationMode.COMPATIBLE
                }
            },
            modifier = Modifier.fillMaxSize(),
            update = { previewView ->
                val provider = cameraProviderFuture.get()
                val preview = androidx.camera.core.Preview.Builder().build().also {
                    it.surfaceProvider = previewView.surfaceProvider
                }
                val analysis = ImageAnalysis.Builder()
                    .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                    .setTargetResolution(Size(1920, 1080))
                    .build()
                analysis.setAnalyzer(executor) { imageProxy ->
                    processFrame(imageProxy, scanner, null) { barcode ->
                        if(barcode != lastBarcode.value) {
                            lastBarcode.value = barcode
                            stableHits.intValue = 1
                        } else {
                            stableHits.intValue += 1
                        }
                        if(stableHits.intValue >= 2) {
                            onBarcode(barcode)
                        }
                    }
                }
                provider.unbindAll()
                val camera = provider.bindToLifecycle(
                    lifecycleOwner,
                    CameraSelector.DEFAULT_BACK_CAMERA,
                    preview,
                    analysis,
                )
                runCatching { camera.cameraControl.setZoomRatio(1.8f) }
            },
        )
        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(12.dp)
                .border(2.dp, Color(0xAA8B78FF), RoundedCornerShape(18.dp)),
        )
    }
}

@OptIn(ExperimentalGetImage::class)
@Composable
private fun BarcodeScannerScreen(onClose: () -> Unit, onBarcode: (String) -> Unit) {
    val context = LocalContext.current
    val lifecycleOwner = LocalLifecycleOwner.current
    val cameraProviderFuture = remember { ProcessCameraProvider.getInstance(context) }
    val executor = remember { Executors.newSingleThreadExecutor() }
    val consumed = remember { AtomicBoolean(false) }
    val scanner = remember {
        BarcodeScanning.getClient(
            BarcodeScannerOptions.Builder()
                .setBarcodeFormats(
                    Barcode.FORMAT_EAN_13,
                    Barcode.FORMAT_EAN_8,
                    Barcode.FORMAT_UPC_A,
                    Barcode.FORMAT_UPC_E,
                    Barcode.FORMAT_CODE_128,
                    Barcode.FORMAT_CODE_39,
                    Barcode.FORMAT_CODE_93,
                    Barcode.FORMAT_ITF,
                    Barcode.FORMAT_CODABAR,
                    Barcode.FORMAT_QR_CODE,
                )
                .build(),
        )
    }

    DisposableEffect(Unit) {
        onDispose {
            executor.shutdown()
            scanner.close()
        }
    }

    Box(modifier = Modifier.fillMaxSize().background(Color.Black)) {
        AndroidView(
            factory = { ctx ->
                PreviewView(ctx).apply {
                    scaleType = PreviewView.ScaleType.FILL_CENTER
                    implementationMode = PreviewView.ImplementationMode.COMPATIBLE
                }
            },
            modifier = Modifier.fillMaxSize(),
            update = { previewView ->
                val provider = cameraProviderFuture.get()
                val preview = androidx.camera.core.Preview.Builder().build().also {
                    it.surfaceProvider = previewView.surfaceProvider
                }
                val analysis = ImageAnalysis.Builder()
                    .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                    .setTargetResolution(Size(1920, 1080))
                    .build()
                analysis.setAnalyzer(executor) { imageProxy ->
                    processFrame(imageProxy, scanner, consumed, onBarcode)
                }
                provider.unbindAll()
                val camera = provider.bindToLifecycle(
                    lifecycleOwner,
                    CameraSelector.DEFAULT_BACK_CAMERA,
                    preview,
                    analysis,
                )
                runCatching { camera.cameraControl.setZoomRatio(1.8f) }
            },
        )

        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(28.dp),
        ) {
            Column(
                modifier = Modifier.align(Alignment.TopCenter).padding(top = 40.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Text("Point at the tag barcode", color = Color.White, textAlign = TextAlign.Center)
                Spacer(Modifier.height(14.dp))
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .aspectRatio(1.8f)
                        .border(2.dp, Color(0xFF8B78FF), RoundedCornerShape(24.dp)),
                )
            }
            IconButton(
                onClick = onClose,
                modifier = Modifier
                    .align(Alignment.TopStart)
                    .statusBarsPadding()
                    .background(Color(0x66000000), CircleShape),
            ) {
                Text("X", color = Color.White)
            }
        }
    }
}

private fun processFrame(
    imageProxy: ImageProxy,
    scanner: com.google.mlkit.vision.barcode.BarcodeScanner,
    consumed: AtomicBoolean?,
    onBarcode: (String) -> Unit,
) {
    val mediaImage = imageProxy.image ?: run {
        imageProxy.close()
        return
    }
    if (consumed?.get() == true) {
        imageProxy.close()
        return
    }
    val image = InputImage.fromMediaImage(mediaImage, imageProxy.imageInfo.rotationDegrees)
    scanner.process(image)
        .addOnSuccessListener { results ->
            val value = results.firstOrNull { !it.rawValue.isNullOrBlank() }?.rawValue
            if (!value.isNullOrBlank()) {
                if(consumed == null || consumed.compareAndSet(false, true)) {
                    onBarcode(value)
                }
            }
        }
        .addOnCompleteListener { imageProxy.close() }
}

@Composable
private fun PanelCard(content: @Composable ColumnScope.() -> Unit) {
    Card(
        colors = cardColors(),
        shape = RoundedCornerShape(24.dp),
        modifier = Modifier.fillMaxWidth(),
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
            content = content,
        )
    }
}

@Composable
private fun cardColors() = CardDefaults.cardColors(containerColor = Color(0xFF0B0F16))

private fun bluetoothPermissions(): Array<String> {
    return buildList {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            add(Manifest.permission.BLUETOOTH_SCAN)
            add(Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            add(Manifest.permission.ACCESS_FINE_LOCATION)
        }
    }.toTypedArray()
}

private fun hasBluetoothPermissions(activity: ComponentActivity): Boolean {
    return bluetoothPermissions().all {
        ContextCompat.checkSelfPermission(activity, it) == PackageManager.PERMISSION_GRANTED
    }
}

private fun hasCameraPermission(activity: ComponentActivity): Boolean {
    return ContextCompat.checkSelfPermission(activity, Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED
}

private fun launchCompanionPicker(
    activity: ComponentActivity,
    viewModel: MainViewModel,
    launcher: androidx.activity.result.ActivityResultLauncher<IntentSenderRequest>,
) {
    val manager = activity.getSystemService(CompanionDeviceManager::class.java)
    if(manager == null) {
        viewModel.setStatus("Companion device picker unavailable")
        return
    }
    val filter = BluetoothLeDeviceFilter.Builder()
        .setNamePattern(Pattern.compile(".*", Pattern.CASE_INSENSITIVE))
        .build()
    val request = AssociationRequest.Builder()
        .addDeviceFilter(filter)
        .setSingleDevice(false)
        .build()
    viewModel.setStatus("Searching nearby BLE devices")
    manager.associate(
        request,
        activity.mainExecutor,
        object : CompanionDeviceManager.Callback() {
            override fun onAssociationPending(intentSender: android.content.IntentSender) {
                launcher.launch(IntentSenderRequest.Builder(intentSender).build())
            }

            override fun onDeviceFound(intentSender: android.content.IntentSender) {
                launcher.launch(IntentSenderRequest.Builder(intentSender).build())
            }

            override fun onFailure(error: CharSequence?) {
                viewModel.setStatus(error?.toString() ?: "No nearby device found")
            }
        },
    )
}

@Suppress("DEPRECATION")
private fun extractCompanionBluetoothDevice(intent: android.content.Intent?): BluetoothDevice? {
    if(intent == null) return null
    
    // Some Android versions/devices wrap the device in a ScanResult
    val scanResult = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        intent.getParcelableExtra(CompanionDeviceManager.EXTRA_DEVICE, android.bluetooth.le.ScanResult::class.java)
    } else {
        intent.getParcelableExtra(CompanionDeviceManager.EXTRA_DEVICE) as? android.bluetooth.le.ScanResult
    }
    if (scanResult != null) return scanResult.device

    // Others return the BluetoothDevice directly
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        intent.getParcelableExtra(CompanionDeviceManager.EXTRA_DEVICE, BluetoothDevice::class.java)
    } else {
        intent.getParcelableExtra(CompanionDeviceManager.EXTRA_DEVICE)
    }
}
