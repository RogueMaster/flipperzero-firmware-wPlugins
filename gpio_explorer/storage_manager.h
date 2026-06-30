#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <storage/storage.h>
#include <print/wrappers.h>
#include "gpio_explorer_configuration_struct.h"

#define CONF_FILE_FULL_PATH  "/ext/apps_data/gpio_explorer/gpio_explorer.conf"
#define CONF_FOLDER_PATH     "/ext/apps_data/gpio_explorer"
#define READ_BUFFER_SIZE     58
#define CONFIG_FILE_SIZE     50

uint8_t configuration_file_init();
uint8_t configuration_file_read(struct gpio_explorer_configure_struct* configuration);
uint8_t configuration_file_write(struct gpio_explorer_configure_struct* configuration);
void free_storage_manager();

#endif //STORAGE_MANAGER_H