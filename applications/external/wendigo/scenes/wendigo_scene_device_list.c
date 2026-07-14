#include "../wendigo_scan.h"

/** Public method from wendigo_scene_device_detail.c */
extern void wendigo_scene_device_detail_set_device(wendigo_device* d);
/** Public method from wendigo_scene_pnl_list.c */
extern void wendigo_scene_pnl_list_set_device(wendigo_device* d, WendigoApp* app);
/** Internal method - I don't wan't to move all calling functions below it */
static void wendigo_scene_device_list_var_list_change_callback(VariableItem* item);

/** TODO: For some obscene reason the ifndef barrier isn't stopping these
 *  from showing up in every single object file. No longer shared.
 */
char* wifi_auth_mode_strings[] = {
    "Open",
    "WEP",
    "WPA",
    "WPA2",
    "WPA+WPA2",
    "EAP",
    "EAP",
    "WPA3",
    "WPA2+WPA3",
    "WAPI",
    "OWE",
    "WPA3 Enterprise 192-bit",
    "WPA3 EXT",
    "WPA3 EXT Mixed Mode",
    "DPP",
    "WPA3 Enterprise",
    "WPA3 Enterprise Transition",
    "Unknown"};

/** Enum to index the options menu for devices */
enum wendigo_device_list_bt_options {
    WendigoOptionBTRSSI = 0,
    WendigoOptionBTTagUntag,
    WendigoOptionBTScanType,
    WendigoOptionBTCod,
    WendigoOptionBTLastSeen,
    WendigoOptionsBTCount
};

enum wendigo_device_list_ap_options {
    WendigoOptionAPRSSI = 0,
    WendigoOptionAPTagUntag,
    WendigoOptionAPScanType,
    WendigoOptionAPStaCount,
    WendigoOptionAPAuthMode,
    WendigoOptionAPChannel,
    WendigoOptionAPLastSeen,
    WendigoOptionsAPCount
};

enum wendigo_device_list_sta_options {
    WendigoOptionSTARSSI = 0,
    WendigoOptionSTATagUntag,
    WendigoOptionSTAScanType,
    WendigoOptionSTASavedNetworks,
    WendigoOptionSTAAP,
    WendigoOptionSTAChannel,
    WendigoOptionSTALastSeen,
    WendigoOptionsSTACount
};

/* Enacting nested copies of these scenes, without the luxury of treating them
 * as instances of a class, is achieved by managing an array of
 * DeviceListInstances as a stack - pushing a new set of devices to the stack
 * and initialising a new set of devices for display, or popping the last
 * value off the stack and using them as the current devices to return to the
 * previously-displayed device list.
 * Implementation note: For consistency, current_devices is not pushed onto the
 * stack, unless and until a new device list is being displayed. The idiom here
 * is to push to the stack immediately before preparing a new instance of
 * current_devices, such that everything is in place by the time
 * wendigo_scene_device_list_on_enter() is called, and to pop from the stack
 * in wendigo_scene_device_list_on_exit(), where the existing current_devices[]
 * is also freed. */
uint8_t stack_counter = 0;
DeviceListInstance* stack = NULL;
DeviceListInstance current_devices;

/** Prepare current_devices for use. Provides initial values for the
 * current_devices struct.
 * `config` must be either NULL or a pointer to a DeviceListInstance that
 * provides initial values for current_devices. canfig may be freed after
 * calling - memory is allocated for  its devices if necessary - however
 * the wendigo_device instances referenced by devices[] are expected to
 * remain allocated for the duration of the scene.
 * It is not necessary to call this function more than once. If you do this
 * make sure you call wendigo_scene_device_list_free() before the second
 * call, otherwise any memory allocated to devices[] will be leaked.
 */
void wendigo_scene_device_list_init(void* config) {
    bzero(current_devices.devices_msg, sizeof(current_devices.devices_msg));
    if(config == NULL) {
        current_devices.devices = NULL;
        current_devices.devices_count = 0;
        current_devices.devices_mask = DEVICE_ALL;
        current_devices.view = WendigoAppViewDeviceList;
        current_devices.free_devices = true;
    } else {
        DeviceListInstance* cfg = (DeviceListInstance*)config;
        if(current_devices.devices_count > 0 && current_devices.devices != NULL &&
           current_devices.free_devices) {
            free(current_devices.devices);
        }
        if(cfg->devices_count > 0 && cfg->devices != NULL) {
            current_devices.devices = malloc(sizeof(wendigo_device*) * cfg->devices_count);
            if(current_devices.devices == NULL) {
                char* msg = malloc(sizeof(char) * 68);
                if(msg == NULL) {
                    wendigo_log(
                        MSG_ERROR,
                        "Unable to allocate memory for DeviceListInstance initialiser.");
                } else {
                    // Unable to allocate %d bytes for DeviceListInstance initialiser.
                    snprintf(
                        msg,
                        68,
                        "Unable to allocate %d bytes for DeviceListInstance initialiser.",
                        sizeof(wendigo_device*) * cfg->devices_count);
                    wendigo_log(MSG_ERROR, msg);
                    free(msg);
                }
                current_devices.devices_count = 0;
            } else {
                memcpy(
                    current_devices.devices,
                    cfg->devices,
                    sizeof(wendigo_device*) * cfg->devices_count);
                current_devices.devices_count = cfg->devices_count;
            }
        } else {
            current_devices.devices = NULL;
            current_devices.devices_count = 0;
        }
        current_devices.devices_mask = cfg->devices_mask;
        strncpy(
            current_devices.devices_msg, cfg->devices_msg, sizeof(current_devices.devices_msg));
        current_devices.view = cfg->view;
        current_devices.free_devices = true; /* Because we malloc()d devices[] */
    }
}

/** Clean up current_devices and stack in preparation for application exit. */
void wendigo_scene_device_list_free() {
    /* Clear current_devices */
    if(current_devices.devices_count > 0 && current_devices.devices != NULL) {
        if(current_devices.free_devices) {
            free(current_devices.devices);
        }
        current_devices.devices = NULL;
        current_devices.devices_count = 0;
        current_devices.free_devices = true;
    }
    if(stack_counter > 0 && stack != NULL) {
        /* Free components of the device stack */
        for(uint8_t i = 0; i < stack_counter; ++i) {
            if(stack[i].devices_count > 0 && stack[i].devices != NULL) {
                if(stack[i].free_devices) {
                    free(stack[i].devices);
                }
                stack[i].devices = NULL;
                stack[i].devices_count = 0;
                stack[i].free_devices = true;
            }
        }
        free(stack);
        stack = NULL;
        stack_counter = 0;
    }
}

/** Tihs alternative version of wendigo_device_is_displayed() is less efficient
 * than the original, but works correctly when the displayed device mask
 * includes DEVICE_CUSTOM.
 * This function is called automatically by wendigo_device_is_displayed() if
 * the current device mask includes DEVICE_CUSTOM - it is not necessary to
 * call this function independently.
 * Returns true if a device containing the specified MAC (dev->mac) is
 * displayed in the current scene.
 */
bool wendigo_device_is_displayed_custom(wendigo_device* dev) {
    if(dev == NULL || current_devices.devices == NULL) {
        return false;
    }
    /* Loop through current_devices.devices[] searching for dev */
    uint16_t idx;
    for(idx = 0; idx < current_devices.devices_count &&
                 (current_devices.devices[idx] == NULL ||
                  memcmp(dev->mac, current_devices.devices[idx]->mac, MAC_BYTES));
        ++idx) {
    }
    return (idx < current_devices.devices_count);
}

/** Determine whether the specified device should be displayed, based on the
 * criteria provided in wendigo_scene_device_list_set_current_devices_mask().
 * This function DOES NOT consider devices that may be displayed by the
 * inclusion of the DEVICE_CUSTOM flag - it considers only dynamically-added
 * devices.
 */
bool wendigo_device_is_displayed(wendigo_device* dev) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_device_is_displayed()");
    if(dev == NULL) {
        wendigo_log(MSG_WARN, "wendigo_device_is_displayed(): dev is NULL");
        return false;
    }
    if((current_devices.devices_mask & DEVICE_CUSTOM) == DEVICE_CUSTOM) {
        wendigo_log(
            MSG_INFO,
            "wendigo_device_is_displayed(): The current device mask includes DEVICE_CUSTOM, passing to wendigo_device_is_displayed_custom() for a precise answer.");
        return wendigo_device_is_displayed_custom(dev);
    }
    bool display_selected =
        ((current_devices.devices_mask & DEVICE_SELECTED_ONLY) == DEVICE_SELECTED_ONLY);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_device_is_displayed()");
    return (
        (dev->scanType == SCAN_HCI &&
         ((current_devices.devices_mask & DEVICE_BT_CLASSIC) == DEVICE_BT_CLASSIC) &&
         (!display_selected || dev->tagged)) ||
        (dev->scanType == SCAN_BLE &&
         ((current_devices.devices_mask & DEVICE_BT_LE) == DEVICE_BT_LE) &&
         (!display_selected || dev->tagged)) ||
        (dev->scanType == SCAN_WIFI_AP &&
         ((current_devices.devices_mask & DEVICE_WIFI_AP) == DEVICE_WIFI_AP) &&
         (!display_selected || dev->tagged)) ||
        (dev->scanType == SCAN_WIFI_STA &&
         ((current_devices.devices_mask & DEVICE_WIFI_STA) == DEVICE_WIFI_STA) &&
         (!display_selected || dev->tagged)));
}

/** Replace the contents of current_devices with the specified DeviceListInstance.
 * devices may be freed after calling this function as its contents are copied
 * into current_devices.
 */
void wendigo_scene_device_list_set_current_devices(DeviceListInstance* deviceList) {
    bzero(current_devices.devices_msg, sizeof(current_devices.devices_msg));
    if(deviceList == NULL) {
        /* Re-initialise current_devices */
        current_devices.view = WendigoAppViewDeviceList;
        current_devices.devices_mask = DEVICE_ALL;
        if(current_devices.devices_count > 0 && current_devices.devices != NULL &&
           current_devices.free_devices) {
            free(current_devices.devices);
        }
        current_devices.devices = NULL;
        current_devices.devices_count = 0;
        current_devices.free_devices = true;
        return;
    }
    // TODO: Do I need a mutex over current_devices?
    wendigo_device** new_devices;
    if(current_devices.free_devices) {
        new_devices =
            realloc(current_devices.devices, sizeof(wendigo_device*) * deviceList->devices_count);
    } else {
        if(deviceList->devices_count > 0) {
            new_devices = malloc(sizeof(wendigo_device*) * deviceList->devices_count);
        } else {
            new_devices = NULL;
        }
    }
    if(new_devices == NULL && deviceList->devices_count > 0) {
        /* Log error but proceed if unable to allocate memory for device array */
        char* msg = malloc(sizeof(char) * 49);
        if(msg == NULL) {
            wendigo_log(MSG_ERROR, "Failed to allocate memory to store Device List.");
        } else {
            snprintf(
                msg,
                49,
                "Failed to allocate %d bytes for Device List.",
                sizeof(wendigo_device*) * deviceList->devices_count);
            wendigo_log(MSG_ERROR, msg);
            free(msg);
        }
        /* If we can't resize current_devices.devices[] correctly then free it
     * so that no devices are displayed. */
        if(current_devices.devices_count > 0 && current_devices.devices != NULL) {
            current_devices.devices_count = 0;
            if(current_devices.free_devices) {
                free(current_devices.devices);
            }
            current_devices.devices = NULL;
        }
    } else {
        if(new_devices == NULL) {
            current_devices.devices_count = 0;
        } else {
            /* Copy across deviceList->devices[] */
            memcpy(
                new_devices,
                deviceList->devices,
                sizeof(wendigo_device*) * deviceList->devices_count);
            current_devices.devices_count = deviceList->devices_count;
        }
        current_devices.devices = new_devices;
    }
    current_devices.view = deviceList->view;
    current_devices.devices_mask = deviceList->devices_mask;
    current_devices.free_devices = true; /* I guess, because we allocated it above */
    memcpy(
        current_devices.devices_msg, deviceList->devices_msg, sizeof(current_devices.devices_msg));
}

/** Restrict displayed devices based on the specified filters
 * deviceMask: A bitmask of DeviceMask values. e.g. DEVICE_WIFI_AP |
 * DEVICE_WIFI_STA to display all WiFi devices; 0 to display all device types.
 * Returns the number of devices that meet the specified criteria.
 * If DEVICE_CUSTOM is included as part of the device mask this function WILL
 * NOT modify the contents of current_devices[], but will simply return the
 * number of devices currently displayed.
 */
uint16_t wendigo_scene_device_list_set_current_devices_mask(uint8_t deviceMask) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_list_set_current_devices_mask()");
    if(deviceMask == 0) {
        deviceMask = DEVICE_ALL;
    }
    current_devices.devices_mask = deviceMask;
    /* If custom devices are being displayed there's nothing for this function to do */
    if((deviceMask & DEVICE_CUSTOM) == DEVICE_CUSTOM) {
        FURI_LOG_T(
            WENDIGO_TAG,
            "End wendigo_scene_device_list_set_current_devices_mask(): DEVICE_CUSTOM specified.");
        return current_devices
            .devices_count; // I'm pretty sure this is safe - correctly initialised as 0, then updated
    }
    bool display_selected = ((deviceMask & DEVICE_SELECTED_ONLY) == DEVICE_SELECTED_ONLY);
    /* To ensure we only malloc the required memory, run an initial pass to count the number of devices */
    uint16_t deviceCount = 0;
    for(uint16_t idx = 0; idx < devices_count; ++idx) {
        if((devices[idx]->scanType == SCAN_HCI &&
            ((deviceMask & DEVICE_BT_CLASSIC) == DEVICE_BT_CLASSIC) &&
            (!display_selected || devices[idx]->tagged)) ||
           (devices[idx]->scanType == SCAN_BLE && ((deviceMask & DEVICE_BT_LE) == DEVICE_BT_LE) &&
            (!display_selected || devices[idx]->tagged)) ||
           (devices[idx]->scanType == SCAN_WIFI_AP &&
            ((deviceMask & DEVICE_WIFI_AP) == DEVICE_WIFI_AP) &&
            (!display_selected || devices[idx]->tagged)) ||
           (devices[idx]->scanType == SCAN_WIFI_STA &&
            ((deviceMask & DEVICE_WIFI_STA) == DEVICE_WIFI_STA) &&
            (!display_selected || devices[idx]->tagged))) {
            ++deviceCount;
        }
    }
    if(deviceCount > 0) {
        /* Allocate new storage for current_devices.devices[] - malloc or
     * realloc depending on current_devices.free_devices */
        wendigo_device** new_devices;
        if(current_devices.free_devices) {
            new_devices = realloc(current_devices.devices, sizeof(wendigo_device*) * deviceCount);
        } else {
            new_devices = malloc(sizeof(wendigo_device*) * deviceCount);
        }
        if(new_devices == NULL) {
            char* msg = malloc(sizeof(char) * 104);
            if(msg == NULL) {
                wendigo_log(
                    MSG_ERROR,
                    "Unable to allocate current_devices.devices[] for new device mask, keeping devices[] unchanged.");
            } else {
                snprintf(
                    msg,
                    104,
                    "Unable to allocate current_devices.devices[%d] for new device mask %d, keeping devices[] unchanged.",
                    deviceCount,
                    deviceMask);
                wendigo_log(MSG_ERROR, msg);
                free(msg);
            }
            return 0;
        }
        current_devices.devices = new_devices;
    }
    current_devices.devices_count = deviceCount;
    /* Populate current_devices.devices[] */
    uint16_t current_index = 0;
    for(uint16_t i = 0; i < devices_count && current_index < deviceCount; ++i) {
        if((devices[i]->scanType == SCAN_HCI &&
            ((deviceMask & DEVICE_BT_CLASSIC) == DEVICE_BT_CLASSIC) &&
            (!display_selected || devices[i]->tagged)) ||
           (devices[i]->scanType == SCAN_BLE && ((deviceMask & DEVICE_BT_LE) == DEVICE_BT_LE) &&
            (!display_selected || devices[i]->tagged)) ||
           (devices[i]->scanType == SCAN_WIFI_AP &&
            ((deviceMask & DEVICE_WIFI_AP) == DEVICE_WIFI_AP) &&
            (!display_selected || devices[i]->tagged)) ||
           (devices[i]->scanType == SCAN_WIFI_STA &&
            ((deviceMask & DEVICE_WIFI_STA) == DEVICE_WIFI_STA) &&
            (!display_selected || devices[i]->tagged))) {
            current_devices.devices[current_index++] = devices[i];
        }
    }
    furi_assert(current_index == deviceCount);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_list_set_current_devices_mask()");
    return deviceCount;
}

/** A more flexible version of elapsedTime() that lets us avoid running
 * furi_hal_rtc_get_timestamp(). This version is suitable for running at high
 * frequency - In tick events etc. `from` and `to` are seconds since the Unix
 * Epoch. Returns elapsed seconds and, if elapsedStr is an initialised char[]
 * and strlen > 0, places a text representation of the elapsed time in
 * elapsedStr. Returns zero and the empty string on failure.
 */
double _elapsedTime(uint32_t* from, uint32_t* to, char* elapsedStr, uint8_t strlen) {
    FURI_LOG_T(WENDIGO_TAG, "Start _elapsedTime()");
    /* Validate everything before we touch it */
    if(from == NULL || to == NULL) {
        if(elapsedStr != NULL && strlen > 0) {
            elapsedStr[0] = '\0';
        }
        return 0;
    }
    uint32_t elapsed = *to - *from;
    if(elapsedStr != NULL && strlen > 0) {
        if(elapsed < 60) {
            snprintf(elapsedStr, strlen, "%lds", elapsed);
        } else {
            uint8_t minutes = elapsed / 60;
            uint8_t seconds = elapsed - (minutes * 60);
            snprintf(elapsedStr, strlen, "%2d:%02d", minutes, seconds);
        }
    }
    FURI_LOG_T(WENDIGO_TAG, "End _elapsedTime()");
    return elapsed;
}

/** Calculate the elapsed time since the specified device was last seen.
 * Returns the elapsed time as a uint32 and, if elapsedStr is not NULL,
 * in a string representation there. elapsedStr must be an initialised
 * char[] of at least 7 bytes.
 * If a string representation is not needed provide NULL for elapsedStr.
 * Returns zero and an empty string on failure.
 */
double elapsedTime(wendigo_device* dev, char* elapsedStr, uint8_t strlen) {
    FURI_LOG_T(WENDIGO_TAG, "Start elapsedTime()");
    if(dev == NULL) {
        /* Return sensible values on failure */
        if(strlen > 0 && elapsedStr != NULL) {
            elapsedStr[0] = '\0';
        }
        return 0;
    }
    uint32_t nowTime = furi_hal_rtc_get_timestamp();
    FURI_LOG_T(WENDIGO_TAG, "End elapsedTime()");
    return _elapsedTime(&(dev->lastSeen), &nowTime, elapsedStr, strlen);
}

/** Identify the device represented by the currently-selected menu item. NULL if
 * it cannot be identified.
 */
wendigo_device* wendigo_scene_device_list_selected_device(VariableItem* item) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_list_selected_device()");
    WendigoApp* app = variable_item_get_context(item);

    if(app->device_list_selected_menu_index < current_devices.devices_count) {
        FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_list_selected_device()");
        return current_devices.devices[app->device_list_selected_menu_index];
    }
    // TODO: Compare these methods - I'm tempted to remove the first approach
    // (which is why it now gets its own function)
    /* Instead of indexing into the array, see if we can find the device with a
   * reverse lookup from `item` */
    uint16_t idx = 0;
    for(; idx < current_devices.devices_count && current_devices.devices[idx]->view != item;
        ++idx) {
    }
    if(idx < current_devices.devices_count) {
        FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_list_selected_device()");
        return current_devices.devices[idx];
    }
    /* Device not found */
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_list_selected_device()");
    return NULL;
}

/** Update the current display to reflect a new discovery result for `dev`.
 * This function is called by the functions wendigo_add_device() and
 * wendigo_update_device() in wendigo_scan.c. When this scene and scanning are
 * active at the same time all devices identified, whether newly-identified
 * devices or subsequent packets describing a known device, are passed as an
 * argument to this function to allow the UI to be dynamically updated and to
 * display either information about a new device or updated information about an
 * existing device.
 */
void wendigo_scene_device_list_update(WendigoApp* app, wendigo_device* dev) {
    FURI_LOG_D(WENDIGO_TAG, "Start wendigo_scene_device_list_update()");
    /* This will also cater for a NULL dev */
    if(!wendigo_device_is_displayed(dev)) {
        return;
    }
    char* name;
    bool free_name = false;
    uint8_t optionsCount;
    uint8_t optionIndex;
    char optionValue[MAX_SSID_LEN + 1];
    bzero(optionValue, MAX_SSID_LEN + 1); /* Null out optionValue[] */
    /* Set name and options menus based on dev->scanType */
    if(dev->scanType == SCAN_HCI || dev->scanType == SCAN_BLE) {
        /* Use bdname as name if it's a bluetooth device and we have a name */
        if(dev->radio.bluetooth.bdname_len > 0 && dev->radio.bluetooth.bdname != NULL) {
            name = dev->radio.bluetooth.bdname;
        } else {
            /* Otherwise use MAC */
            name = malloc(sizeof(char) * (MAC_STRLEN + 1));
            free_name = true;
            if(name != NULL) {
                bytes_to_string(dev->mac, MAC_BYTES, name);
            }
        }
        optionsCount = WendigoOptionsBTCount;
        if(dev->view == NULL) {
            optionIndex = WendigoOptionBTScanType;
        } else {
            optionIndex = variable_item_get_current_value_index(dev->view);
        }
        switch(optionIndex) {
        case WendigoOptionBTRSSI:
            snprintf(optionValue, sizeof(optionValue), "%d dB", dev->rssi);
            break;
        case WendigoOptionBTLastSeen:
            elapsedTime(dev, optionValue, sizeof(optionValue));
            break;
        case WendigoOptionBTScanType:
            snprintf(
                optionValue,
                sizeof(optionValue),
                "%s",
                (dev->scanType == SCAN_HCI) ? "BT Classic" :
                (dev->scanType == SCAN_BLE) ? "BLE" :
                                              "Unknown");
            break;
        default:
            /* I'm pretty sure I don't need to worry about static variables */
            break;
        }
    } else if(dev->scanType == SCAN_WIFI_AP) {
        if(dev->radio.ap.ssid[0] == '\0') {
            /* No SSID - Use MAC */
            name = malloc(sizeof(char) * (MAC_STRLEN + 1));
            free_name = true;
            if(name != NULL) {
                bytes_to_string(dev->mac, MAC_BYTES, name);
            }
        } else {
            /* Use access point SSID as name */
            name = dev->radio.ap.ssid;
        }
        optionsCount = WendigoOptionsAPCount;
        if(dev->view == NULL) {
            optionIndex = WendigoOptionAPScanType;
        } else {
            optionIndex = variable_item_get_current_value_index(dev->view);
        }
        switch(optionIndex) {
        case WendigoOptionAPRSSI:
            snprintf(optionValue, sizeof(optionValue), "%d dB", dev->rssi);
            break;
        case WendigoOptionAPLastSeen:
            elapsedTime(dev, optionValue, sizeof(optionValue));
            break;
        case WendigoOptionAPChannel:
            snprintf(optionValue, sizeof(optionValue), "Ch. %d", dev->radio.ap.channel);
            break;
        case WendigoOptionAPAuthMode:
            uint8_t mode = dev->radio.ap.authmode;
            if(mode > WIFI_AUTH_MAX) {
                mode = WIFI_AUTH_MAX;
            }
            snprintf(optionValue, sizeof(optionValue), "%s", wifi_auth_mode_strings[mode]);
            break;
        case WendigoOptionAPScanType:
            snprintf(
                optionValue,
                sizeof(optionValue),
                "%s",
                (dev->scanType == SCAN_WIFI_AP) ? "WiFi AP" : "Unknown");
            break;
        case WendigoOptionAPStaCount:
            snprintf(
                optionValue, sizeof(optionValue), "%d Stations", dev->radio.ap.stations_count);
            break;
        default:
            // Nothing
            break;
        }
    } else if(dev->scanType == SCAN_WIFI_STA) {
        /* Use MAC to label the device */
        name = malloc(sizeof(char) * (MAC_STRLEN + 1));
        free_name = true;
        if(name != NULL) {
            bytes_to_string(dev->mac, MAC_BYTES, name);
        }
        optionsCount = WendigoOptionsSTACount;
        if(dev->view == NULL) {
            optionIndex = WendigoOptionSTAScanType;
        } else {
            optionIndex = variable_item_get_current_value_index(dev->view);
        }
        switch(optionIndex) {
        case WendigoOptionSTARSSI:
            snprintf(optionValue, sizeof(optionValue), "%d dB", dev->rssi);
            break;
        case WendigoOptionSTALastSeen:
            elapsedTime(dev, optionValue, sizeof(optionValue));
            break;
        case WendigoOptionSTAChannel:
            snprintf(optionValue, sizeof(optionValue), "Ch. %d", dev->radio.sta.channel);
            break;
        case WendigoOptionSTAAP:
            if(memcmp(dev->radio.sta.apMac, nullMac, MAC_BYTES)) {
                /* AP has a MAC - Do we have the AP in our cache? */
                uint16_t apIdx = device_index_from_mac(dev->radio.sta.apMac);
                if(apIdx == devices_count || devices == NULL || devices[apIdx] == NULL ||
                   devices[apIdx]->scanType != SCAN_WIFI_AP ||
                   devices[apIdx]->radio.ap.ssid[0] == '\0') {
                    /* Either we don't have the AP in our cache or
             * the AP's SSID is unknown - Use MAC instead */
                    bytes_to_string(dev->radio.sta.apMac, MAC_BYTES, optionValue);
                } else {
                    /* We have an SSID for the AP */
                    snprintf(
                        optionValue, sizeof(optionValue), "%s", devices[apIdx]->radio.ap.ssid);
                }
            } else {
                /* We don't know the AP */
                snprintf(optionValue, sizeof(optionValue), "AP Unknown");
            }
            break;
        case WendigoOptionSTAScanType:
            snprintf(
                optionValue,
                sizeof(optionValue),
                "%s",
                (dev->scanType == SCAN_WIFI_STA) ? "WiFi STA" : "Unknown");
            break;
        case WendigoOptionSTASavedNetworks:
            snprintf(
                optionValue,
                sizeof(optionValue),
                "%d Networks",
                dev->radio.sta.saved_networks_count);
            break;
        default:
            /* Nothing to do, I think */
            break;
        }
    } else {
        /* Invalid device type */
        return;
    }
    uint16_t dev_idx =
        custom_device_index(dev, current_devices.devices, current_devices.devices_count);
    if(dev_idx == current_devices.devices_count) {
        /* Add a new item */
        dev->view = variable_item_list_add(
            app->devices_var_item_list,
            (name == NULL) ? "(Unknown)" : name,
            optionsCount,
            wendigo_scene_device_list_var_list_change_callback,
            app);
        /* And set selected option */
        variable_item_set_current_value_index(dev->view, optionIndex);
        variable_item_set_current_value_text(dev->view, optionValue);
        /* Update current_devices.devices[] to include the new device */
        if(!current_devices.free_devices) {
            char* msg = malloc(
                sizeof(char) *
                (123 +
                 MAX_SSID_LEN)); /* Length assumes that a bdname won't be longer than MAX_SSID_LEN */
            if(msg == NULL || name == NULL) {
                wendigo_log(
                    MSG_ERROR,
                    "Adding new device to current_devices.devices[] but it is configured to be immutable. Ignoring this and adding anyway...");
            } else {
                snprintf(
                    msg,
                    123 + MAX_SSID_LEN,
                    "Adding a new device %s to current_devices.devices[] but it is configured to be immutable. Ignoring this and adding anyway...",
                    name);
                wendigo_log(MSG_ERROR, msg);
            }
            if(msg != NULL) {
                free(msg);
            }
        }
        wendigo_device** new_devices = realloc(
            current_devices.devices,
            sizeof(wendigo_device*) * (current_devices.devices_count + 1));
        if(new_devices != NULL) {
            current_devices.devices = new_devices;
            current_devices.devices[current_devices.devices_count++] = dev;
        }
    } else {
        /* Update dev->view */
        // YAGNI: Seems to be an occasional bug with the API - This results in occasional NULL pointer dereference
        //variable_item_set_item_label(dev->view, (name == NULL) ? "(Unknown)" : name);
    }
    if(dev->view != NULL) {
        variable_item_set_current_value_index(dev->view, optionIndex);
        if(optionValue[0] != '\0') {
            /* New value for the option */
            // YAGNI: Seems to be an occasional bug with the PI - This results in an occasional NULL pointer dereference
            //variable_item_set_current_value_text(dev->view, optionValue);
        }
    }
    /* Free `name` if we malloc'd it */
    if(free_name && name != NULL) {
        free(name);
    }
    FURI_LOG_D(WENDIGO_TAG, "End wendigo_scene_device_list_update()");
}

/** Re-render the variable item list. This function exists because there is no
 * method to remove items from a variable_item_list, but that is sometimes
 * necessary (e.g. when viewing selected devices and de-selecting a device).
 */
void wendigo_scene_device_list_redraw(WendigoApp* app) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_list_redraw()");
    variable_item_list_reset(app->devices_var_item_list);
    char* item_str = NULL;
    uint8_t options_count = 0;
    uint8_t options_index;
    bool free_item_str = false;
    wendigo_scene_device_list_set_current_devices_mask(current_devices.devices_mask);
    /* Set header text for the list if specified. NULL first to prevent text-over-text */
    variable_item_list_set_header(app->devices_var_item_list, NULL);
    if(current_devices.devices_msg[0] != '\0') {
        variable_item_list_set_header(app->devices_var_item_list, current_devices.devices_msg);
    }
    for(uint16_t i = 0; i < current_devices.devices_count; ++i) {
        /* Label with bdname if it's bluetooth & we have a name */
        if(current_devices.devices[i] != NULL &&
           (current_devices.devices[i]->scanType == SCAN_HCI ||
            current_devices.devices[i]->scanType == SCAN_BLE)) {
            if(current_devices.devices[i]->radio.bluetooth.bdname_len > 0 &&
               current_devices.devices[i]->radio.bluetooth.bdname != NULL) {
                item_str = current_devices.devices[i]->radio.bluetooth.bdname;
            } else {
                /* Use MAC */
                item_str = malloc(sizeof(char) * (MAC_STRLEN + 1));
                if(item_str != NULL) {
                    bytes_to_string(current_devices.devices[i]->mac, MAC_BYTES, item_str);
                }
                free_item_str = true;
            }
            options_count = WendigoOptionsBTCount;
            options_index = WendigoOptionBTScanType;
            /* Label with SSID if it's an AP and we have an SSID */
        } else if(
            current_devices.devices[i] != NULL &&
            current_devices.devices[i]->scanType == SCAN_WIFI_AP) {
            if(current_devices.devices[i]->radio.ap.ssid[0] != '\0') {
                item_str = current_devices.devices[i]->radio.ap.ssid;
            } else {
                /* Use MAC */
                item_str = malloc(sizeof(char) * (MAC_STRLEN + 1));
                if(item_str != NULL) {
                    bytes_to_string(current_devices.devices[i]->mac, MAC_BYTES, item_str);
                }
                free_item_str = true;
            }
            options_count = WendigoOptionsAPCount;
            options_index = WendigoOptionAPScanType;
            /* Otherwise use the MAC/BDA */
        } else if(current_devices.devices[i] != NULL) {
            item_str = malloc(sizeof(char) * (MAC_STRLEN + 1));
            if(item_str != NULL) {
                bytes_to_string(current_devices.devices[i]->mac, MAC_BYTES, item_str);
            }
            free_item_str = true;
            options_count = WendigoOptionsSTACount;
            options_index = WendigoOptionSTAScanType;
        }
        if(current_devices.devices[i] != NULL && item_str != NULL && options_count > 0) {
            current_devices.devices[i]->view = variable_item_list_add(
                app->devices_var_item_list,
                item_str,
                options_count,
                wendigo_scene_device_list_var_list_change_callback,
                app);
        }
        /* free item_str if we malloc'd it */
        if(free_item_str && item_str != NULL) {
            free(item_str);
        }
        /* Default to displaying scanType in options menu */
        if(current_devices.devices[i] != NULL && current_devices.devices[i]->view != NULL) {
            variable_item_set_current_value_index(current_devices.devices[i]->view, options_index);
            variable_item_set_current_value_text(
                current_devices.devices[i]->view,
                (current_devices.devices[i]->scanType == SCAN_HCI)      ? "BT Classic" :
                (current_devices.devices[i]->scanType == SCAN_BLE)      ? "BLE" :
                (current_devices.devices[i]->scanType == SCAN_WIFI_AP)  ? "WiFi AP" :
                (current_devices.devices[i]->scanType == SCAN_WIFI_STA) ? "WiFi STA" :
                                                                          "Unknown");
        }
    }
    variable_item_list_set_selected_item(app->devices_var_item_list, 0);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_list_redraw()");
}

static void wendigo_scene_device_list_var_list_enter_callback(void* context, uint32_t index) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_list_var_list_enter_callback()");
    furi_assert(context);
    WendigoApp* app = context;

    furi_assert(index < current_devices.devices_count);

    wendigo_device* item = current_devices.devices[index];
    if(item == NULL) {
        return;
    }

    app->device_list_selected_menu_index = index;

    /* If the tag/untag menu item is selected perform that action, otherwise
   * display details for `item` */
    if(item->view != NULL &&
       (((item->scanType == SCAN_HCI || item->scanType == SCAN_BLE) &&
         variable_item_get_current_value_index(item->view) == WendigoOptionBTTagUntag) ||
        (item->scanType == SCAN_WIFI_AP &&
         variable_item_get_current_value_index(item->view) == WendigoOptionAPTagUntag) ||
        (item->scanType == SCAN_WIFI_STA &&
         variable_item_get_current_value_index(item->view) == WendigoOptionSTATagUntag))) {
        item->tagged = !(item->tagged);
        variable_item_set_current_value_text(item->view, (item->tagged) ? "Untag" : "Tag");
        /* If the device is now untagged and we're viewing tagged devices only,
     * remove the device from view unless custom device view is enabled. */
        if(((current_devices.devices_mask & DEVICE_SELECTED_ONLY) == DEVICE_SELECTED_ONLY) &&
           !item->tagged && ((current_devices.devices_mask & DEVICE_CUSTOM) != DEVICE_CUSTOM)) {
            /* Bugger - There's no method to remove an item from a variable_item_list */
            item->view = NULL; // TODO: Is this leaking memory? Can I free a VariableItem?
            wendigo_scene_device_list_redraw(app);
        }
    } else if(
        item->view != NULL &&
        ((item->scanType == SCAN_WIFI_AP &&
          variable_item_get_current_value_index(item->view) == WendigoOptionAPStaCount) ||
         (item->scanType == SCAN_WIFI_STA &&
          variable_item_get_current_value_index(item->view) == WendigoOptionSTAAP))) {
        /* Push current_devices onto the device list stack */
        DeviceListInstance* new_stack =
            realloc(stack, sizeof(DeviceListInstance) * (stack_counter + 1));
        if(new_stack == NULL) {
            wendigo_display_popup(
                app,
                "Insufficient Memory",
                "Unable to allocate an additional DeviceListInstance.");
            wendigo_log(
                MSG_ERROR,
                "wendigo_scene_device_list_var_list_enter_callback() terminated early. Unable to malloc() additional DeviceListInstance.");
            return;
        } else {
            memcpy(&(new_stack[stack_counter]), &current_devices, sizeof(DeviceListInstance));
            stack = new_stack;
            ++stack_counter;
        }
        /* Re-initialise current_devices */
        current_devices.devices_mask = DEVICE_CUSTOM;
        current_devices.view = WendigoAppViewDeviceList;
        current_devices.free_devices = true;
        current_devices.devices_count = 0;
        current_devices.devices = NULL;
        bzero(current_devices.devices_msg, sizeof(current_devices.devices_msg));
        char* deviceName = malloc(sizeof(char) * (MAX_SSID_LEN + 1));
        if(deviceName == NULL) {
            /* Not using wendigo_log() so I can include %d */
            // TODO: Extend wendigo_log() to support variable arguments
            FURI_LOG_E(
                "wendigo_scene_device_list_var_list_enter_callback()",
                "Failed to allocate deviceName[%d], proceeding without it.",
                MAX_SSID_LEN + 1);
        } else {
            bzero(deviceName, sizeof(char) * (MAX_SSID_LEN + 1));
        }
        if(item->scanType == SCAN_WIFI_AP) {
            current_devices.view = WendigoAppViewAPSTAs;
            if(item->radio.ap.stations_count > 0) {
                current_devices.devices =
                    malloc(sizeof(wendigo_device*) * item->radio.ap.stations_count);
            }
            if(item->radio.ap.stations_count > 0 && current_devices.devices == NULL) {
                char* msg = malloc(sizeof(char) * 56);
                if(msg == NULL) {
                    wendigo_log(MSG_ERROR, "Unable to allocate memory to display AP's stations.");
                } else {
                    snprintf(
                        msg,
                        56,
                        "Unable to allocate %d bytes to display AP's stations.",
                        sizeof(wendigo_device*) * item->radio.ap.stations_count);
                    wendigo_log(MSG_ERROR, msg);
                    free(msg);
                }
                wendigo_display_popup(
                    app, "Out of memory", "Unable to allocate memory for AP's stations.");
                current_devices.devices_count = 0;
            } else { /* There are no stations to display or malloc() succeeded */
                current_devices.free_devices =
                    true; /* Don't forget to only free if stations_count > 0 as well */
                /* Loop through item->radio.ap.stations, adding devices with the
         * specified MACs if we can find them. */
                uint16_t idx_src;
                uint16_t idx_dest;
                uint16_t idx_sta;
                for(idx_src = 0, idx_dest = 0; idx_src < item->radio.ap.stations_count;
                    ++idx_src) {
                    idx_sta = device_index_from_mac(item->radio.ap.stations[idx_src]);
                    if(idx_sta < devices_count) {
                        /* The station exists in the cache - add it to current_devices */
                        current_devices.devices[idx_dest++] = devices[idx_sta];
                    }
                }
                /* If there were stations not in the cache, current_devices will have empty
         * elements - if this occurs, shrink current_devices.devices[]. */
                if(idx_dest < item->radio.ap.stations_count) {
                    wendigo_device** tmp_devices =
                        realloc(current_devices.devices, sizeof(wendigo_device*) * idx_dest);
                    if(tmp_devices != NULL) {
                        /* If realloc() worked, just set devices[] to tmp_devices[] */
                        current_devices.devices = tmp_devices;
                    }
                }
                /* Set devices_count whether or not we had to shrink devices[] */
                current_devices.devices_count = idx_dest;
            }
            /* Set deviceName using SSID if we have it, otherwise MAC */
            if(deviceName == NULL) {
                snprintf(
                    current_devices.devices_msg, sizeof(current_devices.devices_msg), "Stations");
            } else {
                if(item->radio.ap.ssid[0] == '\0') {
                    bytes_to_string(item->mac, MAC_BYTES, deviceName);
                } else {
                    strncpy(deviceName, item->radio.ap.ssid, sizeof(char) * (MAX_SSID_LEN + 1));
                }
                snprintf(
                    current_devices.devices_msg,
                    sizeof(current_devices.devices_msg),
                    "%s STAs",
                    deviceName);
            }
        } else if(item->scanType == SCAN_WIFI_STA) {
            current_devices.view = WendigoAppViewSTAAP;
            /* Use MAC to refer to the device */
            if(deviceName == NULL) {
                snprintf(
                    current_devices.devices_msg,
                    sizeof(current_devices.devices_msg),
                    "Access Point");
            } else {
                bytes_to_string(item->mac, MAC_BYTES, deviceName);
                snprintf(
                    current_devices.devices_msg,
                    sizeof(current_devices.devices_msg),
                    "%s' AP",
                    deviceName);
            }
            /* Station will display one device if it has an AP, otherwise 0 */
            if(memcmp(item->radio.sta.apMac, nullMac, MAC_BYTES)) {
                /* We have a MAC. Find the wendigo_device* */
                uint16_t apIdx = device_index_from_mac(item->radio.sta.apMac);
                if(apIdx < devices_count) {
                    /* Found the AP in the device cache - Display just it */
                    current_devices.devices_count = 1;
                    current_devices.devices = &(devices[apIdx]);
                }
            }
        } else {
            wendigo_log(
                MSG_WARN,
                "Logic error: Fell through conditional nest in wendigo_scene_device_list.c");
        }
        if(deviceName != NULL) {
            free(deviceName);
        }
        view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventListDevices);
    } else if(
        item->view != NULL && item->scanType == SCAN_WIFI_STA &&
        variable_item_get_current_value_index(item->view) == WendigoOptionSTASavedNetworks) {
        /* Tell the scene which device we're interested in */
        wendigo_scene_pnl_list_set_device(item, app);
        view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventListNetworks);
    } else {
        /* Display details for `item` */
        wendigo_scene_device_detail_set_device(item);
        // TODO Fix detail scene, then
        // view_dispatcher_send_custom_event(app->view_dispatcher,
        // Wendigo_EventListDeviceDetails);
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_list_var_list_enter_callback()");
}

static void wendigo_scene_device_list_var_list_change_callback(VariableItem* item) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_list_var_list_change_callback()");
    furi_assert(item);

    /* app->device_list_selected_menu_index should reliably point to the selected
     menu item. Use that to obtain the wendigo_device* */
    WendigoApp* app = variable_item_get_context(item);
    app->device_list_selected_menu_index =
        variable_item_list_get_selected_item_index(app->devices_var_item_list);
    wendigo_device* menu_item = wendigo_scene_device_list_selected_device(item);

    if(menu_item != NULL) {
        uint8_t option_index = variable_item_get_current_value_index(item);
        furi_assert(option_index < MAX_OPTIONS);
        /* Update the current option label */
        char tempStr[MAX_SSID_LEN + 1];
        bzero(tempStr, MAX_SSID_LEN + 1);
        if(((menu_item->scanType == SCAN_HCI || menu_item->scanType == SCAN_BLE) &&
            option_index == WendigoOptionBTRSSI) ||
           (menu_item->scanType == SCAN_WIFI_AP && option_index == WendigoOptionAPRSSI) ||
           (menu_item->scanType == SCAN_WIFI_STA && option_index == WendigoOptionSTARSSI)) {
            snprintf(tempStr, sizeof(tempStr), "%d dB", menu_item->rssi);
        } else if(
            ((menu_item->scanType == SCAN_HCI || menu_item->scanType == SCAN_BLE) &&
             option_index == WendigoOptionBTTagUntag) ||
            (menu_item->scanType == SCAN_WIFI_AP && option_index == WendigoOptionAPTagUntag) ||
            (menu_item->scanType == SCAN_WIFI_STA && option_index == WendigoOptionSTATagUntag)) {
            snprintf(tempStr, sizeof(tempStr), "%s", (menu_item->tagged) ? "Untag" : "Tag");
        } else if(
            ((menu_item->scanType == SCAN_HCI || menu_item->scanType == SCAN_BLE) &&
             option_index == WendigoOptionBTScanType) ||
            (menu_item->scanType == SCAN_WIFI_AP && option_index == WendigoOptionAPScanType) ||
            (menu_item->scanType == SCAN_WIFI_STA && option_index == WendigoOptionSTAScanType)) {
            snprintf(
                tempStr,
                sizeof(tempStr),
                "%s",
                (menu_item->scanType == SCAN_HCI)      ? "BT Classic" :
                (menu_item->scanType == SCAN_BLE)      ? "BLE" :
                (menu_item->scanType == SCAN_WIFI_AP)  ? "WiFi AP" :
                (menu_item->scanType == SCAN_WIFI_STA) ? "WiFi STA" :
                                                         "Unknown Device");
        } else if(
            ((menu_item->scanType == SCAN_HCI || menu_item->scanType == SCAN_BLE) &&
             option_index == WendigoOptionBTLastSeen) ||
            (menu_item->scanType == SCAN_WIFI_AP && option_index == WendigoOptionAPLastSeen) ||
            (menu_item->scanType == SCAN_WIFI_STA && option_index == WendigoOptionSTALastSeen)) {
            elapsedTime(menu_item, tempStr, sizeof(tempStr));
        } else if(
            (menu_item->scanType == SCAN_HCI || menu_item->scanType == SCAN_BLE) &&
            option_index == WendigoOptionBTCod) {
            snprintf(tempStr, sizeof(tempStr), "%s", menu_item->radio.bluetooth.cod_str);
        } else if(menu_item->scanType == SCAN_WIFI_AP && option_index == WendigoOptionAPChannel) {
            snprintf(tempStr, sizeof(tempStr), "Ch. %d", menu_item->radio.ap.channel);
        } else if(menu_item->scanType == SCAN_WIFI_STA && option_index == WendigoOptionSTAChannel) {
            snprintf(tempStr, sizeof(tempStr), "Ch. %d", menu_item->radio.sta.channel);
        } else if(menu_item->scanType == SCAN_WIFI_AP && option_index == WendigoOptionAPAuthMode) {
            uint8_t mode = menu_item->radio.ap.authmode;
            if(mode > WIFI_AUTH_MAX) {
                mode = WIFI_AUTH_MAX;
            }
            snprintf(tempStr, sizeof(tempStr), "%s", wifi_auth_mode_strings[mode]);
        } else if(menu_item->scanType == SCAN_WIFI_AP && option_index == WendigoOptionAPStaCount) {
            snprintf(tempStr, sizeof(tempStr), "%d Stations", menu_item->radio.ap.stations_count);
        } else if(
            menu_item->scanType == SCAN_WIFI_STA &&
            option_index == WendigoOptionSTASavedNetworks) {
            snprintf(
                tempStr, sizeof(tempStr), "%d Networks", menu_item->radio.sta.saved_networks_count);
        } else if(menu_item->scanType == SCAN_WIFI_STA && option_index == WendigoOptionSTAAP) {
            /* Use SSID if available, otherwise MAC */
            uint16_t apIdx = device_index_from_mac(menu_item->radio.sta.apMac);
            if(apIdx == devices_count || devices == NULL || devices[apIdx] == NULL ||
               devices[apIdx]->radio.ap.ssid[0] == '\0') {
                /* Couldn't find the AP, or found the AP with no SSID */
                if(memcmp(menu_item->radio.sta.apMac, nullMac, MAC_BYTES)) {
                    /* We have a MAC */
                    bytes_to_string(menu_item->radio.sta.apMac, MAC_BYTES, tempStr);
                } else {
                    /* No MAC */
                    snprintf(tempStr, sizeof(tempStr), "AP Unknown");
                }
            } else {
                /* We have an SSID */
                snprintf(tempStr, sizeof(tempStr), "%s", devices[apIdx]->radio.ap.ssid);
            }
        } else {
            char* msg = malloc(sizeof(char) * (68 + MAC_STRLEN));
            char* macStr = malloc(sizeof(char) * (1 + MAC_STRLEN));
            if(msg != NULL && macStr != NULL) {
                bytes_to_string(menu_item->mac, MAC_BYTES, macStr);
                snprintf(
                    msg,
                    68 + MAC_STRLEN,
                    "Invalid scanType (%d) / optionIndex (%d) combination for device %s.",
                    menu_item->scanType,
                    option_index,
                    macStr);
                wendigo_log(MSG_ERROR, msg);
            }
            if(msg != NULL) {
                free(msg);
            }
            if(macStr != NULL) {
                free(macStr);
            }
        }
        /* Update the current option's text if we set tempStr */
        if(tempStr[0] != '\0') {
            variable_item_set_current_value_text(item, tempStr);
        }
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_list_var_list_change_callback()");
}

/** Initialise the device list
 *  TODO: Consider sorting options when implemented
 */
void wendigo_scene_device_list_on_enter(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_list_on_enter()");
    FURI_LOG_T(
        WENDIGO_TAG,
        "current_view: %d, devices_count: %d, devices_mask: %d, devices_msg: %s",
        current_devices.view,
        current_devices.devices_count,
        current_devices.devices_mask,
        current_devices.devices_msg);
    WendigoApp* app = context;
    app->current_view = current_devices.view;

    /* Reset and re-populate the list */
    wendigo_scene_device_list_redraw(app);

    variable_item_list_set_enter_callback(
        app->devices_var_item_list, wendigo_scene_device_list_var_list_enter_callback, app);

    /* Restore the selected device index if it's there to restore (e.g. if we're
   * returning from the device detail scene). But first test that it's in
   * bounds, unless we've moved from all devices to a subset. */
    uint8_t selected_item =
        scene_manager_get_scene_state(app->scene_manager, WendigoSceneDeviceList);
    if(selected_item >= current_devices.devices_count) {
        selected_item = 0;
    }
    variable_item_list_set_selected_item(app->devices_var_item_list, selected_item);
    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewDeviceList);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_list_on_enter()");
}

bool wendigo_scene_device_list_on_event(void* context, SceneManagerEvent event) {
    //  FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_list_on_event()");
    WendigoApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case Wendigo_EventListDeviceDetails:
            scene_manager_set_scene_state(
                app->scene_manager, WendigoSceneDeviceList, app->device_list_selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WendigoSceneDeviceDetail);
            break;
        case Wendigo_EventListDevices:
            /* current_devices has been populated with relevant devices - all we
       * need to do here is display them. */
            scene_manager_set_scene_state(
                app->scene_manager, WendigoSceneDeviceList, app->device_list_selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WendigoSceneDeviceList);
            break;
        case Wendigo_EventListNetworks:
            scene_manager_set_scene_state(
                app->scene_manager, WendigoSceneDeviceList, app->device_list_selected_menu_index);
            scene_manager_next_scene(app->scene_manager, WendigoScenePNLList);
            break;
        default:
            char* msg = malloc(sizeof(char) * 54);
            if(msg != NULL) {
                snprintf(
                    msg, 54, "wendigo_scene_device_list received unknown event %ld.", event.event);
                wendigo_log(MSG_WARN, msg);
                free(msg);
            }
            break;
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeTick) {
        app->device_list_selected_menu_index =
            variable_item_list_get_selected_item_index(app->devices_var_item_list);
        consumed = true;

        /* Update displayed value for any device where the currently-selected
     * option is subject to change - lastSeen, RSSI, saved_networks_count,
     * stations_count, STA's AP, or authMode.
     * All attributes except lastSeen are only updated if scanning is active.
     */
        char optionValue[MAX_SSID_LEN + 1];
        uint32_t now = furi_hal_rtc_get_timestamp();
        for(uint16_t i = 0; i < current_devices.devices_count; ++i) {
            if(current_devices.devices != NULL && current_devices.devices[i] != NULL &&
               current_devices.devices[i]->view != NULL) {
                bzero(optionValue, MAX_SSID_LEN + 1);
                if(((current_devices.devices[i]->scanType == SCAN_HCI ||
                     current_devices.devices[i]->scanType == SCAN_BLE) &&
                    variable_item_get_current_value_index(current_devices.devices[i]->view) ==
                        WendigoOptionBTLastSeen) ||
                   (current_devices.devices[i]->scanType == SCAN_WIFI_AP &&
                    variable_item_get_current_value_index(current_devices.devices[i]->view) ==
                        WendigoOptionAPLastSeen) ||
                   (current_devices.devices[i]->scanType == SCAN_WIFI_STA &&
                    variable_item_get_current_value_index(current_devices.devices[i]->view) ==
                        WendigoOptionSTALastSeen)) {
                    /* Update lastSeen for this device */
                    _elapsedTime(
                        &(current_devices.devices[i]->lastSeen),
                        &now,
                        optionValue,
                        sizeof(optionValue));
                } else if(
                    app->is_scanning &&
                    (((current_devices.devices[i]->scanType == SCAN_HCI ||
                       current_devices.devices[i]->scanType == SCAN_BLE) &&
                      variable_item_get_current_value_index(current_devices.devices[i]->view) ==
                          WendigoOptionBTRSSI) ||
                     (current_devices.devices[i]->scanType == SCAN_WIFI_AP &&
                      variable_item_get_current_value_index(current_devices.devices[i]->view) ==
                          WendigoOptionAPRSSI) ||
                     (current_devices.devices[i]->scanType == SCAN_WIFI_STA &&
                      variable_item_get_current_value_index(current_devices.devices[i]->view) ==
                          WendigoOptionSTARSSI))) {
                    /* Update RSSI for the current device */
                    snprintf(
                        optionValue,
                        sizeof(optionValue),
                        "%d dB",
                        current_devices.devices[i]->rssi);
                } else if(
                    app->is_scanning && current_devices.devices[i]->scanType == SCAN_WIFI_AP &&
                    variable_item_get_current_value_index(current_devices.devices[i]->view) ==
                        WendigoOptionAPStaCount) {
                    /* Update stations_count for the current device */
                    snprintf(
                        optionValue,
                        sizeof(optionValue),
                        "%d Stations",
                        current_devices.devices[i]->radio.ap.stations_count);
                } else if(
                    app->is_scanning && current_devices.devices[i]->scanType == SCAN_WIFI_AP &&
                    variable_item_get_current_value_index(current_devices.devices[i]->view) ==
                        WendigoOptionAPAuthMode) {
                    /* Update authMode for the current device */
                    uint8_t mode = current_devices.devices[i]->radio.ap.authmode;
                    if(mode > WIFI_AUTH_MAX) {
                        mode = WIFI_AUTH_MAX;
                    }
                    snprintf(optionValue, sizeof(optionValue), "%s", wifi_auth_mode_strings[mode]);
                } else if(
                    app->is_scanning && current_devices.devices[i]->scanType == SCAN_WIFI_STA &&
                    variable_item_get_current_value_index(current_devices.devices[i]->view) ==
                        WendigoOptionSTASavedNetworks) {
                    /* Update saved_networks_count for the current device */
                    snprintf(
                        optionValue,
                        sizeof(optionValue),
                        "%d Networks",
                        current_devices.devices[i]->radio.sta.saved_networks_count);
                } else if(
                    app->is_scanning && current_devices.devices[i]->scanType == SCAN_WIFI_STA &&
                    variable_item_get_current_value_index(current_devices.devices[i]->view) ==
                        WendigoOptionSTAAP) {
                    /* Update displayed AP for the current device */
                    if(memcmp(current_devices.devices[i]->radio.sta.apMac, nullMac, MAC_BYTES)) {
                        /* AP has a MAC - Do we have the AP in the cache? */
                        uint16_t apIdx =
                            device_index_from_mac(current_devices.devices[i]->radio.sta.apMac);
                        if(apIdx == devices_count || devices == NULL || devices[apIdx] == NULL ||
                           devices[apIdx]->scanType != SCAN_WIFI_AP ||
                           devices[apIdx]->radio.ap.ssid[0] == '\0') {
                            /* Either we don't have the AP in the cache or we don't have an
               * SSID for the AP - Display the MAC */
                            bytes_to_string(
                                current_devices.devices[i]->radio.sta.apMac,
                                MAC_BYTES,
                                optionValue);
                        } else {
                            /* We have an SSID for the AP */
                            snprintf(
                                optionValue,
                                sizeof(optionValue),
                                "%s",
                                devices[apIdx]->radio.ap.ssid);
                        }
                    } else {
                        /* We don't know the AP */
                        snprintf(optionValue, sizeof(optionValue), "AP Unknown");
                    }
                }
                if(optionValue[0] != '\0') {
                    /* We've updated the current device's selected option */
                    variable_item_set_current_value_text(
                        current_devices.devices[i]->view, optionValue);
                }
            }
        }
    }
    //  FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_list_on_event()");
    return consumed;
}

void wendigo_scene_device_list_on_exit(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_device_list_on_exit()");
    WendigoApp* app = context;
    variable_item_list_reset(app->devices_var_item_list);
    for(uint16_t i = 0; i < devices_count; ++i) {
        devices[i]->view = NULL;
    }
    if(app->leaving_scene) {
        /* This condition is met when we are genuinely exiting this scene - when
     * the back button has been pressed. When displaying a device list from
     * another device list, such as displaying an AP's STAs, this function is
     * called but we do not want to replace the current_devices we've just
     * constructed with the stack element we've just pushed. */
        /* Free current_devices.devices[] if necessary */
        if(current_devices.devices != NULL && current_devices.devices_count > 0) {
            if(current_devices.free_devices) {
                free(current_devices.devices);
            }
            current_devices.devices = NULL;
            current_devices.devices_count = 0;
        }
        /* Pop the previous device list off the stack if there's one there */
        if(stack_counter > 0) {
            /* Copy the DeviceListInstance, otherwise it'll be freed during use */
            memcpy(&current_devices, &(stack[stack_counter - 1]), sizeof(DeviceListInstance));
            /* When we pop the final stack element realloc() will act like free()
       * and return NULL */
            DeviceListInstance* stackAfterPop =
                realloc(stack, sizeof(DeviceListInstance) * (stack_counter - 1));
            if(stackAfterPop == NULL && stack_counter > 1) {
                wendigo_log(
                    MSG_ERROR,
                    "Unable to shrink DeviceListInstance stack. Hoping for the best...");
            } else {
                stack = stackAfterPop;
            }
            --stack_counter;
        }
        app->leaving_scene = false;
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_device_list_on_exit()");
}
