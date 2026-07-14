#pragma once

#include "wendigo_app_i.h"

/* Function imports from scenes */
extern void wendigo_scene_device_list_update(WendigoApp* app, wendigo_device* dev);
extern void wendigo_scene_status_add_attribute(WendigoApp* app, char* name, char* value);
extern void wendigo_scene_status_finish_layout(WendigoApp* app);
extern void wendigo_scene_status_begin_layout(WendigoApp* app);
extern uint16_t wendigo_scene_device_list_set_current_devices_mask(uint8_t deviceMask);
extern void wendigo_scene_device_list_set_current_devices(DeviceListInstance* devices);

/* Device caches - Declared extern to get around header spaghetti */
extern wendigo_device** devices;
extern uint16_t devices_count;
extern uint16_t devices_capacity;

void wendigo_set_scanning_interface(WendigoApp* app, InterfaceType interface, bool starting);
void wendigo_set_scanning_active(WendigoApp* app, bool starting);
void wendigo_scan_handle_rx_data_cb(uint8_t* buf, size_t len, void* context);
void wendigo_free_uart_buffer();
void wendigo_version(WendigoApp* app);
void wendigo_esp_status(WendigoApp* app);
void wendigo_free_devices();
uint16_t custom_device_index(wendigo_device* dev, wendigo_device** array, uint16_t array_count);
bool wendigo_update_device(WendigoApp* app, wendigo_device* dev);
bool wendigo_add_device(WendigoApp* app, wendigo_device* dev);
void wendigo_log(MsgType logType, char* message);
void wendigo_log_with_packet(MsgType logType, char* message, uint8_t* packet, uint16_t packet_size);
uint16_t device_index_from_mac(uint8_t mac[MAC_BYTES]);
uint8_t wendigo_index_of_string(char* str, char** array, uint8_t array_len);
