#include "storage_manager.h"

static Storage *storage;

uint8_t configuration_file_init(){
    storage = furi_record_open(RECORD_STORAGE);
    if(storage_common_exists(storage,CONF_FILE_FULL_PATH)){
        return 1;
    }else{
        File *file = storage_file_alloc(storage);
        storage_common_mkdir(storage,CONF_FOLDER_PATH);
        storage_file_open(file,CONF_FILE_FULL_PATH,FSAM_WRITE,FSOM_CREATE_NEW);
        storage_file_close(file);
        return 0;
    }
}

uint8_t configuration_file_read(struct gpio_explorer_configure_struct* configuration){
    File *file = storage_file_alloc(storage);
    char *buffer = malloc(sizeof *buffer * READ_BUFFER_SIZE);

    if(!file || !buffer) {
        storage_file_close(file);
        assert(0);
    }

    storage_file_open(file,CONF_FILE_FULL_PATH,FSAM_READ,FSOM_OPEN_EXISTING);
    storage_file_read(file,buffer,CONFIG_FILE_SIZE);
    storage_file_close(file);

    configuration->rgb_pin_r_index = buffer[11]-'0';
    configuration->rgb_pin_g_index = buffer[24]-'0';
    configuration->rgb_pin_b_index = buffer[37]-'0';
    configuration->led_pin_index = buffer[48]-'0';
    free(buffer);

    return 1;
}

uint8_t configuration_file_write(struct gpio_explorer_configure_struct* configuration){
    File *file = storage_file_alloc(storage);
    char* buffer = malloc(sizeof *buffer * READ_BUFFER_SIZE);
    if(!buffer||!file){
        storage_file_close(file);
        assert(0);
    }
    __wrap_snprintf(buffer,CONFIG_FILE_SIZE,"RGB_R_PIN: %d\nRGB_G_PIN: %d\nRGB_B_PIN: %d\nLED_PIN: %d",
        configuration->rgb_pin_r_index,
        configuration->rgb_pin_g_index,
        configuration->rgb_pin_b_index,
        configuration->led_pin_index);

    storage_file_open(file,CONF_FILE_FULL_PATH,FSAM_WRITE,FSOM_OPEN_EXISTING);
    storage_file_write(file,buffer,CONFIG_FILE_SIZE);
    free(buffer);
    storage_file_close(file);

    return 1;
}

void free_storage_manager(){
    furi_record_close(RECORD_STORAGE);
}