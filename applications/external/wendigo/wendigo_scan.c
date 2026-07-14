#include "wendigo_scan.h"
#include "wendigo_app_i.h"
#include "wendigo_common_defs.h"
#include "missing_api.h"

uint8_t* buffer = NULL;
uint16_t bufferLen = 0; // 65535 should be plenty of length
uint16_t bufferCap = 0; // Buffer capacity - I don't want to allocate 65kb, but
    // don't want to constantly realloc
char* wendigo_popup_text = NULL; // I suspect the popup text is going out of
    // scope when declared at function scope

/* Device caches */
wendigo_device** devices = NULL;
uint16_t devices_count = 0;
uint16_t devices_capacity = 0;

/* How much will we increase bt_devices[] by when additional space is needed? */
#define INC_DEVICE_CAPACITY_BY 10
/* Maximum size of UART buffer - If a packet terminator isn't found within this
   region older data will be removed */
#define BUFFER_MAX_SIZE        4096
/* How much will we increase buffer[] by when additional space is needed? */
#define BUFFER_INC_CAPACITY_BY 128

/** Search for a start-of-packet marker in the specified byte array
 *  This function is used during parsing to skip past any extraneous bytes
 *  that are sent prior to the start-of-packet sequence.
 *  Returns `size` if not found, otherwise returns the index of the first byte
 * of the sequence.
 */
uint16_t start_of_packet(uint8_t* bytes, uint16_t size) {
    FURI_LOG_T(WENDIGO_TAG, "Start start_of_packet()");
    uint16_t result = 0;
    if(bytes == NULL) {
        return size;
    }
    for(; result + PREAMBLE_LEN <= size && memcmp(bytes + result, PREAMBLE_BT_BLE, PREAMBLE_LEN) &&
          memcmp(bytes + result, PREAMBLE_WIFI_AP, PREAMBLE_LEN) &&
          memcmp(bytes + result, PREAMBLE_WIFI_STA, PREAMBLE_LEN) &&
          memcmp(bytes + result, PREAMBLE_STATUS, PREAMBLE_LEN) &&
          memcmp(bytes + result, PREAMBLE_CHANNELS, PREAMBLE_LEN) &&
          memcmp(bytes + result, PREAMBLE_VER, PREAMBLE_LEN) &&
          memcmp(bytes + result, PREAMBLE_MAC, PREAMBLE_LEN);
        ++result) {
    }
    /* If not found, set result to size */
    if(result + PREAMBLE_LEN > size) {
        result = size;
    }
    FURI_LOG_T(WENDIGO_TAG, "End start_of_packet()");
    return result;
}

/** Search for the end-of-packet marker in the specified byte array
 *  A packet is terminated by a sequence of 4*0xAA and 4*0xFF.
 *  Returns the index of the last byte in this sequence, or the
 *  specified `size` if not found.
 */
uint16_t end_of_packet(uint8_t* theBytes, uint16_t size) {
    FURI_LOG_T(WENDIGO_TAG, "Start end_of_packet()");
    uint16_t result = PREAMBLE_LEN - 1; /* Start by checking bytes [0] through [7] */
    if(theBytes == NULL) {
        return size;
    }
    for(; result < size && memcmp(theBytes + result - PREAMBLE_LEN + 1, PACKET_TERM, PREAMBLE_LEN);
        ++result) {
    }
    FURI_LOG_T(WENDIGO_TAG, "End end_of_packet()");
    return result;
}

/** Send the status command to ESP32 */
void wendigo_esp_status(WendigoApp* app) {
    char cmd[] = "s\n";
    wendigo_uart_tx(app->uart, (uint8_t*)cmd, strlen(cmd) + 1);
}

/** Send the version command to ESP32 */
void wendigo_version(WendigoApp* app) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_version()");
    char cmd[] = "v\n";
    wendigo_uart_tx(app->uart, (uint8_t*)cmd, strlen(cmd) + 1);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_version()");
}

/** This callback is called by app->scan_timer. If a Wendigo packet
 * hasn't been received in the last 3 seconds it sends commands to
 * restart scanning, on the assumption that the ESP32 has reset.
 */
static void wendigo_scan_timer_callback(void* context) {
    furi_assert(context);
    WendigoApp* app = (WendigoApp*)context;
    if(app->is_scanning) {
        uint32_t now = furi_hal_rtc_get_timestamp();
        if(now - app->last_packet > ESP32_POLL_INTERVAL) {
            for(uint8_t i = 0; i < IF_COUNT; ++i) {
                if(app->interfaces[i].scanning) {
                    wendigo_set_scanning_interface(app, i, true);
                }
            }
        }
    }
}

void wendigo_start_scan_timer(WendigoApp* app) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_start_scan_timer()");
    if(app == NULL) {
        wendigo_log(MSG_ERROR, "End wendigo_start_scan_timer() - Invalid arguments.");
        return;
    }
    if(app->scan_timer == NULL) {
        app->scan_timer =
            furi_timer_alloc(wendigo_scan_timer_callback, FuriTimerTypePeriodic, app);
    }
    if(app->scan_timer == NULL) {
        wendigo_log(MSG_ERROR, "Failed to create scan poll timer.");
        return;
    }
    if(furi_timer_is_running(app->scan_timer) == 0) {
        furi_timer_start(app->scan_timer, furi_ms_to_ticks(ESP32_POLL_INTERVAL * 1000));
    }
}

/** Enable or disable Wendigo scanning on the specified interface.
 * interface->active determines whether the interface can be enabled,
 * if this is false the interface will not be started regardless of
 * the value of starting.
 */
void wendigo_set_scanning_interface(WendigoApp* app, InterfaceType interface, bool starting) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_set_scanning_interface()");
    if(app == NULL || interface >= IF_COUNT) {
        wendigo_log(MSG_ERROR, "wendigo_set_scanning_interface() called with invalid arguments");
        return;
    }
    /* Start the scan polling timer if it's not already running */
    wendigo_start_scan_timer(app);

    const uint8_t CMD_LEN = 5; // e.g. "b 1\n\0"
    char cmdString[CMD_LEN];
    uint8_t arg = (app->interfaces[interface].active && starting) ? 1 : 0;
    char cmd = (interface == IF_BLE) ? 'b' : (interface == IF_BT_CLASSIC) ? 'h' : 'w';
    snprintf(cmdString, CMD_LEN, "%c %d\n", cmd, arg);
    wendigo_uart_tx(app->uart, (uint8_t*)cmdString, CMD_LEN);
    app->interfaces[interface].scanning = (arg == 1);
    /* Update app->is_scanning to reflect current status */
    if(app->interfaces[interface].scanning) {
        app->is_scanning = true;
    } else {
        /* Loop through all iterfaces to see if we're still scanning */
        uint8_t idx;
        for(idx = 0; idx < IF_COUNT && !app->interfaces[idx].scanning; ++idx) {
        }
        app->is_scanning = (idx < IF_COUNT);
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_set_scanning_interface()");
}

/** Enable or disable Wendigo scanning on all interfaces, using app->interfaces
 *  to determine which radios should be enabled/disabled when starting to scan.
 *  This function is called by the UI handler (wendigo_scene_start) when scanning
 *  is started or stopped. app->interfaces is updated via the Setup menu.
 *  Because wendigo_uart_tx returns void there is no way to identify failure,
 *  so this function also returns void.
 */
void wendigo_set_scanning_active(WendigoApp* app, bool starting) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_set_scanning_active()");
    /* This flag will cause incomplete packets to be ignored */
    app->is_scanning = starting;
    for(uint8_t i = 0; i < IF_COUNT; ++i) {
        wendigo_set_scanning_interface(app, i, starting);
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_set_scanning_active()");
}

/** Returns the index into `array` of the device with MAC/BDA matching dev->mac.
 *  Returns `array_count` if the device was not found.
 */
uint16_t custom_device_index(wendigo_device* dev, wendigo_device** array, uint16_t array_count) {
    FURI_LOG_T(WENDIGO_TAG, "Start custom_device_index()");
    uint16_t result;
    if(dev == NULL || array == NULL) {
        FURI_LOG_T(WENDIGO_TAG, "End custom_device_index() ABNORMAL");
        return array_count;
    }
    for(result = 0; result < array_count &&
                    (array[result] == NULL || memcmp(dev->mac, array[result]->mac, MAC_BYTES));
        ++result) {
    }
    FURI_LOG_T(WENDIGO_TAG, "End custom_device_index()");
    return result;
}

/** Returns the index into devices[] of the device with MAC/BDA matching
 *  dev->mac. Returns devices_count if the device was not found. A NULL value for
 *  `dev` is handled correctly by custom_device_index()
 */
uint16_t device_index(wendigo_device* dev) {
    FURI_LOG_T(WENDIGO_TAG, "Start+End device_index()");
    return custom_device_index(dev, devices, devices_count);
}

/** Returns the index into devices[] of the device with MAC/BDA matching mac.
 *  Returns devices_count if the device was not found.
 */
uint16_t device_index_from_mac(uint8_t mac[MAC_BYTES]) {
    FURI_LOG_T(WENDIGO_TAG, "Start device_index_from_mac()");
    wendigo_device dev;
    memcpy(dev.mac, mac, MAC_BYTES);
    FURI_LOG_T(WENDIGO_TAG, "End device_index_from_mac()");
    return device_index(&dev);
}

/** Determines whether a device with the MAC/BDA dev->mac exists in devices[].
 *  A NULL value for `dev` is handled correctly at the top of the call stack,
 *  by custom_device_index().
 */
bool device_exists(wendigo_device* dev) {
    return device_index(dev) < devices_count;
}

/** Check whether the provided MAC is present in the specified array of MACs.
 * Returns the index of the matching array element, or array_len if not found.
 */
uint16_t wendigo_index_of(uint8_t mac[MAC_BYTES], uint8_t** array, uint16_t array_len) {
    if(mac == NULL || !memcmp(mac, nullMac, MAC_BYTES) || array == NULL || array_len == 0) {
        return array_len;
    }
    uint16_t idx = 0;
    for(; idx < array_len && (array[idx] == NULL || memcmp(array[idx], mac, MAC_BYTES)); ++idx) {
    }
    return idx;
} // TODO: this, the following, and device_index_* functions are also defined in ESP32-Wendigo, in common.c. Merge and move to wendigo_common_defs.c

uint8_t wendigo_index_of_string(char* str, char** array, uint8_t array_len) {
    if(str == NULL || array == NULL || array_len == 0 || str[0] == '\0') {
        return array_len;
    }
    uint8_t idx = 0;
    for(; idx < array_len && (array[idx] == NULL || strncasecmp(str, array[idx], MAX_SSID_LEN));
        ++idx) {
    }
    return idx;
}

/** Called from parseBufferBluetooth(), this function updates the existing device
 *  `dev` with new attributes from `new_device`.
 *  Returns a boolean representing the success or failure of the function.
 */
bool wendigo_add_bt_device(wendigo_device* dev, wendigo_device* new_device) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_add_bt_device()");
    new_device->radio.bluetooth.cod = dev->radio.bluetooth.cod;
    if(dev->radio.bluetooth.cod_str != NULL) {
        new_device->radio.bluetooth.cod_str = malloc(strlen(dev->radio.bluetooth.cod_str) + 1);
        if(new_device->radio.bluetooth.cod_str != NULL) {
            /* No need to handle allocation failure - we simply don't copy cod_str */
            strncpy(
                new_device->radio.bluetooth.cod_str,
                dev->radio.bluetooth.cod_str,
                strlen(dev->radio.bluetooth.cod_str));
            new_device->radio.bluetooth.cod_str[strlen(dev->radio.bluetooth.cod_str)] = '\0';
        }
    } else {
        new_device->radio.bluetooth.cod_str = NULL;
    }
    /* Device name */
    new_device->radio.bluetooth.bdname_len = dev->radio.bluetooth.bdname_len;
    if(dev->radio.bluetooth.bdname_len > 0 && dev->radio.bluetooth.bdname != NULL) {
        new_device->radio.bluetooth.bdname = malloc(dev->radio.bluetooth.bdname_len + 1);
        if(new_device->radio.bluetooth.bdname == NULL) {
            /* Skip the device's bdname and hope we have memory for it next time it's seen */
            new_device->radio.bluetooth.bdname_len = 0;
        } else {
            strncpy(
                new_device->radio.bluetooth.bdname,
                dev->radio.bluetooth.bdname,
                dev->radio.bluetooth.bdname_len);
            new_device->radio.bluetooth.bdname[dev->radio.bluetooth.bdname_len] = '\0';
        }
    } else {
        new_device->radio.bluetooth.bdname = NULL;
        new_device->radio.bluetooth.bdname_len = 0;
    }
    /* EIR */
    new_device->radio.bluetooth.eir_len = dev->radio.bluetooth.eir_len;
    if(dev->radio.bluetooth.eir_len > 0 && dev->radio.bluetooth.eir != NULL) {
        new_device->radio.bluetooth.eir = malloc(dev->radio.bluetooth.eir_len);
        if(new_device->radio.bluetooth.eir == NULL) {
            /* Skip EIR from this packet */
            new_device->radio.bluetooth.eir_len = 0;
        } else {
            memcpy(
                new_device->radio.bluetooth.eir,
                dev->radio.bluetooth.eir,
                dev->radio.bluetooth.eir_len);
        }
    } else {
        new_device->radio.bluetooth.eir = NULL;
        new_device->radio.bluetooth.eir_len = 0;
    }
    /* BT services */
    new_device->radio.bluetooth.bt_services.num_services =
        dev->radio.bluetooth.bt_services.num_services;
    if(dev->radio.bluetooth.bt_services.num_services > 0 &&
       dev->radio.bluetooth.bt_services.service_uuids != NULL) {
        new_device->radio.bluetooth.bt_services.service_uuids =
            malloc(sizeof(void*) * dev->radio.bluetooth.bt_services.num_services);
        if(new_device->radio.bluetooth.bt_services.service_uuids == NULL) {
            new_device->radio.bluetooth.bt_services.num_services = 0;
        } else {
            memcpy(
                new_device->radio.bluetooth.bt_services.service_uuids,
                dev->radio.bluetooth.bt_services.service_uuids,
                sizeof(void*) * dev->radio.bluetooth.bt_services.num_services);
        }
    } else {
        new_device->radio.bluetooth.bt_services.service_uuids = NULL;
        new_device->radio.bluetooth.bt_services.num_services = 0;
    }
    /* Known services */
    new_device->radio.bluetooth.bt_services.known_services_len =
        dev->radio.bluetooth.bt_services.known_services_len;
    if(dev->radio.bluetooth.bt_services.known_services_len > 0 &&
       dev->radio.bluetooth.bt_services.known_services != NULL) {
        /* known_services is an array of pointers - this is where we stop
            controlling memory allocation so that bt_uuid's can be allocated a single
            time and reused. This function will copy the *pointers* in
            known_services[], but not the bt_uuid structs that they point to. */
        new_device->radio.bluetooth.bt_services.known_services =
            malloc(sizeof(bt_uuid*) * dev->radio.bluetooth.bt_services.known_services_len);
        if(new_device->radio.bluetooth.bt_services.known_services == NULL) {
            new_device->radio.bluetooth.bt_services.known_services_len = 0;
        } else {
            memcpy(
                new_device->radio.bluetooth.bt_services.known_services,
                dev->radio.bluetooth.bt_services.known_services,
                sizeof(bt_uuid*) * dev->radio.bluetooth.bt_services.known_services_len);
            // TODO: Despite the caveat above, it might be nice to validate that the
            // bt_uuid's we're pointing to actually exist.
        }
    } else {
        new_device->radio.bluetooth.bt_services.known_services = NULL;
        new_device->radio.bluetooth.bt_services.known_services_len = 0;
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_add_bt_device()");
    return true;
}

/** Updates the existing device `dev` based on the newly-received device `new_device`.
 *  This function is called from parseBufferBluetooth() when a bluetooth packet
 *  referring to an existing device is received.
 *  Returns a boolean indicating the success/failure of the function.
 */
bool wendigo_update_bt_device(wendigo_device* dev, wendigo_device* new_device) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_update_bt_device()");
    new_device->radio.bluetooth.cod = dev->radio.bluetooth.cod;
    /* cod_str present in update? */
    if(dev->radio.bluetooth.cod_str != NULL && strlen(dev->radio.bluetooth.cod_str) > 0) {
        char* new_cod =
            realloc(new_device->radio.bluetooth.cod_str, strlen(dev->radio.bluetooth.cod_str) + 1);
        if(new_cod != NULL) {
            strncpy(new_cod, dev->radio.bluetooth.cod_str, strlen(dev->radio.bluetooth.cod_str));
            new_cod[strlen(dev->radio.bluetooth.cod_str)] = '\0';
            new_device->radio.bluetooth.cod_str = new_cod;
        }
    }
    /* Is bdname in update? */
    if(dev->radio.bluetooth.bdname_len > 0 && dev->radio.bluetooth.bdname != NULL) {
        char* new_name =
            realloc(new_device->radio.bluetooth.bdname, dev->radio.bluetooth.bdname_len + 1);
        if(new_name != NULL) { /* If allocation fails target's existing bdname and
                                    bdname_len are unmodified */
            strncpy(new_name, dev->radio.bluetooth.bdname, dev->radio.bluetooth.bdname_len);
            new_name[dev->radio.bluetooth.bdname_len] = '\0';
            new_device->radio.bluetooth.bdname = new_name;
            new_device->radio.bluetooth.bdname_len = dev->radio.bluetooth.bdname_len;
        }
    }
    /* How about EIR? */
    if(dev->radio.bluetooth.eir_len > 0 && dev->radio.bluetooth.eir != NULL) {
        uint8_t* new_eir = realloc(new_device->radio.bluetooth.eir, dev->radio.bluetooth.eir_len);
        if(new_eir != NULL) {
            memcpy(
                new_eir, dev->radio.bluetooth.eir, sizeof(uint8_t) * dev->radio.bluetooth.eir_len);
            new_device->radio.bluetooth.eir = new_eir;
            new_device->radio.bluetooth.eir_len = dev->radio.bluetooth.eir_len;
        }
    }
    /* Number of services */
    if(dev->radio.bluetooth.bt_services.num_services > 0 &&
       dev->radio.bluetooth.bt_services.service_uuids != NULL) {
        void* new_svcs = realloc(
            new_device->radio.bluetooth.bt_services.service_uuids,
            sizeof(void*) * dev->radio.bluetooth.bt_services.num_services);
        if(new_svcs != NULL) {
            memcpy(
                new_svcs,
                dev->radio.bluetooth.bt_services.service_uuids,
                sizeof(void*) * dev->radio.bluetooth.bt_services.num_services);
            new_device->radio.bluetooth.bt_services.service_uuids = new_svcs;
            new_device->radio.bluetooth.bt_services.num_services =
                dev->radio.bluetooth.bt_services.num_services;
        }
    }
    /* Known services */
    if(dev->radio.bluetooth.bt_services.known_services_len > 0 &&
       dev->radio.bluetooth.bt_services.known_services != NULL) {
        bt_uuid** new_known_svcs = realloc(
            new_device->radio.bluetooth.bt_services.known_services,
            sizeof(bt_uuid*) * dev->radio.bluetooth.bt_services.known_services_len);
        if(new_known_svcs != NULL) {
            memcpy(
                new_known_svcs,
                dev->radio.bluetooth.bt_services.known_services,
                sizeof(bt_uuid*) * dev->radio.bluetooth.bt_services.known_services_len);
            new_device->radio.bluetooth.bt_services.known_services = new_known_svcs;
            new_device->radio.bluetooth.bt_services.known_services_len =
                dev->radio.bluetooth.bt_services.known_services_len;
        }
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_update_bt_device()");
    return true;
}

/** Add the specified device to devices[], extending the length of devices[] if
 * necessary. If the specified device has a MAC/BDA which is already present in
 * devices[] a new entry will not be made, instead the element with the same
 * MAC/BDA will be updated based on the specified device. Returns true if the
 * device was successfully added to (or updated in) devices[]. NOTE: The calling
 * function may free the specified wendigo_device or any of its attributes when
 * this function returns. To minimise the likelihood of memory leaks this
 * function will allocate its own memory to hold the specified device and its
 * attributes.
 */
bool wendigo_add_device(WendigoApp* app, wendigo_device* dev) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_add_device()");
    uint16_t idx = device_index(dev);
    if(idx < devices_count) {
        /* A device with the provided BDA already exists - Update that instead */
        return wendigo_update_device(app, dev);
    }
    /* Adding to devices - Increase capacity by an additional INC_DEVICE_CAPACITY_BY if necessary */
    if(devices == NULL || devices_capacity == devices_count) {
        wendigo_device** new_devices = realloc(
            devices, sizeof(wendigo_device*) * (devices_capacity + INC_DEVICE_CAPACITY_BY));
        if(new_devices == NULL) {
            /* Can't store the device */
            return false;
        }
        devices = new_devices;
        devices_capacity += INC_DEVICE_CAPACITY_BY;
    }
    devices[devices_count] = malloc(sizeof(wendigo_device));
    if(devices[devices_count] == NULL) {
        /* That's unfortunate */
        return false;
    }
    wendigo_device* new_device = devices[devices_count++];
    /* Copy common attributes */
    new_device->rssi = dev->rssi;
    new_device->scanType = dev->scanType;
    /* Don't copy tagged status - A new device is not tagged. */
    new_device->tagged = false;
    new_device->view = dev->view; /* Copy the view pointer if we have one, NULL if we don't */
    /* ESP32 doesn't know the real time/date so overwrite the lastSeen value.
       time_t is just another way of saying long long int, so casting is OK */
    new_device->lastSeen = furi_hal_rtc_get_timestamp();
    /* Copy MAC/BDA */
    memcpy(new_device->mac, dev->mac, MAC_BYTES);
    /* Copy protocol-specific attributes */
    if(dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) {
        wendigo_add_bt_device(dev, new_device);
    } else if(dev->scanType == SCAN_WIFI_AP) {
        new_device->radio.ap.channel = dev->radio.ap.channel;
        new_device->radio.ap.authmode = dev->radio.ap.authmode;
        new_device->radio.ap.stations_count = dev->radio.ap.stations_count;
        if(dev->radio.ap.stations_count > 0 && dev->radio.ap.stations != NULL) {
            /* Copy stations[] */
            new_device->radio.ap.stations =
                malloc(sizeof(uint8_t*) * dev->radio.ap.stations_count);
            if(new_device->radio.ap.stations == NULL) {
                new_device->radio.ap.stations_count = 0;
            } else {
                for(uint8_t i = 0; i < dev->radio.ap.stations_count; ++i) {
                    new_device->radio.ap.stations[i] = malloc(MAC_BYTES);
                    if(new_device->radio.ap.stations[i] != NULL) {
                        memcpy(
                            new_device->radio.ap.stations[i],
                            dev->radio.ap.stations[i],
                            MAC_BYTES);
                    }
                }
            }
        } else {
            new_device->radio.ap.stations = NULL;
        }
        memcpy(new_device->radio.ap.ssid, dev->radio.ap.ssid, MAX_SSID_LEN);
        new_device->radio.ap.ssid[MAX_SSID_LEN] = '\0';
    } else if(dev->scanType == SCAN_WIFI_STA) {
        new_device->radio.sta.channel = dev->radio.sta.channel;
        memcpy(new_device->radio.sta.apMac, dev->radio.sta.apMac, MAC_BYTES);
        /* Copy saved_networks[] to new_device */
        if(dev->radio.sta.saved_networks_count > 0 && dev->radio.sta.saved_networks != NULL) {
            new_device->radio.sta.saved_networks_count = dev->radio.sta.saved_networks_count;
            new_device->radio.sta.saved_networks =
                malloc(sizeof(char*) * dev->radio.sta.saved_networks_count);
            if(new_device->radio.sta.saved_networks == NULL) {
                new_device->radio.sta.saved_networks_count = 0;
            } else {
                uint8_t this_ssid_len;
                for(uint8_t i = 0; i < dev->radio.sta.saved_networks_count; ++i) {
                    if(dev->radio.sta.saved_networks[i] == NULL) {
                        new_device->radio.sta.saved_networks[i] = NULL;
                    } else {
                        this_ssid_len = strlen(dev->radio.sta.saved_networks[i]);
                        new_device->radio.sta.saved_networks[i] = malloc(this_ssid_len + 1);
                        if(new_device->radio.sta.saved_networks[i] != NULL) {
                            strncpy(
                                new_device->radio.sta.saved_networks[i],
                                dev->radio.sta.saved_networks[i],
                                this_ssid_len);
                            new_device->radio.sta.saved_networks[i][this_ssid_len] = '\0';
                        }
                        if(networks_count > 0 && networks != NULL) {
                            /* PNL data model has been initialised - ensure the
                             * current SSID and device are in it. */
                            PNL_Result res = pnl_find_or_create_device(
                                app, dev->radio.sta.saved_networks[i], dev);
                            pnl_log_result(
                                "wendigo_add_device()", res, dev->radio.sta.saved_networks[i], dev);
                        }
                    }
                }
            }
        } else {
            new_device->radio.sta.saved_networks = NULL;
            new_device->radio.sta.saved_networks_count = 0;
        }
    }

    /* If the device list scene is currently displayed, add the device to the UI */
    if(app->current_view == WendigoAppViewDeviceList) {
        wendigo_scene_device_list_update(app, new_device);
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_add_device()");
    return true;
}

/** Locate the device in devices[] with the same MAC/BDA as `dev` and update the
 * object based on the contents of `dev`. If a device with the specified MAC/BDA
 * does not exist a new device will be added to devices[]. Returns true if the
 * specified device has been updated (or a new device added) successfully. NOTE:
 * The calling function may free `dev` or any of its members immediately upon
 * this function returning. To minimise the likelihood of memory leaks, the
 * device cache management functions manage the allocation and free'ing of all
 * components of the device cache *except for* bt_uuid objects that form part of
 * service descriptors.
 */
bool wendigo_update_device(WendigoApp* app, wendigo_device* dev) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_update_device()");
    uint16_t idx = device_index(dev);
    if(idx == devices_count) {
        /* Device doesn't exist in bt_devices[] - Add it instead */
        return wendigo_add_device(app, dev);
    }
    wendigo_device* target = devices[idx];
    /* Copy common attributes */
    target->rssi = dev->rssi;
    target->scanType = dev->scanType;
    /* Replace lastSeen */
    target->lastSeen = furi_hal_rtc_get_timestamp();
    /* Copy protocol-specific attributes */
    if(dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) {
        wendigo_update_bt_device(dev, target);
    } else if(dev->scanType == SCAN_WIFI_AP) {
        /* Copy channel, ssid, sta_count, stations */
        target->radio.ap.channel = dev->radio.ap.channel;
        if(dev->radio.ap.authmode != WIFI_AUTH_MAX) {
            target->radio.ap.authmode = dev->radio.ap.authmode;
        }
        uint8_t ssid_len = local_strnlen((char*)dev->radio.ap.ssid, MAX_SSID_LEN);
        if(ssid_len > 0) {
            memcpy(target->radio.ap.ssid, dev->radio.ap.ssid, ssid_len + 1);
            target->radio.ap.ssid[ssid_len] = '\0';
        }
        /* Merge dev->radio.ap.stations with target->radio.ap.stations */
        uint8_t new_stations = 0;
        if(target->radio.ap.stations_count == 0) {
            new_stations = dev->radio.ap.stations_count;
        } else {
            /* Loop through dev->radio.ap.stations and count the MACs not present in target->radio.ap.stations */
            for(uint8_t i = 0; i < dev->radio.ap.stations_count; ++i) {
                if(wendigo_index_of(
                       dev->radio.ap.stations[i],
                       target->radio.ap.stations,
                       target->radio.ap.stations_count) == target->radio.ap.stations_count) {
                    ++new_stations;
                }
            }
        }
        if(new_stations > 0) {
            /* Append new_stations elements to target->radio.ap.stations */
            uint8_t** updated_stations = realloc(
                target->radio.ap.stations,
                sizeof(uint8_t*) * (target->radio.ap.stations_count + new_stations));
            if(updated_stations != NULL) {
                uint8_t stationIdx = target->radio.ap.stations_count;
                for(uint8_t i = 0; i < dev->radio.ap.stations_count; ++i) {
                    if(wendigo_index_of(
                           dev->radio.ap.stations[i],
                           updated_stations,
                           target->radio.ap.stations_count) == target->radio.ap.stations_count) {
                        /* Add dev->radio.ap.stations[i] */
                        updated_stations[stationIdx] = malloc(MAC_BYTES);
                        if(updated_stations[stationIdx] != NULL) {
                            memcpy(
                                updated_stations[stationIdx],
                                dev->radio.ap.stations[i],
                                MAC_BYTES);
                        }
                        ++stationIdx;
                    }
                }
                target->radio.ap.stations = updated_stations;
                target->radio.ap.stations_count += new_stations;
            }
        }
    } else if(dev->scanType == SCAN_WIFI_STA) {
        /* Copy channel, AP MAC, AP */
        target->radio.sta.channel = dev->radio.sta.channel;
        /* Only replace AP MAC if dev has a "better" AP than target - If dev isn't nullMac. */
        if(memcmp(dev->radio.sta.apMac, nullMac, MAC_BYTES)) {
            memcpy(target->radio.sta.apMac, dev->radio.sta.apMac, MAC_BYTES);
        }
        if(dev->radio.sta.saved_networks_count > 0 && dev->radio.sta.saved_networks != NULL) {
            /* `dev` has saved networks, copy across any not already
                in `target`.
                First count the number of new SSIDs.
            */
            uint8_t target_idx;
            uint8_t new_pnl_count = 0;
            for(uint8_t i = 0; i < dev->radio.sta.saved_networks_count; ++i) {
                target_idx = wendigo_index_of_string(
                    dev->radio.sta.saved_networks[i],
                    target->radio.sta.saved_networks,
                    target->radio.sta.saved_networks_count);
                if(target_idx == target->radio.sta.saved_networks_count &&
                   dev->radio.sta.saved_networks[i] != NULL) {
                    ++new_pnl_count;
                }
            }
            if(new_pnl_count > 0) {
                /* There are new SSIDs to add - realloc target */
                new_pnl_count += target->radio.sta.saved_networks_count;
                char** new_pnl =
                    realloc(target->radio.sta.saved_networks, sizeof(char*) * new_pnl_count);
                if(new_pnl != NULL) {
                    /* Copy across new elements */
                    uint8_t pnl_len;
                    uint8_t pnl_idx = target->radio.sta.saved_networks_count;
                    for(uint8_t i = 0; i < dev->radio.sta.saved_networks_count; ++i) {
                        target_idx = wendigo_index_of_string(
                            dev->radio.sta.saved_networks[i],
                            new_pnl,
                            target->radio.sta.saved_networks_count);
                        if(target_idx == target->radio.sta.saved_networks_count &&
                           dev->radio.sta.saved_networks[i] != NULL) {
                            /* Copy dev[i] to target[pnl_idx] */
                            pnl_len = strlen(dev->radio.sta.saved_networks[i]);
                            new_pnl[pnl_idx] = malloc(pnl_len + 1);
                            if(new_pnl[pnl_idx] != NULL) {
                                strncpy(
                                    new_pnl[pnl_idx], dev->radio.sta.saved_networks[i], pnl_len);
                                new_pnl[pnl_idx][pnl_len] = '\0';
                                ++pnl_idx;
                            }
                            if(networks_count > 0 && networks != NULL) {
                                /* PNL data model has been initialised - ensure
                                 * the current device and SSID are in it. */
                                PNL_Result res = pnl_find_or_create_device(
                                    app, dev->radio.sta.saved_networks[i], dev);
                                pnl_log_result(
                                    "wendigo_update_device()",
                                    res,
                                    dev->radio.sta.saved_networks[i],
                                    dev);
                            }
                        }
                    }
                    /* Hopefully pnl_idx == new_pnl_count. If not some mallocs
                        have failed, but that's OK */
                    target->radio.sta.saved_networks = new_pnl;
                    target->radio.sta.saved_networks_count = pnl_idx;
                } /* If we failed to malloc new_pnl don't do anything */
            }
        }
    }
    /* Update the device list if it's currently displayed */
    if(app->current_view == WendigoAppViewDeviceList) {
        wendigo_scene_device_list_update(app, target);
    } else if(app->current_view == WendigoAppViewDeviceDetail) { // && selectedDevice == bt_devices[idx]
        // TODO: Update existing view
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_update_device()");
    return true;
}

/** Free all memory allocated to the specified device.
 * After free'ing its members this function will also free `dev` itself.
 * dev->view will not be touched by this function - It is allocated and freed in
 * wendigo_scene_device_list.c. NOTE: I've gone back and forth between setting
 * attributes to NULL and 0 after freeing and not (because the attributes aren't
 * accessible after freeing `dev`), but have finally made a decision: Attributes
 * representing allocated memory ARE set to NULL (for pointers) and 0 (for
 * counts) to reduce the likelihood that any orphan pointers will attempt to use
 * deallocated memory.
 */
void wendigo_free_device(wendigo_device* dev) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_free_device()");
    if(dev == NULL) {
        return;
    }
    if(dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) {
        if(dev->radio.bluetooth.cod_str != NULL) {
            free(dev->radio.bluetooth.cod_str);
            dev->radio.bluetooth.cod_str = NULL;
        }
        if(dev->radio.bluetooth.bdname != NULL) {
            free(dev->radio.bluetooth.bdname);
            dev->radio.bluetooth.bdname = NULL;
            dev->radio.bluetooth.bdname_len = 0;
        }
        if(dev->radio.bluetooth.eir != NULL) {
            free(dev->radio.bluetooth.eir);
            dev->radio.bluetooth.eir = NULL;
        }
        if(dev->radio.bluetooth.bt_services.service_uuids != NULL) {
            free(dev->radio.bluetooth.bt_services.service_uuids);
            dev->radio.bluetooth.bt_services.service_uuids = NULL;
            dev->radio.bluetooth.bt_services.num_services = 0;
        }
        if(dev->radio.bluetooth.bt_services.known_services != NULL) {
            // TODO: After implementing BT services, ensure that the bt_uuid's are
            // freed somewhere
            free(dev->radio.bluetooth.bt_services.known_services);
            dev->radio.bluetooth.bt_services.known_services = NULL;
            dev->radio.bluetooth.bt_services.known_services_len = 0;
        }
    } else if(dev->scanType == SCAN_WIFI_AP) {
        if(dev->radio.ap.stations != NULL && dev->radio.ap.stations_count > 0) {
            for(uint8_t i = 0; i < dev->radio.ap.stations_count; ++i) {
                if(dev->radio.ap.stations[i] != NULL) {
                    free(dev->radio.ap.stations[i]);
                }
            }
            free(dev->radio.ap.stations);
            dev->radio.ap.stations = NULL;
            dev->radio.ap.stations_count = 0;
        }
    } else if(dev->scanType == SCAN_WIFI_STA) {
        /* Free preferred network list if there is one */
        if(dev->radio.sta.saved_networks_count > 0 && dev->radio.sta.saved_networks != NULL) {
            for(uint8_t i = 0; i < dev->radio.sta.saved_networks_count; ++i) {
                if(dev->radio.sta.saved_networks[i] != NULL) {
                    free(dev->radio.sta.saved_networks[i]);
                }
            }
            free(dev->radio.sta.saved_networks);
        }
        dev->radio.sta.saved_networks = NULL;
        dev->radio.sta.saved_networks_count = 0;
    } else {
        /* Nothing to do */
    }
    dev->view = NULL;
    free(dev);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_free_device()");
}

/** Deallocates all memory allocated to the device cache.
 * This function deallocates all elements of the device cache, devices[].
 * These arrays are left in a coherent state, with the arrays set to
 * NULL and their count & capacity variables set to zero.
 */
void wendigo_free_devices() {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_free_devices()");
    /* For devices[] we also want to free device attributes and the devices
     * themselves */
    if(devices_capacity > 0 && devices != NULL) {
        for(uint16_t i = 0; i < devices_count; ++i) {
            wendigo_free_device(devices[i]);
            devices[i] = NULL;
        }
        free(devices);
        devices = NULL;
        devices_count = 0;
        devices_capacity = 0;
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_free_devices()");
}

/** During testing I bumped the ESP32 reset button and was irritated that
 * Flipper's device cache was working perfectly but status claimed there were no
 * devices in the cache despite displaying them). This function provides a
 * location to validate and alter status attributes prior to displaying them,
 * allowing this issue to be overcome.
 */
void process_and_display_status_attribute(
    WendigoApp* app,
    char* attribute_name,
    char* attribute_value) {
    FURI_LOG_T(WENDIGO_TAG, "Start process_and_display_status_attribute()");
    char* interesting_attributes[] = {
        "BT Classic Devices:", "BT Low Energy Devices:", "WiFi STA Devices:", "WiFi APs:"};
    uint8_t interesting_attributes_count = 4;
    uint8_t interested;
    uint16_t count = 0;
    char strVal[6];
    for(interested = 0; interested < interesting_attributes_count &&
                        strcmp(interesting_attributes[interested], attribute_name);
        ++interested) {
    }
    if(interested < interesting_attributes_count) {
        /* We're interested in this attribute */
        for(uint16_t i = 0; i < devices_count; ++i) {
            /* Does the current device contribute to the interesting attribute count? */
            if((interested == 0 && devices[i]->scanType == SCAN_HCI) ||
               (interested == 1 && devices[i]->scanType == SCAN_BLE) ||
               (interested == 2 && devices[i]->scanType == SCAN_WIFI_STA) ||
               (interested == 3 && devices[i]->scanType == SCAN_WIFI_AP)) {
                ++count;
            }
            snprintf(strVal, sizeof(strVal), "%d", count);
            attribute_value = strVal;
        }
    }
    wendigo_scene_status_add_attribute(app, attribute_name, attribute_value);
    FURI_LOG_T(WENDIGO_TAG, "End process_and_display_status_attribute()");
}

/** Parse a Wendigo packet representing a Bluetooth device.
 *  This function, and packet type, caters for both BT Classic
 *  and BT Low Energy devices. Creates a new wendigo_device and
 *  passes it to wendigo_add_device().
 */
uint16_t parseBufferBluetooth(WendigoApp* app, uint8_t* packet, uint16_t packetLen) {
    FURI_LOG_T(WENDIGO_TAG, "Start parseBufferBluetooth");
    /* Sanity check - we should have at least WENDIGO_OFFSET_BT_COD_LEN + PREAMBLE_LEN bytes */
    if(packetLen < (WENDIGO_OFFSET_BT_COD_LEN + PREAMBLE_LEN)) {
        char* msg = malloc(98);
        if(msg != NULL) {
            snprintf(
                msg,
                98,
                "Bluetooth packet too short even before considering BDName, EIR and CoD. Expected %d, actual %d.",
                (WENDIGO_OFFSET_BT_COD_LEN + PREAMBLE_LEN),
                packetLen);
            wendigo_log_with_packet(MSG_WARN, msg, packet, packetLen);
            free(msg);
        }
        return packetLen;
    }
    wendigo_device* dev = malloc(sizeof(wendigo_device));
    if(dev == NULL) {
        wendigo_log_with_packet(
            MSG_ERROR,
            "parseBufferBluetooth(): Unable to allocate memory for a wendigo_device, skipping packet.",
            packet,
            packetLen);
        return packetLen;
    }
    dev->radio.bluetooth.bdname = NULL;
    dev->radio.bluetooth.eir = NULL;
    dev->radio.bluetooth.bt_services.known_services = NULL;
    dev->radio.bluetooth.bt_services.service_uuids = NULL;
    dev->radio.bluetooth.cod_str = NULL;
    dev->view = NULL;
    /* Temporary variables for attributes that require transformation */
    uint8_t tagged;
    /* Copy fixed-byte members */
    memcpy(
        &(dev->radio.bluetooth.bdname_len),
        packet + WENDIGO_OFFSET_BT_BDNAME_LEN,
        sizeof(uint8_t));
    memcpy(&(dev->radio.bluetooth.eir_len), packet + WENDIGO_OFFSET_BT_EIR_LEN, sizeof(uint8_t));
    memcpy(&(dev->rssi), packet + WENDIGO_OFFSET_BT_RSSI, sizeof(int16_t));
    memcpy(&(dev->radio.bluetooth.cod), packet + WENDIGO_OFFSET_BT_COD, sizeof(uint32_t));
    memcpy(dev->mac, packet + WENDIGO_OFFSET_BT_BDA, MAC_BYTES);
    memcpy(&(dev->scanType), packet + WENDIGO_OFFSET_BT_SCANTYPE, sizeof(uint8_t));
    memcpy(&tagged, packet + WENDIGO_OFFSET_BT_TAGGED, sizeof(uint8_t));
    dev->tagged = (tagged == 1);
    /* Ignore lastSeen */
    // memcpy(&(dev->lastSeen.tv_sec), buffer + WENDIGO_OFFSET_BT_LASTSEEN,
    // sizeof(int64_t));
    memcpy(
        &(dev->radio.bluetooth.bt_services.num_services),
        packet + WENDIGO_OFFSET_BT_NUM_SERVICES,
        sizeof(uint8_t));
    memcpy(
        &(dev->radio.bluetooth.bt_services.known_services_len),
        packet + WENDIGO_OFFSET_BT_KNOWN_SERVICES_LEN,
        sizeof(uint8_t));
    uint8_t cod_len;
    memcpy(&cod_len, packet + WENDIGO_OFFSET_BT_COD_LEN, sizeof(uint8_t));
    /* Do we have a bdname? */
    uint16_t index = WENDIGO_OFFSET_BT_BDNAME;
    /* Rudimentary buffer capacity check - Is the buffer large enough to hold the
     * specified bdname and packet terminator? */
    if(dev->radio.bluetooth.bdname_len > 0 &&
       packetLen >=
           (WENDIGO_OFFSET_BT_BDNAME + dev->radio.bluetooth.bdname_len + PREAMBLE_LEN - 1)) {
        dev->radio.bluetooth.bdname = malloc(dev->radio.bluetooth.bdname_len + 1);
        if(dev->radio.bluetooth.bdname == NULL) {
            /* Can't store bdname so set bdname_len to 0 */
            dev->radio.bluetooth.bdname_len = 0;
        } else {
            memcpy(
                dev->radio.bluetooth.bdname,
                packet + WENDIGO_OFFSET_BT_BDNAME,
                dev->radio.bluetooth.bdname_len);
            dev->radio.bluetooth.bdname[dev->radio.bluetooth.bdname_len] = '\0';
        }
        /* Move index past bdname */
        index += dev->radio.bluetooth.bdname_len;
    }
    /* EIR? */
    if(dev->radio.bluetooth.eir_len > 0 &&
       packetLen >= (index + dev->radio.bluetooth.eir_len + PREAMBLE_LEN - 1)) {
        dev->radio.bluetooth.eir = malloc(dev->radio.bluetooth.eir_len);
        if(dev->radio.bluetooth.eir == NULL) {
            dev->radio.bluetooth.eir_len = 0;
        } else {
            memcpy(dev->radio.bluetooth.eir, packet + index, dev->radio.bluetooth.eir_len);
        }
        index += dev->radio.bluetooth.eir_len;
    }
    /* Class of Device? */
    if(cod_len > 0 && packetLen >= (index + cod_len + PREAMBLE_LEN - 1)) {
        dev->radio.bluetooth.cod_str = malloc(cod_len + 1);
        if(dev->radio.bluetooth.cod_str != NULL) {
            memcpy(dev->radio.bluetooth.cod_str, packet + index, cod_len);
            dev->radio.bluetooth.cod_str[cod_len] = '\0';
        }
        index += cod_len; /* Cater for the trailing '\0' */
    }
    // TODO: Services to go here

    /* Hopefully `index` now points to the packet terminator (unless scanning has
       stopped, then all bets are off) */
    if(app->is_scanning && memcmp(PACKET_TERM, packet + index, PREAMBLE_LEN)) {
        char* msg = malloc(62);
        if(msg != NULL) {
            snprintf(
                msg, 62, "Bluetooth packet terminator expected at index %d, not found.", index);
            wendigo_log_with_packet(MSG_WARN, msg, packet, packetLen);
            free(msg);
        }
    }

    /* Add or update the device in devices[] - No longer need to check
        whether we're adding or updating, the add/update functions will call
        each other if required. */
    wendigo_add_device(app, dev);
    /* Clean up memory */
    wendigo_free_device(dev);
    FURI_LOG_T(WENDIGO_TAG, "End parseBufferBluetooth()");
    return index + PREAMBLE_LEN;
}

/** Parse a Wendigo packet representing an Access Point.
 *  Creates a new wendigo_device and passes it to wendigo_add_device().
 *  This function does not manipulate the contents of the packet, and
 *  does not read the buffer.
 */
uint16_t parseBufferWifiAp(WendigoApp* app, uint8_t* packet, uint16_t packetLen) {
    FURI_LOG_T(WENDIGO_TAG, "Start parseBufferWifiAp()");
    /* Check the packet is a reasonable size */
    uint16_t expectedLen = WENDIGO_OFFSET_AP_SSID + PREAMBLE_LEN;
    if(packetLen < expectedLen) {
        char* shortMsg = malloc(56);
        if(shortMsg != NULL) {
            snprintf(
                shortMsg,
                56,
                "AP packet too short, expected %d actual %d. Skipping.",
                expectedLen,
                packetLen);
            // Popup disabled while debugging device list wendigo_display_popup(app,
            // "AP too short", shortMsg);
            wendigo_log_with_packet(MSG_ERROR, shortMsg, packet, packetLen);
            free(shortMsg);
        }
        FURI_LOG_T(WENDIGO_TAG, "End parseBufferWifiAp() - Packet too short");
        return packetLen;
    }
    /* Retrieve stations_count and ssid_len from the packet for further validation */
    uint8_t sta_count;
    uint8_t ssid_len;
    memcpy(&ssid_len, packet + WENDIGO_OFFSET_AP_SSID_LEN, sizeof(uint8_t));
    memcpy(&sta_count, packet + WENDIGO_OFFSET_AP_STA_COUNT, sizeof(uint8_t));
    /* Now we have stations_count and ssid_len we know exactly how big the packet should be */
    expectedLen = WENDIGO_OFFSET_AP_SSID + ssid_len + (MAC_BYTES * sta_count) + PREAMBLE_LEN;
    if(packetLen < expectedLen) {
        /* Packet is too short - Likely reflects a corrupted packet with the wrong
            byte in stations_count. Log and display the error.
        */
        char* shortMsg = malloc(77);
        if(shortMsg != NULL) {
            snprintf(
                shortMsg,
                77,
                "Packet's stations_count requires packet of size %d, actual %d. "
                "Skipping.",
                expectedLen,
                packetLen);
            wendigo_log_with_packet(MSG_ERROR, shortMsg, packet, packetLen);
            // Popup disabled while debugging device list wendigo_display_popup(app, "AP
            // too short for STAtions", shortMsg);
            free(shortMsg);
        }
        FURI_LOG_T(WENDIGO_TAG, "End parseBufferWifiAp() - Packet too short for stations");
        return packetLen;
    }
    wendigo_device* dev = malloc(sizeof(wendigo_device));
    if(dev == NULL) {
        return packetLen;
    }
    /* Initialise pointers */
    dev->view = NULL;
    dev->radio.ap.stations = NULL;
    bzero(dev->radio.ap.ssid, MAX_SSID_LEN + 1);
    /* Temporary variables for elements that need to be transformed */
    uint8_t tagged;
    /* Copy fixed-length attributes from the packet */
    memcpy(&(dev->scanType), packet + WENDIGO_OFFSET_WIFI_SCANTYPE, sizeof(uint8_t));
    memcpy(dev->mac, packet + WENDIGO_OFFSET_WIFI_MAC, MAC_BYTES);
    memcpy(&(dev->radio.ap.channel), packet + WENDIGO_OFFSET_WIFI_CHANNEL, sizeof(uint8_t));
    memcpy(&(dev->rssi), packet + WENDIGO_OFFSET_WIFI_RSSI, sizeof(int16_t));
    memcpy(&(dev->radio.ap.authmode), packet + WENDIGO_OFFSET_AP_AUTH_MODE, sizeof(uint8_t));
    /* Ignore lastSeen */
    // memcpy(&(dev->lastSeen.tv_sec), buffer + WENDIGO_OFFSET_WIFI_LASTSEEN, sizeof(int64_t));
    memcpy(&tagged, packet + WENDIGO_OFFSET_WIFI_TAGGED, sizeof(uint8_t));
    dev->tagged = (tagged == 1);
    dev->radio.ap.stations_count = sta_count;
    memcpy(dev->radio.ap.ssid, packet + WENDIGO_OFFSET_AP_SSID, ssid_len);

    /* Retrieve stations_count MAC addresses */
    uint8_t buffIndex = WENDIGO_OFFSET_AP_SSID + ssid_len;
    uint8_t** stations = NULL;
    if(dev->radio.ap.stations_count > 0) {
        stations = malloc(sizeof(uint8_t*) * dev->radio.ap.stations_count);
        if(stations == NULL) {
            free(dev);
            FURI_LOG_T(WENDIGO_TAG, "End parseBufferWifiAp() - Unable to malloc() stations[]");
            return packetLen;
        }
        for(uint8_t staIdx = 0; staIdx < dev->radio.ap.stations_count; ++staIdx) {
            stations[staIdx] = malloc(MAC_BYTES);
            if(stations[staIdx] == NULL) {
                buffIndex += MAC_BYTES;
                continue; /* Progress to the next station... Who knows, maybe it'll work */
            }
            memcpy(stations[staIdx], packet + buffIndex, MAC_BYTES);
            buffIndex += MAC_BYTES;
        }
    }
    /* buffIndex should now point to the packet terminator */
    if(memcmp(packet + buffIndex, PACKET_TERM, PREAMBLE_LEN)) {
        char* bytesFound = malloc(3 * PREAMBLE_LEN);
        char* popupMsg = malloc((3 * PREAMBLE_LEN) + 30);
        if(bytesFound != NULL && popupMsg != NULL) {
            bytes_to_string(packet + buffIndex, PREAMBLE_LEN, bytesFound);
            snprintf(
                popupMsg, (3 * PREAMBLE_LEN) + 30, "Expected end of packet, found %s", bytesFound);
            popupMsg[3 * PREAMBLE_LEN + 29] = '\0';
            // Popup disabled while debugging device list wendigo_display_popup(app, "AP
            // Packet Error", popupMsg);
            wendigo_log_with_packet(MSG_ERROR, popupMsg, packet, packetLen);
        };
        if(bytesFound != NULL) {
            free(bytesFound);
        }
        if(popupMsg != NULL) {
            free(popupMsg);
        }
        if(stations != NULL) {
            for(uint8_t i = 0; i < dev->radio.ap.stations_count; ++i) {
                if(stations[i] != NULL) {
                    free(stations[i]);
                }
            }
            free(stations);
        }
    } else {
        if(dev->radio.ap.stations_count > 0) {
            dev->radio.ap.stations = stations;
        }
        wendigo_add_device(app, dev);
    }
    wendigo_free_device(dev);
    FURI_LOG_T(WENDIGO_TAG, "End parseBufferWifiAp()");
    return packetLen;
}

uint16_t parseBufferWifiSta(WendigoApp* app, uint8_t* packet, uint16_t packetLen) {
    FURI_LOG_T(WENDIGO_TAG, "Start parseBufferWifiSta()");
    /* This flag allows us to skip validation that the packet is the correct
     * length. Used when the Preferred Network List can't be malloc()d, to
     * allow the rest of the packet to be used. */
    bool skip_validation = false;
    uint8_t expectedLen = WENDIGO_OFFSET_STA_AP_SSID + PREAMBLE_LEN;
    if(packetLen < expectedLen) {
        /* Packet is too short - Log the issue along with the current packet */
        char* shortMsg = malloc(60);
        if(shortMsg != NULL) {
            snprintf(
                shortMsg,
                60,
                "STA packet too short: Expected %d, actual %d. Skipping...",
                expectedLen,
                packetLen);
            wendigo_log_with_packet(MSG_ERROR, shortMsg, packet, packetLen);
            /* Also display a popup */
            // Popup disabled while debugging device list wendigo_display_popup(app,
            // "STA packet too short", shortMsg);
            free(shortMsg);
        }
        FURI_LOG_T(WENDIGO_TAG, "End parseBufferWifiSta() - Short packet");
        return packetLen;
    }
    /* Get ap_ssid_len and pnl_count to validate the full packet size */
    uint8_t ap_ssid_len;
    uint8_t pnl_count;
    memcpy(&ap_ssid_len, packet + WENDIGO_OFFSET_STA_AP_SSID_LEN, sizeof(uint8_t));
    expectedLen += ap_ssid_len;
    memcpy(&pnl_count, packet + WENDIGO_OFFSET_STA_PNL_COUNT, sizeof(uint8_t));
    /* This still won't be the full packet length because each PNL has a 1-byte
        length and a length-byte SSID */
    expectedLen += pnl_count;
    if(packetLen < expectedLen) {
        /* Packet too short - Log and return */
        char* shortMsg = malloc(57);
        if(shortMsg != NULL) {
            snprintf(
                shortMsg,
                57,
                "STA packet too short for SSID: Expected %d, actual %d.",
                expectedLen,
                packetLen);
            wendigo_log_with_packet(MSG_ERROR, shortMsg, packet, packetLen);
            free(shortMsg);
        }
        FURI_LOG_T(WENDIGO_TAG, "End parseBufferWifiSta() - Packet too short for SSID");
        return packetLen;
    }
    wendigo_device* dev = malloc(sizeof(wendigo_device));
    if(dev == NULL) {
        return packetLen;
    }
    /* Initialise all pointers */
    dev->view = NULL;
    dev->radio.sta.saved_networks = NULL;
    memcpy(dev->radio.sta.apMac, nullMac, MAC_BYTES);
    /* Temp variables for attributes that need to be transformed */
    uint8_t tagged;
    /* Copy fixed-length attributes */
    memcpy(&(dev->scanType), packet + WENDIGO_OFFSET_WIFI_SCANTYPE, sizeof(uint8_t));
    memcpy(dev->mac, packet + WENDIGO_OFFSET_WIFI_MAC, MAC_BYTES);
    memcpy(&(dev->radio.sta.channel), packet + WENDIGO_OFFSET_WIFI_CHANNEL, sizeof(uint8_t));
    memcpy(&(dev->rssi), packet + WENDIGO_OFFSET_WIFI_RSSI, sizeof(int16_t));
    /* Ignore lastSeen */
    // memcpy(&(dev->lastSeen.tv_sec), buffer + WENDIGO_OFFSET_WIFI_LASTSEEN,
    // sizeof(int64_t));
    memcpy(&tagged, packet + WENDIGO_OFFSET_WIFI_TAGGED, sizeof(uint8_t));
    dev->tagged = (tagged == 1);
    dev->radio.sta.saved_networks_count = pnl_count;
    memcpy(dev->radio.sta.apMac, packet + WENDIGO_OFFSET_STA_AP_MAC, MAC_BYTES);
    char ap_ssid[MAX_SSID_LEN + 1];
    bzero(ap_ssid, MAX_SSID_LEN + 1);
    memcpy(ap_ssid, packet + WENDIGO_OFFSET_STA_AP_SSID, ap_ssid_len);
    ap_ssid[ap_ssid_len] = '\0';
    /* Do I want to do anything with ap_ssid? Not right now... */
    FURI_LOG_D(
        "parseBufferWifiSta()",
        "STA %02x:%02x:%02x:%02x:%02x:%02x has a PNL of %d",
        dev->mac[0],
        dev->mac[1],
        dev->mac[2],
        dev->mac[3],
        dev->mac[4],
        dev->mac[5],
        pnl_count);
    if(pnl_count > 0) {
        /* Retrieve pnl_count saved networks */
        dev->radio.sta.saved_networks = malloc(sizeof(char*) * pnl_count);
        if(dev->radio.sta.saved_networks == NULL) {
            /* Alert insufficient memory - But allow the device to be added
             * anyway, just without its Preferred Network List. */
            dev->radio.sta.saved_networks_count = 0;
            skip_validation = true;
            char* errMsg = malloc(39);
            if(errMsg == NULL) {
                wendigo_log_with_packet(MSG_ERROR, "Can't allocate PNL.", packet, packetLen);
            } else {
                snprintf(
                    errMsg, 39, "Failed to allocate %d bytes for PNL.", sizeof(char*) * pnl_count);
                wendigo_log_with_packet(MSG_WARN, errMsg, packet, packetLen);
                free(errMsg);
            }
        }
    }
    uint16_t packet_idx = WENDIGO_OFFSET_STA_AP_SSID + ap_ssid_len;
    uint8_t pnl_idx = 0;
    uint8_t this_pnl_len;
    bool short_pkt = false;
    while(pnl_idx < dev->radio.sta.saved_networks_count && !short_pkt) {
        /* Ensure the packet is big enough */
        if(packetLen >= (packet_idx + PREAMBLE_LEN)) {
            /* Get current SSID length */
            memcpy(&this_pnl_len, packet + packet_idx, sizeof(uint8_t));
            ++packet_idx;
            /* Make sure the packet is big enough to contain the SSID */
            if(packetLen >= (packet_idx + this_pnl_len + PREAMBLE_LEN)) {
                if(this_pnl_len == 0) {
                    dev->radio.sta.saved_networks[pnl_idx] = NULL;
                } else {
                    dev->radio.sta.saved_networks[pnl_idx] = malloc(this_pnl_len + 1);
                    if(dev->radio.sta.saved_networks[pnl_idx] != NULL) {
                        memcpy(
                            dev->radio.sta.saved_networks[pnl_idx],
                            packet + packet_idx,
                            this_pnl_len);
                        FURI_LOG_D(
                            "parseBufferWifiSta()",
                            "Retrieved STA %02x:%02x:%02x:%02x:%02x:%02x PNL %d: %s",
                            dev->mac[0],
                            dev->mac[1],
                            dev->mac[2],
                            dev->mac[3],
                            dev->mac[4],
                            dev->mac[5],
                            pnl_idx,
                            dev->radio.sta.saved_networks[pnl_idx]);
                    }
                    packet_idx += this_pnl_len;
                }
                ++pnl_idx;
            } else { // packetLen < (packet_idx + this_pnl_len + PREAMBLE_LEN)
                char* shortMsg = malloc(103);
                if(shortMsg == NULL) {
                    wendigo_log_with_packet(
                        MSG_DEBUG, "Packet too short for specified PNL length.", packet, packetLen);
                } else {
                    snprintf(
                        shortMsg,
                        103,
                        "Packet too short: packetLen (%d) <= (packet_idx (%d) + this_pnl_len (%d) + PREAMBLE_LEN (%d)).",
                        packetLen,
                        packet_idx,
                        this_pnl_len,
                        PREAMBLE_LEN);
                    wendigo_log_with_packet(MSG_DEBUG, shortMsg, packet, packetLen);
                    free(shortMsg);
                }
                short_pkt = true;
            }
        } else { // packetLen < (packet_idx + PREAMBLE_LEN)
            char* shortMsg = malloc(73);
            if(shortMsg == NULL) {
                wendigo_log_with_packet(
                    MSG_DEBUG, "Packet too short to search for a PNL.", packet, packetLen);
            } else {
                snprintf(
                    shortMsg,
                    73,
                    "Packet too short to contain another PNL. pnl_idx: %d packet_idx: %d.",
                    pnl_idx,
                    packet_idx);
                wendigo_log_with_packet(MSG_DEBUG, shortMsg, packet, packetLen);
                free(shortMsg);
            }
            short_pkt = true;
        }
    }
    if(short_pkt) {
        wendigo_log_with_packet(
            MSG_ERROR, "STA packet too short to extract PNL.", packet, packetLen);
        if(pnl_idx > 0) {
            for(uint8_t i = 0; i < pnl_idx; ++i) {
                if(dev->radio.sta.saved_networks[i] != NULL) {
                    free(dev->radio.sta.saved_networks[i]);
                }
            }
        }
        if(dev->radio.sta.saved_networks != NULL) {
            free(dev->radio.sta.saved_networks);
        }
        dev->radio.sta.saved_networks_count = 0;
        /* Otherwise we should find the packet terminator at packet_idx */
    } else if(!skip_validation && memcmp(PACKET_TERM, packet + packet_idx, PREAMBLE_LEN)) {
        // Popup disabled while debugging device list        wendigo_display_popup(
        //            app, "STA Packet", "STA Packet terminator not found where
        //            expected.");
        /* Log the packet so the cause can hopefully be found */
        wendigo_log_with_packet(
            MSG_ERROR,
            "STA packet terminator not found where expected, skipping.",
            packet,
            packetLen);
    } else {
        wendigo_add_device(app, dev);
    }
    wendigo_free_device(dev);
    FURI_LOG_T(WENDIGO_TAG, "End parseBufferWifiSta()");
    return packetLen;
}

/** Parse a version packet and display both Flipper- and ESP32-Wendigo versions.
 * Returns the number of bytes consumed from the buffer - DOES NOT remove
 * consumed bytes, this must be handled by the calling function.
 */
uint16_t parseBufferVersion(WendigoApp* app, uint8_t* packet, uint16_t packetLen) {
    FURI_LOG_T(WENDIGO_TAG, "Start parseBufferVersion()");
    /* Find the end-of-packet sequence to determine version string length */
    uint16_t endSeq = packetLen - PREAMBLE_LEN; /* To reach first byte in seq */
    /* Add space for Flipper-Wendigo version information. */
    uint16_t messageLen = endSeq + 26 + strlen(FLIPPER_WENDIGO_VERSION);
    char* versionStr = realloc(wendigo_popup_text, messageLen);
    if(versionStr == NULL) {
        char* msg = malloc(60);
        if(msg == NULL) {
            wendigo_log_with_packet(
                MSG_ERROR,
                "Unable to reallocate memory to display version packet.",
                packet,
                packetLen);
        } else {
            snprintf(
                msg,
                60,
                "Unable to reallocate %d bytes to display version packet.",
                sizeof(char) * messageLen);
            wendigo_log_with_packet(MSG_ERROR, msg, packet, packetLen);
            free(msg);
        }
        // Consume this message
        return endSeq + PREAMBLE_LEN;
    }
    wendigo_popup_text = versionStr;
    snprintf(
        wendigo_popup_text, messageLen, "Flipper-Wendigo v%s\n ESP32-", FLIPPER_WENDIGO_VERSION);
    memcpy(wendigo_popup_text + 25 + strlen(FLIPPER_WENDIGO_VERSION), packet, endSeq);
    wendigo_popup_text[messageLen - 1] = '\0'; /* Just in case */
    wendigo_display_popup(app, "Wendigo Version", wendigo_popup_text);
    FURI_LOG_T(WENDIGO_TAG, "End parseBufferVersion()");
    return packetLen;
}

/** Parses a MAC packet and updates app->interfaces[]. The packet can contain zero or more
 * MACs. Packet structure:
 * * Preamble (4 bytes)
 * * Interface count (1 byte), then for each interface
 *   * Interface type (1 byte, WendigoMAC enum)
 *   * Interface MAC (6 bytes)
 * * Packet terminator (4 bytes)
 */
uint16_t parseBufferMAC(WendigoApp* app, uint8_t* packet, uint16_t packetLen) {
    FURI_LOG_T(WENDIGO_TAG, "Start parseBufferMAC()");
    if(packetLen < (2 * PREAMBLE_LEN)) {
        wendigo_log_with_packet(MSG_ERROR, "MAC packet too short", packet, packetLen);
        return packetLen;
    }
    uint8_t interface_count;
    memcpy(&interface_count, packet + WENDIGO_OFFSET_MAC_IF_COUNT, sizeof(uint8_t));
    uint8_t expected_len = (2 * PREAMBLE_LEN) + (interface_count * (MAC_BYTES + 1)) + 1;
    if(packetLen != expected_len) {
        char* msg = malloc(55);
        if(msg == NULL) {
            wendigo_log_with_packet(
                MSG_ERROR, "MAC packet length different from expected.", packet, packetLen);
        } else {
            snprintf(
                msg,
                55,
                "MAC packet is wrong length. Expected %d actual %d.",
                expected_len,
                packetLen);
            wendigo_log_with_packet(MSG_ERROR, msg, packet, packetLen);
            free(msg);
        }
        return packetLen;
    }
    /* The packet's the right length - Process its contents */
    uint8_t thisMac[MAC_BYTES];
    uint8_t thisIface;
    uint16_t offset = WENDIGO_OFFSET_MAC_IF_COUNT + 1;
    uint8_t interface = 0;
    while(interface < interface_count && offset + MAC_BYTES + PREAMBLE_LEN < packetLen) {
        /* Get the interface type */
        memcpy(&thisIface, packet + offset, sizeof(uint8_t));
        ++offset;
        /* Get the MAC */
        memcpy(thisMac, packet + offset, MAC_BYTES);
        offset += MAC_BYTES;
        if(thisIface == WENDIGO_MAC_WIFI) {
            memcpy(app->interfaces[IF_WIFI].mac_bytes, thisMac, MAC_BYTES);
            app->interfaces[IF_WIFI].initialised = true;
        } else if(thisIface == WENDIGO_MAC_BLUETOOTH) {
            memcpy(app->interfaces[IF_BT_CLASSIC].mac_bytes, thisMac, MAC_BYTES);
            memcpy(app->interfaces[IF_BLE].mac_bytes, thisMac, MAC_BYTES);
            app->interfaces[IF_BT_CLASSIC].initialised = true;
            app->interfaces[IF_BLE].initialised = true;
        } else if(thisIface == WENDIGO_MAC_BASE) {
            /* Custom MAC isn't reflected in the get_mac function we're
             * using, but SoftAP is base MAC + 1, Bluetooth base MAC + 2.
             * Calculate them. */
            // TODO: Update MAC retrieval to also include base MAC, and figure out how to display it effectively
        } else {
            wendigo_log_with_packet(
                MSG_WARN, "MAC packet specifies unknown interface.", packet, packetLen);
        }
        ++interface;
    }
    /* Finally, confirm that offset now points to the packet terminator */
    if(memcmp(packet + offset, PACKET_TERM, PREAMBLE_LEN)) {
        wendigo_log_with_packet(
            MSG_ERROR, "MAC packet terminator not found where expected.", packet, packetLen);
    }
    /* Invoke the 'MAC received' callback if there is one */
    wendigo_mac_rcvd_callback(app);
    FURI_LOG_T(WENDIGO_TAG, "End parseBufferMAC()");
    return packetLen;
}

uint16_t parseBufferChannels(WendigoApp* app, uint8_t* packet, uint16_t packetLen) {
    UNUSED(app);
    FURI_LOG_T(WENDIGO_TAG, "Start parseBufferChannels()");
    uint8_t channels_count;
    uint8_t* channels = NULL;
    memcpy(&channels_count, packet + WENDIGO_OFFSET_CHANNEL_COUNT, sizeof(uint8_t));
    if(channels_count > 0) {
        channels = malloc(channels_count);
        if(channels != NULL) {
            memcpy(channels, packet + WENDIGO_OFFSET_CHANNELS, channels_count);
        }
    }
    if(memcmp(PACKET_TERM, packet + WENDIGO_OFFSET_CHANNELS + channels_count, PREAMBLE_LEN)) {
        //        wendigo_display_popup(app, "Channel Packet", "Channel packet is
        //        unexpected length");
        wendigo_log_with_packet(
            MSG_ERROR, "Channels packet terminator not found where expected", packet, packetLen);
        FURI_LOG_T(WENDIGO_TAG, "End parseBufferChannels() - Terminator not found");
        return packetLen;
    }
    // TODO: Do something with channels
    FURI_LOG_T(WENDIGO_TAG, "End parseBufferChannels()");
    return packetLen;
}

/** Parse a status packet and display in the status view.
 * This function requires that Wendigo_AppViewStatus be the
 * currently-displayed view (otherwise the packet is discarded).
 */
uint16_t parseBufferStatus(WendigoApp* app, uint8_t* packet, uint16_t packetLen) {
    FURI_LOG_T(WENDIGO_TAG, "Start parseBufferStatus()");
    /* Ignore the packet if the status scene isn't displayed */
    if(app->current_view != WendigoAppViewStatus) {
        return packetLen;
    }
    wendigo_scene_status_begin_layout(app);
    // TODO: Document the packet somewhere. Less straightforward than the
    // Bluetooth packet because it's a dynamic size, but it needs *something*
    uint8_t attribute_count;
    uint8_t attribute_name_len;
    uint8_t attribute_value_len;
    char* attribute_name = NULL;
    char* attribute_value = NULL;
    uint16_t offset = PREAMBLE_LEN;
    memcpy(&attribute_count, packet + offset, sizeof(uint8_t));
    ++offset;
    for(uint8_t i = 0; i < attribute_count; ++i) {
        /* Parse attribute name length */
        memcpy(&attribute_name_len, packet + offset, sizeof(uint8_t));
        if(attribute_name_len == 0) {
            wendigo_log_with_packet(
                MSG_ERROR,
                "Status packet contained an attribute of length 0, skipping.",
                packet,
                packetLen);
            return packetLen;
        }
        ++offset;
        /* Name */
        attribute_name = malloc(attribute_name_len + 1);
        if(attribute_name == NULL) {
            char* msg = malloc(51);
            if(msg != NULL) {
                snprintf(
                    msg,
                    51,
                    "Failed to allocate %d bytes for status attribute.",
                    attribute_name_len + 1);
                wendigo_log_with_packet(MSG_ERROR, msg, packet, packetLen);
                free(msg);
            }
            return packetLen;
        }
        memcpy(attribute_name, packet + offset, attribute_name_len);
        attribute_name[attribute_name_len] = '\0';
        offset += attribute_name_len;
        /* Attribute value length */
        memcpy(&attribute_value_len, packet + offset, sizeof(uint8_t));
        ++offset;
        /* It's valid for this to have a length of 0 - attribute_value will be "" */
        attribute_value = malloc(attribute_value_len + 1);
        if(attribute_value == NULL) {
            char* msg = malloc(50);
            if(msg != NULL) {
                snprintf(
                    msg,
                    50,
                    "Failed to allocate %d bytes for attribute value.",
                    attribute_value_len + 1);
                wendigo_log_with_packet(MSG_ERROR, msg, packet, packetLen);
                free(msg);
            }
            free(attribute_name);
            return packetLen;
        }
        memcpy(attribute_value, packet + offset, attribute_value_len);
        attribute_value[attribute_value_len] = '\0';
        offset += attribute_value_len;
        /* Send the attribute off for validation and display */
        process_and_display_status_attribute(app, attribute_name, attribute_value);
        free(attribute_name);
        free(attribute_value);
    }
    wendigo_scene_status_finish_layout(app);

    /* buffer + offset should now point to the end of packet sequence */
    if(memcmp(PACKET_TERM, packet + offset, PREAMBLE_LEN)) {
        wendigo_log_with_packet(
            MSG_WARN, "Status packet terminator not found where expected.", packet, packetLen);
        return packetLen;
    }
    FURI_LOG_T(WENDIGO_TAG, "End parseBufferStatus()");
    return offset + PREAMBLE_LEN;
}

/** When the end of a packet is reached this function is called to parse the
 *  buffer and take the appropriate action. We expect to see one of the
 * packet preambles defined in wendigo_common_defs.c
 *  The end of packet is marked by 0xAA,0xBB,0xCC,0xDD.
 */
void parsePacket(WendigoApp* app, uint8_t* packet, uint16_t packetLen) {
    FURI_LOG_T(WENDIGO_TAG, "Begin parsePacket(len: %d)", packetLen);
    /* Update buffer last received time */
    app->last_packet = furi_hal_rtc_get_timestamp();

    /* Development: Dump packet for inspection */
    //wendigo_log_with_packet(MSG_DEBUG, "parsePacket() received packet", packet, packetLen);

    if(packetLen < (2 * PREAMBLE_LEN)) {
        wendigo_log_with_packet(MSG_WARN, "parsePacket(): Packet too short:", packet, packetLen);
        return;
    }

    /* Pass packet off to the relevant packet parser */
    if(!memcmp(PREAMBLE_BT_BLE, packet, PREAMBLE_LEN)) {
        parseBufferBluetooth(app, packet, packetLen);
    } else if(!memcmp(PREAMBLE_WIFI_AP, packet, PREAMBLE_LEN)) {
        parseBufferWifiAp(app, packet, packetLen);
    } else if(!memcmp(PREAMBLE_WIFI_STA, packet, PREAMBLE_LEN)) {
        parseBufferWifiSta(app, packet, packetLen);
    } else if(!memcmp(PREAMBLE_STATUS, packet, PREAMBLE_LEN)) {
        parseBufferStatus(app, packet, packetLen);
    } else if(!memcmp(PREAMBLE_VER, packet, PREAMBLE_LEN)) {
        parseBufferVersion(app, packet, packetLen);
    } else if(!memcmp(PREAMBLE_CHANNELS, packet, PREAMBLE_LEN)) {
        parseBufferChannels(app, packet, packetLen);
    } else if(!memcmp(PREAMBLE_MAC, packet, PREAMBLE_LEN)) {
        parseBufferMAC(app, packet, packetLen);
    } else {
        wendigo_log_with_packet(
            MSG_WARN, "Packet doesn't have a valid preamble", packet, packetLen);
    }
    FURI_LOG_T(WENDIGO_TAG, "End parsePacket()");
}

/** Callback invoked when UART data is received. When an end-of-packet sequence
 *  is received the buffer is sent to the appropriate handler for display. At the
 *  time of writing a similar callback exists in wendigo_scene_console_output,
 *  that is being retained through initial stages of development because it
 *  provides a useful way to run ad-hoc tests from Flipper. Eventually the entire
 *  scene wendigo_scene_console_output will likely be removed, until then
 *  collision can be avoided by limiting its use, ensuring that
 *  wendigo_uart_set_handle_rx_data_cb() is called after using the console to
 *  re-establish this function as the UART callback.
 */
void wendigo_scan_handle_rx_data_cb(uint8_t* buf, size_t len, void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scan_handle_rx_data_cb()");
    furi_assert(context);
    WendigoApp* app = context;

    /* Get a mutex lock on the buffer */
    FuriStatus status = furi_mutex_acquire(app->bufferMutex, FuriWaitForever);
    if(status != FuriStatusOk) {
        uint8_t statusLen = 29;
        char* statusStr = malloc(statusLen);
        if(statusStr == NULL) {
            // TODO: Convert to wendigo_log() after it supports variable arguments
            FURI_LOG_W(
                "wendigo_scan_handle_rx_data_cb()",
                "furi_mutex_acquire() returned %d, unable to allocate %d bytes to display a string representation.",
                status,
                statusLen);
        } else {
            furi_status_to_string(status, statusStr, statusLen);
            statusStr[statusLen - 1] = '\0'; /* Just in case */
            // wendigo_scan_handle_rx_data_cb(): furi_mutex_acquire() returned %s. 66+strlen
            uint8_t msgLen = 66;
            if(statusStr != NULL) {
                msgLen += strlen(statusStr);
            }
            char* msg = malloc(msgLen);
            if(msg == NULL) {
                FURI_LOG_W(
                    "wendigo_scan_handle_rx_data_cb()",
                    "furi_mutex_acquire() returned %s and Wendigo was unable to allocate %d bytes for log message.",
                    (statusStr == NULL) ? "NULL" : statusStr,
                    msgLen);
            } else {
                snprintf(
                    msg,
                    msgLen,
                    "wendigo_scan_handle_rx_data_cb(): furi_mutex_acquire() returned %s.",
                    statusStr);
                wendigo_log(MSG_WARN, msg);
                free(msg);
            }
            free(statusStr);
        }
    }

    /* Extend the buffer if necessary */
    if(bufferLen + len >= bufferCap) {
        /* Extend it by the larger of len and BUFFER_INC_CAPACITY_BY bytes to avoid constant realloc()s */
        uint16_t increase_by = (len > BUFFER_INC_CAPACITY_BY) ? len : BUFFER_INC_CAPACITY_BY;
        uint16_t newCapacity = bufferCap + increase_by;
        FURI_LOG_D(WENDIGO_TAG, "Expanding buffer from %d to %d", bufferCap, newCapacity);
        /* Will this exceed the maximum capacity? */
        if(newCapacity > BUFFER_MAX_SIZE) {
            /* The buffer has reached the maximum-configured size. This is an exceptional circumstance,
             * it doesn't need to be handled elegantly - The only time I've heard of it happening was
             * someone running Flipper-Wendigo with an ESP32 running Marauder. Marauder was sending data
             * to Flipper, but a packet terminator was never received and the stack eventually overflowed.
             */
            FURI_LOG_D(WENDIGO_TAG, "Buffer too large, resetting...");
            bzero(buffer, bufferCap);
            bufferLen = 0;
            /* Release mutex before returning */
            furi_mutex_release(app->bufferMutex);
            return;
            /* Remove increase_by bytes from beginning of buffer[] instead */
            uint8_t* new_start = buffer + increase_by;
            memcpy(buffer, new_start, bufferLen - increase_by);
            memset(buffer + bufferLen - increase_by, '\0', increase_by);
            bufferLen -= increase_by;
        } else {
            uint8_t* newBuffer =
                realloc(buffer, newCapacity); // Behaves like malloc() when buffer==NULL
            if(newBuffer == NULL) {
                /* Out of memory */
                // Unable to expand UART buffer to %d bytes, abandoning rx.
                char* strMsg = malloc(59);
                if(strMsg == NULL) {
                    wendigo_log(MSG_ERROR, "Unable to allocate memory for UART buffer.");
                } else {
                    snprintf(
                        strMsg,
                        59,
                        "Unable to expand UART buffer to %d bytes, abandoning rx.",
                        newCapacity);
                    wendigo_log(MSG_ERROR, strMsg);
                    free(strMsg);
                }
                /* Release buffer mutex */
                furi_mutex_release(app->bufferMutex);
                return;
            }
            buffer = newBuffer;
            bufferCap = newCapacity;
        }
    }
    memcpy(buffer + bufferLen, buf, len);
    bufferLen += len;

    /* Parse any complete packets we have received */
    uint8_t* packet;
    uint16_t packetLen;
    uint16_t startIdx = start_of_packet(buffer, bufferLen);
    uint16_t endIdx = end_of_packet(buffer, bufferLen);
    uint8_t** packets = NULL;
    uint16_t* packetSize = NULL;
    uint8_t packetsCount = 0;
    bool interrupted = false;
    while(startIdx < endIdx && endIdx < bufferLen && !interrupted) {
        /* We have a complete packet - extract it for parsing */
        packetLen = endIdx - startIdx + 1;
        packet = buffer + startIdx;
        /* Copy the packet into packets[] so we can deal with it later */
        uint8_t** new_packets = realloc(packets, sizeof(uint8_t*) * (packetsCount + 1));
        uint16_t* new_packetSize = realloc(packetSize, sizeof(uint16_t) * (packetsCount + 1));
        if(new_packets == NULL || new_packetSize == NULL) {
            wendigo_log_with_packet(
                MSG_ERROR,
                "UART RX: Unable to allocate memory for packets cache.",
                packet,
                packetLen);
            /* Since we can't allocate any more memory exit the loop now */
            interrupted = true;
            break;
        }
        new_packetSize[packetsCount] = packetLen;
        new_packets[packetsCount] = malloc(packetLen);
        if(new_packets[packetsCount] == NULL) {
            /* Bugger, continue with what we've gotten so far */
            char* errorMsg = malloc(57);
            if(errorMsg == NULL) {
                wendigo_log_with_packet(
                    MSG_ERROR,
                    "UART RX: Unable to allocate memory to cache packet.",
                    packet,
                    packetLen);
            } else {
                snprintf(
                    errorMsg,
                    57,
                    "UART RX: Unable to allocate %d bytes to cache packet.",
                    packetLen);
                wendigo_log_with_packet(MSG_ERROR, errorMsg, packet, packetLen);
                free(errorMsg);
            }
            /* Since we can't allocate more memory exit the loop now */
            interrupted = true;
            /* First shrink packets[] and packetSize[] back to their previous size */
            uint8_t** new_packets2 = realloc(new_packets, sizeof(uint8_t*) * packetsCount);
            uint16_t* new_packetSize2 = realloc(new_packetSize, sizeof(uint16_t) * packetsCount);
            if(new_packets2 == NULL || new_packetSize2 == NULL) {
                /* Because this is shrinking in place it's tempting to not even check for failure */
                wendigo_log(
                    MSG_ERROR,
                    "UART RX: Unable to shrink packets[] and packetSize[] after a failed malloc().");
            } else {
                new_packets = new_packets2;
                new_packetSize = new_packetSize2;
            }
            break;
        }
        memcpy(new_packets[packetsCount], packet, packetLen);
        packets = new_packets;
        packetSize = new_packetSize;
        ++packetsCount;

        /* Remove this packet, and any preceding junk, from the buffer */
        memset(buffer, 0, packetLen + startIdx);
        /* Get the next start and end indices, ready for the next iteration */
        startIdx = start_of_packet(buffer, bufferLen);
        endIdx = end_of_packet(buffer, bufferLen);
    }
    /* Have we been able to empty the buffer? */
    if(!interrupted && startIdx == bufferLen && endIdx == bufferLen) {
        /* Not having a start or end packet sequence is a start - Check for all NULL
         * bytes in buffer */
        // TODO: Improve on that by searching backwards through buffer to find something that can't be a preamble, and removing everything prior
        //       Could also look for any "islands" of bytes that are too short to be a packet
        bool bytesFound = false;
        for(uint16_t i = 0; i < bufferLen && !bytesFound; ++i) {
            if(buffer[i] != 0) {
                bytesFound = true;
            }
        }
        if(!bytesFound) {
            // FURI_LOG_D("wendigo_scan_handle_rx_data_cb()",
            //     "Buffer is empty, resetting bufferLen");
            bufferLen = 0;
        }
    }
    /* Release the mutex and parse the packets */
    // TODO: Replace with with a message queue.
    furi_mutex_release(app->bufferMutex);
    for(uint8_t i = 0; i < packetsCount; ++i) {
        //wendigo_log_with_packet(MSG_DEBUG, "wendigo_scan_handle_rx_data() Parsing packet", packets[i], packetSize[i]);
        parsePacket(app, packets[i], packetSize[i]);
        free(packets[i]);
    }
    if(packetsCount > 0) {
        free(packets);
        free(packetSize);
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scan_handle_rx_data_cb()");
}

void wendigo_free_uart_buffer() {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_free_uart_buffer()");
    if(bufferCap > 0) {
        free(buffer);
        bufferLen = 0;
        bufferCap = 0;
        buffer = NULL;
    }
    /* Used by the version command */
    if(wendigo_popup_text != NULL) {
        free(wendigo_popup_text);
        wendigo_popup_text = NULL;
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_free_uart_buffer()");
}

void wendigo_log(MsgType logType, char* message) {
    if(message == NULL) {
        return;
    }
    switch(logType) {
    case MSG_ERROR:
        FURI_LOG_E(WENDIGO_TAG, message);
        break;
    case MSG_WARN:
        FURI_LOG_W(WENDIGO_TAG, message);
        break;
    case MSG_INFO:
        FURI_LOG_I(WENDIGO_TAG, message);
        break;
    case MSG_DEBUG:
        FURI_LOG_D(WENDIGO_TAG, message);
        break;
    case MSG_TRACE:
        FURI_LOG_T(WENDIGO_TAG, message);
        break;
    default:
        break;
    }
}

void wendigo_log_with_packet(MsgType logType, char* message, uint8_t* packet, uint16_t packet_size) {
    if(packet == NULL || packet_size == 0) {
        return;
    }
    uint16_t messageLen = 3 * packet_size;
    uint8_t commentLen;
    if(message == NULL) {
        commentLen = 0;
    } else {
        messageLen += strlen(message) + 1;
        commentLen = strlen(message) + 1; /* Account for the newline */
    }
    char* finalMessage = malloc(messageLen);
    if(finalMessage != NULL) {
        if(message != NULL) {
            memcpy(
                finalMessage,
                message,
                strlen(message)); // Breaks when message is NULL. So do the next 2 lines
            finalMessage[strlen(message)] = '\n';
        }
        bytes_to_string(packet, packet_size, finalMessage + commentLen);
    }
    /* Just in case my counting is out */
    finalMessage[messageLen - 1] = '\0';
    wendigo_log(logType, finalMessage);
    free(finalMessage);
}
