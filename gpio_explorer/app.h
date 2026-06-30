#ifndef APP_H
#define APP_H
///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// Includes ///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "storage_manager.h"
#include "gpio_explorer_app_struct.h"
#include "gpio_explorer_led_struct.h"
#include "gpio_explorer_rgb_struct.h"
#include "gpio_explorer_gpio_reader_struct.h"
#include "gpio_explorer_submenu_index_enum.h"
#include "gpio_explorer_view_enum.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// Defines and Macros /////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#define TAG                  "GPIOExplorer"

#define BACKLIGHT_ON         0
#define BUFFER_SIZE          2048
#define THREAD_SIZE          1024
#define DEFAULT_BAUD_RATE    115200
#define ON                   1
#define OFF                  0
#define PIN_R_CONFIG_LABEL   "RGB R Pin"
#define PIN_G_CONFIG_LABEL   "RGB G Pin"
#define PIN_B_CONFIG_LABEL   "RGB B Pin"
#define PIN_LED_CONFIG_LABEL "LED Pin"
#define DEFAULT_LED_PIN      0
#define DEFAULT_RGB_R_PIN    5
#define DEFAULT_RGB_G_PIN    4
#define DEFAULT_RGB_B_PIN    3

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// Global Veriables ///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

static const char* const rgb_setting_pins[] =
    {"PA7", "PA6", "PA4", "PB3", "PB2", "PC3", "PC1", "PC0"};
static const char* const rgb_colors[] =
    {"White", "Red", "Green", "Blue", "Purple", "Yellow", "Cyan"};
static const GpioPin* const pins[] = {
    &gpio_ext_pa7,
    &gpio_ext_pa6,
    &gpio_ext_pa4,
    &gpio_ext_pb3,
    &gpio_ext_pb2,
    &gpio_ext_pc3,
    &gpio_ext_pc1,
    &gpio_ext_pc0};

int32_t main_gpio_explorer_app(void* _p);

#endif //APP_H