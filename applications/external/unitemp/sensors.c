#include "sensors.h"
#include "unitemp.h"

#include "./interfaces/i2c_sensor.h"
#include "./interfaces/onewire_sensor.h"
#include "./interfaces/singlewire_sensor.h"
#include "./interfaces/spi_sensor.h"

#include "sensors/DHTxx.h"
#include "sensors/AM2320.h"
#include "./sensors/LM75.h"
//BMP280, BME280, BME680
#include "./sensors/BMx280.h"
#include "./sensors/BME680.h"
#include "./sensors/SHT3x.h"
#include "./sensors/SHT4x.h"
#include "./sensors/BMP180.h"
#include "./sensors/HTU21x.h"
#include "./sensors/HDC1080.h"
#include "./sensors/HDC2080.h"
#include "./sensors/MAX31855.h"
#include "./sensors/MAX6675.h"
#include "./sensors/DS18x2x.h"
#include "./sensors/SCD30.h"
#include "./sensors/MAX31725.h"
#include "./sensors/SCD4x.h"
#include "./sensors/TMP102.h"
#include "./sensors/SHTC3.h"

#define DISPLAY_UPDATE_PERIOD_MS 250UL
#define APP_SENSORS_FILENAME     "sensors.list"

static Sensor** sensors_list = NULL;
//Number of loaded sensors
static uint8_t sensors_count = 0;

//List of sensor models
static const SensorModel* sensor_model_list[] = {
    &AHT10, //tested
    &AHT20, //tested
    &AM2320_SW, //tested
    &AM2320_I2C, //tested
    &BMP180, //tested
    &BMP280,     &BME280,
    &BME680, //tested
    &DHT11, //tested
    &DHT20, //tested
    &DHT21, //tested
    &DHT22, //tested
    &Dallas, //tested
    &GXHT30, //tested
    &HDC1080, //tested
    &HDC2080, //tested
    &HTU21x, //tested
    &LM75, //tested
    &MAX6675, //tested
    &MAX31725,
    &MAX31855, //tested
    &SCD30, //tested
    &SCD4x, //tested
    &SHT2x, //tested
    &SHT3x, //tested
    &SHT4x, //tested
    &SHTC3, //tested
    &SI7021, //tested
    &TMP102, //tested

};
//Number of sensor models
#define SENSOR_MODELS_COUNT (int)(sizeof(sensor_model_list) / sizeof(const SensorModel*))

Sensor* unitemp_sensor_alloc(char* name, const SensorModel* model, char* args) {
    if(name == NULL || model == NULL || args == NULL) return NULL;

    bool status = false;

    //Allocation of memory for the sensor
    Sensor* sensor = malloc(sizeof(Sensor));
    if(sensor == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor %s allocation error", name);
        return NULL;
    }

    //Allocating memory for a name
    sensor->name = malloc(11);
    if(sensor->name == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor %s name allocation error", name);
        return NULL;
    }
    //Recording the sensor name
    strcpy(sensor->name, name);
    //Sensor model
    sensor->model = model;
    //Status sensor by default
    sensor->status = UT_SENSORSTATUS_UNINITIALIZED;
    //Time of last poll
    sensor->last_polling_time =
        furi_get_tick() - 10000; //so that the first survey occurs as early as possible

    sensor->temperature = -128.0f;
    sensor->humidity = -128.0f;
    sensor->pressure = -128.0f;
    sensor->temperature_offset = 0;
    //Memory allocation for a sensor instance depending on its interface
    status = sensor->model->interface->allocator(sensor, args);

    //Exit if the sensor is successfully deployed
    if(status) {
        UNITEMP_DEBUG("Sensor %s allocated", name);
        return sensor;
    }
    //Exit with clearing if memory for the sensor has not been allocated
    free(sensor->name);
    free(sensor);
    FURI_LOG_E(APP_NAME, "Sensor %s(%s) allocation error", name, model->modelname);
    return NULL;
}

void unitemp_sensor_free(Sensor* sensor) {
    if(sensor == NULL) {
        FURI_LOG_E(APP_NAME, "Null pointer sensor releasing");
        return;
    }
    if(sensor->model == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor model is null");
        return;
    }
    if(sensor->model->mem_releaser == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor releaser is null");
        return;
    }
    bool status = false;
    //Freeing up memory for an instance
    status = sensor->model->interface->mem_releaser(sensor);

    if(status) {
        UNITEMP_DEBUG("Sensor %s memory successfully released", sensor->name);
    } else {
        FURI_LOG_E(APP_NAME, "Sensor %s memory is not released", sensor->name);
    }
    free(sensor->name);
    free(sensor);
}

bool unitemp_sensor_delete(Sensor* sensor) {
    if(sensor == NULL) {
        FURI_LOG_E(APP_NAME, "Null pointer sensor deleting");
        return false;
    }

    uint8_t index_to_remove = 255;
    for(uint8_t i = 0; i < sensors_count; i++) {
        if(sensors_list[i] == sensor) {
            index_to_remove = i;
            break;
        }
    }
    if(index_to_remove >= sensors_count) {
        FURI_LOG_E(APP_NAME, "Sensor %s not found in sensor list", sensor->name);
        return false;
    }

    unitemp_sensor_deinit(sensor);
    unitemp_sensor_free(sensor);

    memmove(
        sensors_list + index_to_remove,
        sensors_list + index_to_remove + 1,
        (sensors_count - index_to_remove - 1) * sizeof(Sensor*));

    sensors_count--;

    if(sensors_count != 0) {
        sensors_list = (Sensor**)realloc(sensors_list, (sensors_count) * sizeof(Sensor*));
        if(sensors_list == NULL) {
            FURI_LOG_E(APP_NAME, "Failed to reallocate memory");
            return false;
        }
    } else {
        free(sensors_list);
    }
    UNITEMP_DEBUG("Sensor successfully deleted");
    return true;
}

bool unitemp_sensor_init(Sensor* sensor) {
    bool result = sensor->model->initializer(sensor);
    if(result) {
        UNITEMP_DEBUG("Sensor %s successfully initialized", sensor->name);
        sensor->status = UT_SENSORSTATUS_INITIALIZED;
    } else {
        UNITEMP_DEBUG("Sensor %s initialization failed", sensor->name);
        sensor->status = UT_SENSORSTATUS_UNINITIALIZED;
    }
    return result;
}

bool unitemp_sensor_deinit(Sensor* sensor) {
    if(sensor->status == UT_SENSORSTATUS_UNINITIALIZED) {
        FURI_LOG_W(APP_NAME, "Sensor %s is already uninitialized!", sensor->name);
        return true;
    }
    bool result = sensor->model->deinitializer(sensor);
    sensor->status = UT_SENSORSTATUS_UNINITIALIZED;
    if(result) {
        UNITEMP_DEBUG("Sensor %s successfully deinitialized", sensor->name);
    } else {
        UNITEMP_DEBUG("Sensor %s deinitialization failed", sensor->name);
    }
    return result;
}

SensorStatus unitemp_sensor_update(Sensor* sensor, void* context) {
    if(sensor == NULL || context == NULL) {
        return UT_SENSORSTATUS_ERROR;
    }
    if(sensor->status == UT_SENSORSTATUS_INACTIVE) {
        return UT_SENSORSTATUS_INACTIVE;
    }

    UnitempApp* app = context;

    //Checking the validity of the sensor polling
    if(furi_get_tick() - sensor->last_polling_time < sensor->model->polling_interval) {
        //Return an error if the last sensor poll was unsuccessful
        if(sensor->status == UT_SENSORSTATUS_TIMEOUT) {
            return UT_SENSORSTATUS_TIMEOUT;
        }
        return UT_SENSORSTATUS_EARLYPOOL;
    }
    sensor->last_polling_time = furi_get_tick();

    if(sensor->status == UT_SENSORSTATUS_UNINITIALIZED) {
        UNITEMP_DEBUG("Attempting to initialize the sensor %s", sensor->name);
        if(!unitemp_sensor_init(sensor)) {
            return UT_SENSORSTATUS_UNINITIALIZED;
        }
    }

    if(app->settings->otg_auto_on && !power_is_otg_enabled(app->power)) {
        power_enable_otg(app->power, true);
    }

    //Если датчик дважды не ответил, то он переводится в неинициализированные (требуется для BME* и SDC30)
    SensorStatus status = sensor->model->interface->updater(sensor);
    if(status == UT_SENSORSTATUS_TIMEOUT && sensor->status == UT_SENSORSTATUS_TIMEOUT) {
        unitemp_sensor_deinit(sensor);
        FURI_LOG_W(
            APP_NAME, "Sensor %s not responding and was moved to uninitialized", sensor->name);
    } else {
        sensor->status = status;
    }

    if(sensor->status != UT_SENSORSTATUS_OK && sensor->status != UT_SENSORSTATUS_POLLING) {
        FURI_LOG_W(APP_NAME, "Sensor %s update status %d", sensor->name, sensor->status);
    } else {
        UNITEMP_DEBUG(
            "Sensor %s successfully updated. Values: temp=%.2f, hum=%.2f, pres=%.2f, co=%.2f",
            sensor->name,
            (double)sensor->temperature,
            (double)sensor->humidity,
            (double)sensor->pressure,
            (double)sensor->co2);
    }

    if(sensor->status == UT_SENSORSTATUS_OK) {
        sensor->temperature += sensor->temperature_offset / 10.f;
    }
    return sensor->status;
}

bool unitemp_sensor_in_list(Sensor* sensor) {
    for(uint8_t i = 0; i < unitemp_sensors_get_count(); i++) {
        if(sensors_list[i] == sensor) return true;
    }
    return false;
}

/* Periodically requests measurements and reads temperature. This function runs in a separare thread. */
int32_t unitemp_sensors_update_callback(void* context) {
    furi_check(context);

    UnitempApp* app = context;

    for(;;) {
        for(uint8_t i = 0; i < unitemp_sensors_get_count(); i++) {
            Sensor* sensor = unitemp_sensors_get(i);
            unitemp_sensor_update(sensor, app);
        }

        const uint32_t flags = furi_thread_flags_wait(
            UnitempThreadFlagExit, FuriFlagWaitAny, DISPLAY_UPDATE_PERIOD_MS);

        /* If an exit signal was received, return from this thread. */
        if(flags != (unsigned)FuriFlagErrorTimeout) break;
    }
    return 0;
}

void unitemp_sensors_free(void) {
    for(uint8_t i = 0; i < sensors_count; i++) {
        unitemp_sensor_free(sensors_list[i]);
    }
    free(sensors_list);
    sensors_count = 0;
}

uint8_t unitemp_sensors_get_count(void) {
    if(sensors_list == NULL) return 0;
    return sensors_count;
}

bool unitemp_sensors_load(void* context) {
    if(context == NULL) return false;
    UnitempApp* app = context;
    UNITEMP_DEBUG("Loading sensors...");

    app->file_stream = file_stream_alloc(app->storage);

    bool success = false;
    bool migration = false;

    do {
        if(!file_stream_open(
               app->file_stream,
               APP_DATA_PATH(APP_SENSORS_FILENAME),
               FSAM_READ,
               FSOM_OPEN_EXISTING)) {
            if(file_stream_get_error(app->file_stream) == FSE_NOT_EXIST) {
                file_stream_close(app->file_stream);
                FURI_LOG_W(
                    APP_NAME,
                    "Missing sensors file. Trying to read a file in the old directory...");
                if(!file_stream_open(
                       app->file_stream,
                       "/ext/unitemp/sensors.cfg",
                       FSAM_READ,
                       FSOM_OPEN_EXISTING)) {
                    FURI_LOG_W(APP_NAME, "The old sensor file is missing");
                    file_stream_close(app->file_stream);
                    break;
                }
                migration = true;
            } else {
                FURI_LOG_E(
                    APP_NAME,
                    "An error occurred while loading the sensors file: %d",
                    file_stream_get_error(app->file_stream));
                file_stream_close(app->file_stream);
                break;
            }
        }

        uint16_t file_size = stream_size(app->file_stream);
        //If the file is empty, then:
        if(file_size == (uint8_t)0) {
            FURI_LOG_W(APP_NAME, "Sensors file is empty");
            file_stream_close(app->file_stream);
            break;
        }

        FuriString* line = furi_string_alloc();

        while(stream_read_line(app->file_stream, line)) {
            furi_string_trim(line);
            if(furi_string_empty(line)) continue;

            char name[11] = {0};
            char model[11] = {0};
            int temp_offset = 0;
            int str_offset;

            const char* line_cstr = furi_string_get_cstr(line);
            sscanf(line_cstr, "%s %s %d %n", name, model, &temp_offset, &str_offset);
            char* args = ((char*)(line_cstr + str_offset));

            //Replacement ?
            for(uint8_t i = 0; i < 10; i++) {
                if(name[i] == '?') name[i] = ' ';
            }

            UNITEMP_DEBUG(
                "Name: %s, model: %s, offset: %d, args: %s", name, model, temp_offset, args);

            const SensorModel* sensor_model = unitemp_sensors_get_model_from_str(model);

            //Checking the sensor model
            if(sensor_model != NULL && sizeof(name) > 0 && sizeof(name) <= 11) {
                Sensor* sensor = unitemp_sensor_alloc(name, sensor_model, args);
                if(sensor != NULL) {
                    sensor->temperature_offset = temp_offset;
                    unitemp_sensors_add(sensor);
                } else {
                    FURI_LOG_E(
                        APP_NAME,
                        "Failed sensor (%s:%s) mem allocation",
                        name,
                        sensor_model->modelname);
                }
            } else {
                FURI_LOG_E(
                    APP_NAME, "Unsupported sensor name (%s) or sensor model (%s)", name, model);
                furi_string_free(line);
                file_stream_close(app->file_stream);
                break;
            }
        }
        file_stream_close(app->file_stream);
        furi_string_free(line);
        success = true;
    } while(0);
    stream_free(app->file_stream);

    if(success) {
        FURI_LOG_I(APP_NAME, "Loaded %d sensors from file.", unitemp_sensors_get_count());
    } else {
        FURI_LOG_E(APP_NAME, "Failed to load sensors");
    }
    if(migration) {
        unitemp_sensors_save(app);
        FS_Error error = storage_simply_remove_recursive(app->storage, "/ext/unitemp");
        if(error == FSE_OK) {
            FURI_LOG_I(APP_NAME, "Old Unitemp folder deleted successfully");
        } else {
            FURI_LOG_W(APP_NAME, "Failed to delete old Unitemp folder. Error: %d", error);
        }
    }

    return success;
}

bool unitemp_sensors_save(void* context) {
    if(context == NULL) return false;
    UnitempApp* app = context;
    UNITEMP_DEBUG("Saving sensors...");

    app->file_stream = file_stream_alloc(app->storage);

    //Creating a plugin folder
    storage_common_mkdir(app->storage, APP_DATA_PATH());
    //Opening a stream
    if(!file_stream_open(
           app->file_stream, APP_DATA_PATH(APP_SENSORS_FILENAME), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(
            APP_NAME,
            "An error occurred while saving the sensors file: %d",
            file_stream_get_error(app->file_stream));
        //Closing a stream and freeing memory
        file_stream_close(app->file_stream);
        stream_free(app->file_stream);
        return false;
    }

    //Saving sensors
    for(uint8_t i = 0; i < unitemp_sensors_get_count(); i++) {
        Sensor* sensor = unitemp_sensors_get(i);
        //Replacing a space with ?

        char tmp_sensor_name[11];
        strcpy(tmp_sensor_name, sensor->name);
        for(uint8_t i = 0; i < 10; i++) {
            if(tmp_sensor_name[i] == ' ') tmp_sensor_name[i] = '?';
        }

        stream_write_format(
            app->file_stream,
            "%s %s %d ",
            tmp_sensor_name,
            sensor->model->modelname,
            sensor->temperature_offset);

        if(sensor->model->interface == &unitemp_singlewire) {
            stream_write_format(
                app->file_stream, "%d\n", unitemp_singlewire_sensor_gpio_get(sensor)->num);
        }
        if(sensor->model->interface == &unitemp_spi) {
            uint8_t gpio_num = ((SPISensor*)sensor->instance)->cs_pin->num;
            stream_write_format(app->file_stream, "%d\n", gpio_num);
        }

        if(sensor->model->interface == &unitemp_i2c) {
            stream_write_format(
                app->file_stream, "%X\n", ((I2CSensor*)sensor->instance)->current_i2c_adress);
        }
        if(sensor->model->interface == &unitemp_1w) {
            stream_write_format(
                app->file_stream,
                "%d %02X%02X%02X%02X%02X%02X%02X%02X\n",
                ((OneWireSensor*)sensor->instance)->bus->bus_pin->num,
                ((OneWireSensor*)sensor->instance)->deviceID[0],
                ((OneWireSensor*)sensor->instance)->deviceID[1],
                ((OneWireSensor*)sensor->instance)->deviceID[2],
                ((OneWireSensor*)sensor->instance)->deviceID[3],
                ((OneWireSensor*)sensor->instance)->deviceID[4],
                ((OneWireSensor*)sensor->instance)->deviceID[5],
                ((OneWireSensor*)sensor->instance)->deviceID[6],
                ((OneWireSensor*)sensor->instance)->deviceID[7]);
        }
    }

    //Closing a stream and freeing memory
    file_stream_close(app->file_stream);
    stream_free(app->file_stream);
    FURI_LOG_I(APP_NAME, "Sensors have been successfully saved");
    return true;
}

bool unitemp_sensors_init(void* context) {
    if(context == NULL) return false;
    UnitempApp* app = context;

    bool result = true;

    //Searching through sensors from the list
    for(uint8_t i = 0; i < unitemp_sensors_get_count(); i++) {
        //Turning on 5V if there is none on port 1 FZ
        //May disappear when USB is disconnected
        if(app->settings->otg_auto_on && !power_is_otg_enabled(app->power)) {
            power_enable_otg(app->power, true);
        }

        if(!unitemp_sensor_init(sensors_list[i])) {
            FURI_LOG_E(
                APP_NAME,
                "An error occurred during sensor initialization %s",
                sensors_list[i]->name);

            result = false;
        } else {
            FURI_LOG_I(APP_NAME, "Sensor %s successfully initialized", sensors_list[i]->name);
        }
    }
    return result;
}

bool unitemp_sensors_deinit(void* context) {
    if(context == NULL) return false;
    UnitempApp* app = context;

    bool result = true;

    //Turning off 5 V if it was not turned on before
    power_enable_otg(app->power, app->settings->otg_latest_state);

    //Searching through sensors from the list
    for(uint8_t i = 0; i < unitemp_sensors_get_count(); i++) {
        if(!(unitemp_sensor_deinit(sensors_list[i]))) {
            FURI_LOG_E(
                APP_NAME,
                "An error occurred during sensor deinitialization %s",
                sensors_list[i]->name);
            result = false;
        } else {
            FURI_LOG_I(APP_NAME, "Sensor %s successfully deinitialized", sensors_list[i]->name);
            sensors_list[i]->status = UT_SENSORSTATUS_UNINITIALIZED;
        }
    }

    return result;
}

Sensor* unitemp_sensors_get(uint8_t index) {
    return sensors_list[index];
}

void unitemp_sensors_reload(void* context) {
    unitemp_sensors_deinit(context);
    unitemp_sensors_free();

    unitemp_sensors_load(context);
    unitemp_sensors_init(context);
}

const SensorModel** unitemp_sensors_models_get(void) {
    return sensor_model_list;
}

uint8_t unitemp_sensors_models_get_count(void) {
    return SENSOR_MODELS_COUNT;
}

const SensorModel* unitemp_sensors_get_model_from_str(char* str) {
    if(str == NULL) return NULL;

    for(uint8_t i = 0; i < SENSOR_MODELS_COUNT; i++) {
        if(!strcmp(str, sensor_model_list[i]->modelname)) {
            return sensor_model_list[i];
        }
    }
    UNITEMP_DEBUG("Unknown sensor model: %s", str);
    return NULL;
}

bool unitemp_sensors_add(Sensor* sensor) {
    furi_check(sensor);

    sensors_list = (Sensor**)realloc(sensors_list, (sensors_count + 1) * sizeof(Sensor*));
    if(sensors_list == NULL) {
        FURI_LOG_E(APP_NAME, "Failed to allocate memory for new sensor");
        return false;
    }
    sensors_list[sensors_count] = sensor;
    sensors_count++;
    UNITEMP_DEBUG("Sensor %s memory successfully added", sensor->name);
    return true;
}
