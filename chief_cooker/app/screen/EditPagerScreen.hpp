#pragma once

#include "SelectCategoryScreen.hpp"
#include "lib/HandlerContext.hpp"
#include "lib/String.hpp"
#include "app/pager/PagerReceiver.hpp"
#include "lib/hardware/subghz/SubGhzModule.hpp"
#include "lib/ui/view/UiView.hpp"
#include "lib/ui/view/VariableItemListUiView.hpp"
#include "lib/ui/view/TextInputUiView.hpp"
#include "lib/ui/view/DialogUiView.hpp"
#include "lib/FlipperDolphin.hpp"
#include "lib/ui/UiManager.hpp"
#include "app/AppFileSystem.hpp"
#include "app/pager/PagerSerializer.hpp"

#define TE_DIV 10

class EditPagerScreen {
private:
    AppConfig* config;
    SubGhzModule* subghz;
    PagerReceiver* receiver;
    PagerDataGetter getPager;
    VariableItemListUiView* varItemList;

    UiVariableItem* encodingItem = NULL;
    UiVariableItem* stationItem = NULL;
    UiVariableItem* pagerItem = NULL;
    UiVariableItem* actionItem = NULL;
    UiVariableItem* hexItem = NULL;
    UiVariableItem* protocolItem = NULL;
    UiVariableItem* frequencyItem = NULL;
    UiVariableItem* teItem = NULL;
    UiVariableItem* repeatsItem = NULL;

    UiVariableItem* saveAsItem = NULL;
    UiVariableItem* deleteItem = NULL;

    String stationStr;
    String pagerStr;
    String actionStr;
    String hexStr;
    String repeatsStr;
    String frequencyStr;
    String teStr;
    int32_t saveAsItemIndex = -1;
    int32_t deleteItemIndex = -1;

    bool isFromFile;
    const char* saveAsName = NULL;

public:
    EditPagerScreen(
        AppConfig* config,
        SubGhzModule* subghz,
        PagerReceiver* receiver,
        PagerDataGetter pagerGetter,
        bool isFromFile
    ) {
        this->config = config;
        this->subghz = subghz;
        this->receiver = receiver;
        this->getPager = pagerGetter;
        this->isFromFile = isFromFile;

        StoredPagerData* pager = getPager();
        PagerDecoder* decoder = receiver->decoders[pager->decoder];
        PagerProtocol* protocol = receiver->protocols[pager->protocol];
        uint32_t frequency = FrequencyManager::GetInstance()->GetFrequency(pager->frequency);

        varItemList = new VariableItemListUiView();
        varItemList->SetOnDestroyHandler(HANDLER(&EditPagerScreen::destroy));
        varItemList->SetEnterPressHandler(HANDLER_1ARG(&EditPagerScreen::enterPressed));

        varItemList->AddItem(
            encodingItem = new UiVariableItem(
                "Encoding", pager->decoder, receiver->decodersCount, HANDLER_1ARG(&EditPagerScreen::encodingValueChanged)
            )
        );

        varItemList->AddItem(stationItem = new UiVariableItem("Station", HANDLER_1ARG(&EditPagerScreen::stationValueChanged)));
        varItemList->AddItem(pagerItem = new UiVariableItem("Pager", HANDLER_1ARG(&EditPagerScreen::pagerValueChanged)));
        updatePagerIsEditable();

        varItemList->AddItem(
            actionItem = new UiVariableItem(
                "Action",
                decoder->GetActionValue(pager->data),
                decoder->GetActionsCount(),
                HANDLER_1ARG(&EditPagerScreen::actionValueChanged)
            )
        );

        varItemList->AddItem(hexItem = new UiVariableItem("HEX value", HANDLER_1ARG(&EditPagerScreen::hexValueChanged)));
        varItemList->AddItem(protocolItem = new UiVariableItem("Protocol", protocol->GetSystemName()));
        varItemList->AddItem(
            frequencyItem = new UiVariableItem(
                "Frequency", frequencyStr.format("%lu.%02lu", frequency / 1000000, (frequency % 1000000) / 10000)
            )
        );
        varItemList->AddItem(
            teItem = new UiVariableItem(
                "TE", pager->te / TE_DIV, protocol->GetMaxTE() / TE_DIV, HANDLER_1ARG(&EditPagerScreen::teValueChanged)
            )
        );
        varItemList->AddItem(
            repeatsItem = new UiVariableItem(
                "Signal Repeats", repeatsStr.format(pager->repeats == MAX_REPEATS ? "%d+" : "%d", pager->repeats)
            )
        );

        if(canSave()) {
            const char* saveAsItemName = isFromFile ? "Save / Rename" : "Save signal as...";
            saveAsItemIndex = varItemList->AddItem(saveAsItem = new UiVariableItem(saveAsItemName, ""));
        }

        if(canDelete()) {
            deleteItemIndex = varItemList->AddItem(deleteItem = new UiVariableItem("Delete station", ""));
        }
    }

private:
    bool canSave() {
        return !receiver->IsKnown(getPager()) || this->isFromFile;
    }

    bool canDelete() {
        return isFromFile;
    }

    String* currentStationName() {
        return receiver->GetName(getPager());
    }

    void updatePagerIsEditable() {
        StoredPagerData* pager = getPager();
        int pagerNum = receiver->decoders[pager->decoder]->GetPager(pager->data);
        if(pagerNum < UINT8_MAX) {
            pagerItem->SetSelectedItem(pagerNum, UINT8_MAX);
        } else {
            pagerItem->SetSelectedItem(0, 1);
        }
    }

    void enterPressed(int32_t index) {
        if(index == saveAsItemIndex) {
            saveAs();
        } else if(index == deleteItemIndex) {
            DialogUiView* removeConfirmation = new DialogUiView("Really delete?", currentStationName()->cstr());
            removeConfirmation->AddLeftButton("Nope");
            removeConfirmation->AddRightButton("Yup");
            removeConfirmation->SetResultHandler(HANDLER_1ARG(&EditPagerScreen::confirmDelete));

            UiManager::GetInstance()->PushView(removeConfirmation);
        } else {
            transmitMessage();
        }
    }

    void confirmDelete(DialogExResult result) {
        if(result == DialogExResultRight) {
            AppFileSysytem().DeletePager(receiver->GetCurrentUserCategory(), getPager());
            receiver->ReloadKnownStations();
            UiManager::GetInstance()->PopView(false);
        }
    }

    void transmitMessage() {
        StoredPagerData* pager = getPager();
        PagerProtocol* protocol = receiver->protocols[pager->protocol];
        uint32_t frequency = FrequencyManager::GetInstance()->GetFrequency(pager->frequency);
        subghz->Transmit(protocol->CreatePayload(pager->data, pager->te, config->SignalRepeats), frequency);

        FlipperDolphin::Deed(DolphinDeedSubGhzSend);
    }

    void saveAs() {
        TextInputUiView* nameInputView = new TextInputUiView("Enter station name", NAME_MIN_LENGTH, NAME_MAX_LENGTH);
        String* name = currentStationName();
        if(name != NULL) {
            nameInputView->SetDefaultText(name);
        }
        nameInputView->SetResultHandler(HANDLER_1ARG(&EditPagerScreen::saveAsHandler));
        UiManager::GetInstance()->PushView(nameInputView);
    }

    void saveAsHandler(const char* name) {
        saveAsName = name;

        UiManager::GetInstance()->ShowLoading();
        UiManager::GetInstance()->PushView(
            (new SelectCategoryScreen(true, User, HANDLER_2ARG(&EditPagerScreen::categorySelected)))->GetView()
        );
    }

    void categorySelected(CategoryType, const char* category) {
        StoredPagerData* pager = getPager();
        PagerDecoder* decoder = receiver->decoders[pager->decoder];
        PagerProtocol* protocol = receiver->protocols[pager->protocol];
        uint32_t frequency = FrequencyManager::GetInstance()->GetFrequency(pager->frequency);

        AppFileSysytem().SaveToUserCategory(category, saveAsName, pager, decoder, protocol, frequency);
        FlipperDolphin::Deed(DolphinDeedSubGhzSave);

        receiver->ReloadKnownStations();

        for(int i = 0; i < 3; i++) {
            UiManager::GetInstance()->PopView(false);
        }
    }

    const char* encodingValueChanged(uint8_t index) {
        StoredPagerData* pager = getPager();
        PagerDecoder* decoder = receiver->decoders[index];
        pager->decoder = index;

        if(stationItem != NULL) {
            stationItem->Refresh();
        }
        if(pagerItem != NULL) {
            updatePagerIsEditable();
            pagerItem->Refresh();
        }
        if(actionItem != NULL) {
            actionItem->SetSelectedItem(decoder->GetActionValue(pager->data), decoder->GetActionsCount());
        }
        return receiver->decoders[pager->decoder]->GetShortName();
    }

    const char* stationValueChanged(uint8_t) {
        StoredPagerData* pager = getPager();
        return stationStr.fromInt(receiver->decoders[pager->decoder]->GetStation(pager->data));
    }

    const char* pagerValueChanged(uint8_t newPager) {
        StoredPagerData* pager = getPager();
        PagerDecoder* decoder = receiver->decoders[pager->decoder];
        if(pagerItem->Editable() && newPager != decoder->GetPager(pager->data)) {
            pager->data = decoder->SetPager(pager->data, newPager);
            pager->edited = true;

            if(hexItem != NULL) {
                hexItem->Refresh();
            }
        }
        return pagerStr.fromInt(decoder->GetPager(pager->data));
    }

    const char* actionValueChanged(uint8_t value) {
        StoredPagerData* pager = getPager();
        PagerDecoder* decoder = receiver->decoders[pager->decoder];
        if(decoder->GetActionValue(pager->data) != value) {
            pager->data = decoder->SetActionValue(pager->data, value);
            pager->edited = true;
        }

        if(hexItem != NULL) {
            hexItem->Refresh();
        }

        uint8_t actionValue = decoder->GetActionValue(pager->data);
        PagerAction action = decoder->GetAction(pager->data);
        const char* actionDesc = PagerActions::GetDescription(action);

        return actionStr.format("%d (%s)", actionValue, actionDesc);
    }

    const char* hexValueChanged(uint8_t) {
        StoredPagerData* pager = getPager();
        return hexStr.format("%06X", (unsigned int)pager->data);
    }

    const char* teValueChanged(uint8_t newTeIndex) {
        StoredPagerData* pager = getPager();
        if(newTeIndex != pager->te / TE_DIV) {
            int teDiff = pager->te % TE_DIV;
            int newTe = newTeIndex * TE_DIV + teDiff;

            pager->te = newTe;
            pager->edited = true;
        }

        return teStr.format("%d", pager->te);
    }

    void destroy() {
        delete encodingItem;
        delete stationItem;
        delete pagerItem;
        delete actionItem;
        delete hexItem;
        delete frequencyItem;
        delete teItem;
        delete protocolItem;
        delete repeatsItem;

        if(saveAsItem != NULL) {
            delete saveAsItem;
        }

        if(deleteItem != NULL) {
            delete deleteItem;
        }

        delete this;
    }

public:
    UiView* GetView() {
        return varItemList;
    }
};
