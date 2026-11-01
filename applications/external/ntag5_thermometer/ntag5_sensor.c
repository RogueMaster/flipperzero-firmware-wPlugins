#include "ntag5_sensor.h"

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>

#define TAG "Ntag5Sensor"

#define NTAG5_BUFFER_SIZE (64)

#define NTAG5_MEASURE_INTERVAL_MS (1000)
#define NTAG5_RETRY_INTERVAL_MS   (300)
#define NTAG5_SLEEP_STEP_MS       (50)
#define NTAG5_ONE_SHOT_WAIT_MS    (50)

/* Commands that reach the I2C bus may answer late, allow up to 20 ms */
#define NTAG5_FWT_FC (271200U)

#define ISO15693_FLAG_ERROR      (0x01)
#define ISO15693_FLAGS_ADDRESSED (0x22) /* high data rate, addressed */
#define ISO15693_FLAGS_SELECTED  (0x12) /* high data rate, selected */
#define ISO15693_CMD_SELECT      (0x25)

#define NXP_MANUFACTURER (0x04)

/* NTAG 5 link custom commands */
#define NTAG5_CMD_READ_CONFIG (0xC0)
#define NTAG5_CMD_READ_SRAM   (0xD2)
#define NTAG5_CMD_WRITE_I2C   (0xD4)
#define NTAG5_CMD_READ_I2C    (0xD5)

#define NTAG5_CONFIG_EH       (0x3D)
#define NTAG5_I2C_READ_STOP   (0x80)
#define NTAG5_EH_ENABLED_MASK (0x01)

/* TMP117 behind the NTAG 5 I2C master */
#define TMP117_ADDRESS         (0x48)
#define TMP117_REG_TEMPERATURE (0x00)
#define TMP117_REG_CONFIG      (0x01)
#define TMP117_CONFIG_ONE_SHOT (0x0C00)
#define TMP117_RESET_VALUE     (0x8000)

typedef enum {
    Ntag5ResultOk,
    Ntag5ResultNoAnswer, /**< The tag is gone */
    Ntag5ResultTagError, /**< The tag rejected the command */
} Ntag5Result;

struct Ntag5Sensor {
    Nfc* nfc;
    NfcPoller* poller;
    BitBuffer* tx;
    BitBuffer* rx;

    TempSensorCallback callback;
    void* context;
    volatile bool stopping;

    /* NFC worker thread state */
    bool selected;
    TempSensorStatus last_status;
    char id[TEMP_SENSOR_ID_SIZE]; /**< UID of the attached tag */
};

static void ntag5_sensor_report(Ntag5Sensor* instance, TempSensorReading* reading) {
    strlcpy(reading->id, instance->id, sizeof(reading->id));
    instance->last_status = reading->status;
    instance->callback(reading, instance->context);
}

static void ntag5_sensor_report_status(Ntag5Sensor* instance, TempSensorStatus status) {
    TempSensorReading reading = {.status = status};
    ntag5_sensor_report(instance, &reading);
}

/* Wait in short steps, so stopping does not take long */
static void ntag5_sensor_sleep(Ntag5Sensor* instance, uint32_t duration_ms) {
    for(uint32_t waited = 0; waited < duration_ms && !instance->stopping;
        waited += NTAG5_SLEEP_STEP_MS) {
        furi_delay_ms(NTAG5_SLEEP_STEP_MS);
    }
}

static Ntag5Result ntag5_sensor_exchange(Ntag5Sensor* instance, Iso15693_3Poller* poller) {
    const Iso15693_3Error error =
        iso15693_3_poller_send_frame(poller, instance->tx, instance->rx, NTAG5_FWT_FC);

    if(error != Iso15693_3ErrorNone || bit_buffer_get_size_bytes(instance->rx) == 0) {
        return Ntag5ResultNoAnswer;
    }
    if(bit_buffer_get_byte(instance->rx, 0) & ISO15693_FLAG_ERROR) {
        return Ntag5ResultTagError;
    }

    return Ntag5ResultOk;
}

static void ntag5_sensor_begin_custom(Ntag5Sensor* instance, uint8_t command) {
    bit_buffer_reset(instance->tx);
    bit_buffer_append_byte(instance->tx, ISO15693_FLAGS_SELECTED);
    bit_buffer_append_byte(instance->tx, command);
    bit_buffer_append_byte(instance->tx, NXP_MANUFACTURER);
}

/* WRITE I2C: address, number of bytes - 1, data */
static Ntag5Result ntag5_sensor_write_i2c(
    Ntag5Sensor* instance,
    Iso15693_3Poller* poller,
    const uint8_t* data,
    size_t size) {
    ntag5_sensor_begin_custom(instance, NTAG5_CMD_WRITE_I2C);
    bit_buffer_append_byte(instance->tx, TMP117_ADDRESS);
    bit_buffer_append_byte(instance->tx, size - 1);
    bit_buffer_append_bytes(instance->tx, data, size);

    return ntag5_sensor_exchange(instance, poller);
}

/* Find the tag, select it and check that it is an NTAG 5 link with energy harvesting on */
static Ntag5Result ntag5_sensor_attach(Ntag5Sensor* instance, Iso15693_3Poller* poller) {
    uint8_t uid[ISO15693_3_UID_SIZE];

    if(iso15693_3_poller_inventory(poller, uid) != Iso15693_3ErrorNone) {
        return Ntag5ResultNoAnswer;
    }

    // Flipper keeps the UID most significant byte first: E0, manufacturer, ...
    // Shown the same way as in the NFC app
    char* id = instance->id;
    for(size_t i = 0; i < ISO15693_3_UID_SIZE; i++) {
        snprintf(id + 2 * i, sizeof(instance->id) - 2 * i, "%02X", uid[i]);
    }

    if(uid[1] != NXP_MANUFACTURER) {
        ntag5_sensor_report_status(instance, TempSensorStatusNotSensor);
        return Ntag5ResultTagError;
    }

    bit_buffer_reset(instance->tx);
    bit_buffer_append_byte(instance->tx, ISO15693_FLAGS_ADDRESSED);
    bit_buffer_append_byte(instance->tx, ISO15693_CMD_SELECT);
    for(size_t i = 0; i < ISO15693_3_UID_SIZE; i++) {
        bit_buffer_append_byte(instance->tx, uid[ISO15693_3_UID_SIZE - 1 - i]);
    }

    Ntag5Result result = ntag5_sensor_exchange(instance, poller);
    if(result != Ntag5ResultOk) {
        return result;
    }

    // Energy harvesting configuration, only read
    ntag5_sensor_begin_custom(instance, NTAG5_CMD_READ_CONFIG);
    bit_buffer_append_byte(instance->tx, NTAG5_CONFIG_EH);
    bit_buffer_append_byte(instance->tx, 0x00);

    result = ntag5_sensor_exchange(instance, poller);
    if(result == Ntag5ResultTagError || bit_buffer_get_size_bytes(instance->rx) < 2) {
        ntag5_sensor_report_status(instance, TempSensorStatusNotSensor);
        return Ntag5ResultTagError;
    }
    if(result != Ntag5ResultOk) {
        return result;
    }

    if(!(bit_buffer_get_byte(instance->rx, 1) & NTAG5_EH_ENABLED_MASK)) {
        ntag5_sensor_report_status(instance, TempSensorStatusNoPower);
        return Ntag5ResultTagError;
    }

    instance->selected = true;
    return Ntag5ResultOk;
}

/* Same sequence as the original phone app */
static Ntag5Result ntag5_sensor_measure(Ntag5Sensor* instance, Iso15693_3Poller* poller) {
    const uint8_t start_one_shot[] = {
        TMP117_REG_CONFIG,
        TMP117_CONFIG_ONE_SHOT >> 8,
        TMP117_CONFIG_ONE_SHOT & 0xFF,
    };
    const uint8_t select_temperature[] = {TMP117_REG_TEMPERATURE};

    Ntag5Result result =
        ntag5_sensor_write_i2c(instance, poller, start_one_shot, sizeof(start_one_shot));
    if(result != Ntag5ResultOk) {
        return result;
    }

    furi_delay_ms(NTAG5_ONE_SHOT_WAIT_MS);

    result =
        ntag5_sensor_write_i2c(instance, poller, select_temperature, sizeof(select_temperature));
    if(result != Ntag5ResultOk) {
        return result;
    }

    // READ I2C: 2 bytes into the SRAM
    ntag5_sensor_begin_custom(instance, NTAG5_CMD_READ_I2C);
    bit_buffer_append_byte(instance->tx, NTAG5_I2C_READ_STOP | TMP117_ADDRESS);
    bit_buffer_append_byte(instance->tx, 2 - 1);
    result = ntag5_sensor_exchange(instance, poller);
    if(result != Ntag5ResultOk) {
        return result;
    }

    // READ SRAM block 0
    ntag5_sensor_begin_custom(instance, NTAG5_CMD_READ_SRAM);
    bit_buffer_append_byte(instance->tx, 0x00);
    bit_buffer_append_byte(instance->tx, 0x00);
    result = ntag5_sensor_exchange(instance, poller);
    if(result != Ntag5ResultOk) {
        return result;
    }
    if(bit_buffer_get_size_bytes(instance->rx) < 3) {
        return Ntag5ResultTagError;
    }

    const uint16_t raw = (bit_buffer_get_byte(instance->rx, 1) << 8) |
                         bit_buffer_get_byte(instance->rx, 2);
    TempSensorReading reading = {.status = TempSensorStatusOk};

    if(raw == TMP117_RESET_VALUE) {
        reading.status = TempSensorStatusNoValue;
    } else {
        // Signed, 1/128 degree per LSB
        reading.temperature = (int16_t)raw / 128.0f;
    }

    ntag5_sensor_report(instance, &reading);
    return Ntag5ResultOk;
}

/* NFC worker thread */
static NfcCommand ntag5_sensor_poller_callback(NfcGenericEvent event, void* context) {
    Ntag5Sensor* instance = context;
    const Iso15693_3PollerEvent* poller_event = event.event_data;
    Iso15693_3Poller* poller = event.instance;

    if(instance->stopping) {
        return NfcCommandStop;
    }

    if(poller_event->type != Iso15693_3PollerEventTypeReady) {
        // The poller found nothing in the field, it pauses by itself
        instance->selected = false;
        if(instance->last_status != TempSensorStatusWaiting) {
            ntag5_sensor_report_status(instance, TempSensorStatusWaiting);
        }
        return NfcCommandContinue;
    }

    Ntag5Result result = Ntag5ResultOk;
    if(!instance->selected) {
        result = ntag5_sensor_attach(instance, poller);
    }
    if(result == Ntag5ResultOk) {
        result = ntag5_sensor_measure(instance, poller);
    }

    if(result == Ntag5ResultOk) {
        ntag5_sensor_sleep(instance, NTAG5_MEASURE_INTERVAL_MS);
        return NfcCommandContinue;
    }

    // Tag gone or unusable: start over with an inventory next time
    instance->selected = false;
    if(result == Ntag5ResultNoAnswer) {
        if(instance->last_status != TempSensorStatusWaiting) {
            ntag5_sensor_report_status(instance, TempSensorStatusWaiting);
        }
    } else if(
        instance->last_status != TempSensorStatusNotSensor &&
        instance->last_status != TempSensorStatusNoPower) {
        // Rejected command without a more specific report, e.g. no sensor on the I2C bus
        ntag5_sensor_report_status(instance, TempSensorStatusNotSensor);
    }
    ntag5_sensor_sleep(instance, NTAG5_RETRY_INTERVAL_MS);

    return NfcCommandContinue;
}

Ntag5Sensor* ntag5_sensor_alloc(void) {
    Ntag5Sensor* instance = malloc(sizeof(Ntag5Sensor));

    instance->nfc = nfc_alloc();
    instance->tx = bit_buffer_alloc(NTAG5_BUFFER_SIZE);
    instance->rx = bit_buffer_alloc(NTAG5_BUFFER_SIZE);

    return instance;
}

void ntag5_sensor_free(Ntag5Sensor* instance) {
    furi_check(instance);
    furi_check(instance->poller == NULL);

    bit_buffer_free(instance->tx);
    bit_buffer_free(instance->rx);
    nfc_free(instance->nfc);

    free(instance);
}

void ntag5_sensor_start(Ntag5Sensor* instance, TempSensorCallback callback, void* context) {
    furi_check(instance);
    furi_check(callback);
    furi_check(instance->poller == NULL);

    instance->callback = callback;
    instance->context = context;
    instance->stopping = false;
    instance->selected = false;
    instance->last_status = TempSensorStatusWaiting;
    instance->id[0] = '\0';

    ntag5_sensor_report_status(instance, TempSensorStatusWaiting);

    instance->poller = nfc_poller_alloc(instance->nfc, NfcProtocolIso15693_3);
    nfc_poller_start(instance->poller, ntag5_sensor_poller_callback, instance);
}

void ntag5_sensor_stop(Ntag5Sensor* instance) {
    furi_check(instance);
    furi_check(instance->poller);

    instance->stopping = true;
    nfc_poller_stop(instance->poller);
    nfc_poller_free(instance->poller);
    instance->poller = NULL;
}
