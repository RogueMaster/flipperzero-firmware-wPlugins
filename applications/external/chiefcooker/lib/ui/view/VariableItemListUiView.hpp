#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/variable_item_list.h>

#include "UiView.hpp"
#include "item/UiVariableItem.hpp"

#undef LOG_TAG
#define LOG_TAG "UI_VARITEMLST"

using namespace std;

class VariableItemListUiView : public UiView {
private:
    uint32_t itemCounter = 0;
    VariableItemList* varItemList;
    function<void(uint32_t)> enterPressHandler;

    static void onEnterCallback(void* context, uint32_t index) {
        VariableItemListUiView* view = (VariableItemListUiView*)context;
        view->enterPressHandler(index);
    }

public:
    VariableItemListUiView() {
        varItemList = variable_item_list_alloc();
    }

    uint32_t AddItem(UiVariableItem* item) {
        item->AddTo(varItemList);
        return itemCounter++;
    }

    void SetEnterPressHandler(function<void(uint32_t)> handler) {
        enterPressHandler = handler;
        variable_item_list_set_enter_callback(varItemList, onEnterCallback, this);
    }

    View* GetNativeView() {
        return variable_item_list_get_view(varItemList);
    }

    ~VariableItemListUiView() {
        if(varItemList != NULL) {
            OnDestory();

            variable_item_list_free(varItemList);
            varItemList = NULL;
        }
    }
};
