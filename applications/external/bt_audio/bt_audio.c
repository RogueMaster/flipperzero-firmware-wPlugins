#include "bt_audio.h"
#include "lib/qrcodegen/qrcodegen.h"
#include <stdlib.h>
#include <string.h>
#include <expansion/expansion.h>

// Default initial volume (~75% of max 127)
#define BT_AUDIO_DEFAULT_VOLUME 96

typedef struct {
    uint8_t* qrcode;
    uint8_t* buffer;
} QRCodeModel;

// Config load/save functions
void bt_audio_config_load(BtAudio* app) {
    // Set defaults
    // Note: Changed default from Flipper SD to ESP32 SD in v2.2 for better performance
    // Users upgrading from v2.1 or earlier may need to adjust this setting
    app->config.sd_source = BtAudioSdSourceESP32; // ESP32 SD is default for better performance
    app->config.tx_power = BtAudioTxPowerMax; // Max power for best range
    app->config.enable_5v_gpio = true; // Auto-enable 5V by default
    app->config.background_mode = false; // Background audio off by default
    app->config.keep_backlight_on = false; // Backlight timeout by default
    app->config.shuffle_mode = false; // Shuffle mode off by default
    app->config.continuous_play =
        false; // Repeat single file by default (OFF = repeat, ON = play next)
    app->config.initial_volume = BT_AUDIO_DEFAULT_VOLUME; // ~75% volume by default
    app->config.eq_bass = 0; // EQ flat by default
    app->config.eq_mid = 0;
    app->config.eq_treble = 0;
    app->config.last_connected_mac[0] = '\0';
    strncpy(
        app->config.bt_device_name, "FlipperAudio", BT_AUDIO_BT_NAME_LEN - 1); // Default BT name
    app->config.bt_device_name[BT_AUDIO_BT_NAME_LEN - 1] = '\0';

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, BT_AUDIO_CONFIG_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[128];
        size_t bytes_read;
        size_t line_pos = 0;

        while((bytes_read = storage_file_read(file, line + line_pos, 1)) > 0) {
            if(line[line_pos] == '\n' || line_pos >= sizeof(line) - 1) {
                line[line_pos] = '\0';

                // Parse key=value pairs
                if(strncmp(line, "sd_source=", 10) == 0) {
                    int val = atoi(line + 10);
                    if(val >= 0 && val < BtAudioSdSourceCount) {
                        app->config.sd_source = (BtAudioSdSource)val;
                    }
                } else if(strncmp(line, "tx_power=", 9) == 0) {
                    int val = atoi(line + 9);
                    if(val >= 0 && val < BtAudioTxPowerCount) {
                        app->config.tx_power = (BtAudioTxPower)val;
                    }
                } else if(strncmp(line, "enable_5v=", 10) == 0) {
                    int val = atoi(line + 10);
                    app->config.enable_5v_gpio = (val != 0);
                } else if(strncmp(line, "background=", 11) == 0) {
                    int val = atoi(line + 11);
                    app->config.background_mode = (val != 0);
                } else if(strncmp(line, "backlight=", 10) == 0) {
                    int val = atoi(line + 10);
                    app->config.keep_backlight_on = (val != 0);
                } else if(strncmp(line, "shuffle=", 8) == 0) {
                    int val = atoi(line + 8);
                    app->config.shuffle_mode = (val != 0);
                } else if(strncmp(line, "continuous=", 11) == 0) {
                    int val = atoi(line + 11);
                    app->config.continuous_play = (val != 0);
                } else if(strncmp(line, "init_vol=", 9) == 0) {
                    int val = atoi(line + 9);
                    if(val >= 0 && val <= 127) {
                        app->config.initial_volume = (uint8_t)val;
                    }
                } else if(strncmp(line, "eq_bass=", 8) == 0) {
                    int val = atoi(line + 8);
                    if(val >= -10 && val <= 10) {
                        app->config.eq_bass = (int8_t)val;
                    }
                } else if(strncmp(line, "eq_mid=", 7) == 0) {
                    int val = atoi(line + 7);
                    if(val >= -10 && val <= 10) {
                        app->config.eq_mid = (int8_t)val;
                    }
                } else if(strncmp(line, "eq_treble=", 10) == 0) {
                    int val = atoi(line + 10);
                    if(val >= -10 && val <= 10) {
                        app->config.eq_treble = (int8_t)val;
                    }
                } else if(strncmp(line, "last_mac=", 9) == 0) {
                    strncpy(app->config.last_connected_mac, line + 9, BT_AUDIO_MAC_LEN - 1);
                    app->config.last_connected_mac[BT_AUDIO_MAC_LEN - 1] = '\0';
                } else if(strncmp(line, "bt_name=", 8) == 0) {
                    strncpy(app->config.bt_device_name, line + 8, BT_AUDIO_BT_NAME_LEN - 1);
                    app->config.bt_device_name[BT_AUDIO_BT_NAME_LEN - 1] = '\0';
                }

                line_pos = 0;
            } else {
                line_pos++;
            }
        }
        storage_file_close(file);
        FURI_LOG_I(TAG, "Config loaded");
    } else {
        FURI_LOG_I(TAG, "No config file, using defaults");
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void bt_audio_config_save(BtAudio* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Ensure directory exists
    storage_simply_mkdir(storage, BT_AUDIO_CONFIG_DIR);

    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, BT_AUDIO_CONFIG_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[128];
        int len;

        len = snprintf(buf, sizeof(buf), "sd_source=%d\n", app->config.sd_source);
        storage_file_write(file, buf, len);

        len = snprintf(buf, sizeof(buf), "tx_power=%d\n", app->config.tx_power);
        storage_file_write(file, buf, len);

        len = snprintf(buf, sizeof(buf), "enable_5v=%d\n", app->config.enable_5v_gpio ? 1 : 0);
        storage_file_write(file, buf, len);

        len = snprintf(buf, sizeof(buf), "background=%d\n", app->config.background_mode ? 1 : 0);
        storage_file_write(file, buf, len);

        len = snprintf(buf, sizeof(buf), "backlight=%d\n", app->config.keep_backlight_on ? 1 : 0);
        storage_file_write(file, buf, len);

        len = snprintf(buf, sizeof(buf), "shuffle=%d\n", app->config.shuffle_mode ? 1 : 0);
        storage_file_write(file, buf, len);

        len = snprintf(buf, sizeof(buf), "continuous=%d\n", app->config.continuous_play ? 1 : 0);
        storage_file_write(file, buf, len);

        len = snprintf(buf, sizeof(buf), "init_vol=%d\n", app->config.initial_volume);
        storage_file_write(file, buf, len);

        len = snprintf(buf, sizeof(buf), "eq_bass=%d\n", app->config.eq_bass);
        storage_file_write(file, buf, len);

        len = snprintf(buf, sizeof(buf), "eq_mid=%d\n", app->config.eq_mid);
        storage_file_write(file, buf, len);

        len = snprintf(buf, sizeof(buf), "eq_treble=%d\n", app->config.eq_treble);
        storage_file_write(file, buf, len);

        if(app->config.last_connected_mac[0] != '\0') {
            len = snprintf(buf, sizeof(buf), "last_mac=%s\n", app->config.last_connected_mac);
            storage_file_write(file, buf, len);
        }

        if(app->config.bt_device_name[0] != '\0') {
            len = snprintf(buf, sizeof(buf), "bt_name=%s\n", app->config.bt_device_name);
            storage_file_write(file, buf, len);
        }

        storage_file_close(file);
        FURI_LOG_I(TAG, "Config saved");
    } else {
        FURI_LOG_E(TAG, "Failed to save config");
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// WiFi config load/save functions
void bt_audio_wifi_config_load(BtAudio* app) {
    // Set defaults
    app->wifi_config.wifi_enabled = false;
    app->wifi_config.ssid[0] = '\0';
    app->wifi_config.password[0] = '\0';
    app->wifi_config.stream_url[0] = '\0';

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, BT_AUDIO_WIFI_CONFIG_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[512]; // Longer buffer to accommodate URLs
        size_t bytes_read;
        size_t line_pos = 0;

        while((bytes_read = storage_file_read(file, line + line_pos, 1)) > 0) {
            if(line[line_pos] == '\n' || line_pos >= sizeof(line) - 1) {
                line[line_pos] = '\0';

                // Parse key=value pairs
                if(strncmp(line, "wifi_enabled=", 13) == 0) {
                    int val = atoi(line + 13);
                    app->wifi_config.wifi_enabled = (val != 0);
                } else if(strncmp(line, "ssid=", 5) == 0) {
                    strncpy(app->wifi_config.ssid, line + 5, BT_AUDIO_WIFI_SSID_LEN - 1);
                    app->wifi_config.ssid[BT_AUDIO_WIFI_SSID_LEN - 1] = '\0';
                } else if(strncmp(line, "password=", 9) == 0) {
                    strncpy(app->wifi_config.password, line + 9, BT_AUDIO_WIFI_PASS_LEN - 1);
                    app->wifi_config.password[BT_AUDIO_WIFI_PASS_LEN - 1] = '\0';
                } else if(strncmp(line, "stream_url=", 11) == 0) {
                    strncpy(app->wifi_config.stream_url, line + 11, BT_AUDIO_STREAM_URL_LEN - 1);
                    app->wifi_config.stream_url[BT_AUDIO_STREAM_URL_LEN - 1] = '\0';
                }

                line_pos = 0;
            } else {
                line_pos++;
            }
        }
        storage_file_close(file);
        FURI_LOG_I(TAG, "WiFi config loaded");
    } else {
        FURI_LOG_I(TAG, "No WiFi config file, using defaults");
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void bt_audio_wifi_config_save(BtAudio* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Ensure directory exists
    storage_simply_mkdir(storage, BT_AUDIO_CONFIG_DIR);

    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, BT_AUDIO_WIFI_CONFIG_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[512]; // Longer buffer to accommodate URLs
        int len;

        len =
            snprintf(buf, sizeof(buf), "wifi_enabled=%d\n", app->wifi_config.wifi_enabled ? 1 : 0);
        storage_file_write(file, buf, len);

        if(app->wifi_config.ssid[0] != '\0') {
            len = snprintf(buf, sizeof(buf), "ssid=%s\n", app->wifi_config.ssid);
            storage_file_write(file, buf, len);
        }

        if(app->wifi_config.password[0] != '\0') {
            len = snprintf(buf, sizeof(buf), "password=%s\n", app->wifi_config.password);
            storage_file_write(file, buf, len);
        }

        if(app->wifi_config.stream_url[0] != '\0') {
            len = snprintf(buf, sizeof(buf), "stream_url=%s\n", app->wifi_config.stream_url);
            storage_file_write(file, buf, len);
        }

        storage_file_close(file);
        FURI_LOG_I(TAG, "WiFi config saved");
    } else {
        FURI_LOG_E(TAG, "Failed to save WiFi config");
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// Favorites load/save functions
void bt_audio_favorites_load(BtAudio* app) {
    app->favorites_count = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, BT_AUDIO_FAVORITES_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[BT_AUDIO_FILENAME_LEN];
        size_t line_pos = 0;
        size_t bytes_read;

        while((bytes_read = storage_file_read(file, line + line_pos, 1)) > 0) {
            if(line[line_pos] == '\n' || line_pos >= sizeof(line) - 1) {
                line[line_pos] = '\0';

                // Skip empty lines and comments
                if(line[0] != '\0' && line[0] != '#' &&
                   app->favorites_count < BT_AUDIO_MAX_FAVORITES) {
                    strncpy(app->favorites[app->favorites_count], line, BT_AUDIO_FILENAME_LEN - 1);
                    app->favorites[app->favorites_count][BT_AUDIO_FILENAME_LEN - 1] = '\0';
                    app->favorites_count++;
                }
                line_pos = 0;
            } else {
                line_pos++;
            }
        }
        storage_file_close(file);
        FURI_LOG_I(TAG, "Loaded %d favorites", app->favorites_count);
    } else {
        FURI_LOG_I(TAG, "No favorites file, starting empty");
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void bt_audio_favorites_save(BtAudio* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Ensure directory exists
    storage_simply_mkdir(storage, BT_AUDIO_CONFIG_DIR);

    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, BT_AUDIO_FAVORITES_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        // Write header comment
        const char* header = "# BT Audio Favorites\n# One filename per line\n";
        storage_file_write(file, header, strlen(header));

        for(uint8_t i = 0; i < app->favorites_count; i++) {
            storage_file_write(file, app->favorites[i], strlen(app->favorites[i]));
            storage_file_write(file, "\n", 1);
        }

        storage_file_close(file);
        FURI_LOG_I(TAG, "Saved %d favorites", app->favorites_count);
    } else {
        FURI_LOG_E(TAG, "Failed to save favorites");
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool bt_audio_is_favorite(BtAudio* app, const char* filename) {
    if(!filename || filename[0] == '\0') return false;

    for(uint8_t i = 0; i < app->favorites_count; i++) {
        if(strcmp(app->favorites[i], filename) == 0) {
            return true;
        }
    }
    return false;
}

bool bt_audio_add_favorite(BtAudio* app, const char* filename) {
    if(!filename || filename[0] == '\0') return false;

    // Check if already a favorite
    if(bt_audio_is_favorite(app, filename)) {
        return false;
    }

    // Check if we have room
    if(app->favorites_count >= BT_AUDIO_MAX_FAVORITES) {
        FURI_LOG_W(TAG, "Favorites list full");
        return false;
    }

    // Add to favorites
    strncpy(app->favorites[app->favorites_count], filename, BT_AUDIO_FILENAME_LEN - 1);
    app->favorites[app->favorites_count][BT_AUDIO_FILENAME_LEN - 1] = '\0';
    app->favorites_count++;

    // Save immediately
    bt_audio_favorites_save(app);

    FURI_LOG_I(TAG, "Added favorite: %s", filename);
    return true;
}

bool bt_audio_remove_favorite(BtAudio* app, const char* filename) {
    if(!filename || filename[0] == '\0') return false;

    // Find the favorite
    for(uint8_t i = 0; i < app->favorites_count; i++) {
        if(strcmp(app->favorites[i], filename) == 0) {
            // Remove by shifting remaining items down
            for(uint8_t j = i; j < app->favorites_count - 1; j++) {
                strncpy(app->favorites[j], app->favorites[j + 1], BT_AUDIO_FILENAME_LEN);
            }
            app->favorites_count--;

            // Save immediately
            bt_audio_favorites_save(app);

            FURI_LOG_I(TAG, "Removed favorite: %s", filename);
            return true;
        }
    }
    return false;
}

bool bt_audio_toggle_favorite(BtAudio* app, const char* filename) {
    if(bt_audio_is_favorite(app, filename)) {
        return bt_audio_remove_favorite(app, filename);
    } else {
        return bt_audio_add_favorite(app, filename);
    }
}

// Device history load/save functions
void bt_audio_device_history_load(BtAudio* app) {
    app->device_history_count = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, BT_AUDIO_DEVICE_HISTORY_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[BT_AUDIO_DEVICE_NAME_LEN + BT_AUDIO_MAC_LEN + 2]; // MAC,name\n
        size_t line_pos = 0;
        size_t bytes_read;

        while((bytes_read = storage_file_read(file, line + line_pos, 1)) > 0) {
            if(line[line_pos] == '\n' || line_pos >= sizeof(line) - 1) {
                line[line_pos] = '\0';

                // Skip empty lines and comments
                if(line[0] != '\0' && line[0] != '#' &&
                   app->device_history_count < BT_AUDIO_DEVICE_HISTORY_SIZE) {
                    // Parse "MAC,Name" format
                    const char* comma = strchr(line, ',');
                    if(comma) {
                        size_t mac_len = comma - line;
                        if(mac_len > 0 && mac_len < BT_AUDIO_MAC_LEN) {
                            strncpy(
                                app->device_history[app->device_history_count].mac, line, mac_len);
                            app->device_history[app->device_history_count].mac[mac_len] = '\0';
                            strncpy(
                                app->device_history[app->device_history_count].name,
                                comma + 1,
                                BT_AUDIO_DEVICE_NAME_LEN - 1);
                            app->device_history[app->device_history_count]
                                .name[BT_AUDIO_DEVICE_NAME_LEN - 1] = '\0';
                            app->device_history_count++;
                        }
                    }
                }
                line_pos = 0;
            } else {
                line_pos++;
            }
        }
        storage_file_close(file);
        FURI_LOG_I(TAG, "Loaded %d device history entries", app->device_history_count);
    } else {
        FURI_LOG_I(TAG, "No device history file, starting empty");
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void bt_audio_device_history_save(BtAudio* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Ensure directory exists
    storage_simply_mkdir(storage, BT_AUDIO_CONFIG_DIR);

    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, BT_AUDIO_DEVICE_HISTORY_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        // Write header comment
        const char* header = "# BT Audio Device History\n# Format: MAC,Name\n";
        storage_file_write(file, header, strlen(header));

        for(uint8_t i = 0; i < app->device_history_count; i++) {
            char line[BT_AUDIO_DEVICE_NAME_LEN + BT_AUDIO_MAC_LEN + 4];
            int len = snprintf(
                line,
                sizeof(line),
                "%s,%s\n",
                app->device_history[i].mac,
                app->device_history[i].name);
            storage_file_write(file, line, len);
        }

        storage_file_close(file);
        FURI_LOG_I(TAG, "Saved %d device history entries", app->device_history_count);
    } else {
        FURI_LOG_E(TAG, "Failed to save device history");
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void bt_audio_device_history_add(BtAudio* app, const char* mac, const char* name) {
    if(!mac || mac[0] == '\0') return;

    // Check if device already exists in history
    for(uint8_t i = 0; i < app->device_history_count; i++) {
        if(strcmp(app->device_history[i].mac, mac) == 0) {
            // Device already exists - move it to the top (most recent)
            BtAudioDeviceEntry temp;
            memcpy(&temp, &app->device_history[i], sizeof(BtAudioDeviceEntry));

            // Update name if provided (might have changed)
            if(name && name[0] != '\0') {
                strncpy(temp.name, name, BT_AUDIO_DEVICE_NAME_LEN - 1);
                temp.name[BT_AUDIO_DEVICE_NAME_LEN - 1] = '\0';
            }

            // Shift entries down
            for(uint8_t j = i; j > 0; j--) {
                memcpy(
                    &app->device_history[j],
                    &app->device_history[j - 1],
                    sizeof(BtAudioDeviceEntry));
            }

            // Put at top
            memcpy(&app->device_history[0], &temp, sizeof(BtAudioDeviceEntry));

            bt_audio_device_history_save(app);
            FURI_LOG_I(TAG, "Moved device to top of history: %s", mac);
            return;
        }
    }

    // New device - add to top
    // Shift existing entries down (drop oldest if full)
    uint8_t entries_to_shift = app->device_history_count;
    if(entries_to_shift >= BT_AUDIO_DEVICE_HISTORY_SIZE) {
        entries_to_shift = BT_AUDIO_DEVICE_HISTORY_SIZE - 1;
    }

    for(int j = entries_to_shift; j > 0; j--) {
        memcpy(&app->device_history[j], &app->device_history[j - 1], sizeof(BtAudioDeviceEntry));
    }

    // Add new entry at top
    strncpy(app->device_history[0].mac, mac, BT_AUDIO_MAC_LEN - 1);
    app->device_history[0].mac[BT_AUDIO_MAC_LEN - 1] = '\0';

    if(name && name[0] != '\0') {
        strncpy(app->device_history[0].name, name, BT_AUDIO_DEVICE_NAME_LEN - 1);
        app->device_history[0].name[BT_AUDIO_DEVICE_NAME_LEN - 1] = '\0';
    } else {
        strncpy(app->device_history[0].name, "Unknown Device", BT_AUDIO_DEVICE_NAME_LEN - 1);
    }

    if(app->device_history_count < BT_AUDIO_DEVICE_HISTORY_SIZE) {
        app->device_history_count++;
    }

    bt_audio_device_history_save(app);
    FURI_LOG_I(TAG, "Added device to history: %s (%s)", mac, app->device_history[0].name);
}

// ============================================================================
// Flipper SD Streaming Functions
// Stream MP3 data from Flipper's SD card to ESP32 over UART using base64
// ============================================================================

// Base64 encoding lookup table
static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Chunk size for streaming (512 bytes raw = ~684 bytes base64)
#define FLIPPER_CHUNK_SIZE    512
// Buffer sizes for base64 encoding and UART command construction
#define FLIPPER_BASE64_MARGIN 16 // Extra space for base64 padding
#define FLIPPER_CMD_OVERHEAD  32 // Space for "MP3_CHUNK:" prefix and newline

/**
 * Encode binary data to base64 for safe UART transmission
 * @param data Input binary data
 * @param len Length of input data
 * @param output Output buffer (must be at least (len * 4 / 3) + 4 bytes)
 * @return Length of encoded output
 */
static size_t bt_audio_base64_encode(const uint8_t* data, size_t len, char* output) {
    size_t i = 0, j = 0;
    uint8_t char_array_3[3];
    uint8_t char_array_4[4];
    size_t input_pos = 0;

    while(len--) {
        char_array_3[i++] = data[input_pos++];
        if(i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; i < 4; i++)
                output[j++] = base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if(i) {
        for(size_t k = i; k < 3; k++)
            char_array_3[k] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for(size_t k = 0; k < i + 1; k++)
            output[j++] = base64_chars[char_array_4[k]];

        while(i++ < 3)
            output[j++] = '=';
    }

    output[j] = '\0';
    return j;
}

bool bt_audio_stream_flipper_sd_file(BtAudio* app, const char* filepath) {
    if(!app || !filepath || filepath[0] == '\0') {
        FURI_LOG_E(TAG, "Invalid parameters for Flipper SD streaming");
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, filepath, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open file for streaming: %s", filepath);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    FURI_LOG_I(TAG, "Starting Flipper SD stream: %s", filepath);

    // Send initialization command to ESP32
    bt_audio_uart_tx(app->uart, "PLAY_MP3_FLIPPER\n");
    furi_delay_ms(100); // Wait for ESP32 to prepare

    // Send filename for display (optional, uses just the base filename)
    const char* basename = strrchr(filepath, '/');
    if(basename) {
        basename++; // Skip the '/'
    } else {
        basename = filepath;
    }

    char filename_cmd[BT_AUDIO_FILENAME_LEN + 16];
    snprintf(filename_cmd, sizeof(filename_cmd), "FLIPPER_FILE:%s\n", basename);
    bt_audio_uart_tx(app->uart, filename_cmd);
    furi_delay_ms(10);

    // Read and send file in chunks
    uint8_t chunk_buffer[FLIPPER_CHUNK_SIZE];
    char encoded_buffer[(FLIPPER_CHUNK_SIZE * 4 / 3) + FLIPPER_BASE64_MARGIN];
    char cmd_buffer[(FLIPPER_CHUNK_SIZE * 4 / 3) + FLIPPER_CMD_OVERHEAD];

    size_t total_bytes = 0;
    size_t bytes_read;

    while((bytes_read = storage_file_read(file, chunk_buffer, FLIPPER_CHUNK_SIZE)) > 0) {
        // Encode chunk to base64 for safe UART transmission
        size_t encoded_len = bt_audio_base64_encode(chunk_buffer, bytes_read, encoded_buffer);
        UNUSED(encoded_len);

        // Build command
        snprintf(cmd_buffer, sizeof(cmd_buffer), "MP3_CHUNK:%s\n", encoded_buffer);
        bt_audio_uart_tx(app->uart, cmd_buffer);

        total_bytes += bytes_read;

        // Small delay to avoid overwhelming ESP32 UART buffer
        // This is critical for reliable streaming
        furi_delay_ms(15); // ~66 chunks/sec, allows for ACK processing
    }

    // Send end marker
    bt_audio_uart_tx(app->uart, "STREAM_END\n");

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    FURI_LOG_I(TAG, "Flipper SD stream complete: %zu bytes sent", total_bytes);
    return true;
}

void bt_audio_stop_flipper_stream(BtAudio* app) {
    if(!app) return;
    bt_audio_uart_tx(app->uart, "FLIPPER_STOP\n");
    FURI_LOG_I(TAG, "Flipper SD streaming stop requested");
}

static void draw_qrcode(Canvas* canvas, void* model) {
    QRCodeModel* qr_model = (QRCodeModel*)model;

    if(!qr_model->qrcode) return;

    int size = qrcodegen_getSize(qr_model->qrcode);
    const int scale = 1;
    const int offset_x = 64 - (size * scale) / 2;
    const int offset_y = 32 - (size * scale) / 2;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    for(int y = 0; y < size; y++) {
        for(int x = 0; x < size; x++) {
            if(qrcodegen_getModule(qr_model->qrcode, x, y)) {
                canvas_draw_box(canvas, offset_x + x * scale, offset_y + y * scale, scale, scale);
            }
        }
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, "Scan for setup guide");
}

static bool bt_audio_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    BtAudio* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool bt_audio_back_event_callback(void* context) {
    furi_assert(context);
    BtAudio* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static BtAudio* bt_audio_alloc() {
    BtAudio* app = malloc(sizeof(BtAudio));

    // Initialize all fields to safe defaults first
    app->device_count = 0;
    app->selected_device = -1;
    app->esp32_present = false;
    app->firmware_check_done = false;
    app->scan_timer = NULL;
    app->version_timer = NULL;
    app->connect_timer = NULL;
    app->init_timer = NULL;
    app->play_timer = NULL;
    app->scroll_timer = NULL;
    app->firmware_response[0] = '\0';
    app->diag_buffer[0] = '\0';
    app->current_filename[0] = '\0';
    app->uart_rx_active = false;
    app->last_rx_time = 0;
    app->is_connected = false;
    app->is_playing = false;
    app->is_paused = false;
    app->reconnect_in_progress = false;
    app->scroll_offset = 0;
    app->last_left_press = 0;
    app->button_held = false;
    app->playing_favorites = false;
    app->current_is_favorite = false;
    app->favorites_count = 0;
    app->device_history_count = 0;
    app->connection_retry_count = 0;

    // Load config from SD card BEFORE GPIO initialization
    // This ensures enable_5v_gpio setting is available for the GPIO power check below (line ~180)
    bt_audio_config_load(app);
    bt_audio_wifi_config_load(app); // Load WiFi configuration

    // Load favorites list
    bt_audio_favorites_load(app);

    // Load device history
    bt_audio_device_history_load(app);

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Turn on backlight - if keep_backlight_on is enabled, enforce always on
    if(app->config.keep_backlight_on) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_on);
        FURI_LOG_I(TAG, "Backlight enforced always on");
    } else {
        notification_message(app->notifications, &sequence_display_backlight_on);
    }

    // Enable 5V output on GPIO to power ESP32 (if configured)
    // ESP32 Bluetooth operations require more current than 3.3V can provide
    // This enables the same "5V on GPIO" feature as the GPIO app
    if(app->config.enable_5v_gpio && !furi_hal_power_is_otg_enabled()) {
        furi_hal_power_enable_otg();
        // Add small delay for power stabilization
        furi_delay_ms(100);
        // Verify OTG was enabled successfully
        if(furi_hal_power_is_otg_enabled()) {
            FURI_LOG_I(TAG, "Enabled 5V on GPIO for ESP32 power");
        } else {
            FURI_LOG_W(TAG, "Failed to enable 5V on GPIO - ESP32 may not have power");
        }
    } else if(furi_hal_power_is_otg_enabled()) {
        FURI_LOG_I(TAG, "5V on GPIO already enabled");
    } else {
        FURI_LOG_I(TAG, "5V auto-enable disabled in settings");
    }

    // View dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
#if defined(FW_ORIGIN_RM)
#endif
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, bt_audio_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, bt_audio_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Scene manager
    app->scene_manager = scene_manager_alloc(&bt_audio_scene_handlers, app);

    // Submenu
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BtAudioViewSubmenu, submenu_get_view(app->submenu));

    // Text box
    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BtAudioViewTextBox, text_box_get_view(app->text_box));
    app->text_box_store = furi_string_alloc();

    // Widget
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BtAudioViewWidget, widget_get_view(app->widget));

    // Variable item list (for settings)
    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        BtAudioViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));

    // QR code view
    app->qr_view = view_alloc();
    view_allocate_model(app->qr_view, ViewModelTypeLockFree, sizeof(QRCodeModel));
    view_set_context(app->qr_view, app);
    view_set_draw_callback(app->qr_view, (ViewDrawCallback)draw_qrcode);
    view_dispatcher_add_view(app->view_dispatcher, BtAudioViewQRCode, app->qr_view);

    // Player view (will be configured by control scene)
    app->player_view = view_alloc();
    view_allocate_model(app->player_view, ViewModelTypeLockFree, sizeof(PlayerViewModel));
    view_set_context(app->player_view, app);
    // Initialize the player model with app reference
    PlayerViewModel* player_model = view_get_model(app->player_view);
    if(player_model) {
        player_model->app = app;
    }
    view_dispatcher_add_view(app->view_dispatcher, BtAudioViewPlayer, app->player_view);

    // Initialize UART
    app->uart = bt_audio_uart_init(app);

    // Start with the start scene
    scene_manager_next_scene(app->scene_manager, BtAudioSceneStart);

    return app;
}

static void bt_audio_free(BtAudio* app) {
    furi_assert(app);

    // Restore normal backlight behavior when exiting
    // (undoes sequence_display_backlight_enforce_on if it was used)
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);

    // Save config before exiting
    bt_audio_config_save(app);

    // Check if background mode is enabled and audio is playing
    bool keep_playing = app->config.background_mode && (app->is_playing || app->is_paused);

    // Send disconnect command if connected (before freeing UART)
    // Skip if background mode is keeping audio playing
    if(app->is_connected && !keep_playing) {
        bt_audio_uart_tx(app->uart, "DISCONNECT\n");
        furi_delay_ms(100); // Give ESP32 time to process
    } else if(keep_playing) {
        FURI_LOG_I(TAG, "Background mode: keeping audio playing");
    }

    // Disable 5V output on GPIO when exiting
    // Skip if background mode is keeping audio playing (ESP32 needs power)
    if(furi_hal_power_is_otg_enabled() && !keep_playing) {
        furi_hal_power_disable_otg();
        FURI_LOG_I(TAG, "Disabled 5V on GPIO");
    } else if(keep_playing) {
        FURI_LOG_I(TAG, "Background mode: keeping 5V enabled for ESP32");
    }

    // Free UART
    bt_audio_uart_free(app->uart);

    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, BtAudioViewSubmenu);
    submenu_free(app->submenu);

    view_dispatcher_remove_view(app->view_dispatcher, BtAudioViewTextBox);
    text_box_free(app->text_box);
    furi_string_free(app->text_box_store);

    view_dispatcher_remove_view(app->view_dispatcher, BtAudioViewWidget);
    widget_free(app->widget);

    // Free variable item list
    view_dispatcher_remove_view(app->view_dispatcher, BtAudioViewVariableItemList);
    variable_item_list_free(app->variable_item_list);

    // Free QR view
    view_dispatcher_remove_view(app->view_dispatcher, BtAudioViewQRCode);
    QRCodeModel* qr_model = view_get_model(app->qr_view);
    if(qr_model->buffer) free(qr_model->buffer);
    if(qr_model->qrcode) free(qr_model->qrcode);
    view_free_model(app->qr_view);
    view_free(app->qr_view);

    // Free player view
    view_dispatcher_remove_view(app->view_dispatcher, BtAudioViewPlayer);
    view_free_model(app->player_view);
    view_free(app->player_view);

    // Free scene manager
    scene_manager_free(app->scene_manager);

    // Free view dispatcher
    view_dispatcher_free(app->view_dispatcher);

    // Close records
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    if(app->scan_timer) {
        furi_timer_stop(app->scan_timer);
        furi_timer_free(app->scan_timer);
    }
    if(app->version_timer) {
        furi_timer_stop(app->version_timer);
        furi_timer_free(app->version_timer);
    }
    if(app->connect_timer) {
        furi_timer_stop(app->connect_timer);
        furi_timer_free(app->connect_timer);
    }
    if(app->init_timer) {
        furi_timer_stop(app->init_timer);
        furi_timer_free(app->init_timer);
    }
    if(app->play_timer) {
        furi_timer_stop(app->play_timer);
        furi_timer_free(app->play_timer);
    }
    if(app->scroll_timer) {
        furi_timer_stop(app->scroll_timer);
        furi_timer_free(app->scroll_timer);
    }

    free(app);
}

int32_t bt_audio_app(void* p) {
    UNUSED(p);

    // Disable expansion protocol to avoid interference with UART Handle
    // This prevents "Stack watermark is too low" errors on Momentum firmware
    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);

    BtAudio* app = bt_audio_alloc();

    view_dispatcher_run(app->view_dispatcher);

    bt_audio_free(app);

    // Return previous state of expansion
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);

    return 0;
}
