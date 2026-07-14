/*
 * BT Audio - ESP32 A2DP Source Firmware
 * 
 * This firmware enables ESP32 to act as an A2DP SOURCE, streaming audio
 * TO Bluetooth headphones/speakers from the Flipper Zero.
 * 
 * Hardware: ESP32 (original), ESP32-C3 (Bluetooth Classic support required)
 * Note: ESP32-S2 does NOT support Bluetooth Classic/A2DP
 * 
 * Mode Selection:
 * - Default: A2DP SOURCE (stream TO headphones/speakers)
 * - To enable SINK mode: Uncomment #define BT_AUDIO_SINK_MODE below
 * 
 * UART Protocol (115200 baud):
 * Commands from Flipper:
 *   SCAN\n           - Scan for Bluetooth devices
 *   CONNECT <MAC>\n  - Connect to device (source mode: connect to headphones)
 *   PLAY\n           - Play test tone (440Hz sine wave, 2 seconds)
 *   PLAY_TREK\n      - Play Star Trek theme (8 notes, ~5 seconds)
 *   DISCONNECT\n     - Disconnect from device
 * 
 * Responses to Flipper:
 *   DEVICE:<MAC>,<Name>\n  - Found device during scan
 *   SCAN_COMPLETE\n        - Scan finished
 *   CONNECTED\n            - Successfully connected
 *   FAILED\n               - Connection failed
 *   PLAYING\n              - Started playing tone
 *   DISCONNECTED\n         - Disconnected
 */

// ============================================================================
// MODE SELECTION - Uncomment to enable SINK mode (receive audio FROM source)
// ============================================================================
// #define BT_AUDIO_SINK_MODE

// ============================================================================
// INCLUDES
// ============================================================================
#ifdef BT_AUDIO_SINK_MODE
  #include "BluetoothA2DPSink.h"
  BluetoothA2DPSink a2dp_sink;
#else
  #include "BluetoothA2DPSource.h"
  BluetoothA2DPSource a2dp_source;
#endif

#include <math.h>

// ============================================================================
// CONFIGURATION
// ============================================================================
#define UART_BAUD 115200
#define BT_DEVICE_NAME "Flipper_BT_Audio"
#define TEST_TONE_FREQ 440.0      // A4 note (440 Hz)
#define TEST_TONE_DURATION 2000   // 2 seconds
#define SAMPLE_RATE 44100         // Standard audio sample rate

// ============================================================================
// GLOBAL STATE
// ============================================================================
bool is_connected = false;
bool is_playing = false;
bool is_playing_trek = false;  // Track if playing Star Trek theme
uint32_t play_start_time = 0;
uint32_t tone_sample_count = 0;

// Star Trek TOS theme - first 8 notes of opening fanfare (approximated frequencies)
// Notes: C5, C4, G4, C5, E5, G5, A5, G5
#define TREK_NOTE_COUNT 8
const float trek_notes[TREK_NOTE_COUNT] = {
    523.25,  // C5
    261.63,  // C4  
    392.00,  // G4
    523.25,  // C5
    659.25,  // E5
    783.99,  // G5
    880.00,  // A5
    783.99   // G5
};
const int trek_durations[TREK_NOTE_COUNT] = {
    400, 200, 300, 400, 400, 600, 300, 800  // milliseconds per note (sum ~3400ms, but theme extends to 5s)
};
#define TREK_TOTAL_DURATION 5000  // Total duration ~5 seconds (includes final note sustain)

// ============================================================================
// AUDIO GENERATION - Test Tone (Sine Wave)
// ============================================================================
#ifndef BT_AUDIO_SINK_MODE
// Get current note for Star Trek theme based on elapsed time
int get_trek_note_index(uint32_t elapsed_ms) {
    uint32_t time_accum = 0;
    for (int i = 0; i < TREK_NOTE_COUNT; i++) {
        time_accum += trek_durations[i];
        if (elapsed_ms < time_accum) {
            return i;
        }
    }
    return -1;  // Done playing
}

// Generate audio samples for A2DP source
// This function is called by the A2DP library to get audio data
int32_t get_audio_data(Frame* frame, int32_t frame_count) {
    if (!is_playing) {
        // Silence when not playing
        for (int i = 0; i < frame_count; i++) {
            frame[i].channel1 = 0;
            frame[i].channel2 = 0;
        }
        return frame_count;
    }
    
    uint32_t elapsed = millis() - play_start_time;
    uint32_t duration = is_playing_trek ? TREK_TOTAL_DURATION : TEST_TONE_DURATION;
    
    // Check if we should stop playing
    if (elapsed > duration) {
        is_playing = false;
        is_playing_trek = false;
        Serial.println("STOPPED");
        // Send silence for this frame
        for (int i = 0; i < frame_count; i++) {
            frame[i].channel1 = 0;
            frame[i].channel2 = 0;
        }
        return frame_count;
    }
    
    // Generate audio samples
    for (int i = 0; i < frame_count; i++) {
        float t = (float)tone_sample_count / (float)SAMPLE_RATE;
        float freq;
        
        if (is_playing_trek) {
            // Star Trek theme - get current note
            uint32_t sample_time_ms = (uint32_t)((float)tone_sample_count * 1000.0 / SAMPLE_RATE);
            int note_idx = get_trek_note_index(sample_time_ms);
            if (note_idx >= 0) {
                freq = trek_notes[note_idx];
            } else {
                freq = 0;  // Silence after notes
            }
        } else {
            // Simple test tone
            freq = TEST_TONE_FREQ;
        }
        
        float sample = (freq > 0) ? sin(2.0 * PI * freq * t) : 0;
        
        // Convert to 16-bit signed integer
        int16_t sample_i16 = (int16_t)(sample * 32767.0 * 0.5); // 50% volume
        
        // Stereo output (same signal to both channels)
        frame[i].channel1 = sample_i16;
        frame[i].channel2 = sample_i16;
        
        tone_sample_count++;
    }
    
    return frame_count;
}
#endif

// ============================================================================
// COMMAND PROCESSING
// ============================================================================
void process_command(String cmd) {
    cmd.trim();
    
    if (cmd == "SCAN") {
        // Start Bluetooth scan
        Serial.println("Starting scan...");
        
        // NOTE: This is a MOCK implementation for demonstration
        // Real implementation would use ESP32 Bluetooth scan APIs:
        //   - esp_bt_gap_start_discovery() for Classic BT
        //   - Enumerate discovered devices
        //   - Return actual MAC addresses and device names
        
        #ifdef BT_AUDIO_SINK_MODE
        // In sink mode, scan for audio sources (phones, PCs)
        // Mock devices for testing
        delay(1000);
        Serial.println("DEVICE:00:11:22:33:44:55,Android Phone");
        Serial.println("DEVICE:AA:BB:CC:DD:EE:FF,iPhone");
        #else
        // In source mode, scan for audio sinks (headphones, speakers)
        // Mock devices for testing
        delay(1000);
        Serial.println("DEVICE:11:22:33:44:55:66,BT Headphones");
        Serial.println("DEVICE:AA:BB:CC:DD:EE:00,BT Speaker");
        #endif
        
        delay(500);
        Serial.println("SCAN_COMPLETE");
        
    } else if (cmd.startsWith("CONNECT ")) {
        String mac = cmd.substring(8);
        Serial.print("Connecting to: ");
        Serial.println(mac);
        
        // NOTE: This is a MOCK implementation for demonstration
        // Real implementation would:
        //   - Parse MAC address
        //   - Call a2dp_source.connect(mac_address) or a2dp_sink.connect()
        //   - Handle pairing and authentication
        //   - Return actual connection status
        
        #ifdef BT_AUDIO_SINK_MODE
        // Sink mode: We receive audio FROM a source device
        // The sink starts and waits for connection
        if (!is_connected) {
            delay(1500);  // Simulate connection time
            is_connected = true;
            Serial.println("CONNECTED");
        } else {
            Serial.println("FAILED");
        }
        #else
        // Source mode: We connect TO a sink device (headphones/speakers)
        if (!is_connected) {
            delay(1500);  // Simulate connection time
            is_connected = true;
            Serial.println("CONNECTED");
        } else {
            Serial.println("FAILED");
        }
        #endif
        
    } else if (cmd == "PLAY") {
        #ifdef BT_AUDIO_SINK_MODE
        // In sink mode, we receive audio - can't generate locally
        Serial.println("ERROR: Sink mode receives audio, cannot play");
        #else
        // Source mode: Play test tone TO the connected sink
        if (is_connected) {
            Serial.println("PLAYING");
            is_playing = true;
            is_playing_trek = false;
            play_start_time = millis();
            tone_sample_count = 0;
        } else {
            Serial.println("ERROR: Not connected");
        }
        #endif
        
    } else if (cmd == "PLAY_TREK") {
        #ifdef BT_AUDIO_SINK_MODE
        // In sink mode, we receive audio - can't generate locally
        Serial.println("ERROR: Sink mode receives audio, cannot play");
        #else
        // Source mode: Play Star Trek theme TO the connected sink
        if (is_connected) {
            Serial.println("PLAYING");
            is_playing = true;
            is_playing_trek = true;
            play_start_time = millis();
            tone_sample_count = 0;
        } else {
            Serial.println("ERROR: Not connected");
        }
        #endif
        
    } else if (cmd == "DISCONNECT") {
        if (is_connected) {
            #ifdef BT_AUDIO_SINK_MODE
            a2dp_sink.disconnect();
            #else
            a2dp_source.disconnect();
            #endif
            is_connected = false;
            is_playing = false;
            is_playing_trek = false;
            Serial.println("DISCONNECTED");
        } else {
            Serial.println("DISCONNECTED");
        }
        
    } else if (cmd == "STATUS") {
        // Query current status
        Serial.print("MODE:");
        #ifdef BT_AUDIO_SINK_MODE
        Serial.println("SINK");
        #else
        Serial.println("SOURCE");
        #endif
        Serial.print("CONNECTED:");
        Serial.println(is_connected ? "YES" : "NO");
        Serial.print("PLAYING:");
        Serial.println(is_playing ? "YES" : "NO");
        
    } else {
        Serial.print("UNKNOWN: ");
        Serial.println(cmd);
    }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    // Initialize serial communication
    Serial.begin(UART_BAUD);
    delay(1000);
    
    Serial.println("=================================");
    Serial.println("BT Audio ESP32 Firmware");
    #ifdef BT_AUDIO_SINK_MODE
    Serial.println("Mode: A2DP SINK (receive audio)");
    #else
    Serial.println("Mode: A2DP SOURCE (transmit audio)");
    #endif
    Serial.println("=================================");
    
    // Initialize Bluetooth A2DP
    #ifdef BT_AUDIO_SINK_MODE
    a2dp_sink.start(BT_DEVICE_NAME);
    Serial.println("A2DP Sink started");
    #else
    a2dp_source.start(BT_DEVICE_NAME, get_audio_data);
    Serial.println("A2DP Source started");
    #endif
    
    Serial.println("Ready for commands");
    Serial.println("Send 'STATUS' to check mode");
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
    // Read and process UART commands
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        process_command(cmd);
    }
    
    // Small delay to prevent tight loop
    delay(10);
}
