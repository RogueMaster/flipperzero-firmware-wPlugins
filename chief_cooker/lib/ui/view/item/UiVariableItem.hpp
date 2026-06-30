#pragma once

#include <functional>

#include "gui/modules/variable_item_list.h"

using namespace std;

class UiVariableItem {
private:
    VariableItem* item = NULL;
    const char* label;

    uint8_t selectedIndex;
    uint8_t valuesCount;

    function<const char*(uint8_t)> changeHandler;

    static void itemChangeCallback(VariableItem* item) {
        UiVariableItem* uiItem = (UiVariableItem*)variable_item_get_context(item);
        uint8_t index = variable_item_get_current_value_index(item);
        variable_item_set_current_value_text(item, uiItem->changeHandler(index));
    }

public:
    UiVariableItem(const char* label, const char* staticValue) :
            UiVariableItem(label, [staticValue](uint8_t) { return staticValue; }) {
    }

    UiVariableItem(const char* label, function<const char*(uint8_t)> changeHandler) : UiVariableItem(label, 0, 1, changeHandler) {
    }

    UiVariableItem(const char* label, uint8_t selectedIndex, uint8_t valuesCount, function<const char*(uint8_t)> changeHandler) {
        this->label = label;
        this->selectedIndex = selectedIndex;
        this->valuesCount = valuesCount;
        this->changeHandler = changeHandler;
    }

    void AddTo(VariableItemList* varItemList) {
        item = variable_item_list_add(varItemList, label, valuesCount, itemChangeCallback, this);
        Refresh();
    }

    void SetSelectedItem(uint8_t selectedIndex, uint8_t valuesCount) {
        this->selectedIndex = selectedIndex;
        this->valuesCount = valuesCount;

        Refresh();
    }

    void Refresh() {
        if(item == NULL) {
            return;
        }

        variable_item_set_values_count(item, valuesCount);
        variable_item_set_current_value_index(item, selectedIndex);
        itemChangeCallback(item);
    }

    bool Editable() {
        return valuesCount > 1;
    }

    ~UiVariableItem() {
    }
};
