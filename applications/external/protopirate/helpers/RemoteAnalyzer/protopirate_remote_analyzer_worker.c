#ifdef PROTOPIRATE_REMOTE_ANALYZER_PLUGIN_BUILD //These are excluded in FAM file, but heres a double catch!
#include "protopirate_remote_analyzer_worker.h"

SubGhzSetting* txrx_get_setting(ProtoPirateApp* instance) {
    furi_assert(instance);
    return instance->setting;
}

#include <furi.h>
#include <float_tools.h>
#include "cc1101.h"

#define TAG "PPFAWorker"

#define SUBGHZ_REMOTE_ANALYZER_THRESHOLD -97.0f

static const uint8_t subghz_preset_ook_58khz[][2] = {
    {CC1101_MDMCFG4, 0b11110111}, // Rx BW filter is 58.035714kHz
    /* End  */
    {0, 0},
};

static const uint8_t subghz_preset_ook_650khz[][2] = {
    {CC1101_MDMCFG4, 0b00010111}, // Rx BW filter is 650.000kHz
    /* End  */
    {0, 0},
};

struct ProtoPirateRemoteAnalyzerWorker {
    FuriThread* thread;
    void* context;
    ProtoPirateRemoteAnalyzerWorkerPairCallback pair_callback;
    uint8_t sample_hold_counter;
    SubGhzSetting* setting;
    float filVal;
    float trigger_level;
    FrequencyRSSI frequency_rssi_buf;
    volatile bool worker_running;
};

static void protopirate_remote_analyzer_worker_load_registers(const uint8_t data[][2]) {
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    size_t i = 0;
    while(data[i][0]) {
        cc1101_write_reg(&furi_hal_spi_bus_handle_subghz, data[i][0], data[i][1]);
        i++;
    }
    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);
}

// running average with adaptive coefficient
static uint32_t protopirate_remote_analyzer_worker_expRunningAverageAdaptive(
    ProtoPirateRemoteAnalyzerWorker* instance,
    uint32_t newVal) {
    float k;
    float newValFloat = newVal;
    // the sharpness of the filter depends on the absolute value of the difference
    if(fabsf(newValFloat - instance->filVal) > 500000.f)
        k = 0.9;
    else
        k = 0.03;

    instance->filVal += (newValFloat - instance->filVal) * k;
    return (uint32_t)instance->filVal;
}

/** Worker thread
 * 
 * @param context 
 * @return exit code 
 */
static int32_t protopirate_remote_analyzer_worker_thread(void* context) {
    ProtoPirateRemoteAnalyzerWorker* instance = context;

    FrequencyRSSI frequency_rssi = {
        .frequency_coarse = 0, .rssi_coarse = 0, .frequency_fine = 0, .rssi_fine = 0};
    float rssi = 0;
    uint32_t frequency = 0;
    float rssi_temp = 0;
    uint32_t frequency_temp = 0;

    FURI_LOG_D("TEST", "Resetting Radio");

    //Start CC1101
    furi_hal_subghz_reset();

    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
    cc1101_flush_rx(&furi_hal_spi_bus_handle_subghz);
    cc1101_flush_tx(&furi_hal_spi_bus_handle_subghz);
    cc1101_write_reg(&furi_hal_spi_bus_handle_subghz, CC1101_IOCFG0, CC1101IocfgHW);
    cc1101_write_reg(&furi_hal_spi_bus_handle_subghz, CC1101_MDMCFG3,
                     0b01111111); // symbol rate
    cc1101_write_reg(
        &furi_hal_spi_bus_handle_subghz,
        CC1101_AGCCTRL2,
        0b00000111); // 00 - DVGA all; 000 - MAX LNA+LNA2; 111 - MAGN_TARGET 42 dB
    cc1101_write_reg(
        &furi_hal_spi_bus_handle_subghz,
        CC1101_AGCCTRL1,
        0b00001000); // 0; 0 - LNA 2 gain is decreased to minimum before decreasing LNA gain; 00 - Relative carrier sense threshold disabled; 1000 - Absolute carrier sense threshold disabled
    cc1101_write_reg(
        &furi_hal_spi_bus_handle_subghz,
        CC1101_AGCCTRL0,
        0b00110000); // 00 - No hysteresis, medium asymmetric dead zone, medium gain ; 11 - 64 samples agc; 00 - Normal AGC, 00 - 4dB boundary

    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);

    furi_hal_subghz_set_path(FuriHalSubGhzPathIsolate);

    while(instance->worker_running) {
        furi_delay_ms(10);

        float rssi_min = 26.0f;
        float rssi_avg = 0;
        size_t rssi_avg_samples = 0;

        frequency_rssi.rssi_coarse = -127.0f;
        frequency_rssi.rssi_fine = -127.0f;
        furi_hal_subghz_idle();
        protopirate_remote_analyzer_worker_load_registers(subghz_preset_ook_650khz);
        bool is_am = false;

        // First stage: coarse scan
        for(size_t i = 0; i < subghz_setting_get_frequency_count(instance->setting); i++) {
            uint32_t current_frequency = subghz_setting_get_frequency(instance->setting, i);
            //           furi_hal_subghz_is_frequency_valid(current_frequency) &&
            //FURI_LOG_D(TAG, "Freqency  %lu", current_frequency);
            if((((current_frequency != 462750000) && (current_frequency != 467750000) &&
                 (current_frequency != 464000000)) &&
                (current_frequency <= 920000000))) {
                furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
                cc1101_switch_to_idle(&furi_hal_spi_bus_handle_subghz);
                frequency = cc1101_set_frequency(
                    &furi_hal_spi_bus_handle_subghz,
                    subghz_setting_get_frequency(instance->setting, i));

                cc1101_calibrate(&furi_hal_spi_bus_handle_subghz);

                furi_check(cc1101_wait_status_state(
                    &furi_hal_spi_bus_handle_subghz, CC1101StateIDLE, 10000));

                cc1101_switch_to_rx(&furi_hal_spi_bus_handle_subghz);
                furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);

                furi_delay_ms(2);

                rssi = furi_hal_subghz_get_rssi();

                rssi_avg += rssi;
                rssi_avg_samples++;

                if(rssi < rssi_min) rssi_min = rssi;

                if(frequency_rssi.rssi_coarse < rssi) {
                    frequency_rssi.rssi_coarse = rssi;
                    frequency_rssi.frequency_coarse = frequency;
                }
                //}
            }

            /*FURI_LOG_D(
                TAG,
                "RSSI: avg %f, max %f at %lu, min %f",
                (double)(rssi_avg / rssi_avg_samples),
                (double)frequency_rssi.rssi_coarse,
                frequency_rssi.frequency_coarse,
                (double)rssi_min);
*/

            // Second stage: fine scan
            if(frequency_rssi.rssi_coarse > instance->trigger_level) {
                furi_hal_subghz_idle();
                protopirate_remote_analyzer_worker_load_registers(subghz_preset_ook_58khz);
#define SWEEP_POINTS 31 // 600 kHz / 20 kHz + 1
                float sweep_rssi[SWEEP_POINTS];
                //uint32_t sweep_freq[SWEEP_POINTS];
                size_t idx = 0;
                //for example -0.3 ... 433.92 ... +0.3 step 20KHz
                for(uint32_t i = frequency_rssi.frequency_coarse - 300000;
                    i < frequency_rssi.frequency_coarse + 300000;
                    i += 20000) {
                    //if(furi_hal_subghz_is_frequency_valid(i)) {
                    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz);
                    cc1101_switch_to_idle(&furi_hal_spi_bus_handle_subghz);
                    frequency = cc1101_set_frequency(&furi_hal_spi_bus_handle_subghz, i);

                    cc1101_calibrate(&furi_hal_spi_bus_handle_subghz);

                    furi_check(cc1101_wait_status_state(
                        &furi_hal_spi_bus_handle_subghz, CC1101StateIDLE, 10000));

                    cc1101_switch_to_rx(&furi_hal_spi_bus_handle_subghz);
                    furi_hal_spi_release(&furi_hal_spi_bus_handle_subghz);

                    furi_delay_ms(2);

                    rssi = furi_hal_subghz_get_rssi();

                    //FURI_LOG_D(TAG, "#:%lu:%f", frequency, (double)rssi);

                    //sweep_freq[idx] = frequency;
                    sweep_rssi[idx] = rssi;
                    idx++;

                    if(frequency_rssi.rssi_fine < rssi) {
                        frequency_rssi.rssi_fine = rssi;
                        frequency_rssi.frequency_fine = frequency;
                    }
                }

                float max_rssi = -999;
                size_t max_idx = 0;

                for(size_t i = 0; i < idx; i++) {
                    if(sweep_rssi[i] > max_rssi) {
                        max_rssi = sweep_rssi[i];
                        max_idx = i;
                    }
                }

                // Look at neighbors
                float left = (max_idx > 0) ? sweep_rssi[max_idx - 1] : max_rssi;
                float right = (max_idx + 1 < idx) ? sweep_rssi[max_idx + 1] : max_rssi;

                // AM has steep drop-off
                if((max_rssi - left) > 6.0f && (max_rssi - right) > 6.0f) {
                    is_am = true;
                }

                //if(is_am) {
                //    FURI_LOG_D(TAG, "AM Detected");
                //modulation = MOD_AM;
                //} else {
                //    FURI_LOG_D(TAG, "Unknown Detected");
                //modulation = MOD_UNKNOWN;
                //}
            }
        }

        // Deliver results fine
        if(frequency_rssi.rssi_fine > instance->trigger_level) {
            FURI_LOG_D(
                TAG, "=:%lu:%f", frequency_rssi.frequency_fine, (double)frequency_rssi.rssi_fine);

            instance->sample_hold_counter = 5;
            rssi_temp = frequency_rssi.rssi_fine;
            frequency_temp = frequency_rssi.frequency_fine;

            if(!float_is_equal(instance->filVal, 0.f)) {
                frequency_rssi.frequency_fine =
                    protopirate_remote_analyzer_worker_expRunningAverageAdaptive(
                        instance, frequency_rssi.frequency_fine);
            }
            // Deliver callback
            if(instance->pair_callback) {
                instance->pair_callback(
                    instance->context,
                    frequency_rssi.frequency_fine,
                    frequency_rssi.rssi_fine,
                    is_am,
                    true);
            }
        } else if( // Deliver results coarse
            (frequency_rssi.rssi_coarse > instance->trigger_level) &&
            (instance->sample_hold_counter < 2)) {
            FURI_LOG_D(
                TAG,
                "~:%lu:%f",
                frequency_rssi.frequency_coarse,
                (double)frequency_rssi.rssi_coarse);

            instance->sample_hold_counter = 5;
            rssi_temp = frequency_rssi.rssi_coarse;
            frequency_temp = frequency_rssi.frequency_coarse;
            if(!float_is_equal(instance->filVal, 0.f)) {
                frequency_rssi.frequency_coarse =
                    protopirate_remote_analyzer_worker_expRunningAverageAdaptive(
                        instance, frequency_rssi.frequency_coarse);
            }
            // Deliver callback
            if(instance->pair_callback) {
                instance->pair_callback(
                    instance->context,
                    frequency_rssi.frequency_coarse,
                    frequency_rssi.rssi_coarse,
                    is_am,
                    true);
            }
        } else {
            if(instance->sample_hold_counter > 0) {
                instance->sample_hold_counter--;
                if(instance->sample_hold_counter == 3) {
                    if(instance->pair_callback) {
                        instance->pair_callback(
                            instance->context, frequency_temp, rssi_temp, is_am, false);
                    }
                }
            } else {
                instance->filVal = 0;
                if(instance->pair_callback)
                    instance->pair_callback(instance->context, 0, 0, is_am, false);
            }
        }
    }

    //Stop CC1101
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();

    return 0;
}

ProtoPirateRemoteAnalyzerWorker* protopirate_remote_analyzer_worker_alloc(void* context) {
    ProtoPirateRemoteAnalyzerWorker* instance = malloc(sizeof(ProtoPirateRemoteAnalyzerWorker));

    instance->thread = furi_thread_alloc_ex(
        "PPFAWorker", 2048, protopirate_remote_analyzer_worker_thread, instance);
    ProtoPirateApp* app = context;
    instance->setting = txrx_get_setting(app);
    //instance->trigger_level = subghz->last_settings->remote_analyzer_trigger;
    instance->trigger_level = SUBGHZ_REMOTE_ANALYZER_THRESHOLD;
    return instance;
}

void protopirate_remote_analyzer_worker_free(ProtoPirateRemoteAnalyzerWorker* instance) {
    furi_thread_free(instance->thread);
    free(instance);
}

void protopirate_remote_analyzer_worker_set_pair_callback(
    ProtoPirateRemoteAnalyzerWorker* instance,
    ProtoPirateRemoteAnalyzerWorkerPairCallback callback,
    void* context) {
    instance->pair_callback = callback;
    instance->context = context;
}

void protopirate_remote_analyzer_worker_start(ProtoPirateRemoteAnalyzerWorker* instance) {
    instance->worker_running = true;
    furi_thread_start(instance->thread);
}

void protopirate_remote_analyzer_worker_stop(ProtoPirateRemoteAnalyzerWorker* instance) {
    instance->worker_running = false;
    furi_thread_join(instance->thread);
}

bool protopirate_remote_analyzer_worker_is_running(ProtoPirateRemoteAnalyzerWorker* instance) {
    return instance->worker_running;
}

void protopirate_remote_analyzer_worker_set_trigger_level(
    ProtoPirateRemoteAnalyzerWorker* instance,
    float value) {
    instance->trigger_level = value;
}

float protopirate_remote_analyzer_worker_get_trigger_level(
    ProtoPirateRemoteAnalyzerWorker* instance) {
    return instance->trigger_level;
}

uint32_t protopirate_remote_analyzer_get_nearest_frequency(
    ProtoPirateRemoteAnalyzerWorker* instance,
    uint32_t input) {
    uint32_t prev_freq = 0;
    uint32_t result = 0;
    uint32_t current;

    for(size_t i = 0; i < subghz_setting_get_frequency_count(instance->setting); i++) {
        current = subghz_setting_get_frequency(instance->setting, i);
        if(current == 0) {
            continue;
        }
        if(current == input) {
            result = current;
            break;
        }
        if(current > input && prev_freq < input) {
            if(current - input < input - prev_freq) {
                result = current;
            } else {
                result = prev_freq;
            }
            break;
        }
        prev_freq = current;
    }

    return result;
}
#endif
