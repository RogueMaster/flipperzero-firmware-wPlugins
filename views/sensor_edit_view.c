/*
 * sensor_edit_view.c — sensor configuration editor (name, offset, GPIO, save).
 * Adapted from _ref_unitemp/views/SensorEdit_view.c
 */
#include "../co2_app_i.h"
#include <gui/modules/variable_item_list.h>
#include "../sensors/unitemp/interfaces/SingleWireSensor.h"
#include "../sensors/unitemp/interfaces/OneWireSensor.h"
#include "../sensors/unitemp/interfaces/I2CSensor.h"

static View* view;
static VariableItemList* variable_item_list;
static Sensor* editable_sensor;
static const GPIO* initial_gpio = NULL;

static VariableItem* sensor_name_item;
static VariableItem* onewire_addr_item;
static VariableItem* onewire_type_item;
VariableItem* temp_offset_item;
VariableItem* calibration_item;

#define OFFSET_BUFF_SIZE 5
static char* offset_buff;

/* Computed in view_sensor_edit_switch, used in _enter_callback */
static uint8_t save_item_index = 4;
static uint8_t onewire_scan_item_index = 4;

#define VIEW_ID ViewSensorEdit

bool _onewire_id_exist(uint8_t* id) {
    if(id == NULL) return false;
    for(uint8_t i = 0; i < unitemp_sensors_getActiveCount(); i++) {
        if(unitemp_sensor_getActive(i)->type == &Dallas) {
            if(unitemp_onewire_id_compare(
                   id, ((OneWireSensor*)(unitemp_sensor_getActive(i)->instance))->deviceID)) {
                return true;
            }
        }
    }
    return false;
}

static void _onewire_scan(void) {
    OneWireSensor* ow_sensor = editable_sensor->instance;
    unitemp_onewire_bus_init(ow_sensor->bus);
    uint8_t* id = NULL;
    do {
        id = unitemp_onewire_bus_enum_next(ow_sensor->bus);
    } while(_onewire_id_exist(id));

    if(id == NULL) {
        unitemp_onewire_bus_enum_init();
        id = unitemp_onewire_bus_enum_next(ow_sensor->bus);
        if(_onewire_id_exist(id)) {
            do {
                id = unitemp_onewire_bus_enum_next(ow_sensor->bus);
            } while(_onewire_id_exist(id) && id != NULL);
        }
        if(id == NULL) {
            memset(ow_sensor->deviceID, 0, 8);
            ow_sensor->familyCode = 0;
            unitemp_onewire_bus_deinit(ow_sensor->bus);
            variable_item_set_current_value_text(onewire_addr_item, "empty");
            variable_item_set_current_value_text(
                onewire_type_item, unitemp_onewire_sensor_getModel(editable_sensor));
            return;
        }
    }

    unitemp_onewire_bus_deinit(ow_sensor->bus);
    memcpy(ow_sensor->deviceID, id, 8);
    ow_sensor->familyCode = id[0];

    if(ow_sensor->familyCode != 0) {
        char id_buff[10];
        snprintf(id_buff, 10, "%02X%02X%02X",
            ow_sensor->deviceID[1], ow_sensor->deviceID[2], ow_sensor->deviceID[3]);
        variable_item_set_current_value_text(onewire_addr_item, id_buff);
    } else {
        variable_item_set_current_value_text(onewire_addr_item, "empty");
    }
    variable_item_set_current_value_text(
        onewire_type_item, unitemp_onewire_sensor_getModel(editable_sensor));
}

static uint32_t _exit_callback(void* context) {
    UNUSED(context);
    if(!editable_sensor) return ViewMain;
    editable_sensor->status = UT_SENSORSTATUS_TIMEOUT;
    if(!unitemp_sensor_isContains(editable_sensor)) unitemp_sensor_free(editable_sensor);
    unitemp_sensors_reload();
    return ViewMain;
}

static void _enter_callback(void* context, uint32_t index) {
    UNUSED(context);
    if(index == 0) {
        view_sensor_name_edit_switch(editable_sensor);
    }
    if(index == save_item_index) {
        if(editable_sensor->type->interface == &ONE_WIRE &&
           ((OneWireSensor*)(editable_sensor->instance))->familyCode == 0) {
            return;
        }
        if(initial_gpio != NULL) {
            unitemp_gpio_unlock(initial_gpio);
            initial_gpio = NULL;
        }
        editable_sensor->status = UT_SENSORSTATUS_TIMEOUT;
        if(!unitemp_sensor_isContains(editable_sensor)) unitemp_sensors_add(editable_sensor);
        unitemp_sensors_save();
        app->sensors_ready = false;
        app->sensors_update = true;
        view_main_switch();
    }
    if(editable_sensor->type->interface == &ONE_WIRE && index == onewire_scan_item_index) {
        _onewire_scan();
    }
}

static void _gpio_change_callback(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    if(editable_sensor->type->interface == &SINGLE_WIRE) {
        SingleWireSensor* instance = editable_sensor->instance;
        instance->gpio =
            unitemp_gpio_getAviablePort(editable_sensor->type->interface, index, initial_gpio);
        variable_item_set_current_value_text(item, instance->gpio->name);
    }
    if(editable_sensor->type->interface == &SPI) {
        SPISensor* instance = editable_sensor->instance;
        instance->CS_pin =
            unitemp_gpio_getAviablePort(editable_sensor->type->interface, index, initial_gpio);
        variable_item_set_current_value_text(item, instance->CS_pin->name);
    }
    if(editable_sensor->type->interface == &ONE_WIRE) {
        OneWireSensor* instance = editable_sensor->instance;
        instance->bus->gpio =
            unitemp_gpio_getAviablePort(editable_sensor->type->interface, index, NULL);
        variable_item_set_current_value_text(item, instance->bus->gpio->name);
    }
}

static void _i2caddr_change_callback(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    ((I2CSensor*)editable_sensor->instance)->currentI2CAdr =
        ((I2CSensor*)editable_sensor->instance)->minI2CAdr + index * 2;
    char buff[5];
    snprintf(buff, 5, "0x%2X", ((I2CSensor*)editable_sensor->instance)->currentI2CAdr >> 1);
    variable_item_set_current_value_text(item, buff);
}

static void _name_change_callback(VariableItem* item) {
    variable_item_set_current_value_index(item, 0);
    view_sensor_name_edit_switch(editable_sensor);
}

static void _calibrate_callback(VariableItem* item) {
    variable_item_set_current_value_index(item, 0);
    const SensorTypeWithCalibration* extSensor =
        (const SensorTypeWithCalibration*)editable_sensor->type;
    extSensor->calibrate(editable_sensor, 450);
}

static void _onwire_addr_change_callback(VariableItem* item) {
    variable_item_set_current_value_index(item, 0);
    _onewire_scan();
}

static void _offset_change_callback(VariableItem* item) {
    editable_sensor->temp_offset = variable_item_get_current_value_index(item) - 100;
    snprintf(
        offset_buff, OFFSET_BUFF_SIZE, "%+1.1f", (double)(editable_sensor->temp_offset / 10.0));
    variable_item_set_current_value_text(item, offset_buff);
}

void view_sensor_edit_alloc(void) {
    variable_item_list = variable_item_list_alloc();
    variable_item_list_reset(variable_item_list);
    variable_item_list_set_enter_callback(variable_item_list, _enter_callback, app);
    view = variable_item_list_get_view(variable_item_list);
    view_set_previous_callback(view, _exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID, view);
    offset_buff = malloc(OFFSET_BUFF_SIZE);
}

void view_sensor_edit_switch(Sensor* sensor) {
    editable_sensor = sensor;
    editable_sensor->status = UT_SENSORSTATUS_INACTIVE;

    variable_item_list_reset(variable_item_list);
    variable_item_list_set_selected_item(variable_item_list, 0);

    uint8_t idx = 0;

    /* Name — always index 0 */
    sensor_name_item = variable_item_list_add(
        variable_item_list, "Name", strlen(sensor->name) > 7 ? 1 : 2, _name_change_callback, NULL);
    variable_item_set_current_value_index(sensor_name_item, 0);
    variable_item_set_current_value_text(sensor_name_item, sensor->name);
    idx++;

    /* Type — always index 1 */
    onewire_type_item = variable_item_list_add(variable_item_list, "Type", 1, NULL, NULL);
    variable_item_set_current_value_index(onewire_type_item, 0);
    variable_item_set_current_value_text(
        onewire_type_item,
        (sensor->type->interface == &ONE_WIRE ?
             unitemp_onewire_sensor_getModel(editable_sensor) :
             sensor->type->typename));
    idx++;

    /* Temp. offset — only for sensors that measure temperature */
    if(sensor->type->datatype & UT_TEMPERATURE) {
        temp_offset_item = variable_item_list_add(
            variable_item_list, "Temp. offset", 201, _offset_change_callback, NULL);
        variable_item_set_current_value_index(temp_offset_item, sensor->temp_offset + 100);
        snprintf(
            offset_buff, OFFSET_BUFF_SIZE, "%+1.1f", (double)(editable_sensor->temp_offset / 10.0));
        variable_item_set_current_value_text(temp_offset_item, offset_buff);
        idx++;
    }

    if(sensor->type->interface == &ONE_WIRE || sensor->type->interface == &SINGLE_WIRE ||
       sensor->type->interface == &SPI) {
        if(sensor->type->interface == &ONE_WIRE) {
            initial_gpio = ((OneWireSensor*)editable_sensor->instance)->bus->gpio;
        } else if(sensor->type->interface == &SINGLE_WIRE) {
            initial_gpio = ((SingleWireSensor*)editable_sensor->instance)->gpio;
        } else if(sensor->type->interface == &SPI) {
            initial_gpio = ((SPISensor*)editable_sensor->instance)->CS_pin;
        }

        uint8_t aviable_gpio_count =
            unitemp_gpio_getAviablePortsCount(sensor->type->interface, initial_gpio);
        VariableItem* item = variable_item_list_add(
            variable_item_list, "GPIO", aviable_gpio_count, _gpio_change_callback, app);

        uint8_t gpio_index = 0;
        if(unitemp_sensor_isContains(editable_sensor)) {
            for(uint8_t i = 0; i < aviable_gpio_count; i++) {
                if(unitemp_gpio_getAviablePort(sensor->type->interface, i, initial_gpio) ==
                   initial_gpio) {
                    gpio_index = i;
                    break;
                }
            }
        }
        variable_item_set_current_value_index(item, gpio_index);
        variable_item_set_current_value_text(
            item,
            unitemp_gpio_getAviablePort(sensor->type->interface, gpio_index, initial_gpio)->name);
        idx++;
    }

    if(sensor->type->interface == &I2C) {
        VariableItem* item = variable_item_list_add(
            variable_item_list,
            "I2C address",
            (((I2CSensor*)sensor->instance)->maxI2CAdr >> 1) -
                (((I2CSensor*)sensor->instance)->minI2CAdr >> 1) + 1,
            _i2caddr_change_callback,
            app);
        snprintf(app->buff, 5, "0x%2X", ((I2CSensor*)sensor->instance)->currentI2CAdr >> 1);
        variable_item_set_current_value_index(
            item,
            (((I2CSensor*)sensor->instance)->currentI2CAdr >> 1) -
                (((I2CSensor*)sensor->instance)->minI2CAdr >> 1));
        variable_item_set_current_value_text(item, app->buff);
        idx++;
    }

    if(sensor->type->interface == &ONE_WIRE) {
        onewire_scan_item_index = idx;
        onewire_addr_item = variable_item_list_add(
            variable_item_list, "Address", 2, _onwire_addr_change_callback, NULL);
        OneWireSensor* ow_sensor = sensor->instance;
        if(ow_sensor->familyCode == 0) {
            variable_item_set_current_value_text(onewire_addr_item, "Scan");
        } else {
            snprintf(
                app->buff, 10, "%02X%02X%02X",
                ow_sensor->deviceID[1], ow_sensor->deviceID[2], ow_sensor->deviceID[3]);
            variable_item_set_current_value_text(onewire_addr_item, app->buff);
        }
        idx++;
    }

    if((sensor->type->datatype & UT_CALIBRATION) == UT_CALIBRATION) {
        calibration_item =
            variable_item_list_add(variable_item_list, "Calibrate", 1, _calibrate_callback, NULL);
        idx++;
    }

    save_item_index = idx;
    variable_item_list_add(variable_item_list, "Save", 1, NULL, NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID);
}

void view_sensor_edit_free(void) {
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID);
    variable_item_list_free(variable_item_list);
    free(offset_buff);
}
