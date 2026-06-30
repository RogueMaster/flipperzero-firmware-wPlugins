#pragma once

#include "SelectCategoryScreen.hpp"
#include "app/AppConfig.hpp"
#include "app/pager/PagerReceiver.hpp"
#include "lib/String.hpp"
#include "lib/hardware/subghz/SubGhzModule.hpp"
#include "lib/ui/UiManager.hpp"
#include "lib/ui/view/VariableItemListUiView.hpp"

class SettingsScreen {
private:
    AppConfig* config;
    SubGhzModule* subghz;
    PagerReceiver* receiver;
    VariableItemListUiView* varItemList;

    UiVariableItem* currentCategoryItem;
    UiVariableItem* frequencyItem;
    UiVariableItem* maxPagerItem;
    UiVariableItem* signalRepeatItem;
    UiVariableItem* ignoreSavedItem;
    UiVariableItem* autosaveFoundItem;
    UiVariableItem* debugModeItem;

    String frequencyStr;
    String maxPagerStr;
    String signalRepeatStr;
    bool updateUserCategory;
    uint32_t categoryItemIndex;

public:
    SettingsScreen(AppConfig* config, PagerReceiver* receiver, SubGhzModule* subghz, bool updateUserCategory) {
        this->config = config;
        this->receiver = receiver;
        this->subghz = subghz;
        this->updateUserCategory = updateUserCategory;

        varItemList = new VariableItemListUiView();
        varItemList->SetOnDestroyHandler(HANDLER(&SettingsScreen::destroy));
        varItemList->SetEnterPressHandler(HANDLER_1ARG(&SettingsScreen::enterPressHandler));

        categoryItemIndex = varItemList->AddItem(
            currentCategoryItem = new UiVariableItem("Category", HANDLER_1ARG(&SettingsScreen::categoryChangedHandler))
        );

        varItemList->AddItem(
            frequencyItem = new UiVariableItem(
                "Scan frequency",
                FrequencyManager::GetInstance()->GetFrequencyIndex(config->Frequency),
                FrequencyManager::GetInstance()->GetFrequencyCount(),
                [this](uint8_t val) {
                    uint32_t freq = this->config->Frequency = FrequencyManager::GetInstance()->GetFrequency(val);
                    this->subghz->SetReceiveFrequency(this->config->Frequency);
                    return frequencyStr.format("%lu.%02lu", freq / 1000000, (freq % 1000000) / 10000);
                }
            )
        );

        varItemList->AddItem(
            maxPagerItem = new UiVariableItem(
                "Max pager value",
                config->MaxPagerForBatchOrDetection - 1,
                UINT8_MAX,
                [this](uint8_t val) {
                    this->config->MaxPagerForBatchOrDetection = val + 1;
                    return maxPagerStr.fromInt(this->config->MaxPagerForBatchOrDetection);
                }
            )
        );

        varItemList->AddItem(
            signalRepeatItem = new UiVariableItem(
                "Times to repeat signal",
                config->SignalRepeats - 1,
                UINT8_MAX,
                [this](uint8_t val) {
                    this->config->SignalRepeats = val + 1;
                    return signalRepeatStr.fromInt(this->config->SignalRepeats);
                }
            )
        );

        varItemList->AddItem(
            ignoreSavedItem = new UiVariableItem(
                "Saved stations",
                config->SavedStrategy,
                SavedStationStrategyValuesCount,
                [this](uint8_t val) {
                    this->config->SavedStrategy = static_cast<enum SavedStationStrategy>(val);
                    return savedStationsStrategy(this->config->SavedStrategy);
                }
            )
        );

        varItemList->AddItem(
            autosaveFoundItem = new UiVariableItem(
                "Autosave found signals",
                config->AutosaveFoundSignals,
                2,
                [this](uint8_t val) {
                    this->config->AutosaveFoundSignals = val;
                    return boolOption(val);
                }
            )
        );
    }

    UiView* GetView() {
        return varItemList;
    }

private:
    void enterPressHandler(uint32_t index) {
        if(index != categoryItemIndex) {
            return;
        }
        UiManager::GetInstance()->PushView(
            (new SelectCategoryScreen(false, User, HANDLER_2ARG(&SettingsScreen::categorySelected)))->GetView()
        );
    }

    void categorySelected(CategoryType, const char* category) {
        if(config->CurrentUserCategory != NULL) {
            delete config->CurrentUserCategory;
        }
        config->CurrentUserCategory = category != NULL ? new String("%s", category) : NULL;
        UiManager::GetInstance()->PopView(false);
        currentCategoryItem->Refresh();
    }

    const char* categoryChangedHandler(uint8_t) {
        const char* category = config->GetCurrentUserCategoryCstr();
        if(category == NULL) {
            category = "Default";
        }
        return category;
    }

    const char* boolOption(uint8_t value) {
        return value ? "ON" : "OFF";
    }

    const char* savedStationsStrategy(SavedStationStrategy value) {
        switch(value) {
        case IGNORE:
            return "Ignore";

        case SHOW_NAME:
            return "Show name";

        case HIDE:
            return "Hide";

        default:
            return NULL;
        }
    }

    void destroy() {
        config->Save();
        if(updateUserCategory) {
            receiver->SetUserCategory(config->CurrentUserCategory);
            receiver->ReloadKnownStations();
        }

        delete currentCategoryItem;
        delete frequencyItem;
        delete maxPagerItem;
        delete signalRepeatItem;
        delete ignoreSavedItem;
        delete autosaveFoundItem;
        delete debugModeItem;

        delete this;
    }
};
