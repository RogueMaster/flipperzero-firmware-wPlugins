#include "../Modbus.h"

void BuildSender(App* app, uint8_t* buf);

//////////////////////////   Manual Sender Scene  //////////////////////////
const char* fns[] = {"0x01", "0x02", "0x03", "0x04", "0x05", "0x06", "0x0F", "0x10"};

void itemChangeCB(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t* buf = app->msgBuf;
    uint8_t index = variable_item_get_current_value_index(item);
    uint8_t selectedIndex = variable_item_list_get_selected_item_index(app->varList);
    uint16_t Value;
    char str[10];
    switch(selectedIndex) {
    case 0:
        snprintf(str, sizeof(str), "%d", index + 1);
        variable_item_set_current_value_text(item, strdup(str));
        buf[0] = index + 1;
        break;
    case 1:
        variable_item_set_current_value_text(item, fns[index]);
        FUNCTION = index <= 0x05 ? index + 1 : index + 9;
        buf[4] = 0;
        buf[5] = 1;
        BuildSender(app, buf);
        break;
    case 2:
        snprintf(str, sizeof(str), "%d", index);
        variable_item_set_current_value_text(item, strdup(str));
        buf[2] = index >> 8 & 0x00FF;
        buf[3] = index & 0x00FF;
        break;
    case 3:
        if(FUNCTION != 0x05 && FUNCTION != 0x06) {
            index++;
            snprintf(str, sizeof(str), "%d", index);
            variable_item_set_current_value_text(item, strdup(str));
            buf[4] = index >> 8 & 0x00FF;
            buf[5] = index & 0x00FF;
            if(FUNCTION >= 0x0F) {
                Value = (buf[4] << 8 | buf[5]);
                if(FUNCTION == 0x0F)
                    Value = Value % 8 ? Value / 8 + 1 : Value / 8;
                else
                    Value = Value * 2;
                snprintf(str, sizeof(str), "[ %d ]", Value);
                variable_item_set_current_value_text(item, strdup(str));
                if(BYTECOUNT != Value) {
                    BYTECOUNT = Value;
                    BuildSender(app, buf);
                }
            }
        } else {
            Value = FUNCTION == 5 ? index ? 0xFF00 : 0x0000 : index;
            snprintf(str, sizeof(str), "0x%04X", Value);
            variable_item_set_current_value_text(
                item, FUNCTION == 0x05 ? index ? "ON" : "OFF" : str);
            buf[4] = Value >> 8 & 0x00FF;
            buf[5] = Value & 0x00FF;
        }
        break;
    default:
        Value = index;
        snprintf(str, sizeof(str), FUNCTION == 0x10 ? "0x%04X" : "0x%02X", Value);
        variable_item_set_current_value_text(item, str);
        if(FUNCTION == 0x0F) {
            selectedIndex += 2;
            buf[selectedIndex] = Value;
        } else {
            selectedIndex += selectedIndex - 3;

            buf[selectedIndex] = Value >> 8 & 0x0FF;
            buf[selectedIndex + 1] = Value & 0x00FF;
        }
        break;
    }
}

void itemEnterCB(void* context, uint32_t index) {
    App* app = context;
    uint8_t* buf = app->msgBuf;
    uint8_t SendButton = FUNCTION >= 0x0F ? (FUNCTION == 0x0F ? BYTECOUNT : QUANTITY) + 5 : 4;
    scene_manager_set_scene_state(app->sceneManager, app_scene_manual_sender, index);
    if(index == SendButton) {
        scene_manager_set_scene_state(app->sceneManager, app_scene_sniffer, Sender_Option);
        scene_manager_next_scene(app->sceneManager, app_scene_sniffer);
    }

    else if(index == 1 || (FUNCTION >= 0x0F && index == 4)) {
    } else {
        if(!(FUNCTION == 0x05 && index == 3)) {
            scene_manager_set_scene_state(app->sceneManager, app_scene_byte_input, index);
            scene_manager_next_scene(app->sceneManager, app_scene_byte_input);
        }
    }
}
void BuildValues(App* app, uint16_t byteCount, uint8_t* buf, bool one) {
    VariableItem* item;
    char lbl[20];
    char val[10];
    for(uint16_t i = 0; i < byteCount; i += one ? 1 : 2) {
        snprintf(lbl, sizeof(lbl), one ? "Byte %d" : "Register %d", one ? i + 1 : i / 2 + 1);
        snprintf(
            val, sizeof(val), one ? "0x%02X" : "0x%04X", one ? buf[i] : buf[i] << 8 | buf[i + 1]);
        item = variable_item_list_add(app->varList, strdup(lbl), 255, itemChangeCB, app);
        variable_item_set_current_value_text(item, strdup(val));
        variable_item_set_current_value_index(
            item, MIN(255, one ? buf[i] : buf[i] << 8 | buf[i + 1]));
    }
}
void BuildSender(App* app, uint8_t* buf) {
    variable_item_list_reset(app->varList);
    VariableItem* item;
    uint16_t Value = 0;
    char val[10];
    snprintf(val, sizeof(val), "%d", SLAVE);
    item = variable_item_list_add(app->varList, "Peripheral ID", 0xFF, itemChangeCB, app);
    variable_item_set_current_value_text(item, strdup(val));
    variable_item_set_current_value_index(item, SLAVE - 1);
    item = variable_item_list_add(app->varList, "Function", 8, itemChangeCB, app);
    variable_item_set_current_value_text(item, fns[FUNCTION <= 6 ? FUNCTION - 1 : FUNCTION - 9]);
    variable_item_set_current_value_index(item, FUNCTION <= 6 ? FUNCTION - 1 : FUNCTION - 9);
    Value = STARTADDRESS;
    snprintf(val, sizeof(val), "%d", Value);
    item = variable_item_list_add(app->varList, "Start Address", 255, itemChangeCB, app);
    variable_item_set_current_value_text(item, strdup(val));
    variable_item_set_current_value_index(item, MIN(255, STARTADDRESS));
    if(FUNCTION != 0x05 && FUNCTION != 0x06) {
        uint16_t max = FUNCTION == 0x10 ? 0x0A :
                       FUNCTION == 0x0F ? 0x50 :
                       FUNCTION <= 0x02 ? 0x7D0 :
                                          0x7D;
        Value = MIN(buf[4] << 8 | buf[5], max);
        snprintf(val, sizeof(val), "%d", Value);
        item = variable_item_list_add(app->varList, "Quantity", max, itemChangeCB, app);
        variable_item_set_current_value_text(item, strdup(val));
        variable_item_set_current_value_index(item, MIN(Value - 1, MIN(max, 255)));
        buf[4] = Value >> 8 & 0x00FF;
        buf[5] = Value & 0x00FF;
    } else {
        Value = buf[4] << 8 | buf[5];
        snprintf(val, sizeof(val), "0x%04X", Value);
        item = variable_item_list_add(
            app->varList, "Value", FUNCTION == 0x05 ? 2 : 255, itemChangeCB, app);
        variable_item_set_current_value_text(
            item, FUNCTION == 0x05 ? Value ? "ON" : "OFF" : strdup(val));
        variable_item_set_current_value_index(
            item, FUNCTION == 0x05 ? Value ? 1 : 0 : MIN(Value, 255));
        Value = FUNCTION == 5 ? Value ? 0xFF00 : 0x0000 : Value;
        buf[4] = Value >> 8 & 0x00FF;
        buf[5] = Value & 0x00FF;
    }
    if(FUNCTION >= 0x0F) {
        Value = (buf[4] << 8 | buf[5]);
        if(FUNCTION == 0x0F)
            Value = Value % 8 ? Value / 8 + 1 : Value / 8;
        else
            Value = Value * 2;
        snprintf(val, sizeof(val), "[ %d ]", Value);
        item = variable_item_list_add(app->varList, "ByteCount", 1, NULL, app);
        variable_item_set_current_value_text(item, strdup(val));
        variable_item_set_current_value_index(item, 0);
        BYTECOUNT = Value;
        app->msgLen = Value + 9;
        BuildValues(app, Value, buf + 7, FUNCTION == 0x0F ? true : false);
    }

    item = variable_item_list_add(app->varList, "Send Packet", 1, NULL, app);
    variable_item_list_set_enter_callback(app->varList, itemEnterCB, app);
    variable_item_set_current_value_text(item, "");
    variable_item_set_current_value_index(item, 0);
}
void app_scene_manual_sender_on_enter(void* context) {
    App* app = context;
    BuildSender(app, app->msgBuf);
    variable_item_list_set_selected_item(
        app->varList, scene_manager_get_scene_state(app->sceneManager, app_scene_manual_sender));
    view_dispatcher_switch_to_view(app->viewDispatcher, VarList_View);
}
bool app_scene_manual_sender_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}
void app_scene_manual_sender_on_exit(void* context) {
    App* app = context;
    variable_item_list_reset(app->varList);
}
