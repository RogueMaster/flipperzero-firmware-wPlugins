#pragma once

#include "lib/subghz/subghz_setting.h"

static void* __freq_manager_instance = NULL;

class FrequencyManager {
private:
    uint32_t* frequencies;
    uint8_t frequencyCount;
    uint8_t defaultFreqIndex;

    FrequencyManager() {
        SubGhzSetting* setting = subghz_setting_alloc();
        subghz_setting_load(setting, EXT_PATH("subghz/assets/setting_user"));

        frequencyCount = subghz_setting_get_frequency_count(setting);
        defaultFreqIndex = subghz_setting_get_frequency_default_index(setting);
        frequencies = new uint32_t[frequencyCount];

        for(int i = 0; i < frequencyCount; i++) {
            frequencies[i] = subghz_setting_get_frequency(setting, i);
        }

        subghz_setting_free(setting);
    }

public:
    static FrequencyManager* GetInstance() {
        if(__freq_manager_instance == NULL) {
            __freq_manager_instance = new FrequencyManager();
        }
        return (FrequencyManager*)__freq_manager_instance;
    }

    uint32_t GetFrequency(size_t index) {
        return frequencies[index];
    }

    uint32_t GetDefaultFrequency() {
        return frequencies[defaultFreqIndex];
    }

    size_t GetDefaultFrequencyIndex() {
        return defaultFreqIndex;
    }

    size_t GetFrequencyIndex(uint32_t freq) {
        for(size_t i = 0; i < GetFrequencyCount(); i++) {
            if(GetFrequency(i) == freq) {
                return i;
            }
        }
        return GetDefaultFrequencyIndex();
    }

    size_t GetFrequencyCount() {
        return frequencyCount;
    }

    ~FrequencyManager() {
        if(frequencies != NULL) {
            delete[] frequencies;
        }
    }
};
