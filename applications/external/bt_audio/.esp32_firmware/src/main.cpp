/**
 * BT Audio ESP32 Firmware
 * 
 * This firmware runs on ESP32 and implements the BT Audio UART protocol
 * to enable Flipper Zero to control Bluetooth A2DP audio connections.
 * 
 * Features:
 * - A2DP Source mode (connects TO Bluetooth headphones/speakers)
 * - UART command interface at 115200 baud
 * - Bluetooth device scanning
 * - Audio test tone generation
 * - MP3 playback from SD card (/music folder)
 * - Remote SD card browsing from Flipper
 * - Persistent BT device name (stored in NVS/Preferences)
 * 
 * Hardware Support:
 * - ESP32 (original) - Full Bluetooth Classic support
 * - ESP32-C3 - Full Bluetooth Classic support  
 * - ESP32-S2 - BLE only (A2DP not supported)
 * - ESP32-CAM with SD card slot (recommended for MP3 playback)
 * 
 * UART Protocol:
 * Commands (Flipper -> ESP32):
 *   SCAN\n - Start scanning for Bluetooth devices
 *   CONNECT <MAC>\n - Connect to device with MAC address
 *   PLAY\n - Play test tone through connected device
 *   PLAY_MP3\n - Play all MP3 files from /music folder (base files first, then subfolders)
 *   PLAY_PLAYLIST\n - Play only files listed in /music/playlist1.m3u
 *   PLAY_PLAYLIST2\n - Play only files listed in /music/playlist2.m3u
 *   PLAY_M3U:<path>\n - Play a custom M3U playlist file by full path (via file browser)
 *   PAUSE\n - Pause current playback
 *   RESUME\n - Resume paused playback
 *   NEXT\n - Skip to next track
 *   PREV\n - Go to previous track
 *   RESTART\n - Restart current track from beginning
 *   SEEK_FWD\n - Seek forward ~5 seconds
 *   SEEK_BACK\n - Seek backward ~5 seconds
 *   STOP\n - Stop current playback
 *   DISCONNECT\n - Disconnect from current device
 *   LIST_FILES\n - List MP3 files in /music folder for remote browsing
 *   LIST_FILES:<path>\n - List files in specific directory
 *   PLAY_FILE:<path>\n - Play a specific file by full path (single file repeat mode)
 *   PLAY_FILE_DIR:<dir>|<file>\n - Play file with directory context (continuous play mode)
 *   CONTINUOUS:<ON|OFF>\n - Set continuous play mode (ON = play next files, OFF = repeat single file)
 * 
 * Responses (ESP32 -> Flipper):
 *   DEVICE:<MAC>,<Name>\n - Found device during scan
 *   SCAN_COMPLETE\n - Scanning finished
 *   CONNECTED\n - Successfully connected
 *   FAILED\n - Connection failed
 *   PLAYING\n - Started playing
 *   PLAYING_FILE:<filename>\n - Currently playing file
 *   PAUSED\n - Playback paused
 *   RESUMED\n - Playback resumed
 *   PLAY_COMPLETE\n - Playback finished
 *   STOPPED\n - Playback stopped
 *   DISCONNECTED\n - Disconnected from device
 *   SD_ERROR\n - SD card error
 *   NO_FILES\n - No MP3 files found
 *   PLAYBACK_ERROR:<type>\n - Playback error (e.g., M3U for playlist errors)
 *   M3U_NOT_FOUND:<path>\n - M3U playlist file not found
 *   M3U_EMPTY\n - M3U playlist has no valid MP3 files
 *   LIST_FILES_PATH:<path>\n - Current path being listed
 *   FILE:<full_path>|<filename>|<type>\n - File entry (type: f=file, d=directory)
 *   LIST_FILES_COUNT:<dirs>,<files>\n - Count of directories and files
 *   LIST_FILES_END\n - End of file listing
 *   CONTINUOUS_PLAY:<count> files, starting at index <n>\n - Started continuous playback
 *   LIST_FILES_ERROR:<reason>\n - Error during listing
 * 
 * Wiring:
 *   ESP32 TX -> Flipper RX (pin 14)
 *   ESP32 RX -> Flipper TX (pin 13)
 *   ESP32 GND -> Flipper GND
 * 
 * SD Card (ESP32-CAM or with SD module):
 *   SD_CS  -> GPIO 5 (default for ESP32-CAM)
 *   SD_SCK -> GPIO 18
 *   SD_MOSI -> GPIO 23
 *   SD_MISO -> GPIO 19
 * 
 * Author: FatherDivine
 * License: GPL v3
 */

#include <Arduino.h>
#include <algorithm>
#include <math.h>
#include <Preferences.h>
#include "BluetoothA2DPSource.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt.h"
#include <SD.h>
#include <SD_MMC.h>
#include <SPI.h>
#include <FS.h>
#include <WiFi.h>
#include <HTTPClient.h>

// MP3 decoder using libhelix-mp3
// Library: https://github.com/FatherDivine/esp-libhelix-mp3
// API: MP3InitDecoder(), MP3Decode(), MP3FindSyncWord(), MP3GetLastFrameInfo()
// The decoder operates on raw MP3 frame data and outputs 16-bit PCM samples
extern "C" {
#include "mp3dec.h"
}

// Configuration
#define UART_BAUD 115200
#define BT_DEVICE_NAME_DEFAULT "FlipperAudio"
#define BT_DEVICE_NAME_MAX_LEN 32
#define FW_VERSION "2.3"
#define FW_MODE_DESC "A2DP Source (Connects TO Headphones/Speakers)"
#define FW_MODE_CODE "A2DP_SOURCE"
#define SCAN_DURATION_MS 10000
#define GAP_INQUIRY_DURATION_UNIT_MS 1280
#define TEST_TONE_FREQ 440.0  // A4 note
#define TEST_TONE_DURATION_MS 2000
#define SAMPLE_RATE 44100
#define BT_INIT_DELAY_MS 750  // Delay for Bluetooth stack initialization (increased for reliability)
#define CONNECTION_TIMEOUT_MS 15000  // Connection timeout (increased for simpler BT devices that need more time)
#define FIRST_SCAN_WARMUP_MS 200  // Extra warmup delay for first Bluetooth scan after boot

// Volume Configuration
// Default initial volume at ~75% (96 out of 127)
// This prevents loud playback on first song
#define DEFAULT_INITIAL_VOLUME 96
#define VOLUME_STEP 8  // Volume step for up/down commands (~6%)
#define VOLUME_MAX 127
#define VOLUME_MIN 0

// EQ Configuration  
// Note: EQ is applied as gain multipliers to PCM samples during MP3 playback
// Values are in dB from -10 to +10
#define EQ_DEFAULT 0  // 0 dB = no change
#define EQ_MIN -10
#define EQ_MAX 10

// SD Card Configuration
// ESP32-CAM uses SD_MMC (SDIO mode), generic ESP32 uses SPI mode
// Auto-detect: Try SD_MMC first (ESP32-CAM), fall back to SPI SD
#define SD_CS_PIN 5

// SPI SD pins for generic ESP32 boards (if not using ESP32-CAM)
// These are the default VSPI pins
#define SD_SCK_PIN  18
#define SD_MISO_PIN 19
#define SD_MOSI_PIN 23

// SD initialization retry settings
#define SD_INIT_RETRIES 3
#define SD_INIT_DELAY_MS 100

// Seek configuration - bytes to skip for ~5 seconds at 128kbps
// 128kbps = 16000 bytes/sec, so 5 seconds ≈ 80000 bytes
#define SEEK_BYTES_5_SEC 80000

// MP3 Configuration
// WiFi streaming needs a larger buffer than file playback for network jitter handling
// At 128kbps: 16KB = ~1 second of audio, 32KB = ~2 seconds
// At 192kbps: 16KB = ~0.67 seconds, 32KB = ~1.3 seconds
// At 320kbps: 16KB = ~0.4 seconds, 32KB = ~0.8 seconds
#define MP3_BUF_SIZE 32768      // Input buffer for MP3 data (32KB for ~1-2 seconds of buffer)
#define MP3_BUF_REFILL_THRESHOLD 16384 // Refill buffer when below this many bytes
#define PCM_BUF_SIZE 4608       // Output buffer for PCM (1152 samples * 2 channels * 2 bytes)
#define MUSIC_FOLDER "/music"   // Folder to scan for MP3 files
#define MAX_MP3_FILES 50        // Maximum number of MP3 files to track

// Flipper SD streaming configuration (NEW)
// When streaming from Flipper SD, MP3 data comes in chunks over UART
// The ESP32 buffers this data and decodes it as it arrives
#define FLIPPER_STREAM_CHUNK_SIZE 512       // Max chunk size from Flipper (limited by UART buffer)
#define FLIPPER_STREAM_BUFFER_SIZE 16384    // Buffer for Flipper streaming (16KB = ~1 sec at 128kbps)
#define FLIPPER_STREAM_MIN_BUFFER 4096      // Min bytes before starting playback (4KB)
#define FLIPPER_STREAM_END_MARKER "STREAM_END"  // Marker sent when stream completes

// WiFi Configuration
#define WIFI_CONNECT_TIMEOUT_MS 10000   // 10 seconds to connect to WiFi
#define WIFI_CONNECT_MAX_RETRIES 3      // Max retries for WiFi connection (BT/WiFi coexistence)
#define WIFI_MAX_SSID_LEN 64
#define WIFI_MAX_PASS_LEN 64
#define STREAM_URL_MAX_LEN 256
#define STREAM_BUFFER_SIZE 4096         // Buffer for streaming audio data

// Stream buffering configuration - critical for stable WiFi streaming
// Network streams need significant pre-buffering to handle latency and jitter
// Pre-buffer should be ~75-80% of total buffer to ensure smooth start
// Internet radio typically needs larger buffers for smooth playback
#define STREAM_PREBUFFER_SIZE 24576     // Pre-buffer 24KB before starting playback (~75% of buffer)
#define STREAM_PREBUFFER_MIN_THRESHOLD 8192   // Minimum bytes needed to attempt playback (8KB)
#define STREAM_UNDERRUN_TIMEOUT_MS 10000    // Time to wait for data before declaring stream end (10s)
#define STREAM_UNDERRUN_RETRY_MS 50         // Time between buffer refill attempts during underrun
#define STREAM_PREBUFFER_MAX_ATTEMPTS (STREAM_UNDERRUN_TIMEOUT_MS / STREAM_UNDERRUN_RETRY_MS)

// Stream refill configuration - controls how often we refill the buffer from WiFi
// This is done in the main loop to avoid blocking the audio callback (which caused WDT reset)
// More aggressive refilling helps prevent underruns
#define STREAM_REFILL_INTERVAL_MS 2      // Check for buffer refill every 2ms in main loop (more aggressive)
#define STREAM_LOW_WATER_MARK 8192       // Refill when buffer drops below this level (8KB - trigger earlier)
#define STREAM_CHUNK_SIZE 4096           // Max bytes to read from WiFi at once (4KB chunks for efficiency)
#define STREAM_BUFFER_FULL_THRESHOLD 1024 // Consider buffer full when this many bytes from capacity

// Melody definitions
// "Mary Had a Little Lamb" - E D C D E E E, D D D, E G G
// Using notes: C4=262, D4=294, E4=330, G4=392
#define MARY_NOTE_COUNT 13
const float mary_notes[MARY_NOTE_COUNT] = {
    330.0, 294.0, 262.0, 294.0,  // E D C D
    330.0, 330.0, 330.0,          // E E E (half note)
    294.0, 294.0, 294.0,          // D D D (half note)
    330.0, 392.0, 392.0           // E G G (half note)
};
const int mary_durations[MARY_NOTE_COUNT] = {
    250, 250, 250, 250,   // quarter notes
    250, 250, 500,        // quarter, quarter, half
    250, 250, 500,        // quarter, quarter, half
    250, 250, 500         // quarter, quarter, half
};
#define MARY_TOTAL_DURATION 3500  // ~3.5 seconds total

// Simple 3-tone beep sequence (like a confirmation sound)
#define BEEP_NOTE_COUNT 3
#define BEEP_GAP_MS 100  // Gap between beeps in milliseconds
const float beep_notes[BEEP_NOTE_COUNT] = {
    523.25,  // C5
    659.25,  // E5
    783.99   // G5
};
const int beep_durations[BEEP_NOTE_COUNT] = {
    150, 150, 300  // short, short, long
};
#define BEEP_TOTAL_DURATION 800  // ~0.8 seconds total (includes gaps)

// Global objects - A2DP Source for connecting TO headphones/speakers
BluetoothA2DPSource a2dp_source;

// Preferences for persistent settings (NVS storage)
Preferences preferences;

// State management
bool is_connected = false;
bool is_scanning = false;
bool is_playing = false;
bool is_paused = false;              // Track if playback is paused
bool is_playing_mp3 = false;
bool is_playing_melody = false;  // Track if playing a melody
int current_melody = 0;          // Which melody is playing (0=none, 1=Mary, 2=beeps)
bool connection_pending = false;
String current_device_mac = "";
String target_mac = "";
bool gap_callback_registered = false;
bool first_scan_done = false;        // Track if first scan has been initiated
uint32_t play_start_time = 0;
uint32_t tone_sample_count = 0;
uint32_t connection_start_time = 0;
String current_playing_file = "";    // Currently playing filename (for display)
bool shuffle_mode = false;           // Shuffle/random play mode
bool continuous_play_mode = false;   // Continuous play mode: OFF = repeat single file, ON = play through directory
bool single_file_repeat = false;     // When playing from file browser with continuous_play OFF, repeat the single file

// Reconnection and track protection state
// When BT reconnects, there's a brief window where the audio callback may get
// called rapidly before the stream is stable. This can cause spurious track skips.
// We use these variables to protect against that.
bool was_playing_before_disconnect = false;  // Track if we were playing before disconnect
bool reconnection_in_progress = false;       // True during the reconnection window
uint32_t reconnection_time = 0;              // When reconnection started
#define RECONNECTION_PROTECTION_MS 2000      // Protect against track skips for 2000ms after reconnect (increased for reliability)

// Auto-reconnection state tracking
// The ESP32 library handles auto-reconnect but we need to track state for Flipper notification
bool was_connected_before = false;           // Track if we had an active connection before
uint32_t last_disconnect_time = 0;           // When we last disconnected
#define AUTO_RECONNECT_WINDOW_MS 30000       // Consider reconnection within 30s as "auto-reconnect"
String saved_device_mac_for_reconnect = "";  // MAC address saved for active reconnection
bool reconnection_requested = false;         // True when we should actively try to reconnect
uint32_t last_reconnect_attempt_time = 0;    // When we last attempted reconnection
int reconnect_attempt_count = 0;             // Number of reconnection attempts made
#define RECONNECT_ATTEMPT_INTERVAL_MS 3000   // Try to reconnect every 3 seconds
#define MAX_RECONNECT_ATTEMPTS 10            // Maximum reconnection attempts before giving up

// Consecutive decode failures tracking
// To prevent skipping tracks due to temporary decode errors, we require multiple
// consecutive failures before moving to the next track
int consecutive_decode_failures = 0;
#define MAX_DECODE_FAILURES_BEFORE_SKIP 50   // Allow up to 50 consecutive failures before skipping
#define MIN_PLAYBACK_TIME_BEFORE_SKIP_MS 500  // Must play for 500ms before auto-skip allowed

// Navigation command protection
// When a navigation command is active, block the audio callback's auto-skip behavior
// to prevent race conditions during track transitions
volatile bool navigation_command_active = false;
uint32_t navigation_command_time = 0;
uint32_t track_start_time = 0;  // Track when current track started playing
#define NAVIGATION_PROTECTION_MS 200  // Block auto-skip for 200ms after nav command

// Track switching synchronization
// CRITICAL: This flag prevents race conditions between main loop and audio callback
// When track_switching is true, the audio callback outputs silence and doesn't access buffers
// This prevents xQueueGenericSend assertion failures and audio glitches
volatile bool track_switching = false;

// Navigation rate limiting
// Prevents BT disconnection caused by rapid successive navigation commands
// When user presses next/prev rapidly, we queue the final destination instead of
// executing each command individually which can overwhelm the A2DP stream
uint32_t last_nav_command_time = 0;           // Time of last navigation command
int pending_nav_offset = 0;                    // Accumulated navigation offset (+1 for next, -1 for prev)
bool nav_debounce_active = false;              // True when we're in debounce window
#define NAV_DEBOUNCE_MS 300                    // Minimum time between actual track changes
#define NAV_QUEUE_TIMEOUT_MS 400              // Time to wait for more commands before executing

// Bluetooth device name (configurable via SET_BT_NAME command)
char bt_device_name[BT_DEVICE_NAME_MAX_LEN] = BT_DEVICE_NAME_DEFAULT;
bool bt_name_changed = false;  // Track if name was changed (requires restart)

// Volume state
// Start with initial volume (not 127) to prevent ear-blasting first play
uint8_t initial_volume = DEFAULT_INITIAL_VOLUME;  // Configurable via SET_INIT_VOL command
bool first_play_volume_set = false;  // Track if we've set initial volume on first play

// Volume protection: After setting initial volume, reinforce it for a longer window
// This prevents the remote device from overriding our volume setting via AVRCP
// The remote device often sends ESP_AVRC_RN_VOLUME_CHANGE shortly after connection
// Extended to 5 seconds to handle aggressive BT devices that send multiple volume changes
#define VOLUME_PROTECTION_WINDOW_MS 5000  // Reinforce volume for 5 seconds after first play
uint32_t volume_protection_start_time = 0;  // When we started volume protection
bool volume_protection_active = false;  // Is volume protection currently active

// EQ state (in dB, -10 to +10)
int8_t eq_bass = EQ_DEFAULT;
int8_t eq_mid = EQ_DEFAULT;
int8_t eq_treble = EQ_DEFAULT;

// EQ gain multipliers (calculated from dB values)
// Gain = 10^(dB/20) -- pre-calculated for performance
float eq_bass_gain = 1.0;
float eq_mid_gain = 1.0;
float eq_treble_gain = 1.0;

// EQ Filter State (biquad IIR filters for each band, stereo)
// Using second-order IIR filters for bass (low-shelf), mid (peaking), and treble (high-shelf)
// Filter state: [x1, x2, y1, y2] for each channel for each band
// Bass: low-shelf filter centered at ~200Hz
// Mid: peaking filter centered at ~1kHz  
// Treble: high-shelf filter centered at ~4kHz
float eq_bass_state_L[4] = {0, 0, 0, 0};
float eq_bass_state_R[4] = {0, 0, 0, 0};
float eq_mid_state_L[4] = {0, 0, 0, 0};
float eq_mid_state_R[4] = {0, 0, 0, 0};
float eq_treble_state_L[4] = {0, 0, 0, 0};
float eq_treble_state_R[4] = {0, 0, 0, 0};

// Pre-calculated filter coefficients for each EQ band
// These are biquad coefficients: [b0, b1, b2, a1, a2] (normalized, a0=1)
// Coefficients are calculated for 44.1kHz sample rate
float eq_bass_coef[5] = {1.0, 0.0, 0.0, 0.0, 0.0};   // Default passthrough
float eq_mid_coef[5] = {1.0, 0.0, 0.0, 0.0, 0.0};    // Default passthrough
float eq_treble_coef[5] = {1.0, 0.0, 0.0, 0.0, 0.0}; // Default passthrough

// SD Card state
bool sd_initialized = false;
bool sd_using_mmc = false;  // true if using SD_MMC (ESP32-CAM), false if using SPI SD
bool sd_init_attempted = false;  // Track if we've attempted initialization

// MP3 Playback state
HMP3Decoder mp3_decoder = NULL;
File mp3_file;
uint8_t mp3_buf[MP3_BUF_SIZE];
int16_t pcm_buf[PCM_BUF_SIZE / 2];  // PCM samples (stereo)
int mp3_buf_bytes = 0;
int pcm_buf_samples = 0;
int pcm_buf_read_pos = 0;
String mp3_files[MAX_MP3_FILES];
int mp3_file_count = 0;
int current_mp3_index = 0;

// Favorites playback state
String favorites_list[MAX_MP3_FILES];  // Favorites received from Flipper
int favorites_count = 0;
int favorites_receiving = 0;  // Count of favorites expected (0 = not receiving)
bool playing_favorites = false;  // True if playing favorites playlist (legacy, kept for compatibility)

// Playback mode tracking - used to detect mode changes and always start from track 1
// When the mode changes, playback should always start from the first track
typedef enum {
    PLAYBACK_MODE_NONE = 0,      // Not playing
    PLAYBACK_MODE_FOLDER,        // Play all files in /music folder
    PLAYBACK_MODE_PLAYLIST1,     // Play from playlist1.m3u
    PLAYBACK_MODE_PLAYLIST2,     // Play from playlist2.m3u
    PLAYBACK_MODE_FAVORITES,     // Play favorites list
    PLAYBACK_MODE_CUSTOM_M3U     // Play from custom M3U file (via file browser)
} PlaybackMode;

PlaybackMode current_playback_mode = PLAYBACK_MODE_NONE;  // Current playback mode
// Path to the custom M3U file currently being played (via file browser).
// Used to detect when the same M3U is requested again - if same file,
// playback continues from current position; if different file, starts fresh.
String custom_m3u_path = "";

// WiFi streaming state
bool wifi_connected = false;                    // WiFi connection status
bool wifi_streaming = false;                    // Stream playback active
bool stream_prebuffering = false;               // True during initial buffering phase
String wifi_ssid = "";                          // WiFi SSID
String wifi_password = "";                      // WiFi password
String stream_url = "";                         // Stream URL
HTTPClient http_client;                         // HTTP client for streaming
WiFiClient* stream = nullptr;                   // Stream client
uint8_t stream_buffer[STREAM_BUFFER_SIZE];      // Buffer for streaming data
int stream_buffer_bytes = 0;                    // Bytes in stream buffer
int stream_buffer_read_pos = 0;                 // Read position in stream buffer
String stream_metadata = "";                    // Current stream metadata (song title, artist)
uint32_t stream_underrun_start_time = 0;        // When buffer underrun started (for timeout)
bool stream_in_underrun = false;                // True when buffer is empty and waiting for data
uint32_t last_stream_refill_time = 0;           // Last time we refilled stream buffer (for main loop)
// Note: volatile is sufficient here for simple flag signaling between audio callback and main loop.
// This is a single-producer (callback sets true), single-consumer (loop reads and clears) pattern.
// Worst case of missed signal is acceptable - loop also checks buffer level independently.
volatile bool stream_needs_data = false;        // Flag: audio callback signals it needs more data
// CRITICAL: Signal from audio callback to main loop to stop streaming
// We cannot call http_client.end() from audio callback as it's not thread-safe
// Setting this flag tells main loop to clean up the stream safely
volatile bool stream_stop_requested = false;    // Flag: audio callback signals stream should stop

// Flipper SD streaming state (NEW)
// When Flipper streams MP3 data over UART, we buffer it here before decoding
bool flipper_streaming = false;                 // True when streaming from Flipper SD over UART
uint8_t flipper_stream_buffer[FLIPPER_STREAM_BUFFER_SIZE];  // Buffer for Flipper stream data
int flipper_stream_bytes = 0;                   // Bytes currently in Flipper stream buffer
bool flipper_stream_started = false;            // True after first chunk received and playback started
bool flipper_stream_ended = false;              // True when Flipper sends STREAM_END marker
String flipper_current_filename = "";           // Filename being streamed from Flipper

// EQ filter constants
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define SHELF_Q_FACTOR 0.707f  // Q = 1/sqrt(2) for Butterworth response on shelf filters

/**
 * Convert dB to linear gain multiplier
 * gain = 10^(dB/20)
 */
float dB_to_gain(int8_t dB) {
    if (dB == 0) return 1.0;
    return pow(10.0, (float)dB / 20.0);
}

/**
 * Calculate biquad low-shelf filter coefficients
 * Uses standard biquad coefficient formulas for audio EQ
 * @param fc Center frequency in Hz
 * @param gain_dB Gain in dB (-10 to +10)
 * @param fs Sample rate in Hz (44100)
 * @param coef Output array [b0, b1, b2, a1, a2] - must have at least 5 elements
 */
void calcLowShelfCoef(float fc, float gain_dB, float fs, float* coef) {
    if (gain_dB == 0.0f) {
        // Passthrough
        coef[0] = 1.0f; coef[1] = 0.0f; coef[2] = 0.0f; coef[3] = 0.0f; coef[4] = 0.0f;
        return;
    }
    float A = pow(10.0f, gain_dB / 40.0f);  // sqrt of linear gain
    float w0 = 2.0f * (float)M_PI * fc / fs;
    float cos_w0 = cos(w0);
    float sin_w0 = sin(w0);
    float alpha = sin_w0 / (2.0f * SHELF_Q_FACTOR);  // Butterworth Q for shelf
    float sqrtA = sqrt(A);
    
    float a0 = (A + 1.0f) + (A - 1.0f) * cos_w0 + 2.0f * sqrtA * alpha;
    coef[0] = (A * ((A + 1.0f) - (A - 1.0f) * cos_w0 + 2.0f * sqrtA * alpha)) / a0;
    coef[1] = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_w0)) / a0;
    coef[2] = (A * ((A + 1.0f) - (A - 1.0f) * cos_w0 - 2.0f * sqrtA * alpha)) / a0;
    coef[3] = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cos_w0)) / a0;
    coef[4] = ((A + 1.0f) + (A - 1.0f) * cos_w0 - 2.0f * sqrtA * alpha) / a0;
}

/**
 * Calculate biquad high-shelf filter coefficients
 * @param fc Center frequency in Hz
 * @param gain_dB Gain in dB (-10 to +10)
 * @param fs Sample rate in Hz (44100)
 * @param coef Output array [b0, b1, b2, a1, a2] - must have at least 5 elements
 */
void calcHighShelfCoef(float fc, float gain_dB, float fs, float* coef) {
    if (gain_dB == 0.0f) {
        // Passthrough
        coef[0] = 1.0f; coef[1] = 0.0f; coef[2] = 0.0f; coef[3] = 0.0f; coef[4] = 0.0f;
        return;
    }
    float A = pow(10.0f, gain_dB / 40.0f);  // sqrt of linear gain
    float w0 = 2.0f * (float)M_PI * fc / fs;
    float cos_w0 = cos(w0);
    float sin_w0 = sin(w0);
    float alpha = sin_w0 / (2.0f * SHELF_Q_FACTOR);  // Butterworth Q for shelf
    float sqrtA = sqrt(A);
    
    float a0 = (A + 1.0f) - (A - 1.0f) * cos_w0 + 2.0f * sqrtA * alpha;
    coef[0] = (A * ((A + 1.0f) + (A - 1.0f) * cos_w0 + 2.0f * sqrtA * alpha)) / a0;
    coef[1] = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_w0)) / a0;
    coef[2] = (A * ((A + 1.0f) + (A - 1.0f) * cos_w0 - 2.0f * sqrtA * alpha)) / a0;
    coef[3] = (2.0f * ((A - 1.0f) - (A + 1.0f) * cos_w0)) / a0;
    coef[4] = ((A + 1.0f) - (A - 1.0f) * cos_w0 - 2.0f * sqrtA * alpha) / a0;
}

/**
 * Calculate biquad peaking EQ filter coefficients
 * @param fc Center frequency in Hz
 * @param gain_dB Gain in dB (-10 to +10)
 * @param Q Quality factor (bandwidth control)
 * @param fs Sample rate in Hz (44100)
 * @param coef Output array [b0, b1, b2, a1, a2] - must have at least 5 elements
 */
void calcPeakingCoef(float fc, float gain_dB, float Q, float fs, float* coef) {
    if (gain_dB == 0.0f) {
        // Passthrough
        coef[0] = 1.0f; coef[1] = 0.0f; coef[2] = 0.0f; coef[3] = 0.0f; coef[4] = 0.0f;
        return;
    }
    float A = pow(10.0f, gain_dB / 40.0f);  // sqrt of linear gain
    float w0 = 2.0f * (float)M_PI * fc / fs;
    float cos_w0 = cos(w0);
    float sin_w0 = sin(w0);
    float alpha = sin_w0 / (2.0f * Q);
    
    float a0 = 1.0f + alpha / A;
    coef[0] = (1.0f + alpha * A) / a0;
    coef[1] = (-2.0f * cos_w0) / a0;
    coef[2] = (1.0f - alpha * A) / a0;
    coef[3] = (-2.0f * cos_w0) / a0;
    coef[4] = (1.0f - alpha / A) / a0;
}

/**
 * Process a single sample through a biquad filter
 * Direct Form II Transposed for better numerical stability
 * @param input Input sample
 * @param coef Filter coefficients [b0, b1, b2, a1, a2]
 * @param state Filter state [z1, z2] - must have at least 4 elements
 * @return Filtered output sample
 */
inline float processBiquad(float input, float* coef, float* state) {
    // If coefficients indicate passthrough, skip processing
    if (coef[1] == 0.0f && coef[2] == 0.0f && coef[3] == 0.0f && coef[4] == 0.0f) {
        return input * coef[0];  // Just apply gain (b0)
    }
    
    float output = coef[0] * input + state[0];
    state[0] = coef[1] * input - coef[3] * output + state[1];
    state[1] = coef[2] * input - coef[4] * output;
    return output;
}

/**
 * Apply 3-band EQ to a stereo sample
 * Returns true if EQ is active (any band != 0 dB)
 */
bool isEqActive() {
    return (eq_bass != 0 || eq_mid != 0 || eq_treble != 0);
}

/**
 * Apply EQ to stereo PCM samples
 * Processes samples in-place using cascaded biquad IIR filters
 * @param samples Pointer to interleaved stereo samples (L, R, L, R, ...)
 *                Must contain at least num_samples * 2 elements
 * @param num_samples Number of stereo sample pairs to process
 * @note Called from decodeMP3Frame() which guarantees proper buffer size from MP3 decoder
 */
void applyEQ(int16_t* samples, int num_samples) {
    if (!isEqActive()) return;
    
    for (int i = 0; i < num_samples; i++) {
        // Get stereo sample pair
        float left = (float)samples[i * 2];
        float right = (float)samples[i * 2 + 1];
        
        // Apply bass filter (low-shelf at ~200Hz)
        if (eq_bass != 0) {
            left = processBiquad(left, eq_bass_coef, eq_bass_state_L);
            right = processBiquad(right, eq_bass_coef, eq_bass_state_R);
        }
        
        // Apply mid filter (peaking at ~1kHz)
        if (eq_mid != 0) {
            left = processBiquad(left, eq_mid_coef, eq_mid_state_L);
            right = processBiquad(right, eq_mid_coef, eq_mid_state_R);
        }
        
        // Apply treble filter (high-shelf at ~4kHz)
        if (eq_treble != 0) {
            left = processBiquad(left, eq_treble_coef, eq_treble_state_L);
            right = processBiquad(right, eq_treble_coef, eq_treble_state_R);
        }
        
        // Clip to int16 range and store back
        if (left > 32767.0f) left = 32767.0f;
        if (left < -32768.0f) left = -32768.0f;
        if (right > 32767.0f) right = 32767.0f;
        if (right < -32768.0f) right = -32768.0f;
        
        samples[i * 2] = (int16_t)left;
        samples[i * 2 + 1] = (int16_t)right;
    }
}

/**
 * Reset EQ filter states (call when starting new track)
 */
void resetEqStates() {
    memset(eq_bass_state_L, 0, sizeof(eq_bass_state_L));
    memset(eq_bass_state_R, 0, sizeof(eq_bass_state_R));
    memset(eq_mid_state_L, 0, sizeof(eq_mid_state_L));
    memset(eq_mid_state_R, 0, sizeof(eq_mid_state_R));
    memset(eq_treble_state_L, 0, sizeof(eq_treble_state_L));
    memset(eq_treble_state_R, 0, sizeof(eq_treble_state_R));
}

// =============================================================================
// Base64 Decoding for Flipper SD Streaming
// =============================================================================

/**
 * Base64 decoding lookup table
 * Maps ASCII characters to their 6-bit values
 * Invalid characters map to 0xFF
 */
static const uint8_t base64_decode_table[128] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0-7
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 8-15
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 16-23
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 24-31
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 32-39 (space !"#$%&')
    0xFF, 0xFF, 0xFF, 62,   0xFF, 0xFF, 0xFF, 63,    // 40-47 (()* + ,-  /)
    52,   53,   54,   55,   56,   57,   58,   59,    // 48-55 (0-7)
    60,   61,   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 56-63 (89:;<=>?)
    0xFF, 0,    1,    2,    3,    4,    5,    6,     // 64-71 (@ABCDEFG)
    7,    8,    9,    10,   11,   12,   13,   14,    // 72-79 (HIJKLMNO)
    15,   16,   17,   18,   19,   20,   21,   22,    // 80-87 (PQRSTUVW)
    23,   24,   25,   0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 88-95 (XYZ[\]^_)
    0xFF, 26,   27,   28,   29,   30,   31,   32,    // 96-103 (`abcdefg)
    33,   34,   35,   36,   37,   38,   39,   40,    // 104-111 (hijklmno)
    41,   42,   43,   44,   45,   46,   47,   48,    // 112-119 (pqrstuvw)
    49,   50,   51,   0xFF, 0xFF, 0xFF, 0xFF, 0xFF   // 120-127 (xyz{|}~DEL)
};

/**
 * Decode base64 encoded data for Flipper SD streaming
 * @param input Base64 encoded string (from UART command)
 * @param output Buffer for decoded binary data
 * @param max_output_len Maximum bytes to write to output
 * @return Number of bytes written to output, or -1 on error
 */
int decodeBase64(const String& input, uint8_t* output, int max_output_len) {
    int len = input.length();
    int i = 0, j = 0;
    uint8_t char_array_4[4];
    int char_count = 0;
    
    for (int pos = 0; pos < len; pos++) {
        // Cast to uint8_t to handle signed char properly and ensure ASCII range check
        uint8_t c = (uint8_t)input[pos];
        
        // Skip padding and stop
        if (c == '=') break;
        
        // Skip characters outside valid base64 ASCII range (table only has 128 entries)
        if (c > 127) continue;
        uint8_t val = base64_decode_table[c];
        if (val == 0xFF) continue;
        
        char_array_4[char_count++] = val;
        
        if (char_count == 4) {
            // Check output buffer space
            if (j + 3 > max_output_len) {
                return -1;  // Output buffer overflow
            }
            
            output[j++] = (char_array_4[0] << 2) | (char_array_4[1] >> 4);
            output[j++] = ((char_array_4[1] & 0x0F) << 4) | (char_array_4[2] >> 2);
            output[j++] = ((char_array_4[2] & 0x03) << 6) | char_array_4[3];
            char_count = 0;
        }
    }
    
    // Handle remaining bytes (padding)
    if (char_count > 1) {
        if (j >= max_output_len) return -1;
        output[j++] = (char_array_4[0] << 2) | (char_array_4[1] >> 4);
    }
    if (char_count > 2) {
        if (j >= max_output_len) return -1;
        output[j++] = ((char_array_4[1] & 0x0F) << 4) | (char_array_4[2] >> 2);
    }
    
    return j;
}

/**
 * Update EQ gain multipliers and filter coefficients from dB values
 * Call this whenever EQ settings change
 */
void updateEqGains() {
    eq_bass_gain = dB_to_gain(eq_bass);
    eq_mid_gain = dB_to_gain(eq_mid);
    eq_treble_gain = dB_to_gain(eq_treble);
    
    // Calculate filter coefficients for 44.1kHz sample rate
    // Bass: low-shelf at 200Hz
    calcLowShelfCoef(200.0f, (float)eq_bass, 44100.0f, eq_bass_coef);
    
    // Mid: peaking at 1kHz with Q=1.0
    calcPeakingCoef(1000.0f, (float)eq_mid, 1.0f, 44100.0f, eq_mid_coef);
    
    // Treble: high-shelf at 4kHz
    calcHighShelfCoef(4000.0f, (float)eq_treble, 44100.0f, eq_treble_coef);
    
    Serial.printf("EQ updated: bass=%ddB mid=%ddB treble=%ddB\n", eq_bass, eq_mid, eq_treble);
}

static inline int gap_duration_units(uint32_t ms) {
    // GAP discovery expects duration in 1.28s units; round up to cover requested ms
    return (ms + GAP_INQUIRY_DURATION_UNIT_MS - 1) / GAP_INQUIRY_DURATION_UNIT_MS;
}

// Forward declarations
// Command processing
void processCommand(String cmd);

// Device management
void scanDevices();
void connectToDevice(String mac);
void disconnectDevice();
void printDiagnostics();
void onGapEvent(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param);
void extractNameFromEir(uint8_t* eir, char* name, size_t name_len);

// Playback control
void playTestTone();
void playMelody(int melody_id);
void playMP3FromSD();
void playFavorites();  // Play favorites playlist
void playPlaylistOnly();  // Play only from playlist1.m3u
void playPlaylist2Only();  // Play only from playlist2.m3u
void playCustomM3U(const String& m3u_path);  // Play custom M3U file from file browser
void stopPlayback();
void pausePlayback();
void resumePlayback();
void nextTrack();
void prevTrack();
void restartTrack();
void seekForward();
void seekBackward();
void activateNavigationProtection();  // Helper to activate navigation command protection
void executeQueuedNavigation();  // Execute pending navigation after debounce timeout

// Audio data and codec
int32_t get_audio_data(Frame* frame, int32_t frame_count);
bool decodeMP3Frame();
void skipID3Tag();

// SD card and file management
bool initSDCard();
bool tryInitSDCard();  // Try to initialize SD (called on demand)
fs::FS& getSDFS();  // Get the appropriate SD filesystem
bool fileExists(const String& path);  // Check if file exists on SD
String findMusicFolder();  // Find the music folder with case-insensitive search

// Playlist and file scanning
void scanMP3Files();
void scanMusicFolderOnly();  // Scan directory structure only (no playlist check)
bool scanPlaylistOnly();  // Scan only playlist1.m3u, returns true if found
bool scanPlaylist2Only();  // Scan only playlist2.m3u, returns true if found
bool scanCustomM3U(const String& m3u_path);  // Scan custom M3U file from file browser
bool openNextMP3();
bool openMP3AtIndex(int index);
bool openFavoriteAtIndex(int index);  // Open a favorite file with validation

// WiFi streaming
void refillStreamBuffer();  // Refill stream buffer from WiFi (called from main loop)

// Active music folder path (determined at runtime)
String active_music_folder = "";

/**
 * Find the music folder, checking common case variations
 * Returns the found folder path or empty string if not found
 */
String findMusicFolder() {
    if (!sd_initialized) return "";
    fs::FS& fs = getSDFS();
    
    // Check common case variations
    const char* variations[] = {"/music", "/Music", "/MUSIC"};
    for (int i = 0; i < 3; i++) {
        File dir = fs.open(variations[i]);
        if (dir && dir.isDirectory()) {
            dir.close();
            Serial.print("Found music folder: ");
            Serial.println(variations[i]);
            return String(variations[i]);
        }
        if (dir) dir.close();
    }
    
    // Not found - create lowercase version
    Serial.println("Music folder not found, creating /music");
    if (fs.mkdir("/music")) {
        return "/music";
    }
    return "";
}

/**
 * Check if a file exists on the SD card
 * Used for favorites validation to skip missing files gracefully
 */
bool fileExists(const String& path) {
    if (!sd_initialized) return false;
    fs::FS& fs = getSDFS();
    File f = fs.open(path);
    if (f && !f.isDirectory()) {
        f.close();
        return true;
    }
    return false;
}

/**
 * Get frequency for current position in melody
 * Returns 0 if playback is complete
 */
float getMelodyFrequency(uint32_t sample_position) {
    uint32_t sample_time_ms = (uint32_t)((float)sample_position * 1000.0 / SAMPLE_RATE);
    
    if (current_melody == 1) {  // Mary Had a Little Lamb
        uint32_t time_accum = 0;
        for (int i = 0; i < MARY_NOTE_COUNT; i++) {
            time_accum += mary_durations[i];
            if (sample_time_ms < time_accum) {
                return mary_notes[i];
            }
        }
        return 0;  // Done
    } else if (current_melody == 2) {  // Beep sequence
        uint32_t time_accum = 0;
        for (int i = 0; i < BEEP_NOTE_COUNT; i++) {
            // Each beep has duration + small gap
            if (sample_time_ms < time_accum + beep_durations[i]) {
                return beep_notes[i];
            }
            time_accum += beep_durations[i] + BEEP_GAP_MS;  // Gap between beeps
        }
        return 0;  // Done
    }
    return 0;
}

/**
 * Audio data callback for A2DP Source
 * This is called by the A2DP library to get audio data to send to headphones
 */
int32_t get_audio_data(Frame* frame, int32_t frame_count) {
    // Output silence when not playing, paused, or during track switching
    // CRITICAL: track_switching check prevents race condition with main loop
    // When main loop is opening a new file, we must NOT access mp3_buf/pcm_buf
    if (!is_playing || is_paused || track_switching) {
        for (int i = 0; i < frame_count; i++) {
            frame[i].channel1 = 0;
            frame[i].channel2 = 0;
        }
        return frame_count;
    }
    
    // Check if reconnection protection window has expired
    if (reconnection_in_progress) {
        if (millis() - reconnection_time > RECONNECTION_PROTECTION_MS) {
            reconnection_in_progress = false;
        }
    }
    
    // Check if navigation command protection window has expired
    if (navigation_command_active) {
        if (millis() - navigation_command_time > NAVIGATION_PROTECTION_MS) {
            navigation_command_active = false;
        }
    }
    
    // WiFi streaming mode - decode MP3 from HTTP stream
    // This must be checked BEFORE is_playing_mp3 since WiFi streaming also uses MP3 decoding
    // IMPORTANT: Check stream_stop_requested first to avoid processing after stop was requested
    // Order of checks: stop request first (fail fast), then stream pointer, then mode flag
    if (!stream_stop_requested && stream != nullptr && wifi_streaming) {
        int frames_filled = 0;
        
        // Safety check: ensure mp3_buf_bytes is within valid range
        if (mp3_buf_bytes < 0 || mp3_buf_bytes > MP3_BUF_SIZE) {
            mp3_buf_bytes = 0;  // Reset to safe value
        }
        
        // During pre-buffering phase, just fill with silence until buffer is ready
        if (stream_prebuffering) {
            for (int i = 0; i < frame_count; i++) {
                frame[i].channel1 = 0;
                frame[i].channel2 = 0;
            }
            return frame_count;
        }
        
        while (frames_filled < frame_count) {
            // If we have PCM samples buffered from previous decode, use them first
            if (pcm_buf_read_pos < pcm_buf_samples) {
                frame[frames_filled].channel1 = pcm_buf[pcm_buf_read_pos * 2];
                frame[frames_filled].channel2 = pcm_buf[pcm_buf_read_pos * 2 + 1];
                pcm_buf_read_pos++;
                frames_filled++;
                
                // We have data - clear underrun state
                if (stream_in_underrun) {
                    stream_in_underrun = false;
                    Serial.println("STREAM_BUFFER_RECOVERED");
                }
            } else {
                // Need to decode more MP3 data
                // NOTE: We NO LONGER do blocking WiFi reads here to avoid WDT reset
                // Buffer refilling is done in the main loop via refillStreamBuffer()
                
                // Signal main loop that we need more data if buffer is getting low
                if (mp3_buf_bytes < STREAM_LOW_WATER_MARK) {
                    stream_needs_data = true;
                }
                
                // Try to decode an MP3 frame from existing buffer
                if (mp3_buf_bytes > 0 && decodeMP3Frame()) {
                    // Successfully decoded - samples are now in pcm_buf
                    // Clear underrun state since we have data
                    if (stream_in_underrun) {
                        stream_in_underrun = false;
                        Serial.println("STREAM_BUFFER_RECOVERED");
                    }
                    // Continue loop to use the new samples
                } else {
                    // No data or decode failed - this is a buffer underrun
                    // Don't immediately end the stream - wait for data with timeout
                    if (!stream_in_underrun) {
                        // Start underrun timer
                        stream_in_underrun = true;
                        stream_underrun_start_time = millis();
                        stream_needs_data = true;  // Urgently signal main loop
                        Serial.println("STREAM_BUFFER_UNDERRUN");
                    }
                    
                    // Check if we've exceeded the underrun timeout
                    uint32_t underrun_duration = millis() - stream_underrun_start_time;
                    bool stream_disconnected = (stream == nullptr);
                    bool underrun_timeout = (underrun_duration > STREAM_UNDERRUN_TIMEOUT_MS);
                    
                    if (stream_disconnected || underrun_timeout) {
                        // Stream truly ended or timed out
                        // CRITICAL: Do NOT call http_client.end() here - it's not thread-safe!
                        // Signal the main loop to clean up the stream instead
                        Serial.print("STREAM_ENDED (");
                        if (stream_disconnected) {
                            Serial.println("disconnected)");
                        } else {
                            Serial.print("underrun timeout after ");
                            Serial.print(underrun_duration);
                            Serial.println("ms)");
                        }
                        // Set flags for main loop to clean up - DO NOT access http_client here
                        stream_stop_requested = true;
                        wifi_streaming = false;
                        is_playing = false;
                        stream_in_underrun = false;
                        stream_prebuffering = false;
                        Serial.println("PLAY_COMPLETE");
                    }
                    // Fill remaining with silence (normal during underrun)
                    for (int i = frames_filled; i < frame_count; i++) {
                        frame[i].channel1 = 0;
                        frame[i].channel2 = 0;
                    }
                    return frame_count;
                }
            }
        }
        return frame_count;
    }
    
    // ==========================================================================
    // Flipper SD streaming mode - decode MP3 data sent from Flipper over UART
    // This mode receives base64-encoded MP3 chunks and decodes them on the fly
    // 
    // Thread safety note: flipper_streaming, flipper_stream_started, and 
    // is_playing are simple shared flags used for signaling between the UART
    // command handler (main loop) and this audio callback. They are currently
    // not declared as volatile and are not protected by explicit synchronization,
    // so the audio callback may observe stale values or miss transient changes.
    // For this best-effort audio streaming path we accept this eventual-consistency
    // behavior, since the worst case is a few extra silence frames or slightly
    // delayed stream-end detection. If stronger guarantees are needed in the
    // future, these flags should be made volatile and/or guarded by proper
    // synchronization primitives.
    // ==========================================================================
    if (flipper_streaming) {
        int frames_filled = 0;
        
        while (frames_filled < frame_count) {
            // If we have PCM samples buffered from previous decode, use them first
            if (pcm_buf_read_pos < pcm_buf_samples) {
                frame[frames_filled].channel1 = pcm_buf[pcm_buf_read_pos * 2];
                frame[frames_filled].channel2 = pcm_buf[pcm_buf_read_pos * 2 + 1];
                pcm_buf_read_pos++;
                frames_filled++;
            } else {
                // Need to decode more MP3 data from Flipper stream buffer
                // Check if we have data in the main mp3_buf (copied from flipper_stream_buffer)
                if (mp3_buf_bytes > 0 && decodeMP3Frame()) {
                    // Success - continue filling frames
                    continue;
                } else {
                    // Decode failed or no data - check if stream has ended
                    // Note: These flag reads may be slightly stale due to lack of memory barriers,
                    // but the worst case is extra silence frames which is acceptable for audio
                    if (flipper_stream_ended && mp3_buf_bytes == 0 && flipper_stream_bytes == 0) {
                        // Stream truly ended - no more data to process
                        flipper_streaming = false;
                        flipper_stream_started = false;
                        is_playing = false;
                        Serial.println("PLAY_COMPLETE");
                        Serial.println("STATUS: Flipper stream playback finished");
                    }
                    // Fill remaining frames with silence
                    for (int i = frames_filled; i < frame_count; i++) {
                        frame[i].channel1 = 0;
                        frame[i].channel2 = 0;
                    }
                    return frame_count;
                }
            }
        }
        return frame_count;
    }
    
    // MP3 playback mode (local files from SD card)
    if (is_playing_mp3) {
        int frames_filled = 0;
        
        while (frames_filled < frame_count) {
            // If we have PCM samples buffered, use them
            if (pcm_buf_read_pos < pcm_buf_samples) {
                // PCM buffer has stereo interleaved samples (L, R, L, R, ...)
                frame[frames_filled].channel1 = pcm_buf[pcm_buf_read_pos * 2];
                frame[frames_filled].channel2 = pcm_buf[pcm_buf_read_pos * 2 + 1];
                pcm_buf_read_pos++;
                frames_filled++;
            } else {
                // Need to decode more MP3 data
                if (!decodeMP3Frame()) {
                    // Decoding failed - could be temporary or end of file
                    consecutive_decode_failures++;
                    
                    // Check if this is likely end of file:
                    // 1. File is not open or not available
                    // 2. We've had many consecutive failures (likely not just temporary)
                    // 3. We're not in the reconnection protection window
                    // 4. We're not in the navigation command protection window
                    // 5. Track has been playing for minimum time to avoid premature skips
                    bool likely_eof = !mp3_file || !mp3_file.available();
                    bool too_many_failures = consecutive_decode_failures >= MAX_DECODE_FAILURES_BEFORE_SKIP;
                    bool can_skip_track = !reconnection_in_progress;
                    bool navigation_protected = navigation_command_active;
                    bool played_long_enough = (millis() - track_start_time) >= MIN_PLAYBACK_TIME_BEFORE_SKIP_MS;
                    
                    if ((likely_eof || too_many_failures) && can_skip_track && !navigation_protected && played_long_enough) {
                        // Try next file
                        consecutive_decode_failures = 0;
                        if (!openNextMP3()) {
                            // No more files
                            is_playing = false;
                            is_playing_mp3 = false;
                            Serial.println("PLAY_COMPLETE");
                            // Fill remaining with silence
                            for (int i = frames_filled; i < frame_count; i++) {
                                frame[i].channel1 = 0;
                                frame[i].channel2 = 0;
                            }
                            return frame_count;
                        }
                    } else {
                        // Temporary failure or in protection window - fill with silence and retry later
                        for (int i = frames_filled; i < frame_count; i++) {
                            frame[i].channel1 = 0;
                            frame[i].channel2 = 0;
                        }
                        return frame_count;
                    }
                } else {
                    // Successful decode - reset failure counter
                    consecutive_decode_failures = 0;
                }
            }
        }
        return frame_count;
    }
    
    // Melody playback mode
    if (is_playing_melody) {
        uint32_t duration = (current_melody == 1) ? MARY_TOTAL_DURATION : BEEP_TOTAL_DURATION;
        if (millis() - play_start_time > duration) {
            is_playing = false;
            is_playing_melody = false;
            current_melody = 0;
            Serial.println("PLAY_COMPLETE");
            for (int i = 0; i < frame_count; i++) {
                frame[i].channel1 = 0;
                frame[i].channel2 = 0;
            }
            return frame_count;
        }
        
        // Generate melody samples
        for (int i = 0; i < frame_count; i++) {
            float freq = getMelodyFrequency(tone_sample_count);
            float sample = 0;
            if (freq > 0) {
                float t = (float)tone_sample_count / (float)SAMPLE_RATE;
                sample = sin(2.0 * PI * freq * t);
            }
            
            int16_t sample_i16 = (int16_t)(sample * 32767.0 * 0.5);
            frame[i].channel1 = sample_i16;
            frame[i].channel2 = sample_i16;
            tone_sample_count++;
        }
        return frame_count;
    }
    
    // Test tone mode
    // Check if we should stop playing
    if (millis() - play_start_time > TEST_TONE_DURATION_MS) {
        is_playing = false;
        Serial.println("PLAY_COMPLETE");
        // Send silence for this frame
        for (int i = 0; i < frame_count; i++) {
            frame[i].channel1 = 0;
            frame[i].channel2 = 0;
        }
        return frame_count;
    }
    
    // Generate sine wave test tone
    for (int i = 0; i < frame_count; i++) {
        float t = (float)tone_sample_count / (float)SAMPLE_RATE;
        float sample = sin(2.0 * PI * TEST_TONE_FREQ * t);
        
        // Convert to 16-bit signed integer (50% volume)
        int16_t sample_i16 = (int16_t)(sample * 32767.0 * 0.5);
        
        // Stereo output (same signal to both channels)
        frame[i].channel1 = sample_i16;
        frame[i].channel2 = sample_i16;
        
        tone_sample_count++;
    }
    
    return frame_count;
}

/**
 * Connect to WiFi network
 * NOTE: ESP32 has limited support for WiFi + Bluetooth A2DP coexistence
 * WiFi init may fail if Bluetooth is using too many resources
 * We initialize WiFi in station mode and attempt connection
 * @param ssid WiFi network SSID
 * @param password WiFi network password
 * @return true if connected successfully, false otherwise
 */
bool connectToWiFi(const String& ssid, const String& password) {
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
    Serial.println("NOTE: WiFi+BT coexistence may cause audio stuttering");
    
    // Disconnect if already connected
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect();
        delay(100);
    }
    
    // Initialize WiFi in station mode
    // This may fail if Bluetooth is already using the radio heavily
    WiFi.mode(WIFI_STA);
    
    // Attempt connection with timeout
    WiFi.begin(ssid.c_str(), password.c_str());
    
    unsigned long start_time = millis();
    int retry_count = 1;  // 1-based for display clarity
    
    while (WiFi.status() != WL_CONNECTED && (millis() - start_time) < WIFI_CONNECT_TIMEOUT_MS) {
        delay(500);
        Serial.print(".");
        
        // If WiFi init completely failed, retry with a brief pause
        // Store status to avoid redundant function calls
        wl_status_t status = WiFi.status();
        if (status == WL_NO_SSID_AVAIL || status == WL_CONNECT_FAILED) {
            if (retry_count <= WIFI_CONNECT_MAX_RETRIES) {
                Serial.println();
                Serial.print("WiFi init issue, retry ");
                Serial.println(retry_count);
                WiFi.disconnect(true);
                delay(500);
                WiFi.mode(WIFI_STA);
                WiFi.begin(ssid.c_str(), password.c_str());
                retry_count++;
            }
        }
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WIFI_CONNECTED:");
        Serial.println(WiFi.localIP());
        wifi_connected = true;
        return true;
    } else {
        Serial.println("WIFI_FAILED");
        Serial.println("ERROR: WiFi connection failed - ESP32 BT/WiFi coexistence limitation");
        Serial.println("TIP: Try reducing BT activity or use ESP32-S3 for better coexistence");
        wifi_connected = false;
        WiFi.mode(WIFI_OFF);  // Turn off WiFi radio to free resources
        return false;
    }
}

/**
 * Disconnect from WiFi network
 */
void disconnectWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect(true);  // true = turn off WiFi radio
        wifi_connected = false;
        Serial.println("WIFI_DISCONNECTED");
    }
    WiFi.mode(WIFI_OFF);  // Turn off WiFi radio to free resources
}

/**
 * Start streaming audio from HTTP/HTTPS URL
 * Supports Icecast and Shoutcast streams
 * Includes pre-buffering to ensure smooth playback
 */
bool startStreaming() {
    if (!wifi_connected) {
        Serial.println("ERROR: WiFi not connected");
        return false;
    }
    
    if (stream_url.length() == 0) {
        Serial.println("ERROR: No stream URL set");
        return false;
    }
    
    // CRITICAL: Stop any existing MP3 playback before starting stream
    // This ensures the audio callback doesn't continue playing the old MP3
    if (is_playing || is_paused || is_playing_mp3) {
        Serial.println("Stopping existing playback before streaming...");
        is_playing = false;
        is_paused = false;
        is_playing_mp3 = false;
        is_playing_melody = false;
        playing_favorites = false;
        current_playback_mode = PLAYBACK_MODE_NONE;
        
        // Close any open MP3 file
        if (mp3_file) {
            mp3_file.close();
        }
        
        // Clear MP3 buffers to prevent stale data
        mp3_buf_bytes = 0;
        pcm_buf_samples = 0;
        pcm_buf_read_pos = 0;
        
        // Clear reconnection state
        was_playing_before_disconnect = false;
        reconnection_in_progress = false;
        reconnection_requested = false;
        consecutive_decode_failures = 0;
        
        // Clear navigation queue
        nav_debounce_active = false;
        pending_nav_offset = 0;
    }
    
    // Clear any pending stop request from previous stream
    stream_stop_requested = false;
    
    Serial.print("Starting stream: ");
    Serial.println(stream_url);
    
    // Initialize HTTP client
    http_client.begin(stream_url);
    http_client.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http_client.setTimeout(10000);  // 10 second timeout
    
    // Request stream
    int http_code = http_client.GET();
    
    if (http_code != HTTP_CODE_OK && http_code != 200) {
        Serial.print("STREAM_ERROR:");
        Serial.println(http_code);
        http_client.end();
        return false;
    }
    
    // Get stream
    stream = http_client.getStreamPtr();
    
    if (stream == nullptr) {
        Serial.println("STREAM_ERROR:NULL");
        http_client.end();
        return false;
    }
    
    Serial.println("STREAM_CONNECTED");
    
    // Clear all buffers before pre-buffering
    mp3_buf_bytes = 0;
    pcm_buf_samples = 0;
    pcm_buf_read_pos = 0;
    stream_buffer_bytes = 0;
    stream_buffer_read_pos = 0;
    stream_in_underrun = false;
    stream_needs_data = false;
    
    // Start pre-buffering phase - audio callback will output silence during this
    stream_prebuffering = true;
    wifi_streaming = true;
    is_playing = true;
    is_playing_mp3 = false;  // Streaming, not MP3 file
    current_playing_file = stream_url;  // Show stream URL as "current file"
    
    // Pre-buffer: Fill MP3 buffer before starting actual playback
    // This prevents buffer underruns at the start of streaming
    Serial.print("Pre-buffering ");
    Serial.print(STREAM_PREBUFFER_SIZE);
    Serial.println(" bytes...");
    Serial.println("STREAM_BUFFERING");
    
    uint32_t prebuffer_start = millis();
    int total_buffered = 0;
    int read_attempts = 0;
    
    while (total_buffered < STREAM_PREBUFFER_SIZE && read_attempts < STREAM_PREBUFFER_MAX_ATTEMPTS) {
        // CRITICAL: Yield to other tasks (including watchdog) to prevent WDT reset
        // The combination of WiFi reads + BT A2DP streaming can starve system tasks
        yield();
        
        if (stream->available()) {
            // Only read up to what we can fit in mp3_buf
            int available_space = MP3_BUF_SIZE - mp3_buf_bytes;
            int bytes_to_read = min(STREAM_PREBUFFER_SIZE - total_buffered, available_space);
            if (bytes_to_read > 0) {
                int bytes_read = stream->readBytes(mp3_buf + mp3_buf_bytes, bytes_to_read);
                if (bytes_read > 0) {
                    mp3_buf_bytes += bytes_read;
                    total_buffered += bytes_read;
                    
                    // Progress indicator every 512 bytes
                    if (total_buffered % 512 < bytes_read) {
                        Serial.print(".");
                    }
                }
            } else {
                // Buffer is full - we're done pre-buffering
                break;
            }
        }
        
        // Small delay to allow network data to arrive
        // Using vTaskDelay instead of delay() for better RTOS integration
        vTaskDelay(pdMS_TO_TICKS(STREAM_UNDERRUN_RETRY_MS));
        read_attempts++;
    }
    
    uint32_t prebuffer_time = millis() - prebuffer_start;
    Serial.println();
    Serial.print("Pre-buffered ");
    Serial.print(total_buffered);
    Serial.print(" bytes in ");
    Serial.print(prebuffer_time);
    Serial.println("ms");
    
    if (total_buffered < STREAM_PREBUFFER_MIN_THRESHOLD) {
        // Not enough data buffered - stream may be unavailable
        Serial.println("STREAM_ERROR:BUFFER_FAILED");
        Serial.print("Only buffered ");
        Serial.print(total_buffered);
        Serial.println(" bytes");
        wifi_streaming = false;
        is_playing = false;
        stream_prebuffering = false;
        http_client.end();
        stream = nullptr;
        return false;
    }
    
    // Pre-buffering complete - enable actual playback
    stream_prebuffering = false;
    stream_needs_data = false;  // Start with fresh state
    last_stream_refill_time = millis();
    
    Serial.println("STREAM_STARTED");
    
    // Report playing state for Flipper UI
    Serial.println("PLAYING");
    Serial.print("PLAYING_FILE:");
    Serial.println(stream_url);
    
    return true;
}

/**
 * Stop streaming audio
 */
void stopStreaming() {
    if (wifi_streaming || stream_stop_requested) {
        http_client.end();
        stream = nullptr;
        wifi_streaming = false;
        is_playing = false;
        stream_prebuffering = false;
        stream_in_underrun = false;
        stream_buffer_bytes = 0;
        stream_buffer_read_pos = 0;
        stream_needs_data = false;
        stream_stop_requested = false;  // Clear the stop request flag
        // Also clear MP3 buffers since streaming uses them
        mp3_buf_bytes = 0;
        pcm_buf_samples = 0;
        pcm_buf_read_pos = 0;
        current_playing_file = "";
        Serial.println("STREAM_STOPPED");
        
        // Disconnect WiFi to free radio resources for Bluetooth
        disconnectWiFi();
    }
}

/**
 * Refill stream buffer from WiFi - called from main loop
 * This is the NON-BLOCKING approach to fetching WiFi data
 * It avoids blocking the audio callback (BTC_TASK) which caused WDT reset
 */
void refillStreamBuffer() {
    // Only refill if we're streaming and have a valid stream
    if (!wifi_streaming || stream == nullptr || stream_prebuffering || stream_stop_requested) {
        return;
    }
    
    // Check if we should refill based on time or urgent need
    // Note: Unsigned subtraction handles millis() overflow correctly (~49 days)
    uint32_t now = millis();
    bool time_to_refill = (now - last_stream_refill_time) >= STREAM_REFILL_INTERVAL_MS;
    bool urgent_refill = stream_needs_data || stream_in_underrun;
    
    if (!time_to_refill && !urgent_refill) {
        return;
    }
    
    // Update refill time
    last_stream_refill_time = now;
    
    // Bounds check: ensure mp3_buf_bytes is valid
    if (mp3_buf_bytes < 0 || mp3_buf_bytes > MP3_BUF_SIZE) {
        mp3_buf_bytes = 0;  // Reset to safe value if corrupted
    }
    
    // Check if buffer needs data
    if (mp3_buf_bytes >= MP3_BUF_SIZE - STREAM_BUFFER_FULL_THRESHOLD) {
        // Buffer is mostly full, no need to refill
        stream_needs_data = false;
        return;
    }
    
    // Yield before WiFi operations to let system tasks run
    yield();
    
    // Read multiple chunks when buffer is low or we're in underrun
    // This helps recover from underrun faster and prevents oscillation
    int max_reads = urgent_refill ? 8 : 2;  // More aggressive when urgent
    
    for (int read_count = 0; read_count < max_reads && stream->available(); read_count++) {
        // Calculate safe bytes to read with bounds validation
        int available_space = MP3_BUF_SIZE - mp3_buf_bytes;
        if (available_space <= STREAM_BUFFER_FULL_THRESHOLD) {
            // Buffer is full enough
            break;
        }
        
        if (available_space > 0 && available_space <= MP3_BUF_SIZE) {
            // Read in chunks
            int bytes_to_read = min(available_space, STREAM_CHUNK_SIZE);
            int bytes_read = stream->readBytes(mp3_buf + mp3_buf_bytes, bytes_to_read);
            if (bytes_read > 0 && bytes_read <= bytes_to_read) {
                mp3_buf_bytes += bytes_read;
                
                // Ensure we don't exceed buffer
                if (mp3_buf_bytes > MP3_BUF_SIZE) {
                    mp3_buf_bytes = MP3_BUF_SIZE;
                    break;
                }
            } else {
                // No more data available right now
                break;
            }
        }
        
        // Yield between reads to let other tasks run
        // Skip yield on the last iteration since we yield after the loop anyway
        if (read_count < max_reads - 1) {
            yield();
        }
    }
    
    // Clear urgent flag if we got enough data
    if (mp3_buf_bytes >= STREAM_LOW_WATER_MARK) {
        stream_needs_data = false;
    }
    
    // Yield after WiFi operations
    yield();
}

/**
 * Callback to provide audio samples for WiFi streaming
 * Called by A2DP source when it needs audio data
 */
int32_t read_stream_data(Frame* data, int32_t frame_count) {
    if (!wifi_streaming || stream == nullptr) {
        return 0;  // No data available
    }
    
    int samples_written = 0;
    
    for (int i = 0; i < frame_count; i++) {
        // Read data from stream into buffer if needed
        if (stream_buffer_read_pos >= stream_buffer_bytes) {
            // Need more data from stream
            if (stream->available()) {
                stream_buffer_bytes = stream->readBytes(stream_buffer, STREAM_BUFFER_SIZE);
                stream_buffer_read_pos = 0;
                
                if (stream_buffer_bytes == 0) {
                    // No more data available
                    break;
                }
            } else {
                // Stream ended or no data available
                break;
            }
        }
        
        // Decode MP3 data from stream buffer
        // For now, we'll pass raw PCM data if available
        // TODO: Implement proper MP3 decoding from stream
        
        // Simple passthrough for now (this will need proper MP3 decoding)
        if (stream_buffer_read_pos + 4 <= stream_buffer_bytes) {
            // Read 16-bit stereo samples (left, right)
            int16_t left = (stream_buffer[stream_buffer_read_pos] << 8) | stream_buffer[stream_buffer_read_pos + 1];
            int16_t right = (stream_buffer[stream_buffer_read_pos + 2] << 8) | stream_buffer[stream_buffer_read_pos + 3];
            
            data[i].channel1 = left;
            data[i].channel2 = right;
            
            stream_buffer_read_pos += 4;
            samples_written++;
        } else {
            break;
        }
    }
    
    return samples_written;
}

/**
 * Setup - Initialize UART, SD Card, and A2DP Source
 */
void setup() {
    // Initialize serial for UART communication with Flipper
    Serial.begin(UART_BAUD);
    while (!Serial) {
        delay(10);
    }
    
    Serial.println("ESP32 BT Audio Firmware Ready");
    Serial.print("Version: ");
    Serial.println(FW_VERSION);
    Serial.print("Mode: ");
    Serial.println(FW_MODE_DESC);
    
    // Load persisted settings from NVS (Preferences)
    // This must be done BEFORE a2dp_source.start() so the BT name is applied
    if (preferences.begin("bt_audio", false)) {  // false = read/write mode
        String saved_name = preferences.getString("bt_name", BT_DEVICE_NAME_DEFAULT);
        size_t name_len = saved_name.length();
        // Check name length is valid (non-empty and fits in buffer with null terminator)
        if (name_len > 0 && name_len < BT_DEVICE_NAME_MAX_LEN) {
            snprintf(bt_device_name, BT_DEVICE_NAME_MAX_LEN, "%s", saved_name.c_str());
            Serial.print("Loaded BT name from NVS: ");
            Serial.println(bt_device_name);
        } else {
            Serial.print("Using default BT name: ");
            Serial.println(bt_device_name);
        }
        preferences.end();
    } else {
        Serial.println("Warning: Could not open NVS preferences, using default BT name");
    }
    
    // Initialize MP3 decoder first (doesn't use hardware)
    mp3_decoder = MP3InitDecoder();
    if (mp3_decoder == NULL) {
        Serial.println("ERROR: MP3 decoder init failed");
    } else {
        Serial.println("MP3 Decoder: OK");
    }
    
    // Set up A2DP Source connection state callback
    a2dp_source.set_on_connection_state_changed([](esp_a2d_connection_state_t state, void* ptr) {
        switch (state) {
            case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
                // Track if we had an active connection (for auto-reconnect detection)
                was_connected_before = is_connected;
                last_disconnect_time = millis();
                
                // Remember if we were playing before disconnect (for auto-resume on reconnect)
                // Check playback state - if we were playing/paused MP3, remember it for reconnection
                if ((is_playing || is_paused) && is_playing_mp3) {
                    was_playing_before_disconnect = true;
                    // Save the MAC address for active reconnection attempts
                    if (current_device_mac.length() > 0) {
                        saved_device_mac_for_reconnect = current_device_mac;
                        reconnection_requested = true;
                        reconnect_attempt_count = 0;  // Reset attempt counter
                        last_reconnect_attempt_time = millis();
                        Serial.println("BT_DISCONNECTED_RECONNECTING");
                        Serial.println("STATUS: Disconnected, will attempt reconnection...");
                    } else {
                        Serial.println("BT_DISCONNECTED");
                        Serial.println("STATUS: Disconnected (no MAC saved for reconnect)");
                    }
                    Serial.print("STATUS_FILE:");
                    Serial.println(current_playing_file);
                    // Reset decode failures - don't let them accumulate during disconnect
                    consecutive_decode_failures = 0;
                } else {
                    was_playing_before_disconnect = false;
                    reconnection_requested = false;
                    Serial.println("BT_DISCONNECTED");
                    Serial.println("STATUS: Disconnected");
                }
                is_connected = false;
                connection_pending = false;
                if (target_mac.length() > 0) {
                    Serial.println("FAILED");
                    target_mac = "";
                }
                break;
                
            case ESP_A2D_CONNECTION_STATE_CONNECTING:
                Serial.println("STATUS: Connecting...");
                break;
                
            case ESP_A2D_CONNECTION_STATE_CONNECTED: {
                is_connected = true;
                connection_pending = false;
                // Update current device MAC from either target_mac or saved reconnect MAC
                if (target_mac.length() > 0) {
                    current_device_mac = target_mac;
                } else if (saved_device_mac_for_reconnect.length() > 0) {
                    current_device_mac = saved_device_mac_for_reconnect;
                }
                
                // Clear reconnection request state
                reconnection_requested = false;
                reconnect_attempt_count = 0;
                
                // Check if this is an auto-reconnection (within the reconnect window after disconnect)
                // Note: Unsigned subtraction handles millis() overflow correctly (~49 days)
                bool is_auto_reconnect = was_connected_before && 
                                         (millis() - last_disconnect_time < AUTO_RECONNECT_WINDOW_MS);
                
                if (is_auto_reconnect) {
                    Serial.println("BT_RECONNECTED");  // Notify Flipper about auto-reconnection
                    Serial.println("STATUS: Auto-reconnected");
                } else {
                    Serial.println("CONNECTED");
                    Serial.println("STATUS: Connected");
                }
                
                // CRITICAL FIX: Apply initial volume IMMEDIATELY upon connection
                // This prevents the 3-5 second delay where audio plays at max volume
                // before the volume setting takes effect. Many BT headphones send their
                // own volume commands via AVRCP right after connection, so we must:
                // 1. Set our desired volume immediately
                // 2. Start the volume protection window to reinforce it
                a2dp_source.set_volume(initial_volume);
                Serial.print("INIT_VOLUME_APPLIED:");
                Serial.println(initial_volume);
                first_play_volume_set = true;  // Mark as set so playback functions don't re-apply
                
                // Start volume protection window IMMEDIATELY on connection
                // This ensures we reinforce our volume setting against AVRCP overrides
                // from the remote device during the critical post-connection period
                volume_protection_active = true;
                volume_protection_start_time = millis();
                
                // Handle auto-resume after reconnection
                // was_playing_before_disconnect indicates we were playing MP3 before disconnect
                if (was_playing_before_disconnect) {
                    // We were playing before disconnect - set up protection window
                    // to prevent spurious track skips during stream restart
                    reconnection_in_progress = true;
                    reconnection_time = millis();
                    consecutive_decode_failures = 0;  // Reset failure counter
                    
                    // Ensure playback state is set for resumption
                    is_playing = true;
                    is_playing_mp3 = true;
                    is_paused = false;
                    
                    // Also reset the track start time to prevent premature skips
                    track_start_time = millis();
                    
                    // CRITICAL FIX: Ensure MP3 file is open after reconnection
                    // The file handle may have become invalid during disconnect
                    // Try to reopen current track if file isn't available
                    if (!mp3_file || !mp3_file.available()) {
                        Serial.println("STATUS: Reopening MP3 file after reconnect...");
                        // The file handle is invalid - try to reopen using openMP3AtIndex()
                        // This is cleaner than duplicating the file open logic
                        if (mp3_file_count > 0) {
                            // Try current index first, then fall back to beginning
                            if (openMP3AtIndex(current_mp3_index)) {
                                Serial.println("STATUS: Reopened current track successfully");
                            } else if (openMP3AtIndex(0)) {
                                Serial.println("STATUS: Restarted playback from beginning");
                            } else {
                                // Give up - can't resume
                                is_playing = false;
                                is_playing_mp3 = false;
                                Serial.println("ERROR: Could not resume playback");
                            }
                        }
                    }
                    
                    Serial.println("STATUS: Resuming playback after reconnect");
                    Serial.println("PLAYBACK_RESUMED");
                    Serial.print("PLAYING_FILE:");
                    Serial.println(current_playing_file);
                }
                was_playing_before_disconnect = false;
                was_connected_before = false;  // Reset for next cycle
                break;
            }
                
            case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
                Serial.println("STATUS: Disconnecting");
                break;
                
            default:
                break;
        }
    });

    // Start A2DP Source with device name and audio callback
    // This initializes the Bluetooth stack in SOURCE mode
    Serial.print("Starting A2DP Source as: ");
    Serial.println(bt_device_name);
    
    // Enable auto-reconnect to last paired device
    // This helps maintain connection when there are brief disconnections
    a2dp_source.set_auto_reconnect(true);
    
    a2dp_source.start(bt_device_name, get_audio_data);
    
    // Give the Bluetooth stack time to fully initialize
    delay(BT_INIT_DELAY_MS);
    
    // Register GAP callback for discovery events AFTER A2DP starts
    if(esp_bt_gap_register_callback(onGapEvent) == ESP_OK) {
        gap_callback_registered = true;
        // Set to connectable but not discoverable (we connect TO others)
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
        Serial.println("GAP callback registered successfully");
    } else {
        Serial.println("ERROR: GAP callback registration failed");
    }
    
    Serial.println("A2DP Source Started");
    
    // Initialize SD card AFTER Bluetooth to avoid SPI conflicts
    // Some ESP32 boards have issues if SD is initialized before BT
    Serial.println("Initializing SD card...");
    sd_initialized = initSDCard();
    if (sd_initialized) {
        Serial.print("SD Card: OK (");
        Serial.print(sd_using_mmc ? "SD_MMC" : "SPI");
        Serial.println(" mode)");
        scanMP3Files();
        Serial.print("MP3 Files found: ");
        Serial.println(mp3_file_count);
    } else {
        Serial.println("SD Card: Not available at startup");
        Serial.println("SD Card: Will retry on PLAY_MP3 command");
    }
    Serial.println("BT_AUDIO_READY");
    Serial.println("READY");
}

/**
 * Main loop - Process incoming UART commands
 */
void loop() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        
        if (cmd.length() > 0) {
            processCommand(cmd);
        }
    }
    
    // Check for connection timeout
    if (connection_pending && !is_connected) {
        if (millis() - connection_start_time > CONNECTION_TIMEOUT_MS) {
            Serial.println("FAILED");
            Serial.println("ERROR: Connection timeout");
            connection_pending = false;
            target_mac = "";
        }
    }
    
    // Navigation debounce: Execute queued navigation after timeout
    // This prevents BT disconnection from rapid successive track changes
    if (nav_debounce_active && pending_nav_offset != 0) {
        // Note: Unsigned subtraction handles millis() overflow correctly (~49 days)
        uint32_t time_since_last_nav = millis() - last_nav_command_time;
        if (time_since_last_nav >= NAV_QUEUE_TIMEOUT_MS) {
            executeQueuedNavigation();
        }
    }
    
    // Volume protection: Reinforce initial volume during protection window
    // This prevents Bluetooth remote device from overriding our initial volume setting
    // Many BT devices send ESP_AVRC_RN_VOLUME_CHANGE shortly after connection with MAX volume
    // CRITICAL: Check protection even when NOT playing - headphones send volume commands
    // immediately after connection, before any audio playback starts
    if (volume_protection_active && is_connected) {
        // Note: Unsigned subtraction handles millis() overflow correctly (~49 days)
        uint32_t elapsed = millis() - volume_protection_start_time;
        if (elapsed < VOLUME_PROTECTION_WINDOW_MS) {
            // Check if current volume differs from our initial setting
            int current_vol = a2dp_source.get_volume();
            if (current_vol != initial_volume) {
                // Remote device tried to override our volume - re-apply
                a2dp_source.set_volume(initial_volume);
                Serial.print("VOLUME_PROTECTED:");
                Serial.println(initial_volume);
            }
        } else {
            // Protection window expired - allow normal volume control
            volume_protection_active = false;
        }
    }
    
    // Active reconnection: Attempt to reconnect if we were playing and got disconnected
    // This is more aggressive than the library's built-in auto-reconnect
    if (reconnection_requested && !is_connected && !connection_pending) {
        uint32_t time_since_last_attempt = millis() - last_reconnect_attempt_time;
        
        // Check if we've exceeded max attempts or time window
        bool exceeded_max_attempts = reconnect_attempt_count >= MAX_RECONNECT_ATTEMPTS;
        uint32_t time_since_disconnect = millis() - last_disconnect_time;
        bool exceeded_time_window = time_since_disconnect >= AUTO_RECONNECT_WINDOW_MS;
        
        if (exceeded_max_attempts || exceeded_time_window) {
            // Give up on reconnection
            Serial.println("RECONNECT_TIMEOUT");
            if (exceeded_max_attempts) {
                Serial.print("STATUS: Reconnection failed after ");
                Serial.print(reconnect_attempt_count);
                Serial.println(" attempts");
            } else {
                Serial.println("STATUS: Reconnection timeout, giving up");
            }
            reconnection_requested = false;
            was_playing_before_disconnect = false;
            // Stop playback state since we couldn't reconnect
            is_playing = false;
            is_playing_mp3 = false;
            is_paused = false;
        } else if (time_since_last_attempt >= RECONNECT_ATTEMPT_INTERVAL_MS) {
            // Attempt reconnection
            if (saved_device_mac_for_reconnect.length() > 0) {
                reconnect_attempt_count++;
                Serial.print("STATUS: Reconnection attempt ");
                Serial.print(reconnect_attempt_count);
                Serial.print("/");
                Serial.println(MAX_RECONNECT_ATTEMPTS);
                
                esp_bd_addr_t bda;
                unsigned int values[6];
                if (sscanf(saved_device_mac_for_reconnect.c_str(), "%2x:%2x:%2x:%2x:%2x:%2x", 
                           &values[0], &values[1], &values[2],
                           &values[3], &values[4], &values[5]) == 6) {
                    for (int i = 0; i < 6; i++) {
                        bda[i] = (uint8_t)values[i];
                    }
                    
                    target_mac = saved_device_mac_for_reconnect;
                    connection_pending = true;
                    connection_start_time = millis();
                    
                    if (a2dp_source.connect_to(bda)) {
                        Serial.println("RECONNECT_ATTEMPT");
                        Serial.print("Target: ");
                        Serial.println(saved_device_mac_for_reconnect);
                    } else {
                        Serial.println("RECONNECT_ATTEMPT_FAILED");
                        connection_pending = false;
                    }
                }
            }
            last_reconnect_attempt_time = millis();
        }
    }
    
    // CRITICAL: Handle stream stop requests from audio callback
    // The audio callback cannot safely call http_client.end() (not thread-safe)
    // So it sets stream_stop_requested flag and we clean up here in main loop
    if (stream_stop_requested) {
        Serial.println("STREAM_CLEANUP: Cleaning up stream from main loop");
        http_client.end();
        stream = nullptr;
        stream_stop_requested = false;
        // Also clear any other streaming state that might have been partially set
        wifi_streaming = false;
        stream_prebuffering = false;
        stream_in_underrun = false;
        stream_needs_data = false;
        mp3_buf_bytes = 0;
        pcm_buf_samples = 0;
        pcm_buf_read_pos = 0;
        stream_buffer_bytes = 0;
        stream_buffer_read_pos = 0;
        current_playing_file = "";
        
        // Disconnect WiFi and restart Bluetooth after stream ends
        // ESP32 cannot run WiFi and BT simultaneously, so we restore BT for normal operation
        disconnectWiFi();
    }
    
    // WiFi streaming: Refill buffer from WiFi in main loop (non-blocking)
    // This is the critical fix for WDT reset during streaming
    // Previously, WiFi reads were done in the audio callback which blocked BTC_TASK
    // Note: refillStreamBuffer() has its own guards, so we just need basic check here
    if (wifi_streaming) {
        refillStreamBuffer();
    }
    
    // Adaptive delay: use minimal delay when streaming to be responsive to buffer needs,
    // use normal delay otherwise for power efficiency
    // Using 1ms during streaming for maximum responsiveness to buffer needs
    if (wifi_streaming && !stream_prebuffering && !stream_stop_requested) {
        delay(1);  // Minimal delay during streaming for fastest buffer refills
    } else {
        delay(10);  // Normal delay
    }
}

/**
 * Process incoming UART commands from Flipper
 * 
 * TODO: Consider refactoring into a command dispatch table for better maintainability
 * Current simple if-else chain is adequate for the commands, but could be improved
 * as the protocol expands.
 */
void processCommand(String cmd) {
    // Echo received command for debugging
    Serial.print("CMD_RECV:");
    Serial.println(cmd);
    
    if (cmd == "VERSION") {
        Serial.println("BT_AUDIO_READY");
        Serial.print("VERSION:");
        Serial.println(FW_VERSION);
        Serial.print("MODE:");
        Serial.println(FW_MODE_CODE);
        Serial.print("SD_CARD:");
        Serial.println(sd_initialized ? "YES" : "NO");
        Serial.print("MP3_FILES:");
        Serial.println(mp3_file_count);
    }
    else if (cmd == "PING") {
        // Simple alive check
        Serial.println("PONG");
    }
    else if (cmd == "STATUS") {
        // Report current playback status (for background mode re-entry)
        Serial.print("STATUS_CONNECTED:");
        Serial.println(is_connected ? "YES" : "NO");
        Serial.print("STATUS_PLAYING:");
        Serial.println(is_playing ? "YES" : "NO");
        Serial.print("STATUS_PAUSED:");
        Serial.println(is_paused ? "YES" : "NO");
        if (current_playing_file.length() > 0 && (is_playing || is_paused)) {
            Serial.print("STATUS_FILE:");
            Serial.println(current_playing_file);
        }
        Serial.println("STATUS_COMPLETE");
    }
    else if (cmd == "SCAN") {
        scanDevices();
    } 
    else if (cmd.startsWith("CONNECT ")) {
        String mac = cmd.substring(8);
        connectToDevice(mac);
    }
    else if (cmd.startsWith("TX_POWER:")) {
        // Set Bluetooth transmit power for better range
        String power_level = cmd.substring(9);
        power_level.trim();
        
        esp_power_level_t power;
        if (power_level == "LOW") {
            power = ESP_PWR_LVL_N12;  // -12 dBm
            Serial.println("TX_POWER:SET:LOW");
        } else if (power_level == "MEDIUM") {
            power = ESP_PWR_LVL_N0;   // 0 dBm
            Serial.println("TX_POWER:SET:MEDIUM");
        } else if (power_level == "HIGH") {
            power = ESP_PWR_LVL_P6;   // +6 dBm
            Serial.println("TX_POWER:SET:HIGH");
        } else if (power_level == "MAX") {
            power = ESP_PWR_LVL_P9;   // +9 dBm (default)
            Serial.println("TX_POWER:SET:MAX");
        } else {
            Serial.println("ERROR: Invalid TX power level");
            return;
        }
        
        // Set BR/EDR transmit power (min and max to same value for consistent power)
        esp_err_t err = esp_bredr_tx_power_set(power, power);
        if (err == ESP_OK) {
            Serial.println("TX_POWER:OK");
        } else {
            Serial.print("TX_POWER:ERROR:");
            Serial.println(err, HEX);
        }
    }
    else if (cmd == "VOL_UP") {
        // Volume up via AVRCP (increases by ~6% per step, VOLUME_STEP=8 out of 127)
        if (is_connected) {
            int current_vol = a2dp_source.get_volume();
            int new_vol = std::min(current_vol + VOLUME_STEP, VOLUME_MAX);
            a2dp_source.set_volume(new_vol);
            Serial.print("VOLUME:");
            Serial.println(new_vol);
        } else {
            Serial.println("ERROR: Not connected");
        }
    }
    else if (cmd == "VOL_DOWN") {
        // Volume down via AVRCP (decreases by ~6% per step, VOLUME_STEP=8 out of 127)
        // Fixed: Now properly decrements by VOLUME_STEP instead of jumping to 0
        if (is_connected) {
            int current_vol = a2dp_source.get_volume();
            int new_vol = std::max(current_vol - VOLUME_STEP, VOLUME_MIN);
            a2dp_source.set_volume(new_vol);
            Serial.print("VOLUME:");
            Serial.println(new_vol);
        } else {
            Serial.println("ERROR: Not connected");
        }
    }
    else if (cmd.startsWith("SET_INIT_VOL:")) {
        // Set the initial volume for first play (from Flipper settings)
        int vol = cmd.substring(13).toInt();
        if (vol >= VOLUME_MIN && vol <= VOLUME_MAX) {
            initial_volume = vol;
            Serial.print("INIT_VOL_SET:");
            Serial.println(initial_volume);
            
            // If connected but haven't started playing yet, reset the flag
            // so the new initial volume will be applied on next play
            if (is_connected && !is_playing && !is_paused) {
                first_play_volume_set = false;
            }
        } else {
            Serial.println("ERROR: Invalid volume (0-127)");
        }
    }
    else if (cmd.startsWith("SET_BT_NAME:")) {
        // Set Bluetooth device name and persist to NVS
        // The new name will take effect on next ESP32 restart
        String new_name = cmd.substring(12);
        new_name.trim();
        size_t name_len = new_name.length();
        if (name_len > 0 && name_len < BT_DEVICE_NAME_MAX_LEN) {
            // Use snprintf for safer string handling
            snprintf(bt_device_name, BT_DEVICE_NAME_MAX_LEN, "%s", new_name.c_str());
            bt_name_changed = true;
            
            // Save to NVS so it persists across reboots
            if (preferences.begin("bt_audio", false)) {
                preferences.putString("bt_name", bt_device_name);
                preferences.end();
                Serial.print("BT_NAME_SET:");
                Serial.println(bt_device_name);
                Serial.println("BT_NAME_SAVED");  // Confirm saved to NVS
            } else {
                Serial.print("BT_NAME_SET:");
                Serial.println(bt_device_name);
                Serial.println("WARNING: Could not save to NVS");
            }
            Serial.println("NOTE: Restart ESP32 to apply new name");
        } else {
            Serial.println("ERROR: Invalid name (1-31 characters)");
        }
    }
    else if (cmd.startsWith("EQ_BASS:")) {
        // Set EQ bass gain in dB
        int val = cmd.substring(8).toInt();
        if (val >= EQ_MIN && val <= EQ_MAX) {
            eq_bass = val;
            updateEqGains();
            Serial.print("EQ_BASS_SET:");
            Serial.println(eq_bass);
        } else {
            Serial.println("ERROR: Invalid EQ value (-10 to +10)");
        }
    }
    else if (cmd.startsWith("EQ_MID:")) {
        // Set EQ mid gain in dB
        int val = cmd.substring(7).toInt();
        if (val >= EQ_MIN && val <= EQ_MAX) {
            eq_mid = val;
            updateEqGains();
            Serial.print("EQ_MID_SET:");
            Serial.println(eq_mid);
        } else {
            Serial.println("ERROR: Invalid EQ value (-10 to +10)");
        }
    }
    else if (cmd.startsWith("EQ_TREBLE:")) {
        // Set EQ treble gain in dB
        int val = cmd.substring(10).toInt();
        if (val >= EQ_MIN && val <= EQ_MAX) {
            eq_treble = val;
            updateEqGains();
            Serial.print("EQ_TREBLE_SET:");
            Serial.println(eq_treble);
        } else {
            Serial.println("ERROR: Invalid EQ value (-10 to +10)");
        }
    }
    else if (cmd.startsWith("SHUFFLE:")) {
        // Set shuffle/random play mode
        String mode = cmd.substring(8);
        mode.trim();
        if (mode == "ON") {
            shuffle_mode = true;
            Serial.println("SHUFFLE:ON");
        } else if (mode == "OFF") {
            shuffle_mode = false;
            Serial.println("SHUFFLE:OFF");
        } else {
            Serial.println("ERROR: Invalid shuffle mode (ON/OFF)");
        }
    }
    else if (cmd.startsWith("CONTINUOUS:")) {
        // Set continuous play mode for file browser
        // OFF = repeat single file, ON = play through directory
        String mode = cmd.substring(11);
        mode.trim();
        if (mode == "ON") {
            continuous_play_mode = true;
            Serial.println("CONTINUOUS:ON");
        } else if (mode == "OFF") {
            continuous_play_mode = false;
            Serial.println("CONTINUOUS:OFF");
        } else {
            Serial.println("ERROR: Invalid continuous mode (ON/OFF)");
        }
    }
    else if (cmd == "PLAY") {
        if (is_connected) {
            playTestTone();
        } else {
            Serial.println("ERROR: Not connected");
        }
    }
    else if (cmd == "PLAY_MP3") {
        if (is_connected) {
            playMP3FromSD();
        } else {
            Serial.println("ERROR: Not connected");
        }
    }
    else if (cmd == "PLAY_PLAYLIST") {
        // Play only from playlist.m3u file (not all files)
        if (is_connected) {
            playPlaylistOnly();
        } else {
            Serial.println("ERROR: Not connected");
        }
    }
    else if (cmd == "PLAY_PLAYLIST2") {
        // Play only from playlist2.m3u file
        if (is_connected) {
            playPlaylist2Only();
        } else {
            Serial.println("ERROR: Not connected");
        }
    }
    else if (cmd.startsWith("PLAY_M3U:")) {
        // Play from a custom M3U file (selected via file browser)
        // Format: PLAY_M3U:<full_path_to_m3u>
        if (is_connected) {
            String m3u_path = cmd.substring(9);
            m3u_path.trim();
            playCustomM3U(m3u_path);
        } else {
            Serial.println("ERROR: Not connected");
        }
    }
    else if (cmd.startsWith("PLAY_FAVORITES:")) {
        // Start receiving favorites list
        // Format: PLAY_FAVORITES:<count>
        int count = cmd.substring(15).toInt();
        if (count > 0 && count <= MAX_MP3_FILES) {
            favorites_receiving = count;
            favorites_count = 0;
            playing_favorites = false;
            Serial.print("FAVORITES_RECEIVING:");
            Serial.println(count);
        } else {
            Serial.println("ERROR: Invalid favorites count");
        }
    }
    else if (cmd.startsWith("FAV:")) {
        // Receive a favorite filename
        // Format: FAV:<filename>
        if (favorites_receiving > 0 && favorites_count < MAX_MP3_FILES) {
            String filename = cmd.substring(4);
            filename.trim();
            favorites_list[favorites_count] = filename;
            favorites_count++;
            Serial.print("FAV_ADDED:");
            Serial.println(filename);
        }
    }
    else if (cmd == "FAV_END") {
        // End of favorites list - start playing
        favorites_receiving = 0;
        if (favorites_count > 0) {
            if (is_connected) {
                playFavorites();
            } else {
                Serial.println("ERROR: Not connected");
            }
        } else {
            Serial.println("NO_FAVORITES");
        }
    }
    else if (cmd == "PLAY_JINGLE") {
        if (is_connected) {
            playMelody(1);  // Mary Had a Little Lamb
        } else {
            Serial.println("ERROR: Not connected");
        }
    }
    else if (cmd == "PLAY_TREK") {
        // PLAY_TREK is deprecated - now maps to PLAY_JINGLE
        Serial.println("WARNING: PLAY_TREK is deprecated, use PLAY_JINGLE instead");
        if (is_connected) {
            playMelody(1);  // Mary Had a Little Lamb (replacement for Trek)
        } else {
            Serial.println("ERROR: Not connected");
        }
    }
    else if (cmd == "PLAY_BEEPS") {
        if (is_connected) {
            playMelody(2);  // Simple 3-tone beep sequence
        } else {
            Serial.println("ERROR: Not connected");
        }
    }
    else if (cmd == "PAUSE") {
        pausePlayback();
    }
    else if (cmd == "RESUME") {
        resumePlayback();
    }
    else if (cmd == "NEXT") {
        nextTrack();
    }
    else if (cmd == "PREV") {
        prevTrack();
    }
    else if (cmd == "RESTART") {
        restartTrack();
    }
    else if (cmd == "SEEK_FWD") {
        seekForward();
    }
    else if (cmd == "SEEK_BACK") {
        seekBackward();
    }
    else if (cmd == "STOP") {
        stopPlayback();
    }
    else if (cmd.startsWith("WIFI_CONNECT:")) {
        // Connect to WiFi network
        // Format: WIFI_CONNECT:<ssid>:<password>
        String params = cmd.substring(13);  // Skip "WIFI_CONNECT:"
        int colon_pos = params.indexOf(':');
        
        if (colon_pos == -1) {
            Serial.println("ERROR: Invalid WiFi command format");
            return;
        }
        
        wifi_ssid = params.substring(0, colon_pos);
        wifi_password = params.substring(colon_pos + 1);
        
        if (connectToWiFi(wifi_ssid, wifi_password)) {
            // WiFi connected successfully - message already sent by function
        } else {
            // Connection failed - message already sent by function
        }
    }
    else if (cmd == "WIFI_STATUS") {
        // Report WiFi connection status
        if (wifi_connected && WiFi.status() == WL_CONNECTED) {
            Serial.print("WIFI_STATUS:CONNECTED:");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("WIFI_STATUS:DISCONNECTED");
        }
    }
    else if (cmd == "WIFI_DISCONNECT") {
        // Disconnect from WiFi
        disconnectWiFi();
    }
    else if (cmd.startsWith("STREAM_URL:")) {
        // Set stream URL
        stream_url = cmd.substring(11);  // Skip "STREAM_URL:"
        stream_url.trim();
        Serial.print("STREAM_URL_SET:");
        Serial.println(stream_url);
    }
    else if (cmd == "STREAM_START") {
        // Start streaming from URL
        if (startStreaming()) {
            // Switch audio callback to streaming mode
            // For now, streaming uses the same read_data_stream callback
            // TODO: Implement proper streaming audio callback
            Serial.println("PLAYING");
            Serial.print("PLAYING_FILE:");
            Serial.println(stream_url);
        }
    }
    else if (cmd == "STREAM_STOP") {
        // Stop streaming
        stopStreaming();
    }
    else if (cmd == "DISCONNECT") {
        disconnectDevice();
    }
    else if (cmd == "RECONNECT") {
        // Force reconnection to last connected device
        // Useful when auto-reconnect fails or connection is stale
        if (current_device_mac.length() > 0) {
            Serial.println("STATUS: Forcing reconnection...");
            
            // If still connected (stale connection), disconnect first
            if (is_connected) {
                // Remember playback state before disconnect
                was_playing_before_disconnect = (is_playing || is_paused) && is_playing_mp3;
                was_connected_before = true;
                last_disconnect_time = millis();
                
                a2dp_source.disconnect();
                delay(500);  // Brief delay to allow disconnect to complete
            }
            
            // Attempt reconnection to last device
            esp_bd_addr_t bda;
            unsigned int values[6];
            if (sscanf(current_device_mac.c_str(), "%2x:%2x:%2x:%2x:%2x:%2x", 
                       &values[0], &values[1], &values[2],
                       &values[3], &values[4], &values[5]) == 6) {
                for (int i = 0; i < 6; i++) {
                    bda[i] = (uint8_t)values[i];
                }
                
                target_mac = current_device_mac;
                connection_pending = true;
                connection_start_time = millis();
                
                if (a2dp_source.connect_to(bda)) {
                    Serial.println("RECONNECTING");
                    Serial.print("Target: ");
                    Serial.println(current_device_mac);
                } else {
                    Serial.println("ERROR: Could not initiate reconnection");
                    connection_pending = false;
                }
            } else {
                Serial.println("ERROR: Invalid saved MAC address");
            }
        } else {
            Serial.println("ERROR: No device to reconnect to");
        }
    }
    // ==========================================================================
    // Flipper SD Streaming Commands
    // These commands enable streaming MP3 data from Flipper's SD card over UART
    // ==========================================================================
    else if (cmd == "PLAY_MP3_FLIPPER") {
        // Start Flipper SD streaming mode
        if (is_connected) {
            // Stop any current playback first
            is_playing = false;
            is_playing_mp3 = false;
            wifi_streaming = false;
            is_playing_melody = false;
            
            // Close any currently open MP3 file to avoid resource leaks
            // This ensures the SD card file is released before starting Flipper streaming
            if (mp3_file) {
                mp3_file.close();
            }
            
            // Initialize Flipper streaming state
            flipper_streaming = true;
            flipper_stream_bytes = 0;
            flipper_stream_started = false;
            flipper_stream_ended = false;
            flipper_current_filename = "";
            
            // Clear audio buffers
            mp3_buf_bytes = 0;
            pcm_buf_samples = 0;
            pcm_buf_read_pos = 0;
            consecutive_decode_failures = 0;
            resetEqStates();
            
            Serial.println("FLIPPER_STREAM_READY");
            Serial.println("STATUS: Ready to receive MP3 data from Flipper");
        } else {
            Serial.println("ERROR: Not connected to Bluetooth device");
        }
    }
    else if (cmd.startsWith("FLIPPER_FILE:")) {
        // Optional: Set filename being streamed (for display purposes)
        flipper_current_filename = cmd.substring(13);
        Serial.print("FLIPPER_FILE_ACK:");
        Serial.println(flipper_current_filename);
    }
    else if (cmd.startsWith("MP3_CHUNK:")) {
        // Receive base64-encoded MP3 chunk from Flipper
        if (flipper_streaming) {
            String chunk_data = cmd.substring(10);
            
            // Calculate available space in Flipper stream buffer
            int available_space = FLIPPER_STREAM_BUFFER_SIZE - flipper_stream_bytes;
            
            if (available_space <= 0) {
                Serial.println("ERROR: Flipper buffer full");
                return;
            }
            
            // Decode base64 chunk directly into Flipper stream buffer
            int decoded_len = decodeBase64(chunk_data, 
                                           flipper_stream_buffer + flipper_stream_bytes,
                                           available_space);
            
            if (decoded_len > 0) {
                flipper_stream_bytes += decoded_len;
                
                // Copy data to main MP3 buffer when it has space
                // This is done here instead of audio callback to avoid blocking the audio thread
                // Note: Future optimization could use a circular buffer to avoid memmove overhead
                if (mp3_buf_bytes < MP3_BUF_REFILL_THRESHOLD && flipper_stream_bytes > 0) {
                    int copy_size = min(flipper_stream_bytes, 
                                       (int)(MP3_BUF_SIZE - mp3_buf_bytes));
                    memcpy(mp3_buf + mp3_buf_bytes, flipper_stream_buffer, copy_size);
                    mp3_buf_bytes += copy_size;
                    
                    // Shift remaining data in Flipper buffer
                    // Performance note: memmove is O(n) but typically small since we consume
                    // data as fast as it arrives. For high-bitrate streams, consider circular buffer.
                    if (copy_size < flipper_stream_bytes) {
                        memmove(flipper_stream_buffer, 
                               flipper_stream_buffer + copy_size,
                               flipper_stream_bytes - copy_size);
                    }
                    flipper_stream_bytes -= copy_size;
                }
                
                // Start playback once we have minimum buffer
                if (!flipper_stream_started && mp3_buf_bytes >= FLIPPER_STREAM_MIN_BUFFER) {
                    flipper_stream_started = true;
                    is_playing = true;
                    play_start_time = millis();
                    track_start_time = millis();
                    Serial.println("PLAYING");
                    Serial.println("STATUS: Flipper stream playback started");
                    if (flipper_current_filename.length() > 0) {
                        current_playing_file = flipper_current_filename;
                        Serial.print("PLAYING_FILE:");
                        Serial.println(current_playing_file);
                    }
                }
                
                Serial.println("CHUNK_ACK");
            } else if (decoded_len == -1) {
                Serial.println("ERROR: Chunk decode overflow");
            } else {
                Serial.println("ERROR: Chunk decode failed");
            }
        } else {
            Serial.println("ERROR: Not in Flipper streaming mode");
        }
    }
    else if (cmd == "STREAM_END") {
        // Flipper signals end of stream
        if (flipper_streaming) {
            flipper_stream_ended = true;
            Serial.println("STREAM_END_ACK");
            Serial.println("STATUS: Flipper stream will end after buffer drains");
        } else {
            Serial.println("ERROR: Not in Flipper streaming mode");
        }
    }
    else if (cmd == "FLIPPER_STOP") {
        // Immediately stop Flipper streaming
        if (flipper_streaming) {
            flipper_streaming = false;
            flipper_stream_started = false;
            flipper_stream_ended = true;
            flipper_stream_bytes = 0;
            is_playing = false;
            mp3_buf_bytes = 0;
            pcm_buf_samples = 0;
            pcm_buf_read_pos = 0;
            Serial.println("FLIPPER_STOPPED");
            Serial.println("STATUS: Flipper streaming stopped");
        } else {
            Serial.println("ERROR: Not in Flipper streaming mode");
        }
    }
    // ==========================================================================
    // ESP32 SD Card File Listing Commands
    // These commands allow Flipper to remotely browse ESP32's SD card
    // ==========================================================================
    else if (cmd == "LIST_FILES" || cmd.startsWith("LIST_FILES:")) {
        // List MP3 files on ESP32's SD card for remote browsing from Flipper
        // Format: LIST_FILES or LIST_FILES:<path>
        // Response format: FILE:<full_path>|<filename>|<type>
        //   type: "f" for file, "d" for directory
        // Ends with: LIST_FILES_END
        
        // Try to initialize SD card if not already done
        if (!sd_initialized) {
            Serial.println("SD card not initialized, attempting late initialization...");
            if (!tryInitSDCard()) {
                Serial.println("LIST_FILES_ERROR:SD_NOT_INITIALIZED");
                Serial.println("LIST_FILES_END");
                return;
            }
        }
        
        // Determine path to scan
        String scan_path;
        if (cmd.startsWith("LIST_FILES:")) {
            scan_path = cmd.substring(11);
            scan_path.trim();
        } else {
            // Default to music folder
            scan_path = findMusicFolder();
            if (scan_path.length() == 0) {
                Serial.println("LIST_FILES_ERROR:NO_MUSIC_FOLDER");
                Serial.println("LIST_FILES_END");
                return;
            }
        }
        
        Serial.print("LIST_FILES_PATH:");
        Serial.println(scan_path);
        
        fs::FS& fs = getSDFS();
        File root = fs.open(scan_path);
        
        if (!root) {
            Serial.println("LIST_FILES_ERROR:OPEN_FAILED");
            Serial.println("LIST_FILES_END");
            return;
        }
        
        if (!root.isDirectory()) {
            Serial.println("LIST_FILES_ERROR:NOT_DIRECTORY");
            root.close();
            Serial.println("LIST_FILES_END");
            return;
        }
        
        // Remove trailing slash from scan_path if present for clean path concatenation
        String clean_path = scan_path;
        while (clean_path.endsWith("/") && clean_path.length() > 1) {
            clean_path = clean_path.substring(0, clean_path.length() - 1);
        }
        
        // First pass: collect and list directories
        File file = root.openNextFile();
        int file_count = 0;
        int dir_count = 0;
        
        while (file && (file_count + dir_count) < MAX_MP3_FILES) {
            String filename = file.name();
            if (file.isDirectory()) {
                // Skip hidden directories
                if (!filename.startsWith(".")) {
                    String full_path = clean_path + "/" + filename;
                    Serial.print("FILE:");
                    Serial.print(full_path);
                    Serial.print("|");
                    Serial.print(filename);
                    Serial.println("|d");
                    dir_count++;
                }
            }
            file.close();
            file = root.openNextFile();
        }
        root.close();
        
        // Second pass: list MP3 and M3U files
        root = fs.open(scan_path);
        if (root && root.isDirectory()) {
            file = root.openNextFile();
            while (file && (file_count + dir_count) < MAX_MP3_FILES) {
                String filename = file.name();
                if (!file.isDirectory()) {
                    // Check for MP3 or M3U extension (case insensitive)
                    String lower_name = filename;
                    lower_name.toLowerCase();
                    if (lower_name.endsWith(".mp3") || lower_name.endsWith(".m3u")) {
                        String full_path = clean_path + "/" + filename;
                        Serial.print("FILE:");
                        Serial.print(full_path);
                        Serial.print("|");
                        Serial.print(filename);
                        Serial.println("|f");
                        file_count++;
                    }
                }
                file.close();
                file = root.openNextFile();
            }
            root.close();
        }
        
        Serial.print("LIST_FILES_COUNT:");
        Serial.print(dir_count);
        Serial.print(",");
        Serial.println(file_count);
        Serial.println("LIST_FILES_END");
    }
    else if (cmd.startsWith("PLAY_FILE_DIR:")) {
        // Play a file from ESP32 SD card with directory context for continuous play
        // Format: PLAY_FILE_DIR:<directory>|<filename>
        // When continuous play is ON, ESP32 scans the directory and plays from the selected file
        String params = cmd.substring(14);
        int separator = params.indexOf('|');
        
        if (separator < 0) {
            Serial.println("ERROR: Invalid format, use PLAY_FILE_DIR:<dir>|<file>");
            return;
        }
        
        String directory = params.substring(0, separator);
        String filename = params.substring(separator + 1);
        directory.trim();
        filename.trim();
        
        if (!is_connected) {
            Serial.println("ERROR: Not connected");
            return;
        }
        
        // Try to initialize SD card if not already done
        if (!sd_initialized) {
            if (!tryInitSDCard()) {
                Serial.println("SD_ERROR");
                return;
            }
        }
        
        // Set initial volume on first play
        if (!first_play_volume_set) {
            a2dp_source.set_volume(initial_volume);
            Serial.print("INIT_VOLUME_APPLIED:");
            Serial.println(initial_volume);
            first_play_volume_set = true;
            volume_protection_active = true;
            volume_protection_start_time = millis();
        }
        
        // Stop any current playback
        if (is_playing || is_paused) {
            is_playing = false;
            is_paused = false;
            is_playing_mp3 = false;
            if (mp3_file) {
                mp3_file.close();
            }
        }
        
        // Scan the directory for MP3 files
        fs::FS& fs = getSDFS();
        File dir = fs.open(directory);
        
        if (!dir || !dir.isDirectory()) {
            Serial.print("ERROR: Cannot open directory: ");
            Serial.println(directory);
            return;
        }
        
        // Build list of MP3 files in the directory
        mp3_file_count = 0;
        int start_index = -1;
        String target_path = directory + "/" + filename;
        
        // First pass: collect MP3 files
        File entry = dir.openNextFile();
        while (entry && mp3_file_count < MAX_MP3_FILES) {
            if (!entry.isDirectory()) {
                String name = entry.name();
                String lower_name = name;
                lower_name.toLowerCase();
                if (lower_name.endsWith(".mp3")) {
                    String full_path = directory + "/" + name;
                    mp3_files[mp3_file_count] = full_path;
                    
                    // Check if this is the file we want to start with
                    if (full_path == target_path || name == filename) {
                        start_index = mp3_file_count;
                    }
                    mp3_file_count++;
                }
            }
            entry.close();
            entry = dir.openNextFile();
        }
        dir.close();
        
        if (mp3_file_count == 0) {
            Serial.println("NO_FILES");
            return;
        }
        
        // Sort files alphabetically using bubble sort
        if (mp3_file_count > 1) {
            for (int i = 0; i < mp3_file_count - 1; i++) {
                for (int j = 0; j < mp3_file_count - i - 1; j++) {
                    if (mp3_files[j] > mp3_files[j + 1]) {
                        String temp = mp3_files[j];
                        mp3_files[j] = mp3_files[j + 1];
                        mp3_files[j + 1] = temp;
                    }
                }
            }
            
            // Re-find start_index after sorting
            start_index = -1;
            for (int i = 0; i < mp3_file_count; i++) {
                if (mp3_files[i] == target_path || mp3_files[i].endsWith("/" + filename)) {
                    start_index = i;
                    break;
                }
            }
        }
        
        if (start_index < 0) {
            // Selected file not found in directory listing, start from beginning
            start_index = 0;
            Serial.println("WARNING: Selected file not in directory list, starting from first file");
        }
        
        playing_favorites = false;
        single_file_repeat = false;  // Continuous play mode - don't repeat single file
        current_playback_mode = PLAYBACK_MODE_FOLDER;
        
        Serial.print("CONTINUOUS_PLAY:");
        Serial.print(mp3_file_count);
        Serial.print(" files, starting at index ");
        Serial.println(start_index);
        
        // Open and play starting from the selected file
        if (openMP3AtIndex(start_index)) {
            is_playing = true;
            is_playing_mp3 = true;
            Serial.println("PLAYING");
        } else {
            Serial.println("ERROR: Could not open file");
        }
    }
    else if (cmd.startsWith("PLAY_FILE:")) {
        // Play a specific file from ESP32 SD card (single file repeat mode)
        // Format: PLAY_FILE:<full_path>
        String file_path = cmd.substring(10);
        file_path.trim();
        
        if (!is_connected) {
            Serial.println("ERROR: Not connected");
            return;
        }
        
        // Try to initialize SD card if not already done
        if (!sd_initialized) {
            if (!tryInitSDCard()) {
                Serial.println("SD_ERROR");
                return;
            }
        }
        
        // Verify file exists
        if (!fileExists(file_path)) {
            Serial.print("FILE_NOT_FOUND:");
            Serial.println(file_path);
            return;
        }
        
        // Set initial volume on first play
        if (!first_play_volume_set) {
            a2dp_source.set_volume(initial_volume);
            Serial.print("INIT_VOLUME_APPLIED:");
            Serial.println(initial_volume);
            first_play_volume_set = true;
            volume_protection_active = true;
            volume_protection_start_time = millis();
        }
        
        // Stop any current playback
        if (is_playing || is_paused) {
            is_playing = false;
            is_paused = false;
            is_playing_mp3 = false;
            if (mp3_file) {
                mp3_file.close();
            }
        }
        
        // Set up single file playback with repeat mode
        // This reuses openMP3AtIndex() which handles all the track initialization,
        // ID3 tag skipping, buffer setup, and EQ state reset.
        mp3_file_count = 1;
        mp3_files[0] = file_path;
        current_mp3_index = 0;
        playing_favorites = false;
        single_file_repeat = true;  // Enable single file repeat mode
        current_playback_mode = PLAYBACK_MODE_FOLDER;
        
        // Open and play the file
        if (openMP3AtIndex(0)) {
            is_playing = true;
            is_playing_mp3 = true;
            Serial.println("PLAYING");
        } else {
            Serial.println("ERROR: Could not open file");
        }
    }
    else if (cmd == "DEBUG") {
        printDiagnostics();
    }
    else {
        Serial.print("ERROR: Unknown command: ");
        Serial.println(cmd);
    }
}

/**
 * Scan for Bluetooth devices
 * 
 * Uses ESP32 GAP API for Bluetooth Classic device discovery.
 * On first scan after boot, adds a short warmup delay to ensure
 * the Bluetooth stack is fully ready for discovery operations.
 */
void scanDevices() {
    if (is_scanning) {
        Serial.println("ERROR: Scan already in progress");
        return;
    }

    if(!gap_callback_registered) {
        Serial.println("ERROR: BT GAP not ready");
        Serial.println("SCAN_COMPLETE");
        return;
    }
    
    // On first scan, add extra warmup delay to ensure BT stack is fully ready
    // This helps improve first-scan reliability after device boot
    if(!first_scan_done) {
        Serial.println("STATUS: First scan - warming up BT stack...");
        delay(FIRST_SCAN_WARMUP_MS);
        first_scan_done = true;
    }
    
    is_scanning = true;
    Serial.println("SCAN_START");
    
    const int duration_units = gap_duration_units(SCAN_DURATION_MS);
    esp_err_t err =
        esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, duration_units, 0);
    if(err != ESP_OK) {
        Serial.print("ERROR: GAP_DISCOVERY ");
        Serial.println(err, HEX);
        Serial.println("SCAN_COMPLETE");
        is_scanning = false;
    }
}

/**
 * Connect to a Bluetooth device by MAC address
 * 
 * A2DP Source mode: This ESP32 acts as an audio SOURCE (like a phone)
 * and actively connects TO Bluetooth headphones/speakers (which are SINKs).
 */
void connectToDevice(String mac) {
    if (is_connected) {
        Serial.println("ERROR: Already connected");
        return;
    }
    
    if (connection_pending) {
        Serial.println("ERROR: Connection already in progress");
        return;
    }
    
    Serial.println("CONNECTING");
    Serial.print("Target: ");
    Serial.println(mac);
    
    // Store target MAC for the callback
    target_mac = mac;
    connection_pending = true;
    connection_start_time = millis();
    
    // Parse MAC address string to bytes (format: XX:XX:XX:XX:XX:XX)
    // Uses %2x to limit each octet to 2 hex digits, preventing overflow
    esp_bd_addr_t bda;
    unsigned int values[6];
    if (sscanf(mac.c_str(), "%2x:%2x:%2x:%2x:%2x:%2x", 
               &values[0], &values[1], &values[2],
               &values[3], &values[4], &values[5]) == 6) {
        // Validate all values are within byte range (0-255)
        bool valid = true;
        for (int i = 0; i < 6; i++) {
            if (values[i] > 255) {
                valid = false;
                break;
            }
            bda[i] = (uint8_t)values[i];
        }
        
        if (!valid) {
            Serial.println("FAILED");
            Serial.println("ERROR: MAC address octet out of range");
            connection_pending = false;
            target_mac = "";
            return;
        }
        
        // CRITICAL FIX FOR CONNECTION STABILITY:
        // Before attempting connection, ensure Bluetooth stack is in a clean state
        // This helps prevent connection failures that were happening in recent releases
        
        // 1. Set device as trusted BEFORE connection attempt
        // This is the original fix for saved device connection failures.
        // Setting the device as trusted via set_auto_reconnect() ensures:
        // - The device is treated as a "known/trusted" device by the library
        // - Bonding information is properly associated with this device
        // - Simpler BT devices (like "Jabba") that need trust relationships work correctly
        // - All saved devices (not just the "last" one) can reconnect reliably
        a2dp_source.set_auto_reconnect(bda);
        Serial.println("STATUS: Device set as trusted connection target");
        
        // 2. Give Bluetooth stack a moment to process the trust setting (NEW FIX)
        // Small delay to ensure the stack has fully processed the auto-reconnect setting
        // This prevents race conditions where connect_to() is called before the device
        // is properly marked as trusted, especially important for saved device connections
        // This 50ms delay significantly improves connection reliability
        delay(50);
        Serial.println("STATUS: Bluetooth stack stabilized, ready to connect");
        
        // Use A2DP Source to connect TO the headphones/speakers
        // The BluetoothA2DPSource library's connect() method handles this
        if (a2dp_source.connect_to(bda)) {
            Serial.println("STATUS: Connection initiated successfully");
            Serial.println("STATUS: Waiting for device response...");
            // Connection will complete via state change callback
        } else {
            Serial.println("FAILED");
            Serial.println("ERROR: Could not initiate connection");
            Serial.println("HINT: Device may need to be in pairing mode");
            connection_pending = false;
            target_mac = "";
        }
    } else {
        Serial.println("FAILED");
        Serial.println("ERROR: Invalid MAC address format");
        connection_pending = false;
        target_mac = "";
    }
}

/**
 * Play a test tone through the connected audio device
 * In Source mode, we set is_playing flag and the audio callback generates the tone
 */
void playTestTone() {
    if (!is_connected) {
        Serial.println("ERROR: Not connected");
        return;
    }
    
    Serial.println("PLAYING");
    is_playing = true;
    play_start_time = millis();
    tone_sample_count = 0;
    // The actual audio generation happens in get_audio_data callback
}

/**
 * Play a melody through the connected audio device
 * melody_id: 1 = Mary Had a Little Lamb, 2 = Simple beeps
 */
void playMelody(int melody_id) {
    if (!is_connected) {
        Serial.println("ERROR: Not connected");
        return;
    }
    
    Serial.println("PLAYING");
    is_playing = true;
    is_playing_melody = true;
    current_melody = melody_id;
    play_start_time = millis();
    tone_sample_count = 0;
}

/**
 * Disconnect from current Bluetooth device
 */
void disconnectDevice() {
    if (!is_connected && !connection_pending) {
        Serial.println("ERROR: Not connected");
        return;
    }
    
    a2dp_source.disconnect();
    is_connected = false;
    is_playing = false;
    is_playing_mp3 = false;
    is_paused = false;
    connection_pending = false;
    current_device_mac = "";
    target_mac = "";
    // Reset the first play volume flag so next connection gets proper initial volume
    first_play_volume_set = false;
    // Reset volume protection state
    volume_protection_active = false;
    // Clear reconnection state - explicit disconnect means no auto-resume
    was_playing_before_disconnect = false;
    reconnection_in_progress = false;
    reconnection_requested = false;
    saved_device_mac_for_reconnect = "";
    consecutive_decode_failures = 0;
    // Clear navigation queue
    nav_debounce_active = false;
    pending_nav_offset = 0;
    Serial.println("DISCONNECTED");
}

void onGapEvent(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {
    switch(event) {
    case ESP_BT_GAP_DISC_RES_EVT: {
        char bda_str[18];
        const uint8_t* bda = param->disc_res.bda;
        snprintf(
            bda_str,
            sizeof(bda_str),
            "%02X:%02X:%02X:%02X:%02X:%02X",
            bda[0],
            bda[1],
            bda[2],
            bda[3],
            bda[4],
            bda[5]);

        char name[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
        memset(name, 0, sizeof(name));

        for(int i = 0; i < param->disc_res.num_prop; i++) {
            esp_bt_gap_dev_prop_t* p = param->disc_res.prop + i;
            if(p->type == ESP_BT_GAP_DEV_PROP_BDNAME && p->val) {
                size_t copy_len =
                    std::min((size_t)p->len, (size_t)ESP_BT_GAP_MAX_BDNAME_LEN);
                memcpy(name, (const char*)p->val, copy_len);
                name[copy_len] = '\0';
            } else if(p->type == ESP_BT_GAP_DEV_PROP_EIR && name[0] == '\0') {
                extractNameFromEir((uint8_t*)p->val, name, sizeof(name));
            }
        }

        if(name[0] == '\0') {
            strncpy(name, "Unknown", sizeof(name) - 1);
        }

        Serial.print("DEVICE:");
        Serial.print(bda_str);
        Serial.print(",");
        Serial.println(name);
        break;
    }
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        if(param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
            Serial.println("SCAN_COMPLETE");
            is_scanning = false;
        }
        break;
    // Authentication complete event - important for tracking pairing status
    case ESP_BT_GAP_AUTH_CMPL_EVT:
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            Serial.println("STATUS: Authentication successful");
            // Sanitize device name before printing - replace non-printable characters
            char safe_name[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
            size_t name_len = strnlen((const char*)param->auth_cmpl.device_name, ESP_BT_GAP_MAX_BDNAME_LEN);
            for (size_t i = 0; i < name_len && i < sizeof(safe_name) - 1; i++) {
                char c = param->auth_cmpl.device_name[i];
                safe_name[i] = (c >= 32 && c < 127) ? c : '?';  // Replace non-printable with '?'
            }
            safe_name[std::min(name_len, sizeof(safe_name) - 1)] = '\0';
            Serial.print("STATUS: Paired with ");
            Serial.println(safe_name);
        } else {
            Serial.print("STATUS: Authentication failed (code ");
            Serial.print(param->auth_cmpl.stat);
            Serial.println(")");
            // Common failure codes:
            // 0x05 = PIN or Key Missing - device may need to be in pairing mode
            // 0x06 = Connection Rejected - device rejected connection
            // 0x0E = Connection Failed To Be Established
        }
        break;
    // PIN code request (for legacy pairing)
    // Note: The ESP32-A2DP library handles PIN requests automatically with "1234" default
    // This is standard for most Bluetooth audio devices. More secure pairing would require
    // user interaction which isn't practical for this headless audio streaming use case.
    case ESP_BT_GAP_PIN_REQ_EVT:
        Serial.println("STATUS: PIN code requested (legacy pairing)");
        Serial.println("STATUS: Using default PIN for pairing");
        break;
    // SSP confirmation request
    // Note: SSP (Secure Simple Pairing) is auto-confirmed by the library for usability.
    // This is appropriate for audio devices where the user has physically initiated the
    // connection from the Flipper. The user action of selecting the device and pressing
    // connect serves as implicit authorization.
    case ESP_BT_GAP_CFM_REQ_EVT:
        Serial.println("STATUS: Pairing confirmation requested (SSP)");
        Serial.println("STATUS: Auto-confirming pairing request");
        break;
    default:
        break;
    }
}

void extractNameFromEir(uint8_t* eir, char* name, size_t name_len) {
    uint8_t len = 0;
    auto set_name = [name, name_len](uint8_t* data, uint8_t len_val) {
        size_t copy_len = std::min<size_t>(len_val, name_len - 1);
        memcpy(name, data, copy_len);
        name[copy_len] = '\0';
    };

    uint8_t* data = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &len);
    if(data && len > 0) {
        set_name(data, len);
        return;
    }

    data = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &len);
    if(data && len > 0) {
        set_name(data, len);
    }
}

/**
 * Print diagnostic information for troubleshooting
 * Called via DEBUG command from Flipper
 */
void printDiagnostics() {
    Serial.println("=== BT Audio Diagnostics ===");
    
    // Firmware info
    Serial.print("FW Version: ");
    Serial.println(FW_VERSION);
    Serial.print("Mode: ");
    Serial.println(FW_MODE_DESC);
    
    // Bluetooth status
    Serial.print("BT Enabled: ");
    Serial.println(esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED ? "YES" : "NO");
    
    Serial.print("GAP Callback: ");
    Serial.println(gap_callback_registered ? "YES" : "NO");
    
    Serial.print("Is Scanning: ");
    Serial.println(is_scanning ? "YES" : "NO");
    
    Serial.print("Is Connected: ");
    Serial.println(is_connected ? "YES" : "NO");
    
    Serial.print("Connection Pending: ");
    Serial.println(connection_pending ? "YES" : "NO");
    
    Serial.print("Is Playing: ");
    Serial.println(is_playing ? "YES" : "NO");
    
    Serial.print("Is Playing MP3: ");
    Serial.println(is_playing_mp3 ? "YES" : "NO");
    
    // SD Card status
    Serial.print("SD Card: ");
    if (sd_initialized) {
        Serial.print("OK (");
        Serial.print(sd_using_mmc ? "SD_MMC" : "SPI");
        Serial.println(" mode)");
    } else {
        Serial.println("NOT AVAILABLE");
    }
    
    Serial.print("SD Init Attempted: ");
    Serial.println(sd_init_attempted ? "YES" : "NO");
    
    Serial.print("MP3 Files: ");
    Serial.println(mp3_file_count);
    
    if(current_device_mac.length() > 0) {
        Serial.print("Connected MAC: ");
        Serial.println(current_device_mac);
    }
    
    if(target_mac.length() > 0 && target_mac != current_device_mac) {
        Serial.print("Target MAC: ");
        Serial.println(target_mac);
    }
    
    // Get local MAC address
    const uint8_t* mac = esp_bt_dev_get_address();
    if(mac) {
        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        Serial.print("ESP32 MAC: ");
        Serial.println(mac_str);
    }
    
    // Memory info
    Serial.print("Free Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    
    // Chip info
    Serial.print("Chip Model: ");
    Serial.println(ESP.getChipModel());
    
    Serial.print("CPU Freq: ");
    Serial.print(ESP.getCpuFreqMHz());
    Serial.println(" MHz");
    
    // Troubleshooting hints
    Serial.println("");
    Serial.println("== Troubleshooting ==");
    if(!gap_callback_registered) {
        Serial.println("! GAP not ready - restart ESP32");
    }
    if(esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        Serial.println("! Bluetooth not enabled");
    }
    if(!sd_initialized) {
        Serial.println("! SD Card not available");
        Serial.println("  ESP32-CAM: Ensure card is inserted properly");
        Serial.println("  Generic ESP32 SPI: Check wiring");
        Serial.println("    CS=GPIO5, SCK=GPIO18, MISO=GPIO19, MOSI=GPIO23");
    }
    Serial.println("- Put target device in pairing mode");
    Serial.println("- Move devices closer (<5m)");
    Serial.println("- Some devices need manual pairing");
    Serial.println("- Try restarting ESP32");
    
    Serial.println("DIAG_COMPLETE");
}

// ============================================================================
// SD Card and MP3 Playback Functions
// ============================================================================

/**
 * Get the appropriate SD filesystem based on which interface is active
 */
fs::FS& getSDFS() {
    if (sd_using_mmc) {
        return SD_MMC;
    }
    return SD;
}

/**
 * Try to initialize SD card (can be called multiple times)
 * Attempts SD_MMC first (for ESP32-CAM), then falls back to SPI SD
 * Returns true if SD card is available
 */
bool tryInitSDCard() {
    // If already initialized, return success
    if (sd_initialized) {
        return true;
    }
    
    Serial.println("Attempting SD card initialization...");
    
    // Try SD_MMC first (ESP32-CAM and other boards with SDIO)
    // SD_MMC uses GPIO 14 (CLK), 15 (CMD), 2 (DATA0) for 1-bit mode
    Serial.println("Trying SD_MMC (1-bit mode)...");
    
    for (int attempt = 0; attempt < SD_INIT_RETRIES; attempt++) {
        if (attempt > 0) {
            Serial.printf("SD_MMC retry %d/%d\n", attempt + 1, SD_INIT_RETRIES);
            delay(SD_INIT_DELAY_MS);
        }
        
        // Try 1-bit mode for better compatibility
        if (SD_MMC.begin("/sdcard", true)) {  // true = 1-bit mode
            uint8_t cardType = SD_MMC.cardType();
            if (cardType != CARD_NONE) {
                sd_using_mmc = true;
                sd_initialized = true;
                
                Serial.println("SD_MMC initialization successful!");
                Serial.print("SD Card Type: ");
                if (cardType == CARD_MMC) {
                    Serial.println("MMC");
                } else if (cardType == CARD_SD) {
                    Serial.println("SDSC");
                } else if (cardType == CARD_SDHC) {
                    Serial.println("SDHC");
                } else {
                    Serial.println("UNKNOWN");
                }
                
                uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
                Serial.printf("SD Card Size: %lluMB\n", cardSize);
                return true;
            }
            SD_MMC.end();
        }
    }
    
    Serial.println("SD_MMC failed, trying SPI SD...");
    
    // Fall back to SPI SD (for generic ESP32 with SD module)
    // Initialize SPI with explicit pins (sck, miso, mosi)
    // Note: CS pin is handled by SD.begin(), not SPI.begin()
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN);
    delay(10);
    
    for (int attempt = 0; attempt < SD_INIT_RETRIES; attempt++) {
        if (attempt > 0) {
            Serial.printf("SPI SD retry %d/%d\n", attempt + 1, SD_INIT_RETRIES);
            delay(SD_INIT_DELAY_MS);
        }
        
        if (SD.begin(SD_CS_PIN, SPI, 4000000)) {  // 4MHz SPI speed for reliability
            uint8_t cardType = SD.cardType();
            if (cardType != CARD_NONE) {
                sd_using_mmc = false;
                sd_initialized = true;
                
                Serial.println("SPI SD initialization successful!");
                Serial.print("SD Card Type: ");
                if (cardType == CARD_MMC) {
                    Serial.println("MMC");
                } else if (cardType == CARD_SD) {
                    Serial.println("SDSC");
                } else if (cardType == CARD_SDHC) {
                    Serial.println("SDHC");
                } else {
                    Serial.println("UNKNOWN");
                }
                
                uint64_t cardSize = SD.cardSize() / (1024 * 1024);
                Serial.printf("SD Card Size: %lluMB\n", cardSize);
                return true;
            }
            SD.end();
        }
    }
    
    Serial.println("All SD card initialization attempts failed");
    return false;
}

/**
 * Initialize SD card (called from setup)
 * Returns true if SD card is available
 */
bool initSDCard() {
    sd_init_attempted = true;
    return tryInitSDCard();
}

/**
 * Validate MP3 file before adding to playlist
 * Checks file size and header for valid MP3 format
 * Returns true if file appears to be a valid MP3
 */
bool isValidMP3File(const String& path) {
    fs::FS& fs = getSDFS();
    File f = fs.open(path);
    if (!f || f.isDirectory()) return false;
    
    size_t size = f.size();
    // Minimum reasonable MP3 size (e.g., 10KB for a very short clip)
    if (size < 10240) {
        Serial.print("FILE_TOO_SMALL:");
        Serial.println(path);
        f.close();
        return false;
    }
    
    // Check for MP3 sync word or ID3 header
    uint8_t header[10];
    if (f.read(header, 10) != 10) {
        f.close();
        return false;
    }
    f.close();
    
    // Check for ID3v2 tag ("ID3") or MP3 frame sync (0xFF followed by 3 more bits set = 0xFFE0 pattern)
    bool hasID3 = (header[0] == 'I' && header[1] == 'D' && header[2] == '3');
    bool hasSync = (header[0] == 0xFF && (header[1] & 0xE0) == 0xE0);
    
    if (!hasID3 && !hasSync) {
        Serial.print("FILE_INVALID_HEADER:");
        Serial.println(path);
        return false;
    }
    
    return true;
}

/**
 * Scan /music folder for MP3 files
 * Order: Base files first (alphabetically), then subdirectory files (folder by folder)
 * If playlist1.m3u exists, use that order instead
 * Supports case-insensitive folder names (/music, /Music, /MUSIC)
 */
void scanMP3Files() {
    mp3_file_count = 0;
    
    if (!sd_initialized) {
        Serial.println("SD card not initialized, cannot scan for MP3 files");
        return;
    }
    
    fs::FS& fs = getSDFS();
    
    // Find the music folder (case-insensitive)
    active_music_folder = findMusicFolder();
    if (active_music_folder.length() == 0) {
        Serial.println("Could not find or create music folder");
        return;
    }
    
    // First, check if playlist1.m3u exists (renamed from playlist.m3u)
    String playlist_path = active_music_folder + "/playlist1.m3u";
    File playlist_file = fs.open(playlist_path);
    
    if (playlist_file && !playlist_file.isDirectory()) {
        // M3U playlist found - parse it
        Serial.println("Found playlist1.m3u, using playlist order");
        
        while (playlist_file.available() && mp3_file_count < MAX_MP3_FILES) {
            String line = playlist_file.readStringUntil('\n');
            line.trim();
            
            // Skip empty lines and comments
            if (line.length() == 0 || line.startsWith("#")) {
                continue;
            }
            
            // Handle both relative paths and full paths
            String file_path;
            if (line.startsWith("/")) {
                // Absolute path
                file_path = line;
            } else {
                // Relative path - prepend active music folder
                file_path = active_music_folder + "/" + line;
            }
            
            // Check if file exists and is MP3
            String lower_path = file_path;
            lower_path.toLowerCase();
            if (lower_path.endsWith(".mp3")) {
                File test_file = fs.open(file_path);
                if (test_file && !test_file.isDirectory()) {
                    test_file.close();
                    // Validate MP3 file before adding to playlist
                    if (isValidMP3File(file_path)) {
                        mp3_files[mp3_file_count] = file_path;
                        Serial.print("Playlist entry: ");
                        Serial.println(mp3_files[mp3_file_count]);
                        mp3_file_count++;
                    } else {
                        Serial.print("Warning: Invalid MP3 file skipped: ");
                        Serial.println(file_path);
                    }
                } else {
                    Serial.print("Warning: Playlist entry not found: ");
                    Serial.println(file_path);
                }
            }
        }
        
        playlist_file.close();
        
        if (mp3_file_count > 0) {
            Serial.print("Loaded ");
            Serial.print(mp3_file_count);
            Serial.println(" files from playlist1.m3u");
            return;
        } else {
            Serial.println("Playlist was empty or invalid, falling back to directory scan");
        }
    }
    
    // No playlist or playlist empty - scan directory
    // Play Music order: base files first (alphabetically), then folders (alphabetically)
    File root = fs.open(active_music_folder);
    if (!root) {
        Serial.println("Failed to open music folder");
        return;
    }
    
    if (!root.isDirectory()) {
        Serial.println("Music path is not a directory");
        root.close();
        return;
    }
    
    // Temporary arrays for base files and subdirectories
    String base_files[MAX_MP3_FILES];
    int base_file_count = 0;
    String subdirs[MAX_MP3_FILES];
    int subdir_count = 0;
    
    // First pass: Collect base MP3 files and subdirectory names
    // Skip file validation during scan for speed - invalid files handled at play time
    File file = root.openNextFile();
    while (file) {
        String filename = file.name();
        if (file.isDirectory()) {
            // Skip hidden folders
            if (!filename.startsWith(".") && subdir_count < MAX_MP3_FILES) {
                subdirs[subdir_count] = filename;
                subdir_count++;
            }
        } else {
            // Check if it's an MP3 file (case insensitive)
            String lower_name = filename;
            lower_name.toLowerCase();
            if (lower_name.endsWith(".mp3") && base_file_count < MAX_MP3_FILES) {
                // Store path directly - no validation for speed
                base_files[base_file_count] = active_music_folder + "/" + filename;
                base_file_count++;
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
    
    // Sort base files alphabetically
    if (base_file_count > 1) {
        for (int i = 0; i < base_file_count - 1; i++) {
            for (int j = 0; j < base_file_count - i - 1; j++) {
                if (base_files[j] > base_files[j + 1]) {
                    String temp = base_files[j];
                    base_files[j] = base_files[j + 1];
                    base_files[j + 1] = temp;
                }
            }
        }
    }
    
    // Sort subdirectories alphabetically
    if (subdir_count > 1) {
        for (int i = 0; i < subdir_count - 1; i++) {
            for (int j = 0; j < subdir_count - i - 1; j++) {
                if (subdirs[j] > subdirs[j + 1]) {
                    String temp = subdirs[j];
                    subdirs[j] = subdirs[j + 1];
                    subdirs[j + 1] = temp;
                }
            }
        }
    }
    
    // Add base files to playlist first
    for (int i = 0; i < base_file_count && mp3_file_count < MAX_MP3_FILES; i++) {
        mp3_files[mp3_file_count] = base_files[i];
        mp3_file_count++;
    }
    
    // Then scan each subdirectory and add their files
    for (int d = 0; d < subdir_count && mp3_file_count < MAX_MP3_FILES; d++) {
        String subdir_path = active_music_folder + "/" + subdirs[d];
        
        File subdir = fs.open(subdir_path);
        if (subdir && subdir.isDirectory()) {
            // Collect files from this subdirectory
            String subdir_files[MAX_MP3_FILES];
            int subdir_file_count = 0;
            
            File subfile = subdir.openNextFile();
            while (subfile && subdir_file_count < MAX_MP3_FILES) {
                if (!subfile.isDirectory()) {
                    String subfilename = subfile.name();
                    String lower_name = subfilename;
                    lower_name.toLowerCase();
                    if (lower_name.endsWith(".mp3")) {
                        // Store path directly - no validation for speed
                        subdir_files[subdir_file_count] = subdir_path + "/" + subfilename;
                        subdir_file_count++;
                    }
                }
                subfile.close();
                subfile = subdir.openNextFile();
            }
            subdir.close();
            
            // Sort this subdirectory's files alphabetically
            if (subdir_file_count > 1) {
                for (int i = 0; i < subdir_file_count - 1; i++) {
                    for (int j = 0; j < subdir_file_count - i - 1; j++) {
                        if (subdir_files[j] > subdir_files[j + 1]) {
                            String temp = subdir_files[j];
                            subdir_files[j] = subdir_files[j + 1];
                            subdir_files[j + 1] = temp;
                        }
                    }
                }
            }
            
            // Add to main playlist
            for (int i = 0; i < subdir_file_count && mp3_file_count < MAX_MP3_FILES; i++) {
                mp3_files[mp3_file_count] = subdir_files[i];
                mp3_file_count++;
            }
        }
    }
    
    Serial.print("Scan complete: ");
    Serial.print(mp3_file_count);
    Serial.print(" files, ");
    Serial.print(subdir_count);
    Serial.println(" folders");
}

/**
 * Scan /music folder for MP3 files - directory structure only (no playlist check)
 * Order: Base files first (alphabetically), then subdirectory files (folder by folder)
 * This is used by "Play Music Folder" to always scan the actual directory structure,
 * even if playlist1.m3u exists (which would be used by "Play Playlist 1" instead).
 */
void scanMusicFolderOnly() {
    mp3_file_count = 0;
    
    if (!sd_initialized) {
        Serial.println("SD card not initialized, cannot scan for MP3 files");
        return;
    }
    
    fs::FS& fs = getSDFS();
    
    // Find the music folder (case-insensitive)
    active_music_folder = findMusicFolder();
    if (active_music_folder.length() == 0) {
        Serial.println("Could not find or create music folder");
        return;
    }
    
    // Directly scan directory structure - do NOT check for playlist files
    // Play Music order: base files first (alphabetically), then folders (alphabetically)
    File root = fs.open(active_music_folder);
    if (!root) {
        Serial.println("Failed to open music folder");
        return;
    }
    
    if (!root.isDirectory()) {
        Serial.println("Music path is not a directory");
        root.close();
        return;
    }
    
    // Temporary arrays for base files and subdirectories
    String base_files[MAX_MP3_FILES];
    int base_file_count = 0;
    String subdirs[MAX_MP3_FILES];
    int subdir_count = 0;
    
    // First pass: Collect base MP3 files and subdirectory names
    // Skip file validation during scan for speed - invalid files handled at play time
    File file = root.openNextFile();
    while (file) {
        String filename = file.name();
        if (file.isDirectory()) {
            // Skip hidden folders
            if (!filename.startsWith(".") && subdir_count < MAX_MP3_FILES) {
                subdirs[subdir_count] = filename;
                subdir_count++;
            }
        } else {
            // Check if it's an MP3 file (case insensitive)
            String lower_name = filename;
            lower_name.toLowerCase();
            if (lower_name.endsWith(".mp3") && base_file_count < MAX_MP3_FILES) {
                // Store path directly - no validation for speed
                base_files[base_file_count] = active_music_folder + "/" + filename;
                base_file_count++;
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
    
    // Sort base files alphabetically
    if (base_file_count > 1) {
        for (int i = 0; i < base_file_count - 1; i++) {
            for (int j = 0; j < base_file_count - i - 1; j++) {
                if (base_files[j] > base_files[j + 1]) {
                    String temp = base_files[j];
                    base_files[j] = base_files[j + 1];
                    base_files[j + 1] = temp;
                }
            }
        }
    }
    
    // Sort subdirectories alphabetically
    if (subdir_count > 1) {
        for (int i = 0; i < subdir_count - 1; i++) {
            for (int j = 0; j < subdir_count - i - 1; j++) {
                if (subdirs[j] > subdirs[j + 1]) {
                    String temp = subdirs[j];
                    subdirs[j] = subdirs[j + 1];
                    subdirs[j + 1] = temp;
                }
            }
        }
    }
    
    // Add base files to playlist first
    for (int i = 0; i < base_file_count && mp3_file_count < MAX_MP3_FILES; i++) {
        mp3_files[mp3_file_count] = base_files[i];
        mp3_file_count++;
    }
    
    // Then scan each subdirectory and add their files
    for (int d = 0; d < subdir_count && mp3_file_count < MAX_MP3_FILES; d++) {
        String subdir_path = active_music_folder + "/" + subdirs[d];
        
        File subdir = fs.open(subdir_path);
        if (subdir && subdir.isDirectory()) {
            // Collect files from this subdirectory
            String subdir_files[MAX_MP3_FILES];
            int subdir_file_count = 0;
            
            File subfile = subdir.openNextFile();
            while (subfile && subdir_file_count < MAX_MP3_FILES) {
                if (!subfile.isDirectory()) {
                    String subfilename = subfile.name();
                    String lower_name = subfilename;
                    lower_name.toLowerCase();
                    if (lower_name.endsWith(".mp3")) {
                        // Store path directly - no validation for speed
                        subdir_files[subdir_file_count] = subdir_path + "/" + subfilename;
                        subdir_file_count++;
                    }
                }
                subfile.close();
                subfile = subdir.openNextFile();
            }
            subdir.close();
            
            // Sort this subdirectory's files alphabetically
            if (subdir_file_count > 1) {
                for (int i = 0; i < subdir_file_count - 1; i++) {
                    for (int j = 0; j < subdir_file_count - i - 1; j++) {
                        if (subdir_files[j] > subdir_files[j + 1]) {
                            String temp = subdir_files[j];
                            subdir_files[j] = subdir_files[j + 1];
                            subdir_files[j + 1] = temp;
                        }
                    }
                }
            }
            
            // Add to main playlist
            for (int i = 0; i < subdir_file_count && mp3_file_count < MAX_MP3_FILES; i++) {
                mp3_files[mp3_file_count] = subdir_files[i];
                mp3_file_count++;
            }
        }
    }
    
    Serial.print("Folder scan complete: ");
    Serial.print(mp3_file_count);
    Serial.print(" files, ");
    Serial.print(subdir_count);
    Serial.println(" folders");
}

/**
 * Skip ID3 tag at the start of MP3 file
 * ID3v2 tags start with "ID3" and have size info at bytes 6-9
 */
void skipID3Tag() {
    if (mp3_buf_bytes < 10) return;
    
    // Check for ID3v2 header
    if (mp3_buf[0] == 'I' && mp3_buf[1] == 'D' && mp3_buf[2] == '3') {
        // Get tag size from bytes 6-9 (syncsafe integer)
        uint32_t tag_size = ((mp3_buf[6] & 0x7F) << 21) |
                           ((mp3_buf[7] & 0x7F) << 14) |
                           ((mp3_buf[8] & 0x7F) << 7) |
                           (mp3_buf[9] & 0x7F);
        tag_size += 10;  // Add header size
        
        Serial.print("Skipping ID3 tag: ");
        Serial.print(tag_size);
        Serial.println(" bytes");
        
        // Skip past the ID3 tag
        mp3_file.seek(tag_size);
        mp3_buf_bytes = 0;  // Clear buffer to reload
        
        // Yield to other tasks after seek (especially important for large ID3 tags)
        yield();
    }
}

/**
 * Open the next MP3 file in the playlist
 * Returns true if a file was successfully opened
 * This is called automatically during playback when a file finishes
 * Supports shuffle mode for random track selection
 */
bool openNextMP3() {
    int next_index;
    
    // Check if we're in single file repeat mode (file browser with continuous play OFF)
    if (single_file_repeat && mp3_file_count == 1) {
        // Repeat the same file - keep index at 0
        next_index = 0;
    } else if (shuffle_mode && mp3_file_count > 1) {
        // Shuffle mode: pick a random track different from current
        do {
            next_index = random(0, mp3_file_count);
        } while (next_index == current_mp3_index && mp3_file_count > 1);
    } else {
        // Normal mode: sequential playback
        next_index = current_mp3_index + 1;
        
        // Check if we have more files
        if (next_index >= mp3_file_count) {
            // Wrap to beginning for continuous playback
            next_index = 0;
        }
    }
    
    // Open the next file
    return openMP3AtIndex(next_index);
}

/**
 * Decode one MP3 frame into PCM samples
 * Returns true if decoding was successful
 * 
 * This function can be used in two modes:
 * 1. File mode: mp3_file is open, buffer is refilled from file
 * 2. Stream mode: mp3_file is not open, buffer must be pre-filled by caller
 */
bool decodeMP3Frame() {
    if (mp3_decoder == NULL) {
        return false;
    }
    
    // Refill input buffer from file if available (file mode)
    // In stream mode, the caller pre-fills mp3_buf from HTTP stream
    if (mp3_file && mp3_buf_bytes < MP3_BUF_REFILL_THRESHOLD && mp3_file.available()) {
        // Note: remaining data is already at start of buffer (shifted after decode)
        // Just read more data to fill the rest of the buffer
        int bytes_read = mp3_file.read(mp3_buf + mp3_buf_bytes, MP3_BUF_SIZE - mp3_buf_bytes);
        if (bytes_read > 0) {
            mp3_buf_bytes += bytes_read;
        }
    }
    
    if (mp3_buf_bytes == 0) {
        return false;  // No data left
    }
    
    // Find sync word
    int offset = MP3FindSyncWord(mp3_buf, mp3_buf_bytes);
    if (offset < 0) {
        mp3_buf_bytes = 0;  // No sync found, discard buffer
        return false;
    }
    
    // Adjust buffer position
    const unsigned char* read_ptr = mp3_buf + offset;
    size_t bytes_left = mp3_buf_bytes - offset;
    
    // Decode one frame
    MP3FrameInfo frame_info;
    int err = MP3Decode(mp3_decoder, &read_ptr, &bytes_left, pcm_buf, 0);
    
    if (err != ERR_MP3_NONE) {
        // Decoding error - skip this frame
        if (bytes_left > 0) {
            memmove(mp3_buf, read_ptr, bytes_left);
        }
        mp3_buf_bytes = bytes_left;
        return false;
    }
    
    // Get frame info
    MP3GetLastFrameInfo(mp3_decoder, &frame_info);
    
    // Calculate number of stereo samples
    pcm_buf_samples = frame_info.outputSamps / frame_info.nChans;
    pcm_buf_read_pos = 0;
    
    // Handle mono files - duplicate to stereo
    if (frame_info.nChans == 1) {
        // Convert mono to stereo (interleave)
        for (int i = pcm_buf_samples - 1; i >= 0; i--) {
            pcm_buf[i * 2 + 1] = pcm_buf[i];  // Right
            pcm_buf[i * 2] = pcm_buf[i];      // Left
        }
    }
    
    // Apply 3-band EQ to the decoded PCM samples
    // This processes the samples in-place with biquad IIR filters
    applyEQ(pcm_buf, pcm_buf_samples);
    
    // Update input buffer
    memmove(mp3_buf, read_ptr, bytes_left);
    mp3_buf_bytes = bytes_left;
    
    return true;
}

/**
 * Start MP3 playback from SD card
 * Uses scanMusicFolderOnly() to scan actual directory structure
 * (does not use playlist1.m3u - that's for "Play Playlist 1")
 */
void playMP3FromSD() {
    // Try to initialize SD card if not already done
    if (!sd_initialized) {
        Serial.println("SD card not initialized, attempting late initialization...");
        if (!tryInitSDCard()) {
            Serial.println("SD_ERROR");
            Serial.println("ERROR: SD card not initialized - check SD card and wiring");
            Serial.println("For ESP32-CAM: Ensure SD card is properly inserted");
            Serial.println("For generic ESP32: Check SPI wiring (CS=GPIO5, SCK=GPIO18, MISO=GPIO19, MOSI=GPIO23)");
            return;
        }
    }
    
    // Check if we're changing playback modes - if so, always start from track 1
    bool mode_changed = (current_playback_mode != PLAYBACK_MODE_FOLDER);
    
    // Rescan if mode changed or not playing
    if (mode_changed || mp3_file_count == 0 || (!is_playing && !is_paused)) {
        Serial.println("Scanning music folder (directory structure only)...");
        // Use scanMusicFolderOnly() to ONLY scan directory structure
        // This avoids using playlist1.m3u which is now reserved for "Play Playlist 1"
        scanMusicFolderOnly();
    } else {
        Serial.println("Using cached music folder playlist");
    }
    
    if (mp3_file_count == 0) {
        Serial.println("NO_FILES");
        Serial.println("ERROR: No MP3 files in /music folder");
        return;
    }
    
    if (!is_connected) {
        Serial.println("ERROR: Not connected");
        return;
    }
    
    // Set initial volume on first play to prevent ear-blasting volume
    // This only happens once per connection to ensure a comfortable starting volume
    if (!first_play_volume_set) {
        a2dp_source.set_volume(initial_volume);
        Serial.print("INIT_VOLUME_APPLIED:");
        Serial.println(initial_volume);
        first_play_volume_set = true;
        
        // Activate volume protection to prevent remote device from overriding
        volume_protection_active = true;
        volume_protection_start_time = millis();
    }
    
    // Not playing favorites mode (legacy flag)
    playing_favorites = false;
    // Disable single file repeat when playing from folder (playlist mode plays through all files)
    single_file_repeat = false;
    
    // If we're already in folder mode and playing, continue current playback
    // BUT: After reconnection, the file handle may be invalid even if is_playing is true
    // So check if the file is actually available before deciding to continue
    bool file_available = mp3_file && mp3_file.available();
    if (!mode_changed && (is_playing || is_paused) && is_playing_mp3 && file_available) {
        // Same mode, file is open - continue current playback
        Serial.println("PLAYING");
        Serial.print("PLAYING_FILE:");
        Serial.println(current_playing_file);
        return;
    }
    
    // If we get here and was supposedly playing, log it (helps debug reconnection issues)
    if ((is_playing || is_paused) && !file_available) {
        Serial.println("STATUS: Restarting playback - file was not available");
    }
    
    // Update current mode - MUST be done before starting playback
    current_playback_mode = PLAYBACK_MODE_FOLDER;
    
    // Start from first file (index 0) for new playback session or mode switch
    if (!openMP3AtIndex(0)) {
        Serial.println("ERROR: Could not open MP3 file");
        return;
    }
    
    Serial.println("PLAYING");
    is_playing = true;
    is_playing_mp3 = true;
}

/**
 * Play favorites playlist
 * Files are validated before playing - missing files are skipped
 */
void playFavorites() {
    // Try to initialize SD card if not already done
    if (!sd_initialized) {
        Serial.println("SD card not initialized, attempting late initialization...");
        if (!tryInitSDCard()) {
            Serial.println("SD_ERROR");
            return;
        }
    }
    
    if (favorites_count == 0) {
        Serial.println("NO_FAVORITES");
        return;
    }
    
    if (!is_connected) {
        Serial.println("ERROR: Not connected");
        return;
    }
    
    // Check if we're changing playback modes - if so, always start from track 1
    bool mode_changed = (current_playback_mode != PLAYBACK_MODE_FAVORITES);
    
    // Set initial volume on first play
    if (!first_play_volume_set) {
        a2dp_source.set_volume(initial_volume);
        Serial.print("INIT_VOLUME_APPLIED:");
        Serial.println(initial_volume);
        first_play_volume_set = true;
        
        // Activate volume protection to prevent remote device from overriding
        volume_protection_active = true;
        volume_protection_start_time = millis();
    }
    
    // Copy favorites to mp3_files array, validating each file exists
    mp3_file_count = 0;
    int skipped = 0;
    for (int i = 0; i < favorites_count; i++) {
        if (fileExists(favorites_list[i])) {
            mp3_files[mp3_file_count] = favorites_list[i];
            mp3_file_count++;
        } else {
            // File not found - notify Flipper and skip
            Serial.print("FILE_NOT_FOUND:");
            Serial.println(favorites_list[i]);
            skipped++;
        }
    }
    
    if (mp3_file_count == 0) {
        Serial.println("NO_FILES");
        Serial.println("ERROR: None of the favorite files were found");
        return;
    }
    
    if (skipped > 0) {
        Serial.print("FAVORITES_SKIPPED:");
        Serial.println(skipped);
    }
    
    // Mark as playing favorites mode (legacy flag)
    playing_favorites = true;
    // Disable single file repeat when playing favorites (plays through all files)
    single_file_repeat = false;
    
    // If we're already in favorites mode and playing, continue current playback
    // BUT: After reconnection, the file handle may be invalid even if is_playing is true
    // So check if the file is actually available before deciding to continue
    bool file_available = mp3_file && mp3_file.available();
    if (!mode_changed && (is_playing || is_paused) && is_playing_mp3 && file_available) {
        // Same mode - continue current playback if current track is still in the list
        // Verify the current file is still in the new favorites list
        bool current_still_valid = false;
        for (int i = 0; i < mp3_file_count; i++) {
            if (mp3_files[i] == current_playing_file) {
                current_mp3_index = i;  // Update index in case order changed
                current_still_valid = true;
                break;
            }
        }
        if (current_still_valid) {
            Serial.println("PLAYING");
            Serial.print("PLAYING_FILE:");
            Serial.println(current_playing_file);
            return;
        }
    }
    
    // If we get here and was supposedly playing, log it (helps debug reconnection issues)
    if ((is_playing || is_paused) && !file_available) {
        Serial.println("STATUS: Restarting playback - file was not available");
    }
    
    // Update current mode - MUST be done before starting playback
    current_playback_mode = PLAYBACK_MODE_FAVORITES;
    
    // Start from first valid file
    if (!openMP3AtIndex(0)) {
        Serial.println("ERROR: Could not open favorite file");
        playing_favorites = false;
        current_playback_mode = PLAYBACK_MODE_NONE;
        return;
    }
    
    Serial.println("PLAYING");
    is_playing = true;
    is_playing_mp3 = true;
}

/**
 * Scan only playlist1.m3u file, ignoring directory scan
 * Returns true if playlist was found and has valid files
 * Supports case-insensitive folder names (/music, /Music, /MUSIC)
 */
bool scanPlaylistOnly() {
    mp3_file_count = 0;
    
    if (!sd_initialized) {
        return false;
    }
    
    fs::FS& fs = getSDFS();
    
    // Find the music folder (case-insensitive)
    active_music_folder = findMusicFolder();
    if (active_music_folder.length() == 0) {
        Serial.println("NO_PLAYLIST");
        Serial.println("ERROR: Music folder not found");
        return false;
    }
    
    // Look for playlist1.m3u (renamed from playlist.m3u)
    String playlist_path = active_music_folder + "/playlist1.m3u";
    File playlist_file = fs.open(playlist_path);
    
    if (!playlist_file || playlist_file.isDirectory()) {
        Serial.println("NO_PLAYLIST");
        Serial.print("ERROR: ");
        Serial.print(active_music_folder);
        Serial.println("/playlist1.m3u not found");
        return false;
    }
    
    Serial.println("Found playlist1.m3u, parsing...");
    
    while (playlist_file.available() && mp3_file_count < MAX_MP3_FILES) {
        String line = playlist_file.readStringUntil('\n');
        line.trim();
        
        // Skip empty lines and comments
        if (line.length() == 0 || line.startsWith("#")) {
            continue;
        }
        
        // Handle both relative paths and full paths
        String file_path;
        if (line.startsWith("/")) {
            // Absolute path
            file_path = line;
        } else {
            // Relative path - prepend active music folder
            file_path = active_music_folder + "/" + line;
        }
        
        // Check if file exists and is MP3
        String lower_path = file_path;
        lower_path.toLowerCase();
        if (lower_path.endsWith(".mp3")) {
            if (fileExists(file_path)) {
                // Validate MP3 file before adding to playlist
                if (isValidMP3File(file_path)) {
                    mp3_files[mp3_file_count] = file_path;
                    Serial.print("Playlist entry: ");
                    Serial.println(mp3_files[mp3_file_count]);
                    mp3_file_count++;
                } else {
                    Serial.print("Warning: Invalid MP3 file skipped: ");
                    Serial.println(file_path);
                }
            } else {
                Serial.print("FILE_NOT_FOUND:");
                Serial.println(file_path);
            }
        }
    }
    
    playlist_file.close();
    
    if (mp3_file_count > 0) {
        Serial.print("Loaded ");
        Serial.print(mp3_file_count);
        Serial.println(" files from playlist1.m3u");
        return true;
    }
    
    Serial.println("ERROR: Playlist was empty or all files missing");
    return false;
}

/**
 * Play only from playlist1.m3u file
 * Unlike playMP3FromSD which falls back to directory scan,
 * this only plays files listed in playlist1.m3u
 */
void playPlaylistOnly() {
    // Try to initialize SD card if not already done
    if (!sd_initialized) {
        Serial.println("SD card not initialized, attempting late initialization...");
        if (!tryInitSDCard()) {
            Serial.println("SD_ERROR");
            return;
        }
    }
    
    if (!is_connected) {
        Serial.println("ERROR: Not connected");
        return;
    }
    
    // Check if we're changing playback modes - if so, always start from track 1
    bool mode_changed = (current_playback_mode != PLAYBACK_MODE_PLAYLIST1);
    
    // Rescan if mode changed or not playing
    if (mode_changed || mp3_file_count == 0 || (!is_playing && !is_paused)) {
        // Scan only the playlist file
        if (!scanPlaylistOnly()) {
            return;  // Error already reported
        }
    } else {
        Serial.println("Using cached playlist1.m3u");
    }
    
    // Set initial volume on first play
    if (!first_play_volume_set) {
        a2dp_source.set_volume(initial_volume);
        Serial.print("INIT_VOLUME_APPLIED:");
        Serial.println(initial_volume);
        first_play_volume_set = true;
        
        // Activate volume protection to prevent remote device from overriding
        volume_protection_active = true;
        volume_protection_start_time = millis();
    }
    
    // Not favorites mode (legacy flag)
    playing_favorites = false;
    // Disable single file repeat when playing playlist (plays through all files)
    single_file_repeat = false;
    
    // If we're already in playlist1 mode and playing, continue current playback
    // BUT: After reconnection, the file handle may be invalid even if is_playing is true
    // So check if the file is actually available before deciding to continue
    bool file_available = mp3_file && mp3_file.available();
    if (!mode_changed && (is_playing || is_paused) && is_playing_mp3 && file_available) {
        // Same mode, same playlist, file is open - continue current playback
        Serial.println("PLAYING");
        Serial.print("PLAYING_FILE:");
        Serial.println(current_playing_file);
        return;
    }
    
    // If we get here and was supposedly playing, log it (helps debug reconnection issues)
    if ((is_playing || is_paused) && !file_available) {
        Serial.println("STATUS: Restarting playback - file was not available");
    }
    
    // Update current mode - MUST be done before starting playback
    current_playback_mode = PLAYBACK_MODE_PLAYLIST1;
    
    // Start from first file for new playback session or mode switch
    if (!openMP3AtIndex(0)) {
        Serial.println("ERROR: Could not open playlist file");
        return;
    }
    
    Serial.println("PLAYING");
    is_playing = true;
    is_playing_mp3 = true;
}

/**
 * Scan only playlist2.m3u file, ignoring directory scan
 * Returns true if playlist was found and has valid files
 * Supports case-insensitive folder names (/music, /Music, /MUSIC)
 */
bool scanPlaylist2Only() {
    mp3_file_count = 0;
    
    if (!sd_initialized) {
        return false;
    }
    
    fs::FS& fs = getSDFS();
    
    // Find the music folder (case-insensitive)
    active_music_folder = findMusicFolder();
    if (active_music_folder.length() == 0) {
        Serial.println("NO_PLAYLIST2");
        Serial.println("ERROR: Music folder not found");
        return false;
    }
    
    // Look for playlist2.m3u
    String playlist_path = active_music_folder + "/playlist2.m3u";
    File playlist_file = fs.open(playlist_path);
    
    if (!playlist_file || playlist_file.isDirectory()) {
        Serial.println("NO_PLAYLIST2");
        Serial.print("ERROR: ");
        Serial.print(active_music_folder);
        Serial.println("/playlist2.m3u not found");
        return false;
    }
    
    Serial.println("Found playlist2.m3u, parsing...");
    
    while (playlist_file.available() && mp3_file_count < MAX_MP3_FILES) {
        String line = playlist_file.readStringUntil('\n');
        line.trim();
        
        // Skip empty lines and comments
        if (line.length() == 0 || line.startsWith("#")) {
            continue;
        }
        
        // Handle both relative paths and full paths
        String file_path;
        if (line.startsWith("/")) {
            // Absolute path
            file_path = line;
        } else {
            // Relative path - prepend active music folder
            file_path = active_music_folder + "/" + line;
        }
        
        // Check if file exists and is MP3
        String lower_path = file_path;
        lower_path.toLowerCase();
        if (lower_path.endsWith(".mp3")) {
            if (fileExists(file_path)) {
                // Validate MP3 file before adding to playlist
                if (isValidMP3File(file_path)) {
                    mp3_files[mp3_file_count] = file_path;
                    Serial.print("Playlist2 entry: ");
                    Serial.println(mp3_files[mp3_file_count]);
                    mp3_file_count++;
                } else {
                    Serial.print("Warning: Invalid MP3 file skipped: ");
                    Serial.println(file_path);
                }
            } else {
                Serial.print("FILE_NOT_FOUND:");
                Serial.println(file_path);
            }
        }
    }
    
    playlist_file.close();
    
    if (mp3_file_count > 0) {
        Serial.print("Loaded ");
        Serial.print(mp3_file_count);
        Serial.println(" files from playlist2");
        return true;
    }
    
    Serial.println("ERROR: Playlist2 was empty or all files missing");
    return false;
}

/**
 * Play only from playlist2.m3u file
 * Unlike playMP3FromSD which falls back to directory scan,
 * this only plays files listed in playlist2.m3u
 */
void playPlaylist2Only() {
    // Try to initialize SD card if not already done
    if (!sd_initialized) {
        Serial.println("SD card not initialized, attempting late initialization...");
        if (!tryInitSDCard()) {
            Serial.println("SD_ERROR");
            return;
        }
    }
    
    if (!is_connected) {
        Serial.println("ERROR: Not connected");
        return;
    }
    
    // Check if we're changing playback modes - if so, always start from track 1
    bool mode_changed = (current_playback_mode != PLAYBACK_MODE_PLAYLIST2);
    
    // Rescan if mode changed or not playing
    if (mode_changed || mp3_file_count == 0 || (!is_playing && !is_paused)) {
        // Scan only the playlist2 file
        if (!scanPlaylist2Only()) {
            return;  // Error already reported
        }
    } else {
        Serial.println("Using cached playlist2.m3u");
    }
    
    // Set initial volume on first play
    if (!first_play_volume_set) {
        a2dp_source.set_volume(initial_volume);
        Serial.print("INIT_VOLUME_APPLIED:");
        Serial.println(initial_volume);
        first_play_volume_set = true;
        
        // Activate volume protection to prevent remote device from overriding
        volume_protection_active = true;
        volume_protection_start_time = millis();
    }
    
    // Not favorites mode (legacy flag)
    playing_favorites = false;
    // Disable single file repeat when playing playlist (plays through all files)
    single_file_repeat = false;
    
    // If we're already in playlist2 mode and playing, continue current playback
    // BUT: After reconnection, the file handle may be invalid even if is_playing is true
    // So check if the file is actually available before deciding to continue
    bool file_available = mp3_file && mp3_file.available();
    if (!mode_changed && (is_playing || is_paused) && is_playing_mp3 && file_available) {
        // Same mode, same playlist, file is open - continue current playback
        Serial.println("PLAYING");
        Serial.print("PLAYING_FILE:");
        Serial.println(current_playing_file);
        return;
    }
    
    // If we get here and was supposedly playing, log it (helps debug reconnection issues)
    if ((is_playing || is_paused) && !file_available) {
        Serial.println("STATUS: Restarting playback - file was not available");
    }
    
    // Update current mode - MUST be done before starting playback
    current_playback_mode = PLAYBACK_MODE_PLAYLIST2;
    
    // Start from first file
    if (!openMP3AtIndex(0)) {
        Serial.println("ERROR: Could not open playlist2 file");
        return;
    }
    
    Serial.println("PLAYING");
    is_playing = true;
    is_playing_mp3 = true;
}

/**
 * Scan a custom M3U file (any path, selected via file browser)
 * Returns true if playlist was found and has valid files
 * @param m3u_path Full path to the M3U file
 */
bool scanCustomM3U(const String& m3u_path) {
    mp3_file_count = 0;
    
    if (!sd_initialized) {
        Serial.println("M3U_ERROR:SD not initialized");
        return false;
    }
    
    fs::FS& fs = getSDFS();
    
    File playlist_file = fs.open(m3u_path);
    
    if (!playlist_file || playlist_file.isDirectory()) {
        Serial.print("M3U_NOT_FOUND:");
        Serial.println(m3u_path);
        return false;
    }
    
    // Get the directory containing the M3U file for relative path resolution
    String m3u_dir = m3u_path;
    int last_slash = m3u_dir.lastIndexOf('/');
    if (last_slash > 0) {
        // M3U is in a subdirectory (e.g., /music/playlist.m3u -> /music)
        m3u_dir = m3u_dir.substring(0, last_slash);
    } else if (last_slash == 0) {
        // M3U is at root (e.g., /playlist.m3u -> /)
        m3u_dir = "/";
    } else {
        // No slash found (shouldn't happen with valid absolute paths)
        m3u_dir = "";
    }
    
    Serial.print("Parsing M3U: ");
    Serial.println(m3u_path);
    
    while (playlist_file.available() && mp3_file_count < MAX_MP3_FILES) {
        String line = playlist_file.readStringUntil('\n');
        line.trim();
        
        // Skip empty lines and comments
        if (line.length() == 0 || line.startsWith("#")) {
            continue;
        }
        
        // Handle both relative paths and full paths
        String file_path;
        if (line.startsWith("/")) {
            // Absolute path
            file_path = line;
        } else if (m3u_dir == "/") {
            // Relative path from root directory - prepend just /
            file_path = "/" + line;
        } else if (m3u_dir.length() > 0) {
            // Relative path with non-root, non-empty directory - prepend M3U file's directory
            file_path = m3u_dir + "/" + line;
        } else {
            // No directory info (shouldn't happen) - treat as root relative
            file_path = "/" + line;
        }
        
        // Check if file exists and is MP3
        String lower_path = file_path;
        lower_path.toLowerCase();
        if (lower_path.endsWith(".mp3")) {
            if (fileExists(file_path)) {
                // Validate MP3 file before adding to playlist
                if (isValidMP3File(file_path)) {
                    mp3_files[mp3_file_count] = file_path;
                    Serial.print("M3U entry: ");
                    Serial.println(mp3_files[mp3_file_count]);
                    mp3_file_count++;
                } else {
                    Serial.print("Warning: Invalid MP3 file skipped: ");
                    Serial.println(file_path);
                }
            } else {
                Serial.print("FILE_NOT_FOUND:");
                Serial.println(file_path);
            }
        }
    }
    
    playlist_file.close();
    
    if (mp3_file_count > 0) {
        Serial.print("Loaded ");
        Serial.print(mp3_file_count);
        Serial.print(" files from ");
        Serial.println(m3u_path);
        return true;
    }
    
    Serial.print("M3U_EMPTY:");
    Serial.println(m3u_path);
    return false;
}

/**
 * Play from a custom M3U file (selected via file browser)
 * @param m3u_path Full path to the M3U file
 */
void playCustomM3U(const String& m3u_path) {
    // Try to initialize SD card if not already done
    if (!sd_initialized) {
        Serial.println("SD card not initialized, attempting late initialization...");
        if (!tryInitSDCard()) {
            Serial.println("SD_ERROR");
            return;
        }
    }
    
    if (!is_connected) {
        Serial.println("ERROR: Not connected");
        return;
    }
    
    // Check if we're playing the same M3U file or changing
    bool same_m3u = (current_playback_mode == PLAYBACK_MODE_CUSTOM_M3U && custom_m3u_path == m3u_path);
    
    // Rescan if M3U changed or not playing
    if (!same_m3u || mp3_file_count == 0 || (!is_playing && !is_paused)) {
        // Scan the custom M3U file
        if (!scanCustomM3U(m3u_path)) {
            // Ensure playback state is cleared so the Flipper doesn't wait indefinitely
            is_playing = false;
            is_playing_mp3 = false;
            playing_favorites = false;
            single_file_repeat = false;
            if (mp3_file) {
                mp3_file.close();
            }
            // Send a protocol-friendly error response in addition to any M3U_* messages
            Serial.println("PLAYBACK_ERROR:M3U");
            return;
        }
        custom_m3u_path = m3u_path;  // Save the path for comparison
    } else {
        Serial.print("Using cached M3U: ");
        Serial.println(m3u_path);
    }
    
    // Set initial volume on first play
    if (!first_play_volume_set) {
        a2dp_source.set_volume(initial_volume);
        Serial.print("INIT_VOLUME_APPLIED:");
        Serial.println(initial_volume);
        first_play_volume_set = true;
        
        // Activate volume protection to prevent remote device from overriding
        volume_protection_active = true;
        volume_protection_start_time = millis();
    }
    
    // Not favorites mode
    playing_favorites = false;
    // Disable single file repeat when playing M3U (plays through all files)
    single_file_repeat = false;
    
    // If we're already playing the same M3U, continue current playback
    bool file_available = mp3_file && mp3_file.available();
    if (same_m3u && (is_playing || is_paused) && is_playing_mp3 && file_available) {
        // Same M3U, file is open - continue current playback
        Serial.println("PLAYING");
        Serial.print("PLAYING_FILE:");
        Serial.println(current_playing_file);
        return;
    }
    
    // Update current mode
    current_playback_mode = PLAYBACK_MODE_CUSTOM_M3U;
    
    // Start from first file
    if (!openMP3AtIndex(0)) {
        Serial.println("ERROR: Could not open first file from M3U playlist");
        return;
    }
    
    Serial.println("PLAYING");
    is_playing = true;
    is_playing_mp3 = true;
}

/**
 * Stop current playback
 */
void stopPlayback() {
    if (is_playing || is_paused || wifi_streaming) {
        is_playing = false;
        is_paused = false;
        is_playing_mp3 = false;
        playing_favorites = false;
        single_file_repeat = false;  // Reset single file repeat mode
        current_playing_file = "";
        current_playback_mode = PLAYBACK_MODE_NONE;  // Reset playback mode
        
        // Clear reconnection state
        was_playing_before_disconnect = false;
        reconnection_in_progress = false;
        reconnection_requested = false;
        consecutive_decode_failures = 0;
        
        // Clear navigation queue
        nav_debounce_active = false;
        pending_nav_offset = 0;
        
        if (mp3_file) {
            mp3_file.close();
        }
        
        // Stop WiFi streaming if active
        if (wifi_streaming) {
            http_client.end();
            stream = nullptr;
            wifi_streaming = false;
            stream_prebuffering = false;
            stream_in_underrun = false;
            stream_buffer_bytes = 0;
            stream_buffer_read_pos = 0;
        }
        
        // Clear audio buffers
        mp3_buf_bytes = 0;
        pcm_buf_samples = 0;
        pcm_buf_read_pos = 0;
        
        Serial.println("STOPPED");
    } else {
        Serial.println("ERROR: Not playing");
    }
}

/**
 * Pause current playback
 */
void pausePlayback() {
    if (is_playing && !is_paused) {
        is_paused = true;
        Serial.println("PAUSED");
    } else if (is_paused) {
        Serial.println("ERROR: Already paused");
    } else {
        Serial.println("ERROR: Not playing");
    }
}

/**
 * Resume paused playback
 */
void resumePlayback() {
    if (is_paused) {
        is_paused = false;
        Serial.println("RESUMED");
    } else if (is_playing) {
        Serial.println("ERROR: Not paused");
    } else {
        Serial.println("ERROR: Not playing");
    }
}

/**
 * Open MP3 file at specific index
 * Returns true if file was successfully opened
 * Note: This function sets current_mp3_index to the CURRENTLY PLAYING track
 * CRITICAL: Uses track_switching flag to prevent race conditions with audio callback
 */
bool openMP3AtIndex(int index) {
    // CRITICAL: Set track_switching flag FIRST to prevent audio callback from accessing buffers
    // This prevents xQueueGenericSend assertion failures during track transitions
    track_switching = true;
    
    // Small delay to ensure audio callback sees the flag (it runs at high frequency)
    // This is critical - without this, there's a small window where callback could still be running
    // Note: yield() alone is not a reliable synchronization mechanism for concurrent access
    // Using delayMicroseconds ensures the volatile flag is properly visible to the audio callback thread
    delayMicroseconds(100);
    
    // Close any previously open file
    if (mp3_file) {
        mp3_file.close();
    }
    
    // Check bounds
    if (index < 0 || index >= mp3_file_count) {
        track_switching = false;
        return false;
    }
    
    // Clear all audio buffers BEFORE opening new file
    // This prevents audio glitches from leftover PCM data
    mp3_buf_bytes = 0;
    pcm_buf_samples = 0;
    pcm_buf_read_pos = 0;
    memset(pcm_buf, 0, sizeof(pcm_buf));  // Clear PCM buffer to prevent artifacts
    
    // Open the file using the appropriate filesystem
    fs::FS& fs = getSDFS();
    mp3_file = fs.open(mp3_files[index]);
    if (!mp3_file) {
        Serial.print("Failed to open: ");
        Serial.println(mp3_files[index]);
        track_switching = false;
        return false;
    }
    
    // Update current index to point to THIS track (currently playing)
    current_mp3_index = index;
    
    // Store and report current filename
    current_playing_file = mp3_files[index];
    Serial.print("PLAYING_FILE:");
    Serial.println(current_playing_file);
    
    // Read initial data and skip ID3 tag if present
    mp3_buf_bytes = mp3_file.read(mp3_buf, MP3_BUF_SIZE);
    if (mp3_buf_bytes > 0) {
        skipID3Tag();
        if (mp3_buf_bytes == 0) {
            // Refill buffer after skipping ID3
            mp3_buf_bytes = mp3_file.read(mp3_buf, MP3_BUF_SIZE);
        }
    }
    
    // Reset EQ filter states for new track to avoid audio glitches
    resetEqStates();
    
    // Reset consecutive decode failures counter for new track
    consecutive_decode_failures = 0;
    
    // Record when this track started playing for minimum playback time check
    track_start_time = millis();
    
    // CRITICAL: Clear track_switching flag AFTER all buffer operations complete
    // This allows audio callback to resume reading from buffers safely
    track_switching = false;
    
    return mp3_buf_bytes > 0;
}

/**
 * Activate navigation command protection
 * Resets failure counter and activates protection window to prevent race conditions
 */
void activateNavigationProtection() {
    consecutive_decode_failures = 0;
    navigation_command_active = true;
    navigation_command_time = millis();
}

/**
 * Execute a track change to specific index (internal function)
 * This actually opens the file and starts playback
 */
void executeTrackChange(int target_index) {
    // Bounds check and wrap
    if (target_index < 0) {
        target_index = mp3_file_count - 1;
    } else if (target_index >= mp3_file_count) {
        target_index = 0;
    }
    
    // Activate navigation protection BEFORE any file operations
    activateNavigationProtection();
    
    is_paused = false;
    if (openMP3AtIndex(target_index)) {
        is_playing = true;
        is_playing_mp3 = true;
    } else {
        Serial.println("ERROR: Could not open track");
    }
}

/**
 * Execute queued navigation after debounce timeout
 * Called from main loop when navigation commands have been queued
 */
void executeQueuedNavigation() {
    if (pending_nav_offset == 0) {
        nav_debounce_active = false;
        return;
    }
    
    if (!is_playing_mp3 && !is_paused) {
        Serial.println("ERROR: Not playing MP3");
        nav_debounce_active = false;
        pending_nav_offset = 0;
        return;
    }
    
    // Calculate target index based on accumulated offset
    int target_index;
    
    if (shuffle_mode && mp3_file_count > 1 && pending_nav_offset > 0) {
        // Shuffle mode with forward navigation: pick random track
        do {
            target_index = random(0, mp3_file_count);
        } while (target_index == current_mp3_index && mp3_file_count > 1);
    } else {
        // Normal mode or backward navigation: apply offset
        target_index = current_mp3_index + pending_nav_offset;
        // Wrap around
        while (target_index < 0) {
            target_index += mp3_file_count;
        }
        while (target_index >= mp3_file_count) {
            target_index -= mp3_file_count;
        }
    }
    
    Serial.printf("NAV_EXECUTE: offset=%d, target=%d\n", pending_nav_offset, target_index);
    
    // Clear debounce state BEFORE executing (to avoid re-entry)
    nav_debounce_active = false;
    pending_nav_offset = 0;
    
    // Execute the track change
    executeTrackChange(target_index);
}

/**
 * Queue a navigation command (with debouncing for rapid presses)
 * @param offset +1 for next, -1 for previous
 */
void queueNavigation(int offset) {
    uint32_t now = millis();
    
    // Check if we're within the debounce window
    // Note: Unsigned subtraction handles millis() overflow correctly (~49 days)
    if (nav_debounce_active && (now - last_nav_command_time) < NAV_QUEUE_TIMEOUT_MS) {
        // Within debounce window - accumulate the offset
        pending_nav_offset += offset;
        last_nav_command_time = now;
        Serial.printf("NAV_QUEUED: offset=%d, total=%d\n", offset, pending_nav_offset);
    } else {
        // New navigation sequence - start fresh
        pending_nav_offset = offset;
        nav_debounce_active = true;
        last_nav_command_time = now;
        Serial.printf("NAV_START: offset=%d\n", offset);
    }
}

/**
 * Skip to next track
 * Uses debouncing to prevent BT disconnection from rapid presses
 * Supports shuffle mode for random track selection
 */
void nextTrack() {
    if (!is_playing_mp3 && !is_paused) {
        Serial.println("ERROR: Not playing MP3");
        return;
    }
    
    // Queue the navigation instead of immediate execution
    queueNavigation(+1);
}

/**
 * Go to previous track
 * Uses debouncing to prevent BT disconnection from rapid presses
 * Immediately goes to the previous track (no double-tap behavior)
 * Use RESTART command to restart current track
 */
void prevTrack() {
    if (!is_playing_mp3 && !is_paused) {
        Serial.println("ERROR: Not playing MP3");
        return;
    }
    
    // Queue the navigation instead of immediate execution
    queueNavigation(-1);
}

/**
 * Restart current track from beginning
 * current_mp3_index already points to current track
 * Note: This cancels any pending navigation queue
 */
void restartTrack() {
    if (!is_playing_mp3 && !is_paused) {
        Serial.println("ERROR: Not playing MP3");
        return;
    }
    
    // Cancel any pending navigation
    nav_debounce_active = false;
    pending_nav_offset = 0;
    
    // Reset failure counter and activate navigation protection BEFORE any file operations
    activateNavigationProtection();
    
    // Reopen current file
    int current_index = current_mp3_index;
    
    is_paused = false;
    if (openMP3AtIndex(current_index)) {
        is_playing = true;
        is_playing_mp3 = true;
        Serial.println("RESTARTED");
    } else {
        Serial.println("ERROR: Could not restart track");
    }
}

/**
 * Seek forward approximately 5 seconds
 * Note: This is approximate as MP3 frames vary in size
 */
void seekForward() {
    if (!is_playing_mp3 && !is_paused) {
        Serial.println("ERROR: Not playing MP3");
        return;
    }
    
    if (!mp3_file) {
        Serial.println("ERROR: No file open");
        return;
    }
    
    size_t current_pos = mp3_file.position();
    size_t file_size = mp3_file.size();
    size_t new_pos = current_pos + SEEK_BYTES_5_SEC;
    
    if (new_pos >= file_size) {
        // Near end of file, go to next track
        nextTrack();
        return;
    }
    
    // Protect buffer operations from audio callback
    track_switching = true;
    // Note: yield() alone is not a reliable synchronization mechanism for concurrent access
    // Using delayMicroseconds ensures the volatile flag is properly visible to the audio callback thread
    delayMicroseconds(100);
    
    mp3_file.seek(new_pos);
    // Clear buffers to force re-sync
    mp3_buf_bytes = 0;
    pcm_buf_samples = 0;
    pcm_buf_read_pos = 0;
    memset(pcm_buf, 0, sizeof(pcm_buf));  // Clear PCM buffer to prevent artifacts
    
    track_switching = false;
    Serial.println("SEEK_FWD_OK");
}

/**
 * Seek backward approximately 5 seconds
 * Note: This is approximate as MP3 frames vary in size
 */
void seekBackward() {
    if (!is_playing_mp3 && !is_paused) {
        Serial.println("ERROR: Not playing MP3");
        return;
    }
    
    if (!mp3_file) {
        Serial.println("ERROR: No file open");
        return;
    }
    
    size_t current_pos = mp3_file.position();
    size_t new_pos = 0;
    
    if (current_pos > SEEK_BYTES_5_SEC) {
        new_pos = current_pos - SEEK_BYTES_5_SEC;
    }
    // else stay at beginning
    
    // Protect buffer operations from audio callback
    track_switching = true;
    // Note: yield() alone is not a reliable synchronization mechanism for concurrent access
    // Using delayMicroseconds ensures the volatile flag is properly visible to the audio callback thread
    delayMicroseconds(100);
    
    mp3_file.seek(new_pos);
    // Clear buffers to force re-sync
    mp3_buf_bytes = 0;
    pcm_buf_samples = 0;
    pcm_buf_read_pos = 0;
    memset(pcm_buf, 0, sizeof(pcm_buf));  // Clear PCM buffer to prevent artifacts
    
    track_switching = false;
    Serial.println("SEEK_BACK_OK");
}
