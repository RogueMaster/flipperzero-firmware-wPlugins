/*
    Unitemp - Universal temperature reader
    Copyright (C) 2022-2026  Victor Nikitchuk (https://github.com/quen0n)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "../unitemp.h"
#include "sensors.h"
#include "./interfaces/i2c_sensor.h"
#include "./interfaces/onewire_sensor.h"
#include "./interfaces/singlewire_sensor.h"
#include "./interfaces/spi_sensor.h"
#include "scenes/unitemp_scene.h"

static bool name_edit = false;
static uint8_t i2c_addr;
static VariableItem* model_item;
static VariableItem* onewire_scan_item;
static VariableItem* gpio_pin_item;
static VariableItem* i2c_addr_item;

static void _onewire_scan_event_callback(void* context) {
    UnitempApp* app = context;
    OneWireSensor* ow_sensor = app->editable_sensor->instance;
    FURI_CRITICAL_ENTER();
    bool result = unitemp_onewire_scan(ow_sensor);
    FURI_CRITICAL_EXIT();
    if(!result) {
        variable_item_set_current_value_text(onewire_scan_item, "not found");
    } else {
        snprintf(
            app->txt_buff,
            10,
            "%02X%02X%02X",
            ow_sensor->deviceID[1],
            ow_sensor->deviceID[2],
            ow_sensor->deviceID[3]);
        variable_item_set_current_value_text(onewire_scan_item, app->txt_buff);
        variable_item_set_current_value_text(
            model_item, unitemp_onewire_sensor_get_fc_name(app->editable_sensor));
    }
}

static void _i2c_scan_event_callback(void* context) {
    UnitempApp* app = context;
    I2CSensor* i2c_sensor = app->editable_sensor->instance;

    i2c_addr = unitemp_i2c_bus_scan_next(i2c_sensor);

    if(!i2c_addr) {
        variable_item_set_current_value_text(i2c_addr_item, "not found");
        variable_item_set_values_count(i2c_addr_item, 1);
        variable_item_set_current_value_index(i2c_addr_item, 0);

    } else {
        i2c_sensor->current_i2c_adress = i2c_addr;
        snprintf(app->txt_buff, 5, "0x%2X", i2c_sensor->current_i2c_adress >> 1);
        variable_item_set_current_value_text(i2c_addr_item, app->txt_buff);
        if(i2c_sensor->max_i2c_adress - i2c_sensor->min_i2c_adress) {
            variable_item_set_values_count(i2c_addr_item, 2);
        } else {
            variable_item_set_values_count(i2c_addr_item, 1);
        }

        if(i2c_sensor->current_i2c_adress == i2c_sensor->min_i2c_adress) {
            variable_item_set_current_value_index(i2c_addr_item, 0);
        } else if(i2c_sensor->current_i2c_adress == i2c_sensor->max_i2c_adress) {
            variable_item_set_current_value_index(i2c_addr_item, 1);
        }
    }
}

static void _gpio_change_event_callback(void* context) {
    UnitempApp* app = context;
    Sensor* sensor = app->editable_sensor;
    uint8_t index = variable_item_get_current_value_index(gpio_pin_item);
    const SensorGpioPin* gpio_pin = NULL;
    const SensorConnectionInterface* interface = sensor->model->interface;
    if(interface == &unitemp_1w) {
        gpio_pin = ((OneWireSensor*)sensor->instance)->bus->bus_pin;
    } else if(interface == &unitemp_singlewire) {
        gpio_pin = ((SingleWireSensor*)sensor->instance)->data_pin;
    } else if(interface == &unitemp_spi) {
        gpio_pin = ((SPISensor*)sensor->instance)->cs_pin;
    }
    gpio_pin = unitemp_gpio_get_aviable_pin(interface, index, gpio_pin);

    if(interface == &unitemp_singlewire) {
        SingleWireSensor* instance = sensor->instance;
        unitemp_gpio_unlock(instance->data_pin);
        instance->data_pin = gpio_pin;
        unitemp_gpio_lock(gpio_pin, interface);
        variable_item_set_current_value_text(gpio_pin_item, instance->data_pin->name);
    } else if(interface == &unitemp_spi) {
        SPISensor* instance = sensor->instance;
        unitemp_gpio_unlock(instance->cs_pin);
        instance->cs_pin = unitemp_gpio_get_aviable_pin(interface, index, instance->cs_pin);
        unitemp_gpio_lock(gpio_pin, interface);
        variable_item_set_current_value_text(gpio_pin_item, instance->cs_pin->name);
    } else if(interface == &unitemp_1w) {
        OneWireSensor* instance = sensor->instance;

        //removing old bus
        unitemp_onewire_bus_free(
            instance
                ->bus); //This making a problem for developers. The function deinitializes the port and, for example, disables UART or SWD
        //making new bus
        const SensorGpioPin* bus_pin =
            unitemp_gpio_get_aviable_pin(interface, index, instance->bus->bus_pin);
        instance->bus = unitemp_onewire_bus_alloc(bus_pin);

        variable_item_set_current_value_text(gpio_pin_item, bus_pin->name);
    }
}

static void _name_change_callback(VariableItem* item) {
    name_edit = true;
    UnitempApp* app = variable_item_get_context(item);
    variable_item_set_current_value_index(item, 0);
    scene_manager_next_scene(app->scene_manager, UnitempSceneSensorEditName);
}

static void _offset_change_callback(VariableItem* item) {
    UnitempApp* app = variable_item_get_context(item);
    Sensor* sensor = app->editable_sensor;
    uint8_t index = variable_item_get_current_value_index(item);

    sensor->temperature_offset = index - 20;
    snprintf(
        app->txt_buff,
        5,
        sensor->temperature_offset == 0 ? "%1.0f" : "%+1.1f",
        (double)(sensor->temperature_offset / 10.0));
    variable_item_set_current_value_text(item, app->txt_buff);
}

static void _gpio_change_callback(VariableItem* item) {
    UnitempApp* app = variable_item_get_context(item);
    view_dispatcher_send_custom_event(app->view_dispatcher, CustomEventGPIOChanged);
}

static void _onwire_addr_change_callback(VariableItem* item) {
    UnitempApp* app = variable_item_get_context(item);
    variable_item_set_current_value_index(onewire_scan_item, 0);
    view_dispatcher_send_custom_event(app->view_dispatcher, CustomEventOneWireScan);
}

static void _i2c_addr_change_callback(VariableItem* item) {
    UnitempApp* app = variable_item_get_context(item);
    variable_item_set_current_value_index(i2c_addr_item, 1);
    view_dispatcher_send_custom_event(app->view_dispatcher, CustomEventI2CScan);
}

static void _enter_callback(void* context, uint32_t index) {
    UnitempApp* app = context;
    const SensorConnectionInterface* sensor_interface = app->editable_sensor->model->interface;
    //Name edit
    if(index == 0) {
        name_edit = true;
        scene_manager_next_scene(app->scene_manager, UnitempSceneSensorEditName);
    }

    //1W sensors scan
    if(index == 3 && sensor_interface == &unitemp_1w) {
        view_dispatcher_send_custom_event(app->view_dispatcher, CustomEventOneWireScan);
    }
    //I2C sensors scan
    if(index == 2 && sensor_interface == &unitemp_i2c) {
        view_dispatcher_send_custom_event(app->view_dispatcher, CustomEventI2CScan);
    }

    //Save
    if((index == 4 && sensor_interface != &unitemp_1w) ||
       (index == 5 && sensor_interface == &unitemp_1w)) {
        //Exit if the one wire sensor does not have an ID
        if(sensor_interface == &unitemp_1w &&
           ((OneWireSensor*)(app->editable_sensor->instance))->family_code == 0) {
            return;
        }
        //Exit if the one wire sensor does not have an I2C adress
        if(sensor_interface == &unitemp_i2c && !i2c_addr) {
            return;
        }

        if(!unitemp_sensor_in_list(app->editable_sensor)) {
            unitemp_sensors_add(app->editable_sensor);
            unitemp_sensor_init(app->editable_sensor);
        }

        unitemp_sensors_save(app);
        app->editable_sensor = NULL;
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, UnitempSceneMonitor);
    }
}

void unitemp_scene_sensor_edit_on_enter(void* context) {
    UnitempApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    name_edit = false;
    i2c_addr = 0;

    Sensor* sensor = app->editable_sensor;
    if(sensor == NULL) {
        FURI_LOG_E(APP_NAME, "Editable sensor is NULL!");
        return;
    }

    variable_item_list_set_enter_callback(var_item_list, _enter_callback, app);

    unitemp_sensor_deinit(sensor);

    //Sensor name
    item = variable_item_list_add(
        var_item_list, "Name", strlen(sensor->name) > 7 ? 1 : 2, _name_change_callback, app);
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(item, sensor->name);

    //Sensor model (not editable)
    model_item = variable_item_list_add(var_item_list, "Model", 1, NULL, NULL);
    variable_item_set_current_value_index(model_item, 0);
    variable_item_set_current_value_text(
        model_item,
        (sensor->model->interface == &unitemp_1w ? unitemp_onewire_sensor_get_fc_name(sensor) :
                                                   sensor->model->modelname));

    //Device address on the I2C bus (for I2C sensors)
    if(sensor->model->interface == &unitemp_i2c) {
        i2c_addr_item = variable_item_list_add(
            var_item_list,
            "I2C address",
            (((I2CSensor*)sensor->instance)->max_i2c_adress >> 1) -
                (((I2CSensor*)sensor->instance)->min_i2c_adress >> 1) + 1,
            _i2c_addr_change_callback,
            app);
        if(unitemp_sensor_in_list(sensor)) {
            snprintf(
                app->txt_buff, 5, "0x%2X", ((I2CSensor*)sensor->instance)->current_i2c_adress >> 1);
            variable_item_set_current_value_index(
                i2c_addr_item,
                (((I2CSensor*)sensor->instance)->current_i2c_adress >> 1) -
                    (((I2CSensor*)sensor->instance)->min_i2c_adress >> 1));
            variable_item_set_current_value_text(i2c_addr_item, app->txt_buff);
        } else {
            variable_item_set_values_count(i2c_addr_item, 1);
            variable_item_set_current_value_text(i2c_addr_item, "Scan");
            variable_item_set_current_value_index(i2c_addr_item, 0);
        }
    }
    //Sensor connection port (for one wire, SPI and single wire)
    if(sensor->model->interface == &unitemp_1w ||
       sensor->model->interface == &unitemp_singlewire ||
       sensor->model->interface == &unitemp_spi) {
        const SensorGpioPin* gpio_pin = NULL;
        if(sensor->model->interface == &unitemp_1w) {
            gpio_pin = ((OneWireSensor*)sensor->instance)->bus->bus_pin;
        } else if(sensor->model->interface == &unitemp_singlewire) {
            gpio_pin = ((SingleWireSensor*)sensor->instance)->data_pin;
        } else if(sensor->model->interface == &unitemp_spi) {
            gpio_pin = ((SPISensor*)sensor->instance)->cs_pin;
        }

        uint8_t aviable_gpio_count =
            unitemp_gpio_get_aviable_pin_count(sensor->model->interface, gpio_pin);
        UNITEMP_DEBUG("aviable %d values", aviable_gpio_count);
        gpio_pin_item = variable_item_list_add(
            var_item_list,
            sensor->model->interface == &unitemp_spi ? "CS pin" : "Data pin",
            aviable_gpio_count,
            _gpio_change_callback,
            app);

        uint8_t gpio_index = 0;
        for(uint8_t i = 0; i < aviable_gpio_count; i++) {
            if(unitemp_gpio_get_aviable_pin(sensor->model->interface, i, gpio_pin) == gpio_pin) {
                gpio_index = i;
                break;
            }
        }
        variable_item_set_current_value_index(gpio_pin_item, gpio_index);
        variable_item_set_current_value_text(gpio_pin_item, gpio_pin->name);
    }

    // Device address on the one wire bus (for one wire sensors)
    if(sensor->model->interface == &unitemp_1w) {
        onewire_scan_item = variable_item_list_add(
            var_item_list, "Device ID", 2, _onwire_addr_change_callback, app);
        OneWireSensor* ow_sensor = sensor->instance;
        if(ow_sensor->family_code == 0) {
            variable_item_set_current_value_text(onewire_scan_item, "Scan");
        } else {
            snprintf(
                app->txt_buff,
                10,
                "%02X%02X%02X",
                ow_sensor->deviceID[1],
                ow_sensor->deviceID[2],
                ow_sensor->deviceID[3]);
            variable_item_set_current_value_text(onewire_scan_item, app->txt_buff);
        }
    }

    //Temperature offset
    item = variable_item_list_add(var_item_list, "Temp. offset", 41, _offset_change_callback, app);
    variable_item_set_current_value_index(item, sensor->temperature_offset + 20);

    snprintf(
        app->txt_buff,
        5,
        sensor->temperature_offset == 0 ? "%1.0f" : "%+1.1f",
        (double)(sensor->temperature_offset / 10.0));
    variable_item_set_current_value_text(item, app->txt_buff);

    if(!unitemp_sensor_in_list(sensor)) {
        variable_item_list_add(var_item_list, "Save", 1, NULL, NULL);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, UnitempViewVariableList);
}

bool unitemp_scene_sensor_edit_on_event(void* context, SceneManagerEvent event) {
    UnitempApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == CustomEventOneWireScan) {
            _onewire_scan_event_callback(app);
        } else if(event.event == CustomEventGPIOChanged) {
            _gpio_change_event_callback(app);
        } else if(event.event == CustomEventI2CScan) {
            _i2c_scan_event_callback(app);
        }
        //мегажоский костыль
        //дисплей обновляется только по нажатиям на клавиши влево-вправо. если работать через события, то дисплей обновится на момент нажатия
        //после выполнения события дисплей НЕ ОБНОВЛЯЕТСЯ
        //вот таким способом перерисовываем весь дисплей тогда, когда событие выполнено
        //хоть бы этот костыль не пизданул с очередным обновлением...
        view_dispatcher_switch_to_view(app->view_dispatcher, UnitempViewVariableList);
    }

    return consumed;
}

void unitemp_scene_sensor_edit_on_exit(void* context) {
    UnitempApp* app = context;
    variable_item_list_reset(app->var_item_list);

    Sensor* sensor = app->editable_sensor;

    if(!name_edit) {
        variable_item_list_set_selected_item(app->var_item_list, 0);
        unitemp_sensors_save(app);
    }

    if(sensor != NULL && unitemp_sensor_in_list(sensor)) {
        unitemp_sensor_init(sensor);
    }

    if(sensor != NULL && !unitemp_sensor_in_list(sensor) && !name_edit) {
        unitemp_sensor_free(sensor);
    }
}
