#include "wendigo_app_i.h"
#include "wendigo_scan.h"
#include "wendigo_pnl.h"

/* PNL cache */
PreferredNetwork* networks = NULL;
uint16_t networks_count = 0;
uint16_t networks_capacity = 0;

/** Search networks[] for a PreferredNetwork with the specified SSID.
 * Returns the index of the PreferredNetwork, or networks_count if not
 * found.
 * SSID must be a null-terminated string.
 */
uint16_t index_of_pnl(char* ssid) {
    FURI_LOG_T(WENDIGO_TAG, "Start index_of_pnl()");
    if(ssid == NULL || strlen(ssid) == 0 || networks == NULL || networks_count == 0) {
        FURI_LOG_T(WENDIGO_TAG, "End index_of_pnl() - Invalid arguments.");
        return networks_count;
    }
    uint16_t idx;
    uint8_t compareLen = MAX_SSID_LEN;
    if(strlen(ssid) < compareLen) {
        compareLen = strlen(ssid);
    }
    for(idx = 0; idx < networks_count && strncmp(ssid, networks[idx].ssid, compareLen); ++idx) {
    }
    FURI_LOG_T(WENDIGO_TAG, "End index_of_pnl()");
    return idx;
}

/** Find the PreferredNetwork element of networks[] containing the
 * specified SSID. Returns a reference to the element of networks[]
 * or NULL if not found;
 * SSID must be a null-terminated string.
 */
PreferredNetwork* pnl_for_ssid(char* ssid) {
    FURI_LOG_T(WENDIGO_TAG, "Start pnl_for_ssid()");
    if(ssid == NULL || strlen(ssid) == 0 || networks == NULL || networks_count == 0) {
        FURI_LOG_T(WENDIGO_TAG, "End pnl_for_ssid() - Invalid arguments.");
        return NULL;
    }
    PreferredNetwork* result = NULL;
    uint16_t idx = index_of_pnl(ssid);
    if(idx < networks_count) {
        result = &(networks[idx]);
    }
    FURI_LOG_T(WENDIGO_TAG, "End pnl_for_ssid()");
    return result;
}

/** Returns the number of networks the specified STA has probed for */
uint8_t count_networks_for_device(wendigo_device* dev) {
    FURI_LOG_T(WENDIGO_TAG, "Start count_networks_for_device()");
    if(dev == NULL || dev->scanType != SCAN_WIFI_STA || dev->radio.sta.saved_networks_count == 0 ||
       dev->radio.sta.saved_networks == NULL) {
        FURI_LOG_T(WENDIGO_TAG, "End count_networks_for_device() - No networks.");
        return 0;
    }
    FURI_LOG_T(WENDIGO_TAG, "End count_networks_for_device()");
    return dev->radio.sta.saved_networks_count;
}

/** Process the device cache (devices[]) and generate PreferredNetwork instances
 * that map all identified networks to the wendigo_device instances that have
 * probed for them.
 * networks[] and networks_count are updated by this function.
 * Returns the number of associated networks.
 */
uint16_t map_ssids_to_devices(WendigoApp* app) {
    FURI_LOG_T(WENDIGO_TAG, "Start map_ssids_to_devices()");
    if(networks_count > 0 && networks != NULL) {
        /* If networks[] has been initialised we can assume it's up to date */
        return networks_count;
    }
    /* Input validation */
    if(app == NULL || devices == NULL || devices_count == 0) {
        FURI_LOG_T(WENDIGO_TAG, "End map_ssids_to_devices() - Invalid arguments.");
        return 0;
    }

    uint8_t this_count;
    PreferredNetwork* new_networks;
    furi_mutex_acquire(app->pnlMutex, FuriWaitForever);
    /* Loop over each device */
    for(uint16_t i = 0; i < devices_count; ++i) {
        if(devices == NULL || devices[i] == NULL || devices[i]->scanType != SCAN_WIFI_STA ||
           devices[i]->radio.sta.saved_networks == NULL) {
            this_count = 0;
        } else {
            this_count = devices[i]->radio.sta.saved_networks_count;
        }
        /* Make sure networks[] is large enough to include this_count additional nets */
        if(networks_count + this_count > networks_capacity) {
            new_networks =
                realloc(networks, sizeof(PreferredNetwork) * (networks_count + this_count));
            if(new_networks == NULL) {
                /* Too annoying to continue on - log and alert error, then clean up and exit */
                furi_mutex_release(app->pnlMutex);
                wendigo_log(MSG_ERROR, "Unable to allocate PreferredNetwork elements.");
                wendigo_display_popup(
                    app, "Insufficient memory", "Unable to allocate PreferredNetwork elements.");
                FURI_LOG_T(
                    WENDIGO_TAG, "End map_ssids_to_devices() - Unable to resize networks[].");
                return 0;
            }
            /* Zero out newly-allocated space to initialise new PreferredNetwork elements
             * (NULL ssid[] and devices[], 0 device_count). */
            bzero(
                new_networks + (networks_count * sizeof(PreferredNetwork)),
                this_count * sizeof(PreferredNetwork));
            networks_capacity = networks_count + this_count;
            networks = new_networks;
        }
        /* Check each saved network for devices[i] */
        for(uint8_t j = 0; j < this_count; ++j) {
            new_networks = pnl_for_ssid(devices[i]->radio.sta.saved_networks[j]);
            if(new_networks == NULL) {
                strncpy(
                    networks[networks_count].ssid,
                    devices[i]->radio.sta.saved_networks[j],
                    strlen(devices[i]->radio.sta.saved_networks[j]));
                networks[networks_count].device_count = 0;
                networks[networks_count].devices = NULL;
                new_networks = &(networks[networks_count++]);
            }
            /* Is devices[i] in new_networks->devices[]? */
            uint8_t devIdx;
            for(devIdx = 0;
                devIdx < new_networks->device_count && new_networks->devices[devIdx] != devices[i];
                ++devIdx) {
            }
            if(devIdx == new_networks->device_count) {
                /* Append devices[i] to new_networks->devices[] */
                wendigo_device** new_devices = realloc(
                    new_networks->devices,
                    sizeof(wendigo_device*) * (new_networks->device_count + 1));
                if(new_devices == NULL) {
                    char* msg = malloc(sizeof(char) * (85 + MAX_SSID_LEN + MAC_STRLEN));
                    if(msg == NULL) {
                        wendigo_log(
                            MSG_ERROR,
                            "Unable to allocate memory to store an additional device in a PreferredNetwork.");
                    } else {
                        char* staMac = malloc(sizeof(char) * (MAC_STRLEN + 1));
                        if(staMac == NULL) {
                            snprintf(
                                msg,
                                85 + MAX_SSID_LEN + MAC_STRLEN,
                                "Unable to allocate an additional %d bytes to %s's PreferredNetwork element.",
                                sizeof(wendigo_device*),
                                new_networks->ssid);
                        } else {
                            bytes_to_string(devices[i]->mac, MAC_BYTES, staMac);
                            snprintf(
                                msg,
                                85 + MAX_SSID_LEN + MAC_STRLEN,
                                "Unable to allocate an additional %d bytes to %s's PreferredNetwork element for STA %s.",
                                sizeof(wendigo_device*),
                                new_networks->ssid,
                                staMac);
                            free(staMac);
                        }
                        wendigo_log(MSG_ERROR, msg);
                        free(msg);
                    }
                } else {
                    /* Append devices[i] */
                    new_devices[new_networks->device_count++] = devices[i];
                    new_networks->devices = new_devices;
                }
            }
        }
    }
    if(networks_capacity > networks_count) {
        new_networks = realloc(networks, sizeof(PreferredNetwork) * networks_count);
        if(new_networks == NULL) {
            /* This condition will (should) never be true */
            wendigo_log(MSG_ERROR, "Unable to shrink networks[] to remove spare capacity.");
            /* Don't fret too much */
        } else {
            networks_capacity = networks_count;
            networks = new_networks;
        }
    }
    furi_mutex_release(app->pnlMutex);
    if(app->current_view == WendigoAppViewVarItemList) {
        /* Set/Refresh the SSID count on the main menu */
        view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventRefreshPNLCount);
    }
    FURI_LOG_T(WENDIGO_TAG, "End map_ssids_to_devices()");
    return networks_count;
}

/** Retrieve the SSIDs that the specified device has sent probes for.
 * SSIDs will be stored in ***result, which must be an UNINITIALISED pointer.
 * This function will allocate memory for the pointer array and for each
 * char[], the caller is responsible for freeing this memory when it's no
 * longer needed.
 * To be clear, char ***result means you should declare a char **variable,
 * which is an array of strings. When calling the function you use
 *              get_networks_for_device(dev, &variable);
 * That allows this function to modify the memory where your local
 * char **variable is stored - meaning I can allocate enough pointers to
 * hold the SSIDs.
 * Returns the number of SSIDs in results.
 * CAUTION: This function is unused and has not been tested.
 */
uint8_t get_networks_for_device(WendigoApp* app, wendigo_device* dev, char*** result) {
    FURI_LOG_T(WENDIGO_TAG, "Start get_networks_for_device()");
    if(dev == NULL || result == NULL || dev->scanType != SCAN_WIFI_STA || app == NULL) {
        FURI_LOG_T(WENDIGO_TAG, "End get_networks_for_device() - Invalid arguments.");
        return 0;
    }
    uint8_t nets_count = count_networks_for_device(dev);
    if(nets_count == 0) {
        FURI_LOG_T(WENDIGO_TAG, "End get_networks_for_device() - No networks.");
        return 0;
    }
    /* Allocate memory for the caller's char **result */
    char** res = malloc(sizeof(char*) * nets_count);
    *result = res;
    if(res == NULL) {
        char* msg = malloc(sizeof(char) * 48);
        if(msg == NULL) {
            wendigo_display_popup(
                app,
                "Insufficient memory",
                "Unable to allocate memory for preferred network list.");
            wendigo_log(MSG_ERROR, "Unable to allocate memory for device's PNL.");
        } else {
            snprintf(
                msg,
                48,
                "Unable to allocate %d bytes for device's PNL.",
                sizeof(char) * nets_count);
            wendigo_log(MSG_ERROR, msg);
            wendigo_display_popup(app, "Insufficient memory", msg);
            free(msg);
        }
        FURI_LOG_T(
            WENDIGO_TAG, "End get_networks_for_device() - Unable to initialise results array.");
        return 0;
    }
    /* Copy each of dev's saved_networks[] into result*** */
    for(uint8_t i = 0; i < nets_count; ++i) {
        if(dev->radio.sta.saved_networks[i] != NULL &&
           strlen(dev->radio.sta.saved_networks[i]) > 0) {
            res[i] = malloc(sizeof(char) * (strlen(dev->radio.sta.saved_networks[i]) + 1));
            if(res[i] == NULL) {
                char* msg = malloc(sizeof(char) * 55);
                if(msg == NULL) {
                    wendigo_log(MSG_ERROR, "Failed to allocate memory for a probed network.");
                } else {
                    snprintf(
                        msg,
                        55,
                        "Failed to allocate %d bytes for a probed SSID.",
                        strlen(dev->radio.sta.saved_networks[i]) + 1);
                    wendigo_log(MSG_ERROR, msg);
                    free(msg);
                }
            } else {
                strncpy(
                    res[i],
                    dev->radio.sta.saved_networks[i],
                    strlen(dev->radio.sta.saved_networks[i]) + 1);
            }
        }
    }
    FURI_LOG_T(WENDIGO_TAG, "End get_networks_for_device()");
    return nets_count;
}

/** Search for a PreferredNetwork representing the specified SSID, creating
 * the PreferredNetwork if it does not exist and returning a reference to it.
 * Returns NULL if the SSID is not in networks[] and additional memory could
 * not be allocated to make space for it.
 * If a new PreferredNetwork is created it is initialised, setting devices[]
 * to NULL and device_count to 0.
 * If result is not NULL it will be set to PNL_EXISTS, PNL_CREATED or
 * PNL_FAILED to indicate the result of the operation.
 * CAUTION: This function does not acquire app->pnlMutex. This MUST BE DONE
 * by the calling function in order to prevent concurrency issues.
 */
PreferredNetwork* fetch_or_create_pnl(char* ssid, PNL_Result* result) {
    FURI_LOG_T(WENDIGO_TAG, "Start fetch_or_create_pnl()");
    PreferredNetwork* pnl;
    uint16_t idx = index_of_pnl(ssid);
    if(idx == networks_count) {
        /* Not found - Create a new PreferredNetwork */
        if(networks_count == networks_capacity) {
            /* No spare capacity - Extend networks[] */
            pnl = realloc(networks, sizeof(PreferredNetwork) * (networks_count + 1));
            if(pnl == NULL) {
                if(result != NULL) {
                    *result = PNL_FAILED;
                }
                char* msg = malloc(sizeof(char) * (82 + MAX_SSID_LEN));
                if(msg == NULL) {
                    wendigo_log(
                        MSG_ERROR,
                        "wendigo_add_device(): Failed to increase networks[], skipping PNL.");
                } else {
                    snprintf(
                        msg,
                        82 + MAX_SSID_LEN,
                        "wendigo_add_device(): Failed to increase networks[] to %d bytes, skipping PNL %s.",
                        sizeof(PreferredNetwork) * (networks_count + 1),
                        ssid);
                    wendigo_log(MSG_ERROR, msg);
                    free(msg);
                }
            } else {
                /* Allocated successfully */
                if(result != NULL) {
                    *result = PNL_CREATED;
                }
                networks = pnl;
                ++networks_capacity;
            }
        }
        if(networks_count < networks_capacity) {
            /* Allocated successfully or had spare capacity - Initialise */
            bzero(
                networks + (sizeof(PreferredNetwork) * networks_count), sizeof(PreferredNetwork));
            idx = networks_count++;
            strncpy(networks[idx].ssid, ssid, strlen(ssid));
        }
    } else {
        if(result != NULL) {
            *result = PNL_EXISTS;
        }
    }
    if(idx < networks_count) {
        FURI_LOG_T(WENDIGO_TAG, "End fetch_or_create_pnl() - Succeeded.");
        return &(networks[idx]);
    }
    FURI_LOG_T(WENDIGO_TAG, "End fetch_or_create_pnl() - Failed.");
    return NULL;
}

/** Search the specified PreferredNetwork for a device containing the
 * specified MAC. Returns pnl->device_count if not found.
 */
uint8_t pnl_index_of_mac(PreferredNetwork* pnl, uint8_t mac[MAC_BYTES]) {
    FURI_LOG_T(WENDIGO_TAG, "Start pnl_index_of_mac()");
    if(pnl == NULL || pnl->devices == NULL) {
        wendigo_log(MSG_ERROR, "pnl_index_of_mac() called with NULL arguments.");
        return 0;
    }
    uint8_t idx;
    for(idx = 0; idx < pnl->device_count &&
                 (pnl->devices[idx] == NULL || memcmp(pnl->devices[idx]->mac, mac, MAC_BYTES));
        ++idx) {
    }
    FURI_LOG_T(WENDIGO_TAG, "End pnl_index_of_mac()");
    return idx;
}

/** Search the specified PreferredNetwork for a device with the same MAC as
 * the specified device. Returns pnl->device_count if not found.
 */
uint8_t pnl_index_of_device(PreferredNetwork* pnl, wendigo_device* dev) {
    FURI_LOG_T(WENDIGO_TAG, "Start+End pnl_index_of_device()");
    if(dev == NULL || pnl == NULL || pnl->devices == NULL) {
        wendigo_log(MSG_ERROR, "pnl_index_of_device() called with NULL arguments.");
        return 0;
    }
    return pnl_index_of_mac(pnl, dev->mac);
}

/** Ensure that a PreferredNetwork representing the specified SSID exists,
 * and contains the specified wendigo_device.
 * If a PreferredNetwork for the specified SSID doesn't exist it will be
 * created. If the PreferredNetwork does not contain the specified device
 * it will be added.
 * Returns PNL_EXISTS if the specified SSID and device already exists.
 * Returns PNL_CREATED if a PreferredNetwork containing dev was created.
 * Returns PNL_DEVICE_CREATED if dev was added to an existing PreferredNetwork.
 * Returns PNL_FAILED if the specified PreferredNetwork or wendigo_device did
 * not exist and memory could not be allocated for them.
 */
PNL_Result pnl_find_or_create_device(WendigoApp* app, char* ssid, wendigo_device* dev) {
    FURI_LOG_T(WENDIGO_TAG, "Start pnl_find_or_create_device()");
    if(ssid == NULL || strlen(ssid) == 0 || dev == NULL || dev->scanType != SCAN_WIFI_STA ||
       app == NULL) {
        FURI_LOG_T(WENDIGO_TAG, "End pnl_find_or_create_device() - Invalid arguments.");
        return PNL_FAILED;
    }
    PNL_Result result = PNL_IN_PROGRESS;
    furi_mutex_acquire(app->pnlMutex, FuriWaitForever);
    PreferredNetwork* pnl = fetch_or_create_pnl(ssid, &result);
    if(pnl == NULL) {
        /* Error is logged by fetch_or_create_pnl() - No need to duplicate */
        furi_mutex_release(app->pnlMutex);
        FURI_LOG_T(
            WENDIGO_TAG, "End pnl_find_or_create_device() - Failed to obtain PreferredNetwork.");
        return PNL_FAILED;
    }
    uint8_t devIdx = pnl_index_of_mac(pnl, dev->mac);
    if(devIdx == pnl->device_count) {
        /* Device is not registered in PNL - Append it */
        wendigo_device** new_dev =
            realloc(pnl->devices, sizeof(wendigo_device*) * (pnl->device_count + 1));
        if(new_dev == NULL) {
            /* Failed to extend pnl->devices[] */
            char* msg = malloc(sizeof(char) * (51 + MAX_SSID_LEN));
            if(msg == NULL) {
                wendigo_log(MSG_ERROR, "Failed to extend PNL devices array.");
            } else {
                snprintf(
                    msg,
                    51 + MAX_SSID_LEN,
                    "Failed to extend devices array for %s to %d bytes.",
                    ssid,
                    sizeof(wendigo_device*) * (pnl->device_count + 1));
                wendigo_log(MSG_ERROR, msg);
                free(msg);
            }
            furi_mutex_release(app->pnlMutex);
            return PNL_FAILED;
        } else if(result != PNL_CREATED) {
            /* Extended pnl->devices[] and pnl already existed */
            result = PNL_DEVICE_CREATED;
        }
        pnl->devices = new_dev;
        pnl->devices[pnl->device_count++] = dev;
    } else {
        result = PNL_EXISTS;
    }
    furi_mutex_release(app->pnlMutex);
    if(app->current_view == WendigoAppViewVarItemList) {
        /* Add/Refresh SSID count on main menu */
        // TODO: This couples wendigo_pnl.c to the UI - Change this to a generic inversion of control pattern later on
        view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventRefreshPNLCount);
    }
    FURI_LOG_T(WENDIGO_TAG, "End pnl_find_or_create_device()");
    return result;
}

/* Create a trace log entry describing the specified PNL_Result */
void pnl_log_result(char* tag, PNL_Result res, char* ssid, wendigo_device* dev) {
    FURI_LOG_T(WENDIGO_TAG, "Start pnl_log_result()");
    if(tag == NULL || strlen(tag) == 0 || ssid == NULL || strlen(ssid) == 0 || dev == NULL ||
       res == PNL_IN_PROGRESS) {
        FURI_LOG_T(WENDIGO_TAG, "End pnl_log_result() - Invalid arguments.");
        return;
    }
    char* devMac = malloc(sizeof(char) * (MAC_STRLEN + 1));
    if(devMac != NULL) {
        bytes_to_string(dev->mac, MAC_BYTES, devMac);
    }
    switch(res) {
    case PNL_FAILED:
        FURI_LOG_T(
            tag,
            "Failed to add %s to PreferredNetwork %s.",
            (devMac == NULL) ? "device" : devMac,
            ssid);
        break;
    case PNL_CREATED:
        FURI_LOG_T(
            tag,
            "Created PreferredNetwork %s containing %s.",
            ssid,
            (devMac == NULL) ? "device" : devMac);
        break;
    case PNL_DEVICE_CREATED:
        FURI_LOG_T(
            tag, "Added %s to PreferredNetwork %s.", (devMac == NULL) ? "device" : devMac, ssid);
        break;
    case PNL_EXISTS:
        FURI_LOG_T(
            tag, "PreferredNetwork %s contains %s.", ssid, (devMac == NULL) ? "device" : devMac);
        break;
    default:
        /* No action */
        break;
    }
    if(devMac != NULL) {
        free(devMac);
    }
    FURI_LOG_T(WENDIGO_TAG, "End pnl_log_result()");
}
