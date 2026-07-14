#ifndef _SPI_LIB_H_
#define _SPI_LIB_H_

#include <furi_hal.h>
#include <furi_hal_bus.h>
#include <furi_hal_spi.h>
#include <furi_hal_spi_config.h>

#define TIMEOUT_SPI 100
#define CS          &gpio_ext_pa4
#define SCK         &gpio_ext_pb3
#define MOSI        &gpio_ext_pa7
#define MISO        &gpio_ext_pa6

#define BUS        &furi_hal_spi_bus_r
#define SPEED_8MHZ &furi_hal_spi_preset_1edge_low_8m // 8 MHZ
#define SPEED_4MHZ &furi_hal_spi_preset_1edge_low_4m // 4 MHZ
#define SPEED_2MHZ &furi_hal_spi_preset_1edge_low_2m // 2 MHZ

// FUNCTIONS
FuriHalSpiBusHandle* spi_alloc();

#endif
