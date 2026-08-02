#pragma once

#include <stdint.h>

typedef struct VariableItem VariableItem;
typedef struct VariableItemList VariableItemList;
typedef void (*VariableItemChangeCallback)(VariableItem* item);
typedef void (*VariableItemListEnterCallback)(void* context, uint32_t index);

struct VariableItem {
    const char* label;
    uint8_t values_count;
    uint8_t current_index;
    const char* current_text;
    char value_text[32];
    VariableItemChangeCallback changed;
    void* context;
};

struct VariableItemList {
    VariableItem items[12];
    uint8_t count;
    uint8_t selected;
    uint8_t resets;
    VariableItemListEnterCallback enter;
    void* enter_context;
};

VariableItem* variable_item_list_add(
    VariableItemList* list,
    const char* label,
    uint8_t values_count,
    VariableItemChangeCallback changed,
    void* context);
void variable_item_list_reset(VariableItemList* list);
void variable_item_list_set_enter_callback(
    VariableItemList* list,
    VariableItemListEnterCallback callback,
    void* context);
void variable_item_list_set_selected_item(VariableItemList* list, uint8_t selected);
uint8_t variable_item_list_get_selected_item_index(VariableItemList* list);
void variable_item_set_values_count(VariableItem* item, uint8_t count);
void variable_item_set_current_value_index(VariableItem* item, uint8_t index);
uint8_t variable_item_get_current_value_index(VariableItem* item);
void variable_item_set_current_value_text(VariableItem* item, const char* text);
void* variable_item_get_context(VariableItem* item);
