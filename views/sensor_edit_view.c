/*
 * sensor_edit_view.c — sensor configuration editor.
 * CO2 sensors: type selector (PWM/UART) + CO2 offset + calibration.
 * Climate sensors: sensor selector + temp offset + GPIO/I2C + unit settings.
 */
#include "../air_stats_i.h"
#include <gui/modules/variable_item_list.h>
#include "../sensors/unitemp/interfaces/SingleWireSensor.h"
#include "../sensors/unitemp/interfaces/OneWireSensor.h"
#include "../sensors/unitemp/interfaces/I2CSensor.h"

static View* view;
static VariableItemList* variable_item_list;
static Sensor* editable_sensor;
static const GPIO* initial_gpio = NULL;

static VariableItem* onewire_addr_item;

/* CO2 */
static VariableItem* co2_type_item;
static VariableItem* co2_alert_edit_item;
static const char co2_type_names[2][5] = {"PWM", "UART"};
#define CO2_OFFSET_STEPS    41   /* -1000..+1000 ppm, step 50 */
#define CO2_OFFSET_CENTER   20   /* index 20 = 0 ppm */
#define CO2_OFFSET_STEP_PPM 50
static char co2_offset_buff[8];
static char co2_alert_buff[12];

/* Climate — sensor selector */
static VariableItem* climate_sensor_item;
#define MAX_CLIMATE_TYPES 32
static const SensorType* climate_types[MAX_CLIMATE_TYPES];
static uint8_t climate_types_count = 0;
static uint8_t climate_selected_idx = 0;

/* Climate — units */
static VariableItem* temp_unit_item;
static VariableItem* pressure_unit_item;
static VariableItem* humidity_unit_item;
static VariableItem* heat_index_item;
static const char temp_units[UT_TEMP_COUNT][3]          = {"*C", "*F"};
static const char humidity_units[UT_HUMIDITY_COUNT][12] = {"Relative", "Dewpoint"};
static const char pressure_units[UT_PRESSURE_COUNT][6]  = {"mmHg", "inHg", "kPa", "hPa"};
static const char heat_index_bool[2][4]                 = {"OFF", "ON"};

#define OFFSET_BUFF_SIZE 5
static char* offset_buff;

/* Computed in view_sensor_edit_switch, used in _enter_callback */
static uint8_t onewire_scan_item_index = 4;
static uint8_t eject_item_index = 0xFF; /* 0xFF = not present */

#define VIEW_ID ViewSensorEdit

/* --- OneWire helpers --- */

static bool _onewire_id_exist(uint8_t* id) {
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
}

/* --- CO2 hotswap --- */

static void _climate_hotswap(uint8_t new_idx, uint8_t old_idx);

static void _co2_hotswap(Co2SensorType new_type) {
    for(uint8_t i = 0; i < app->sensors_count; i++) {
        Sensor* si = app->sensors[i];
        if(si->type == &MHZ19C || si->type == &MHZ19C_UART) {
            si->status = UT_SENSORSTATUS_INACTIVE;
        }
        /* UART uses pins 15/16 (same as I2C) — silently disable I2C climate sensors */
        if(new_type == CO2_TYPE_UART &&
           !(si->type->datatype & UT_CO2) &&
           si->type->interface == &I2C) {
            si->status = UT_SENSORSTATUS_INACTIVE;
        }
    }
    const SensorType* stype = (new_type == CO2_TYPE_UART) ? &MHZ19C_UART : &MHZ19C;
    char sname[11];
    snprintf(sname, sizeof(sname), "MH-Z19C");
    Sensor* new_co2 = unitemp_sensor_alloc(sname, stype, "");
    if(new_co2) unitemp_sensors_add(new_co2);

    if(new_type == CO2_TYPE_PWM) {
        /* Pins 15/16 now free — restore climate sensor */
        _climate_hotswap(app->settings.climate_type_idx, app->settings.climate_type_idx);
        return;
    }
    unitemp_sensors_save();
    unitemp_sensors_reload();
}

/* --- Climate hotswap --- */

static void _climate_hotswap(uint8_t new_idx, uint8_t old_idx) {
    if(new_idx >= climate_types_count) return;
    const SensorType* new_type = climate_types[new_idx];

    /* Preserve temp_offset from current active climate sensor */
    int16_t preserved_offset = 0;
    for(uint8_t i = 0; i < app->sensors_count; i++) {
        if(!(app->sensors[i]->type->datatype & UT_CO2) &&
           app->sensors[i]->status != UT_SENSORSTATUS_INACTIVE) {
            preserved_offset = app->sensors[i]->temp_offset;
            break;
        }
    }

    for(uint8_t i = 0; i < app->sensors_count; i++) {
        if(!(app->sensors[i]->type->datatype & UT_CO2)) {
            app->sensors[i]->status = UT_SENSORSTATUS_INACTIVE;
        }
    }

    char args[22] = {0};
    if(new_type->interface == &SINGLE_WIRE || new_type->interface == &SPI) {
        const GPIO* gpio = unitemp_gpio_getAviablePort(new_type->interface, 0, NULL);
        if(gpio) snprintf(args, sizeof(args), "%d", unitemp_gpio_toInt(gpio));
    }

    char sname[11];
    snprintf(sname, sizeof(sname), "%s", new_type->typename);
    Sensor* s = unitemp_sensor_alloc(sname, new_type, args);
    if(!s) {
        for(uint8_t i = 0; i < app->sensors_count; i++) {
            if(!(app->sensors[i]->type->datatype & UT_CO2)) {
                app->sensors[i]->status = UT_SENSORSTATUS_ERROR;
            }
        }
        app->settings.climate_type_idx = old_idx;
        unitemp_saveSettings();
        return;
    }

    s->temp_offset = preserved_offset;
    app->settings.climate_type_idx = new_idx;
    unitemp_sensors_add(s);
    unitemp_sensors_save();
    unitemp_sensors_reload();
}

/* --- Callbacks --- */

static void _co2_type_change(VariableItem* item) {
    variable_item_set_current_value_text(
        item, co2_type_names[variable_item_get_current_value_index(item)]);
}

static void _co2_avg_change_callback(VariableItem* item) {
    uint8_t val = (uint8_t)variable_item_get_current_value_index(item) + 1;
    editable_sensor->co2_avg = val;
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", val);
    variable_item_set_current_value_text(item, buf);
}

static void _co2_alert_edit_change(VariableItem* item) {
    uint8_t idx = (uint8_t)variable_item_get_current_value_index(item);
    uint16_t val = (uint16_t)(800 + idx * 50);
    app->settings.co2_alert_threshold = val;
    snprintf(co2_alert_buff, sizeof(co2_alert_buff), "%d ppm", val);
    variable_item_set_current_value_text(item, co2_alert_buff);
}

static char range_buf[8];
static void _co2_range_change_callback(VariableItem* item) {
    uint8_t idx = (uint8_t)variable_item_get_current_value_index(item);
    uint16_t range = (uint16_t)(2000 + idx * 1000);
    app->settings.co2_pwm_range = range;
    snprintf(range_buf, sizeof(range_buf), "%d", range);
    variable_item_set_current_value_text(item, range_buf);
}

static void _co2_offset_change_callback(VariableItem* item) {
    int16_t val = ((int16_t)variable_item_get_current_value_index(item) - CO2_OFFSET_CENTER)
                  * CO2_OFFSET_STEP_PPM;
    editable_sensor->co2_offset = val;
    snprintf(co2_offset_buff, sizeof(co2_offset_buff), "%+d", (int)val);
    variable_item_set_current_value_text(item, co2_offset_buff);
}

static void _climate_sensor_change(VariableItem* item) {
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx < climate_types_count) {
        const char* name = climate_types[idx]->altname
                               ? climate_types[idx]->altname
                               : climate_types[idx]->typename;
        variable_item_set_current_value_text(item, name);
        climate_selected_idx = idx;
        view_dispatcher_send_custom_event(app->view_dispatcher, 0);
    }
}

static void _unit_change(VariableItem* item) {
    if(item == temp_unit_item) {
        variable_item_set_current_value_text(
            item, temp_units[variable_item_get_current_value_index(item)]);
    } else if(item == pressure_unit_item) {
        variable_item_set_current_value_text(
            item, pressure_units[variable_item_get_current_value_index(item)]);
    } else if(item == humidity_unit_item) {
        variable_item_set_current_value_text(
            item, humidity_units[variable_item_get_current_value_index(item)]);
    } else if(item == heat_index_item) {
        variable_item_set_current_value_text(
            item, heat_index_bool[variable_item_get_current_value_index(item)]);
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

/* --- Build climate items (called on init and sensor dropdown change) --- */

static void _build_climate_items(uint8_t sel_idx, SensorDataType unit_dt) {
    temp_unit_item     = NULL;
    pressure_unit_item = NULL;
    humidity_unit_item = NULL;
    heat_index_item    = NULL;

    variable_item_list_reset(variable_item_list);
    variable_item_list_set_selected_item(variable_item_list, 0);

    uint8_t idx = 0;

    /* Sensor selector */
    climate_sensor_item = variable_item_list_add(
        variable_item_list, "Sensor", climate_types_count, _climate_sensor_change, app);
    variable_item_set_current_value_index(climate_sensor_item, sel_idx);
    const char* sname = climate_types[sel_idx]->altname
                            ? climate_types[sel_idx]->altname
                            : climate_types[sel_idx]->typename;
    variable_item_set_current_value_text(climate_sensor_item, sname);
    idx++;

    /* Temp offset */
    if(editable_sensor->type->datatype & UT_TEMPERATURE) {
        VariableItem* temp_offset_item = variable_item_list_add(
            variable_item_list, "Temp. offset", 201, _offset_change_callback, NULL);
        variable_item_set_current_value_index(
            temp_offset_item, editable_sensor->temp_offset + 100);
        snprintf(
            offset_buff, OFFSET_BUFF_SIZE, "%+1.1f",
            (double)(editable_sensor->temp_offset / 10.0));
        variable_item_set_current_value_text(temp_offset_item, offset_buff);
        idx++;
    }

    /* GPIO for SingleWire, SPI, OneWire */
    if(editable_sensor->type->interface == &ONE_WIRE ||
       editable_sensor->type->interface == &SINGLE_WIRE ||
       editable_sensor->type->interface == &SPI) {
        uint8_t aviable_gpio_count =
            unitemp_gpio_getAviablePortsCount(editable_sensor->type->interface, initial_gpio);
        VariableItem* item = variable_item_list_add(
            variable_item_list, "GPIO", aviable_gpio_count, _gpio_change_callback, app);

        uint8_t gpio_index = 0;
        if(unitemp_sensor_isContains(editable_sensor)) {
            for(uint8_t i = 0; i < aviable_gpio_count; i++) {
                if(unitemp_gpio_getAviablePort(
                       editable_sensor->type->interface, i, initial_gpio) == initial_gpio) {
                    gpio_index = i;
                    break;
                }
            }
        }
        variable_item_set_current_value_index(item, gpio_index);
        variable_item_set_current_value_text(
            item,
            unitemp_gpio_getAviablePort(
                editable_sensor->type->interface, gpio_index, initial_gpio)->name);
        idx++;
    }

    /* I2C address */
    if(editable_sensor->type->interface == &I2C) {
        VariableItem* item = variable_item_list_add(
            variable_item_list,
            "I2C address",
            (((I2CSensor*)editable_sensor->instance)->maxI2CAdr >> 1) -
                (((I2CSensor*)editable_sensor->instance)->minI2CAdr >> 1) + 1,
            _i2caddr_change_callback,
            app);
        snprintf(
            app->buff, 5, "0x%2X",
            ((I2CSensor*)editable_sensor->instance)->currentI2CAdr >> 1);
        variable_item_set_current_value_index(
            item,
            (((I2CSensor*)editable_sensor->instance)->currentI2CAdr >> 1) -
                (((I2CSensor*)editable_sensor->instance)->minI2CAdr >> 1));
        variable_item_set_current_value_text(item, app->buff);
        idx++;
    }

    /* OneWire address scan */
    if(editable_sensor->type->interface == &ONE_WIRE) {
        onewire_scan_item_index = idx;
        onewire_addr_item       = variable_item_list_add(
            variable_item_list, "Address", 2, _onwire_addr_change_callback, NULL);
        OneWireSensor* ow_sensor = editable_sensor->instance;
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

    /* Calibrate */
    if((editable_sensor->type->datatype & UT_CALIBRATION) == UT_CALIBRATION) {
        VariableItem* calibration_item = variable_item_list_add(
            variable_item_list, "Calibrate", 1, _calibrate_callback, NULL);
        (void)calibration_item;
        idx++;
    }

    /* Unit settings — conditional per unit_dt */
    temp_unit_item = variable_item_list_add(
        variable_item_list, "Temperature", UT_TEMP_COUNT, _unit_change, app);
    variable_item_set_current_value_index(temp_unit_item, (uint8_t)app->settings.temp_unit);
    variable_item_set_current_value_text(temp_unit_item, temp_units[app->settings.temp_unit]);
    idx++;

    if(unit_dt & UT_HUMIDITY) {
        humidity_unit_item = variable_item_list_add(
            variable_item_list, "Humidity", UT_HUMIDITY_COUNT, _unit_change, app);
        variable_item_set_current_value_index(
            humidity_unit_item, (uint8_t)app->settings.humidity_unit);
        variable_item_set_current_value_text(
            humidity_unit_item, humidity_units[app->settings.humidity_unit]);
        idx++;
    }

    if(unit_dt & UT_PRESSURE) {
        pressure_unit_item = variable_item_list_add(
            variable_item_list, "Pressure", UT_PRESSURE_COUNT, _unit_change, app);
        variable_item_set_current_value_index(
            pressure_unit_item, (uint8_t)app->settings.pressure_unit);
        variable_item_set_current_value_text(
            pressure_unit_item, pressure_units[app->settings.pressure_unit]);
        idx++;
    }

    if((unit_dt & (UT_TEMPERATURE | UT_HUMIDITY)) == (UT_TEMPERATURE | UT_HUMIDITY)) {
        heat_index_item = variable_item_list_add(
            variable_item_list, "Heat index", 2, _unit_change, app);
        variable_item_set_current_value_index(
            heat_index_item, app->settings.heat_index ? 1 : 0);
        variable_item_set_current_value_text(
            heat_index_item, heat_index_bool[app->settings.heat_index ? 1 : 0]);
        idx++;
    }

    (void)idx;
}

static bool _view_custom_event(uint32_t event, void* context) {
    UNUSED(context);
    UNUSED(event);
    if(temp_unit_item)
        app->settings.temp_unit =
            (tempMeasureUnit)variable_item_get_current_value_index(temp_unit_item);
    if(humidity_unit_item)
        app->settings.humidity_unit =
            (humidityUnit)variable_item_get_current_value_index(humidity_unit_item);
    if(pressure_unit_item)
        app->settings.pressure_unit =
            (pressureMeasureUnit)variable_item_get_current_value_index(pressure_unit_item);
    if(heat_index_item)
        app->settings.heat_index =
            (bool)variable_item_get_current_value_index(heat_index_item);

    _build_climate_items(
        climate_selected_idx, climate_types[climate_selected_idx]->datatype);
    return true;
}

/* --- Exit callback --- */

static uint32_t _exit_callback(void* context) {
    UNUSED(context);
    if(!editable_sensor) return ViewSensorActions;

    bool is_co2 = (editable_sensor->type->datatype & UT_CO2) != 0;

    if(is_co2) {
        Co2SensorType actual_type =
            (editable_sensor->type == &MHZ19C_UART) ? CO2_TYPE_UART : CO2_TYPE_PWM;
        Co2SensorType selected =
            (Co2SensorType)variable_item_get_current_value_index(co2_type_item);
        if(selected != actual_type) {
            app->settings.co2_type = selected;
            unitemp_saveSettings();
            _co2_hotswap(selected);
            return ViewMainMenu;
        }
        editable_sensor->status = UT_SENSORSTATUS_TIMEOUT;
        unitemp_saveSettings();
        unitemp_sensors_save();
        return ViewSensorActions;
    }

    /* Climate branch */

    /* ONE_WIRE without address — discard */
    if(editable_sensor->type->interface == &ONE_WIRE &&
       ((OneWireSensor*)(editable_sensor->instance))->familyCode == 0) {
        if(!unitemp_sensor_isContains(editable_sensor)) unitemp_sensor_free(editable_sensor);
        return ViewSensorActions;
    }

    if(initial_gpio != NULL) {
        unitemp_gpio_unlock(initial_gpio);
        initial_gpio = NULL;
    }

    /* Save unit settings */
    if(temp_unit_item)
        app->settings.temp_unit =
            (tempMeasureUnit)variable_item_get_current_value_index(temp_unit_item);
    if(humidity_unit_item)
        app->settings.humidity_unit =
            (humidityUnit)variable_item_get_current_value_index(humidity_unit_item);
    if(pressure_unit_item)
        app->settings.pressure_unit =
            (pressureMeasureUnit)variable_item_get_current_value_index(pressure_unit_item);
    if(heat_index_item)
        app->settings.heat_index =
            (bool)variable_item_get_current_value_index(heat_index_item);

    uint8_t new_idx = (uint8_t)variable_item_get_current_value_index(climate_sensor_item);
    uint8_t old_idx = app->settings.climate_type_idx;

    if(new_idx != old_idx) {
        unitemp_saveSettings();
        unitemp_loadSettings();
        _climate_hotswap(new_idx, old_idx);
        return ViewMainMenu;
    }

    editable_sensor->status = UT_SENSORSTATUS_TIMEOUT;
    if(!unitemp_sensor_isContains(editable_sensor)) unitemp_sensors_add(editable_sensor);
    unitemp_saveSettings();
    unitemp_loadSettings();
    unitemp_sensors_save();
    app->sensors_ready = false;
    app->sensors_update = true;
    return ViewSensorActions;
}

/* --- Enter callback --- */

static void _enter_callback(void* context, uint32_t index) {
    UNUSED(context);
    if(editable_sensor->type->interface == &ONE_WIRE && index == onewire_scan_item_index) {
        _onewire_scan();
    }
    if(eject_item_index != 0xFF && index == eject_item_index) {
        if(editable_sensor) {
            editable_sensor->needs_reset = true;
        }
    }
}

/* --- Alloc --- */

void view_sensor_edit_alloc(void) {
    /* Build filtered list: all sensor types without CO2 data */
    climate_types_count = 0;
    uint8_t total          = unitemp_sensors_getTypesCount();
    const SensorType** all = unitemp_sensors_getTypes();
    for(uint8_t i = 0; i < total && climate_types_count < MAX_CLIMATE_TYPES; i++) {
        if(!(all[i]->datatype & UT_CO2)) climate_types[climate_types_count++] = all[i];
    }

    variable_item_list = variable_item_list_alloc();
    variable_item_list_reset(variable_item_list);
    variable_item_list_set_enter_callback(variable_item_list, _enter_callback, app);
    view = variable_item_list_get_view(variable_item_list);
    view_set_previous_callback(view, _exit_callback);
    view_set_custom_callback(view, _view_custom_event);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID, view);
    offset_buff = malloc(OFFSET_BUFF_SIZE);
}

/* --- Switch --- */

void view_sensor_edit_switch(Sensor* sensor) {
    editable_sensor = sensor;
    editable_sensor->status = UT_SENSORSTATUS_INACTIVE;
    initial_gpio = NULL;
    eject_item_index = 0xFF;

    variable_item_list_reset(variable_item_list);
    variable_item_list_set_selected_item(variable_item_list, 0);

    uint8_t idx = 0;
    bool is_co2 = (sensor->type->datatype & UT_CO2) != 0;

    if(is_co2) {
        /* CO2 Type selector — show actual running type, not settings */
        Co2SensorType actual_co2_type =
            (sensor->type == &MHZ19C_UART) ? CO2_TYPE_UART : CO2_TYPE_PWM;
        co2_type_item = variable_item_list_add(
            variable_item_list, "CO2 Type", 2, _co2_type_change, app);
        variable_item_set_current_value_index(co2_type_item, (uint8_t)actual_co2_type);
        variable_item_set_current_value_text(co2_type_item, co2_type_names[actual_co2_type]);
        idx++;

        VariableItem* co2_offset_item = variable_item_list_add(
            variable_item_list, "CO2 offset", CO2_OFFSET_STEPS, _co2_offset_change_callback, NULL);
        uint8_t co2_idx = (uint8_t)(sensor->co2_offset / CO2_OFFSET_STEP_PPM + CO2_OFFSET_CENTER);
        variable_item_set_current_value_index(co2_offset_item, co2_idx);
        snprintf(co2_offset_buff, sizeof(co2_offset_buff), "%+d", (int)sensor->co2_offset);
        variable_item_set_current_value_text(co2_offset_item, co2_offset_buff);
        idx++;

        /* CO2 Alert — for both PWM and UART */
        co2_alert_edit_item = variable_item_list_add(
            variable_item_list, "CO2 Alert", 85, _co2_alert_edit_change, NULL);
        uint8_t alert_idx = (app->settings.co2_alert_threshold >= 800) ?
            (uint8_t)((app->settings.co2_alert_threshold - 800) / 50) : 0;
        if(alert_idx > 84) alert_idx = 84;
        variable_item_set_current_value_index(co2_alert_edit_item, alert_idx);
        snprintf(co2_alert_buff, sizeof(co2_alert_buff), "%d ppm", app->settings.co2_alert_threshold);
        variable_item_set_current_value_text(co2_alert_edit_item, co2_alert_buff);
        idx++;

        if(actual_co2_type == CO2_TYPE_PWM) {
            VariableItem* avg_item = variable_item_list_add(
                variable_item_list, "Avg points", 30, _co2_avg_change_callback, NULL);
            variable_item_set_current_value_index(avg_item, sensor->co2_avg - 1);
            char avg_buf[4];
            snprintf(avg_buf, sizeof(avg_buf), "%d", sensor->co2_avg);
            variable_item_set_current_value_text(avg_item, avg_buf);
            idx++;

            VariableItem* range_item = variable_item_list_add(
                variable_item_list, "CO2 Range", 9, _co2_range_change_callback, NULL);
            uint8_t range_idx = (app->settings.co2_pwm_range >= 2000) ?
                (uint8_t)((app->settings.co2_pwm_range - 2000) / 1000) : 0;
            if(range_idx > 8) range_idx = 8;
            variable_item_set_current_value_index(range_item, range_idx);
            snprintf(range_buf, sizeof(range_buf), "%d", app->settings.co2_pwm_range);
            variable_item_set_current_value_text(range_item, range_buf);
            idx++;

            VariableItem* eject_item = variable_item_list_add(
                variable_item_list, "Eject", 1, NULL, NULL);
            variable_item_set_current_value_text(eject_item, "Press OK");
            eject_item_index = idx;
            idx++;
        }

        if((sensor->type->datatype & UT_CALIBRATION) == UT_CALIBRATION) {
            VariableItem* calibration_item = variable_item_list_add(
                variable_item_list, "Calibrate", 1, _calibrate_callback, NULL);
            (void)calibration_item;
            idx++;
        }
    } else {
        /* Detect cur_idx from active sensors */
        uint8_t cur_idx = app->settings.climate_type_idx;
        for(uint8_t i = 0; i < app->sensors_count; i++) {
            if(app->sensors[i] && !(app->sensors[i]->type->datatype & UT_CO2)) {
                for(uint8_t j = 0; j < climate_types_count; j++) {
                    if(climate_types[j] == app->sensors[i]->type) {
                        cur_idx = j;
                        break;
                    }
                }
                break;
            }
        }
        if(cur_idx >= climate_types_count) cur_idx = 0;
        app->settings.climate_type_idx = cur_idx;
        climate_selected_idx           = cur_idx;

        /* Set initial_gpio before building (needed for GPIO items) */
        if(sensor->type->interface == &ONE_WIRE)
            initial_gpio = ((OneWireSensor*)editable_sensor->instance)->bus->gpio;
        else if(sensor->type->interface == &SINGLE_WIRE)
            initial_gpio = ((SingleWireSensor*)editable_sensor->instance)->gpio;
        else if(sensor->type->interface == &SPI)
            initial_gpio = ((SPISensor*)editable_sensor->instance)->CS_pin;

        _build_climate_items(cur_idx, sensor->type->datatype);
    }

    (void)idx;
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID);
}

/* --- Free --- */

void view_sensor_edit_free(void) {
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID);
    variable_item_list_free(variable_item_list);
    free(offset_buff);
}
