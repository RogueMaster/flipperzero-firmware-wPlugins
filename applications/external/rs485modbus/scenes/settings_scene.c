#include "../Modbus.h"
#include "../modbus_storage/modbus_storage.h"
#include "../modbus_uart/modbus_uart.h"

void itemChangedCB(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    uint8_t selectedIndex = variable_item_list_get_selected_item_index(app->varList);
    switch(selectedIndex) {
    case BaudRate_Option:
        variable_item_set_current_value_text(item, baudrateValues[index]);
        app->uart->cfg->baudrate = index;
        break;
    case DataWidth_Option:
        variable_item_set_current_value_text(item, dataWidthValues[index]);
        app->uart->cfg->dataWidth = index;
        break;
    case StopBits_Option:
        variable_item_set_current_value_text(item, stopBitsValues[index]);
        app->uart->cfg->stopBits = index;
        break;
    case Parity_Option:
        variable_item_set_current_value_text(item, parityValues[index]);
        app->uart->cfg->parity = index;
        break;
    case TimeOut_Option:
        app->uart->cfg->timeout = index;
        variable_item_set_current_value_index(item, index);
        furi_string_printf(app->modbus->timeout, "%d", index * TIMEOUT_SCALER);
        variable_item_set_current_value_text(item, furi_string_get_cstr(app->modbus->timeout));
        break;
    case OutputFormat_Option:
        variable_item_set_current_value_text(item, outputFormatValues[index]);
        app->uart->cfg->hexOutput = index;
        break;
    case SaveLOG_Option:
        variable_item_set_current_value_text(item, saveLOGValues[index]);
        app->uart->cfg->saveLOG = index;
        break;
    default:
        break;
    }
}
void app_scene_settings_on_enter(void* context) {
    App* app = context;
    VariableItem* item;
    item = variable_item_list_add(app->varList, "Buadrate", BR_VALUES, itemChangedCB, app);
    variable_item_set_current_value_index(item, app->uart->cfg->baudrate);
    variable_item_set_current_value_text(item, baudrateValues[app->uart->cfg->baudrate]);
    item = variable_item_list_add(app->varList, "Data size", DATAWIDTH_VALUES, itemChangedCB, app);
    variable_item_set_current_value_index(item, app->uart->cfg->dataWidth);
    variable_item_set_current_value_text(item, dataWidthValues[app->uart->cfg->dataWidth]);
    item = variable_item_list_add(app->varList, "Stop bits", STOPBITS_VALUES, itemChangedCB, app);
    variable_item_set_current_value_index(item, app->uart->cfg->stopBits);
    variable_item_set_current_value_text(item, stopBitsValues[app->uart->cfg->stopBits]);
    item = variable_item_list_add(app->varList, "Parity", PARITY_VALUES, itemChangedCB, app);
    variable_item_set_current_value_index(item, app->uart->cfg->parity);
    variable_item_set_current_value_text(item, parityValues[app->uart->cfg->parity]);
    item = variable_item_list_add(app->varList, "TimeOut(ms)", TIMEOUT_VALUES, itemChangedCB, app);
    variable_item_set_current_value_index(item, app->uart->cfg->timeout);
    furi_string_printf(app->modbus->timeout, "%d", app->uart->cfg->timeout * TIMEOUT_SCALER);
    variable_item_set_current_value_text(item, furi_string_get_cstr(app->modbus->timeout));
    item = variable_item_list_add(
        app->varList, "Output Format", DIGITALFORMAT_VALUES, itemChangedCB, app);
    variable_item_set_current_value_index(item, app->uart->cfg->hexOutput ? 1 : 0);
    variable_item_set_current_value_text(
        item, outputFormatValues[app->uart->cfg->hexOutput ? 1 : 0]);
    item = variable_item_list_add(app->varList, "Save LOG?", SAVE_LOG_VALUES, itemChangedCB, app);
    variable_item_set_current_value_index(item, app->uart->cfg->saveLOG ? 1 : 0);
    variable_item_set_current_value_text(item, saveLOGValues[app->uart->cfg->saveLOG ? 1 : 0]);

    variable_item_list_set_selected_item(app->varList, 0);
    view_dispatcher_switch_to_view(app->viewDispatcher, VarList_View);
}
bool app_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}
void app_scene_settings_on_exit(void* context) {
    App* app = context;
    uart_set_config(app);
    variable_item_list_reset(app->varList);
}
