#pragma once

#include <cstdint>

#include "app/AppFileSystem.hpp"
#include "lib/file/FileManager.hpp"
#include "app/pager/SavedStationStrategy.hpp"

#define KEY_CONFIG_FREQUENCY      "Frequency"
#define KEY_CONFIG_MAX_PAGERS     "MaxPagerForBatchOrDetection"
#define KEY_CONFIG_REPEATS        "SignalRepeats"
#define KEY_CONFIG_SAVED_STRATEGY "SavedStationStrategy"
#define KEY_CONFIG_AUTOSAVE       "AutosaveFoundSignals"
#define KEY_CONFIG_USER_CATGEGORY "UserCategory"

class AppConfig {
public:
    uint32_t Frequency = 433920000;
    uint32_t MaxPagerForBatchOrDetection = 30;
    uint32_t SignalRepeats = 10;
    SavedStationStrategy SavedStrategy = SHOW_NAME;
    bool AutosaveFoundSignals = true;
    String* CurrentUserCategory = NULL;

private:
    void readFromFile(FlipperFile* file) {
        String* userCat = new String();
        uint32_t savedStrategyValue = SavedStrategy;
        if(CurrentUserCategory != NULL) {
            delete CurrentUserCategory;
        }

        file->ReadUInt32(KEY_CONFIG_FREQUENCY, &Frequency);
        file->ReadUInt32(KEY_CONFIG_MAX_PAGERS, &MaxPagerForBatchOrDetection);
        file->ReadUInt32(KEY_CONFIG_REPEATS, &SignalRepeats);
        file->ReadUInt32(KEY_CONFIG_SAVED_STRATEGY, &savedStrategyValue);
        file->ReadBool(KEY_CONFIG_AUTOSAVE, &AutosaveFoundSignals);
        file->ReadString(KEY_CONFIG_USER_CATGEGORY, userCat);

        SavedStrategy = static_cast<enum SavedStationStrategy>(savedStrategyValue);
        if(!userCat->isEmpty()) {
            CurrentUserCategory = userCat;
        } else {
            CurrentUserCategory = NULL;
            delete userCat;
        }
    }

    void writeToFile(FlipperFile* file) {
        file->WriteUInt32(KEY_CONFIG_FREQUENCY, Frequency);
        file->WriteUInt32(KEY_CONFIG_MAX_PAGERS, MaxPagerForBatchOrDetection);
        file->WriteUInt32(KEY_CONFIG_REPEATS, SignalRepeats);
        file->WriteUInt32(KEY_CONFIG_SAVED_STRATEGY, SavedStrategy);
        file->WriteBool(KEY_CONFIG_AUTOSAVE, AutosaveFoundSignals);
        file->WriteString(KEY_CONFIG_USER_CATGEGORY, CurrentUserCategory != NULL ? CurrentUserCategory->cstr() : "");
    }

public:
    void Load() {
        FlipperFile* configFile = FileManager().OpenRead(CONFIG_FILE_PATH);
        if(configFile != NULL) {
            readFromFile(configFile);
            delete configFile;
        }
    }

    void Save() {
        FlipperFile* configFile = FileManager().OpenWrite(CONFIG_FILE_PATH);
        if(configFile != NULL) {
            writeToFile(configFile);
            delete configFile;
        }
    }

    const char* GetCurrentUserCategoryCstr() {
        return CurrentUserCategory == NULL ? NULL : CurrentUserCategory->cstr();
    }
};
