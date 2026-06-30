// Sub Analyzer - Analyze SubGhz .sub files
// Extract all available data from captured signals
// RocketGod | betaskynet.com

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_box.h>
#include <gui/modules/loading.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <lib/toolbox/stream/file_stream.h>
#include <notification/notification_messages.h>
#include <lib/toolbox/path.h>
#include <math.h>

#include <lib/subghz/receiver.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/protocols/base.h>
#include <lib/subghz/environment.h>
#include <lib/subghz/subghz_setting.h>
#include <lib/subghz/registry.h>
#include <lib/subghz/subghz_file_encoder_worker.h>
#include <lib/flipper_format/flipper_format.h>
#include <lib/subghz/blocks/encoder.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/generic.h>
#include <lib/subghz/blocks/math.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <lib/subghz/devices/devices.h>

#define TAG "SubAnalyzer"
#define SUB_ANALYZER_VERSION "5.0"
#define TEXT_BUFFER_SIZE 8192
#define SUBGHZ_APP_EXTENSION ".sub"

extern const SubGhzProtocolRegistry subghz_protocol_registry;

typedef struct {
    ViewDispatcher* view_dispatcher;
    Gui* gui;
    NotificationApp* notifications;
    Submenu* submenu;
    Popup* popup;
    TextBox* text_box;
    Loading* loading;
    DialogsApp* dialogs;
    FuriString* text_buffer;
    FuriString* file_path;
    SubGhzEnvironment* environment;
    SubGhzReceiver* receiver;
    SubGhzSetting* setting;
    const SubGhzProtocolRegistry* protocol_registry;
} SubAnalyzerApp;

typedef enum {
    SubAnalyzerViewSubmenu,
    SubAnalyzerViewPopup,
    SubAnalyzerViewTextBox,
    SubAnalyzerViewLoading,
} SubAnalyzerView;

typedef enum {
    SubAnalyzerSubmenuIndexSelectFile,
    SubAnalyzerSubmenuIndexAbout,
} SubAnalyzerSubmenuIndex;

// Forward declarations
static void sub_analyzer_select_file(SubAnalyzerApp* app);
static void sub_analyzer_analyze_file(SubAnalyzerApp* app, const char* path);
static void sub_analyzer_show_about(SubAnalyzerApp* app);

static uint32_t sub_analyzer_exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t sub_analyzer_exit_to_submenu_callback(void* context) {
    UNUSED(context);
    return SubAnalyzerViewSubmenu;
}

static void sub_analyzer_submenu_callback(void* context, uint32_t index) {
    SubAnalyzerApp* app = context;

    if(index == SubAnalyzerSubmenuIndexSelectFile) {
        sub_analyzer_select_file(app);
    } else if(index == SubAnalyzerSubmenuIndexAbout) {
        sub_analyzer_show_about(app);
    }
}

static void sub_analyzer_popup_callback(void* context) {
    SubAnalyzerApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, SubAnalyzerViewSubmenu);
}

// Main analysis function
static void sub_analyzer_analyze_file(SubAnalyzerApp* app, const char* path) {
    view_dispatcher_switch_to_view(app->view_dispatcher, SubAnalyzerViewLoading);
    
    furi_string_reset(app->text_buffer);
    
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* format = flipper_format_file_alloc(storage);
    
    // Store all extracted data
    uint32_t frequency = 0;
    uint32_t bits = 0;
    uint32_t te = 0;
    uint32_t baud = 0;
    uint32_t repeat = 0;
    uint32_t delay = 0;
    uint32_t guard_time = 0;
    FuriString* key_data = furi_string_alloc();
    FuriString* protocol_name = furi_string_alloc();
    FuriString* preset_name = furi_string_alloc();
    
    do {
        if(!flipper_format_file_open_existing(format, path)) {
            furi_string_cat_printf(app->text_buffer, "Error: Cannot open file\n");
            break;
        }
        
        // Get filename
        FuriString* filename = furi_string_alloc();
        path_extract_filename_no_ext(path, filename);
        furi_string_cat_printf(app->text_buffer, "=== SubGhz File Analysis ===\n");
        furi_string_cat_printf(app->text_buffer, "File: %s.sub\n", furi_string_get_cstr(filename));
        furi_string_free(filename);
        
        // Read header
        FuriString* temp_str = furi_string_alloc();
        uint32_t version;
        if(!flipper_format_read_header(format, temp_str, &version)) {
            furi_string_cat_printf(app->text_buffer, "Error: Invalid header\n");
            furi_string_free(temp_str);
            break;
        }
        
        furi_string_cat_printf(app->text_buffer, "Format: %s v%lu\n\n", 
                              furi_string_get_cstr(temp_str), version);
        
        // Read all basic fields
        flipper_format_read_uint32(format, "Frequency", &frequency, 1);
        flipper_format_read_string(format, "Preset", preset_name);
        
        // Check if RAW or Protocol
        FuriString* raw_data = furi_string_alloc();
        bool is_raw = flipper_format_read_string(format, "RAW_Data", raw_data);
        
        if(is_raw) {
            // RAW file analysis
            furi_string_cat_printf(app->text_buffer, "=== RAW Signal ===\n");
            
            if(frequency > 0) {
                furi_string_cat_printf(app->text_buffer, "Frequency: %lu.%03lu MHz\n", 
                                      frequency / 1000000, (frequency / 1000) % 1000);
            }
            
            if(!furi_string_empty(preset_name)) {
                furi_string_cat_printf(app->text_buffer, "Preset: %s\n", 
                                      furi_string_get_cstr(preset_name));
            }
            
            // Analyze RAW timings
            const char* raw_str = furi_string_get_cstr(raw_data);
            int positive_count = 0;
            int negative_count = 0;
            int64_t total_duration = 0;
            int32_t shortest_positive = INT32_MAX;
            int32_t shortest_negative = INT32_MAX;
            
            // Parse timings
            const char* p = raw_str;
            while(*p) {
                while(*p == ' ' || *p == ',') p++;
                if(!*p) break;
                
                int32_t value = 0;
                bool negative = false;
                if(*p == '-') {
                    negative = true;
                    p++;
                }
                
                while(*p >= '0' && *p <= '9') {
                    value = value * 10 + (*p - '0');
                    p++;
                }
                
                if(value > 0) {
                    if(negative) {
                        negative_count++;
                        if(value < shortest_negative) shortest_negative = value;
                    } else {
                        positive_count++;
                        if(value < shortest_positive) shortest_positive = value;
                    }
                    
                    total_duration += value;
                }
            }
            
            furi_string_cat_printf(app->text_buffer, "\nSignal Analysis:\n");
            furi_string_cat_printf(app->text_buffer, "Total pulses: %d\n", 
                                  positive_count + negative_count);
            furi_string_cat_printf(app->text_buffer, "High pulses: %d\n", positive_count);
            furi_string_cat_printf(app->text_buffer, "Low pulses: %d\n", negative_count);
            furi_string_cat_printf(app->text_buffer, "Duration: %lld.%03lld ms\n", 
                                  total_duration / 1000, total_duration % 1000);
            
            // Estimate symbol rate based on shortest pulse
            int32_t min_pulse = shortest_positive;
            if(shortest_negative < min_pulse && shortest_negative < INT32_MAX) {
                min_pulse = shortest_negative;
            }
            
            if(min_pulse < INT32_MAX && min_pulse > 0) {
                uint32_t symbol_rate = 1000000 / min_pulse;
                furi_string_cat_printf(app->text_buffer, "Est. symbol rate: %lu Hz\n", symbol_rate);
                
                // Estimate data rate for common encodings
                furi_string_cat_printf(app->text_buffer, "\nPossible data rates:\n");
                furi_string_cat_printf(app->text_buffer, "  Manchester: %lu bps\n", symbol_rate / 2);
                furi_string_cat_printf(app->text_buffer, "  PWM: %lu bps\n", symbol_rate / 3);
                furi_string_cat_printf(app->text_buffer, "  PPM: %lu bps\n", symbol_rate / 4);
            }
            
        } else {
            // Protocol file - read protocol name
            if(flipper_format_read_string(format, "Protocol", protocol_name)) {
                furi_string_cat_printf(app->text_buffer, "=== Protocol: %s ===\n", 
                                      furi_string_get_cstr(protocol_name));
                
                // Basic info
                if(frequency > 0) {
                    furi_string_cat_printf(app->text_buffer, "Frequency: %lu.%03lu MHz\n", 
                                          frequency / 1000000, (frequency / 1000) % 1000);
                }
                
                if(!furi_string_empty(preset_name)) {
                    furi_string_cat_printf(app->text_buffer, "Preset: %s\n", 
                                          furi_string_get_cstr(preset_name));
                }
                
                // Read all protocol fields
                flipper_format_read_uint32(format, "Bit", &bits, 1);
                flipper_format_read_string(format, "Key", key_data);
                flipper_format_read_uint32(format, "Te", &te, 1);
                flipper_format_read_uint32(format, "Baud", &baud, 1);
                flipper_format_read_uint32(format, "Guard_time", &guard_time, 1);
                flipper_format_read_uint32(format, "Repeat", &repeat, 1);
                flipper_format_read_uint32(format, "Delay", &delay, 1);
                
                // Display extracted data
                furi_string_cat_printf(app->text_buffer, "\nExtracted Data:\n");
                
                if(bits > 0) {
                    furi_string_cat_printf(app->text_buffer, "Bits: %lu (%lu bytes)\n", 
                                          bits, (bits + 7) / 8);
                }
                
                if(!furi_string_empty(key_data)) {
                    furi_string_cat_printf(app->text_buffer, "Key: %s\n", 
                                          furi_string_get_cstr(key_data));
                }
                
                if(te > 0) {
                    furi_string_cat_printf(app->text_buffer, "Te: %lu μs\n", te);
                }
                
                if(baud > 0) {
                    furi_string_cat_printf(app->text_buffer, "Baud: %lu bps\n", baud);
                }
                
                if(guard_time > 0) {
                    furi_string_cat_printf(app->text_buffer, "Guard: %lu μs\n", guard_time);
                }
                
                if(repeat > 0) {
                    furi_string_cat_printf(app->text_buffer, "Repeat: %lu times\n", repeat);
                }
                
                if(delay > 0) {
                    furi_string_cat_printf(app->text_buffer, "Delay: %lu ms\n", delay);
                }
                
                // Try to decode with protocol decoder
                const SubGhzProtocol* protocol = NULL;
                size_t protocol_count = subghz_protocol_registry_count(app->protocol_registry);
                
                for(size_t i = 0; i < protocol_count; i++) {
                    protocol = subghz_protocol_registry_get_by_index(app->protocol_registry, i);
                    if(protocol && protocol->name && 
                       strcmp(protocol->name, furi_string_get_cstr(protocol_name)) == 0) {
                        break;
                    }
                }
                
                if(protocol && protocol->decoder) {
                    furi_string_cat_printf(app->text_buffer, "\nProtocol Type: %s\n", 
                                          protocol->type == SubGhzProtocolTypeStatic ? "Static (Fixed)" :
                                          protocol->type == SubGhzProtocolTypeDynamic ? "Dynamic (Rolling)" :
                                          "RAW");
                    
                    if(protocol->decoder->alloc && protocol->decoder->deserialize) {
                        SubGhzProtocolDecoderBase* decoder = protocol->decoder->alloc(app->environment);
                        
                        if(decoder) {
                            flipper_format_rewind(format);
                            SubGhzProtocolStatus status = protocol->decoder->deserialize(decoder, format);
                            
                            if(status == SubGhzProtocolStatusOk && protocol->decoder->get_string) {
                                FuriString* decoded_info = furi_string_alloc();
                                protocol->decoder->get_string(decoder, decoded_info);
                                
                                furi_string_cat_printf(app->text_buffer, "\nDecoded Info:\n%s", 
                                                      furi_string_get_cstr(decoded_info));
                                
                                furi_string_free(decoded_info);
                            }
                            
                            if(protocol->decoder->free) {
                                protocol->decoder->free(decoder);
                            }
                        }
                    }
                }
                
                // Read any additional protocol-specific fields
                flipper_format_rewind(format);
                
                // Common protocol fields to check
                const char* extra_fields[] = {
                    "Sn", "Btn", "Cnt", "Cmd", "Addr", "Data", "Id", "Ch",
                    "Battery_low", "DIP", "Learning", "Seed", "Hop"
                };
                
                bool found_extra = false;
                for(size_t i = 0; i < COUNT_OF(extra_fields); i++) {
                    uint32_t value;
                    if(flipper_format_read_uint32(format, extra_fields[i], &value, 1)) {
                        if(!found_extra) {
                            furi_string_cat_printf(app->text_buffer, "\nProtocol Fields:\n");
                            found_extra = true;
                        }
                        furi_string_cat_printf(app->text_buffer, "%s: 0x%08lX (%lu)\n", 
                                              extra_fields[i], value, value);
                    }
                }
            }
        }
        
        // CALCULATIONS SECTION
        furi_string_cat_printf(app->text_buffer, "\n=== Calculations ===\n");
        
        // Frequency calculations
        if(frequency > 0) {
            // Wavelength = c / f (speed of light / frequency)
            // c = 299792458 m/s
            uint32_t wavelength_mm = 299792458000ULL / frequency; // in mm
            uint32_t wavelength_m = wavelength_mm / 1000;
            uint32_t wavelength_cm = (wavelength_mm / 10) % 100;
            uint32_t wavelength_mm_frac = wavelength_mm % 10;
            
            furi_string_cat_printf(app->text_buffer, "Wavelength: %lu.%02lu%lu m (%lu.%lu cm)\n", 
                                  wavelength_m, wavelength_cm, wavelength_mm_frac,
                                  wavelength_mm / 10, wavelength_mm % 10);
            
            uint32_t quarter_wave_cm = wavelength_mm / 40; // λ/4 in cm
            uint32_t quarter_wave_mm = (wavelength_mm / 4) % 10;
            furi_string_cat_printf(app->text_buffer, "λ/4 antenna: %lu.%lu cm\n", 
                                  quarter_wave_cm, quarter_wave_mm);
            
            // FSPL calculation using integer math
            // FSPL (dB) = 20*log10(f) - 147.55
            // We'll use a lookup table or approximation for log10
            float freq_mhz = (float)frequency / 1000000.0f;
            float log_freq = log10f(freq_mhz);
            float fspl_1m = 20.0f * log_freq + 20.0f * log10f(1000000.0f) - 147.55f;
            int fspl_1m_int = (int)(fspl_1m * 10.0f);
            
            furi_string_cat_printf(app->text_buffer, "FSPL @ 1m: %d.%d dB\n", 
                                  fspl_1m_int / 10, abs(fspl_1m_int % 10));
            
            int fspl_10m_int = fspl_1m_int + 200; // +20 dB
            int fspl_100m_int = fspl_1m_int + 400; // +40 dB
            furi_string_cat_printf(app->text_buffer, "FSPL @ 10m: %d.%d dB\n", 
                                  fspl_10m_int / 10, abs(fspl_10m_int % 10));
            furi_string_cat_printf(app->text_buffer, "FSPL @ 100m: %d.%d dB\n", 
                                  fspl_100m_int / 10, abs(fspl_100m_int % 10));
        }
        
        // Timing calculations
        if(te > 0) {
            uint32_t symbol_rate = 1000000 / te;
            furi_string_cat_printf(app->text_buffer, "\nTiming Analysis:\n");
            furi_string_cat_printf(app->text_buffer, "Symbol rate: %lu Hz\n", symbol_rate);
            furi_string_cat_printf(app->text_buffer, "Symbol period: %lu μs\n", te);
            
            // Calculate data rates for different encodings
            if(baud == 0) {
                furi_string_cat_printf(app->text_buffer, "\nPossible baud rates:\n");
                furi_string_cat_printf(app->text_buffer, "  Manchester: %lu bps\n", symbol_rate / 2);
                furi_string_cat_printf(app->text_buffer, "  3-PWM: %lu bps\n", symbol_rate / 3);
                furi_string_cat_printf(app->text_buffer, "  4-PWM: %lu bps\n", symbol_rate / 4);
            } else {
                // Determine encoding from Te and baud
                uint32_t symbols_per_bit = (baud * te) / 1000000;
                uint32_t symbols_per_bit_frac = ((baud * te) % 1000000) / 100000;
                
                const char* encoding = "Unknown";
                if(symbols_per_bit == 1 || (symbols_per_bit == 0 && symbols_per_bit_frac >= 9)) {
                    encoding = "NRZ";
                } else if(symbols_per_bit == 2 || (symbols_per_bit == 1 && symbols_per_bit_frac >= 9)) {
                    encoding = "Manchester";
                } else if(symbols_per_bit == 3 || (symbols_per_bit == 2 && symbols_per_bit_frac >= 9)) {
                    encoding = "3-PWM";
                } else if(symbols_per_bit == 4 || (symbols_per_bit == 3 && symbols_per_bit_frac >= 9)) {
                    encoding = "4-PWM";
                }
                
                furi_string_cat_printf(app->text_buffer, "Symbols per bit: %lu.%lu\n", 
                                      symbols_per_bit, symbols_per_bit_frac);
                furi_string_cat_printf(app->text_buffer, "Encoding: %s\n", encoding);
            }
        }
        
        // Data rate calculations
        if(baud > 0) {
            furi_string_cat_printf(app->text_buffer, "\nData Rate Analysis:\n");
            uint32_t bit_period_us = 1000000 / baud;
            furi_string_cat_printf(app->text_buffer, "Bit period: %lu μs\n", bit_period_us);
            
            uint32_t kbps_int = baud / 1000;
            uint32_t kbps_frac = (baud % 1000) / 10;
            furi_string_cat_printf(app->text_buffer, "Data rate: %lu.%02lu kbps\n", 
                                  kbps_int, kbps_frac);
            
            // Bandwidth estimates
            furi_string_cat_printf(app->text_buffer, "Min bandwidth: %lu.%lu kHz\n", 
                                  baud / 1000, (baud % 1000) / 100);
            furi_string_cat_printf(app->text_buffer, "Typical BW: %lu.%lu kHz\n", 
                                  (baud * 2) / 1000, ((baud * 2) % 1000) / 100);
        }
        
        // Transmission time calculations
        if(bits > 0 && (baud > 0 || te > 0)) {
            furi_string_cat_printf(app->text_buffer, "\nTransmission Time:\n");
            
            uint32_t tx_time_us;
            if(baud > 0) {
                tx_time_us = (bits * 1000000) / baud;
            } else {
                // Estimate with Te (assume 4Te per bit for 4-PWM)
                tx_time_us = bits * te * 4;
            }
            
            uint32_t tx_time_ms = tx_time_us / 1000;
            uint32_t tx_time_us_frac = tx_time_us % 1000;
            furi_string_cat_printf(app->text_buffer, "Single TX: %lu.%02lu ms\n", 
                                  tx_time_ms, tx_time_us_frac / 10);
            
            if(repeat > 1) {
                uint32_t total_time_ms = tx_time_ms * repeat;
                if(delay > 0) {
                    total_time_ms += delay * (repeat - 1);
                }
                furi_string_cat_printf(app->text_buffer, "Total TX: %lu ms\n", total_time_ms);
                
                if(delay > 0 && total_time_ms > 0) {
                    uint32_t duty = (tx_time_ms * repeat * 100) / total_time_ms;
                    furi_string_cat_printf(app->text_buffer, "Duty cycle: %lu%%\n", duty);
                }
            }
            
            // Power consumption estimate
            if(repeat > 1 && delay > 0) {
                uint32_t active_time = tx_time_ms * repeat;
                uint32_t total_time = active_time + delay * (repeat - 1);
                // Average current = 30mA * duty + 1mA * (1-duty)
                uint32_t avg_current_ua = (30000 * active_time + 1000 * (total_time - active_time)) / total_time;
                furi_string_cat_printf(app->text_buffer, "Avg current: %lu.%lu mA\n", 
                                      avg_current_ua / 1000, (avg_current_ua % 1000) / 100);
            }
        }
        
        // Key analysis
        if(!furi_string_empty(key_data)) {
            furi_string_cat_printf(app->text_buffer, "\nKey Analysis:\n");
            
            size_t hex_len = furi_string_size(key_data);
            size_t byte_len = hex_len / 2;
            
            furi_string_cat_printf(app->text_buffer, "Hex chars: %zu\n", hex_len);
            furi_string_cat_printf(app->text_buffer, "Bytes: %zu\n", byte_len);
            
            if(bits > 0) {
                size_t expected_bytes = (bits + 7) / 8;
                if(byte_len == expected_bytes) {
                    furi_string_cat_printf(app->text_buffer, "Matches bit count: Yes\n");
                } else {
                    furi_string_cat_printf(app->text_buffer, "Expected bytes: %zu\n", 
                                          expected_bytes);
                }
            }
            
            // Check for patterns
            const char* hex = furi_string_get_cstr(key_data);
            bool all_same = true;
            bool all_zero = true;
            bool all_ff = true;
            
            for(size_t i = 0; i < hex_len - 1; i += 2) {
                if(hex[i] != hex[0] || hex[i+1] != hex[1]) all_same = false;
                if(hex[i] != '0' || hex[i+1] != '0') all_zero = false;
                if((hex[i] != 'F' && hex[i] != 'f') || 
                   (hex[i+1] != 'F' && hex[i+1] != 'f')) all_ff = false;
            }
            
            if(all_same) {
                furi_string_cat_printf(app->text_buffer, "Pattern: All bytes same\n");
            } else if(all_zero) {
                furi_string_cat_printf(app->text_buffer, "Pattern: All zeros\n");
            } else if(all_ff) {
                furi_string_cat_printf(app->text_buffer, "Pattern: All FF\n");
            }
            
            // Calculate entropy (randomness)
            uint8_t byte_count[256] = {0};
            for(size_t i = 0; i < hex_len - 1; i += 2) {
                uint8_t byte_val = 0;
                char c1 = hex[i];
                char c2 = hex[i+1];
                
                if(c1 >= '0' && c1 <= '9') byte_val = (c1 - '0') << 4;
                else if(c1 >= 'A' && c1 <= 'F') byte_val = (c1 - 'A' + 10) << 4;
                else if(c1 >= 'a' && c1 <= 'f') byte_val = (c1 - 'a' + 10) << 4;
                
                if(c2 >= '0' && c2 <= '9') byte_val |= (c2 - '0');
                else if(c2 >= 'A' && c2 <= 'F') byte_val |= (c2 - 'A' + 10);
                else if(c2 >= 'a' && c2 <= 'f') byte_val |= (c2 - 'a' + 10);
                
                byte_count[byte_val]++;
            }
            
            // Simple entropy calculation
            uint32_t unique_bytes = 0;
            for(int i = 0; i < 256; i++) {
                if(byte_count[i] > 0) unique_bytes++;
            }
            
            float entropy = 0.0f;
            for(int i = 0; i < 256; i++) {
                if(byte_count[i] > 0) {
                    float p = (float)byte_count[i] / (float)byte_len;
                    entropy -= p * log2f(p);
                }
            }
            
            uint32_t entropy_int = (uint32_t)(entropy * 100.0f);
            furi_string_cat_printf(app->text_buffer, "Entropy: %lu.%02lu bits/byte\n", 
                                  entropy_int / 100, entropy_int % 100);
            furi_string_cat_printf(app->text_buffer, "Unique bytes: %lu/%zu\n", 
                                  unique_bytes, byte_len);
            
            if(entropy < 2.0f) {
                furi_string_cat_printf(app->text_buffer, "Randomness: Low\n");
            } else if(entropy < 6.0f) {
                furi_string_cat_printf(app->text_buffer, "Randomness: Medium\n");
            } else {
                furi_string_cat_printf(app->text_buffer, "Randomness: High\n");
            }
        }
        
        furi_string_free(raw_data);
        furi_string_free(temp_str);
        
    } while(false);
    
    flipper_format_free(format);
    furi_record_close(RECORD_STORAGE);
    
    furi_string_free(key_data);
    furi_string_free(protocol_name);
    furi_string_free(preset_name);
    
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_buffer));
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubAnalyzerViewTextBox);
}

static void sub_analyzer_select_file(SubAnalyzerApp* app) {
    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, SUBGHZ_APP_EXTENSION, NULL);
    browser_options.base_path = SUBGHZ_APP_FOLDER;
    browser_options.skip_assets = true;

    bool result = dialog_file_browser_show(
        app->dialogs, app->file_path, app->file_path, &browser_options);

    if(result) {
        sub_analyzer_analyze_file(app, furi_string_get_cstr(app->file_path));
    }
}

static void sub_analyzer_show_about(SubAnalyzerApp* app) {
    furi_string_reset(app->text_buffer);
    furi_string_cat_str(app->text_buffer,
                        "Sub Analyzer v5.0\n\n"
                        "RocketGod was here\n"
                        "betaskynet.com\n\n"
                        "Features:\n"
                        "- Complete .sub analysis\n"
                        "- Protocol decoding\n"
                        "- RAW timing analysis\n"
                        "- Accurate calculations\n"
                        "- Pattern detection\n"
                        "- Link budget\n"
                        "- Entropy analysis\n\n"
                        "Extracts everything!");

    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_buffer));
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubAnalyzerViewTextBox);
}

static void sub_analyzer_show_intro_popup(SubAnalyzerApp* app) {
    popup_set_header(app->popup, "Sub Analyzer", 64, 10, AlignCenter, AlignTop);
    popup_set_text(app->popup, "RocketGod\nbetaskynet.com", 64, 20, AlignCenter, AlignTop);
    popup_set_callback(app->popup, sub_analyzer_popup_callback);
    popup_set_context(app->popup, app);
    popup_set_timeout(app->popup, 1500);
    popup_enable_timeout(app->popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubAnalyzerViewPopup);
}

static SubAnalyzerApp* sub_analyzer_app_alloc() {
    SubAnalyzerApp* app = malloc(sizeof(SubAnalyzerApp));

    app->view_dispatcher = view_dispatcher_alloc();
    app->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->submenu = submenu_alloc();
    app->popup = popup_alloc();
    app->text_box = text_box_alloc();
    app->loading = loading_alloc();

    app->text_buffer = furi_string_alloc();
    furi_string_reserve(app->text_buffer, TEXT_BUFFER_SIZE);
    app->file_path = furi_string_alloc();
    furi_string_set(app->file_path, SUBGHZ_APP_FOLDER);

    // Initialize SubGhz
    app->environment = subghz_environment_alloc();
    subghz_environment_load_keystore(app->environment, EXT_PATH("subghz/assets/keeloq_mfcodes"));
    subghz_environment_load_keystore(app->environment, EXT_PATH("subghz/assets/keeloq_mfcodes_user"));
    subghz_environment_set_protocol_registry(app->environment, (void*)&subghz_protocol_registry);
    
    app->protocol_registry = &subghz_protocol_registry;
    app->receiver = subghz_receiver_alloc_init(app->environment);
    app->setting = subghz_setting_alloc();
    subghz_setting_load(app->setting, EXT_PATH("subghz/assets/setting_user"));

    view_set_previous_callback(submenu_get_view(app->submenu), sub_analyzer_exit_callback);
    view_set_previous_callback(popup_get_view(app->popup), sub_analyzer_exit_to_submenu_callback);
    view_set_previous_callback(text_box_get_view(app->text_box), sub_analyzer_exit_to_submenu_callback);
    view_set_previous_callback(loading_get_view(app->loading), sub_analyzer_exit_to_submenu_callback);

    view_dispatcher_add_view(app->view_dispatcher, SubAnalyzerViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->view_dispatcher, SubAnalyzerViewPopup, popup_get_view(app->popup));
    view_dispatcher_add_view(app->view_dispatcher, SubAnalyzerViewTextBox, text_box_get_view(app->text_box));
    view_dispatcher_add_view(app->view_dispatcher, SubAnalyzerViewLoading, loading_get_view(app->loading));

    submenu_add_item(app->submenu, "Select .sub file", SubAnalyzerSubmenuIndexSelectFile, sub_analyzer_submenu_callback, app);
    submenu_add_item(app->submenu, "About", SubAnalyzerSubmenuIndexAbout, sub_analyzer_submenu_callback, app);

    return app;
}

static void sub_analyzer_app_free(SubAnalyzerApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, SubAnalyzerViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, SubAnalyzerViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, SubAnalyzerViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, SubAnalyzerViewLoading);

    submenu_free(app->submenu);
    popup_free(app->popup);
    text_box_free(app->text_box);
    loading_free(app->loading);

    subghz_receiver_free(app->receiver);
    subghz_environment_free(app->environment);
    subghz_setting_free(app->setting);

    furi_string_free(app->text_buffer);
    furi_string_free(app->file_path);

    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);

    free(app);
}

int32_t sub_analyzer_app(void* p) {
    UNUSED(p);
    SubAnalyzerApp* app = sub_analyzer_app_alloc();

    sub_analyzer_show_intro_popup(app);

    view_dispatcher_run(app->view_dispatcher);

    sub_analyzer_app_free(app);
    return 0;
}