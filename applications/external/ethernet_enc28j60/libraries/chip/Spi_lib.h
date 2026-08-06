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

/**
 * @brief Allocates and initializes a new SPI bus handle.
 *
 * This function dynamically allocates memory for a new `FuriHalSpiBusHandle`
 * object, initializes its internal state, and prepares it for use.
 * It's essential to call `spi_free()` to release the memory when the handle is no longer needed.
 *
 * @return A pointer to the newly allocated `FuriHalSpiBusHandle`
 */
FuriHalSpiBusHandle* spi_alloc();

/**
 * @brief Sends data over the SPI bus.
 *
 * This function sends a block of data from a specified buffer
 * to a device on the SPI bus. The data is transmitted using the SPI protocol,
 * respecting the configuration of the provided SPI bus handle.
 *
 * @param spi A pointer to the initialized `FuriHalSpiBusHandle`.
 * @param buffer A pointer to the buffer containing the data to be sent.
 * @param size The number of bytes to send from the buffer.
 * @return `true` if the data was sent successfully, `false` otherwise.
 */
bool spi_send(FuriHalSpiBusHandle* spi, const uint8_t* buffer, size_t size);

/**
 * @brief Sends data and then reads data from an SPI device in a single transaction.
 *
 * This function first transmits a specified number of bytes from `action_address`
 * and then immediately reads a specified number of bytes into the `data_read` buffer
 * without releasing the chip select (CS) line between the two operations. This is
 * useful for read-type commands where a command byte is sent, and a response is
 * expected back from the device.
 *
 * @param spi A pointer to the initialized `FuriHalSpiBusHandle`.
 * @param action_address A pointer to the buffer containing the data to be sent (e.g., a command or register address).
 * @param data_read A pointer to the buffer where the received data will be stored.
 * @param size_to_send The number of bytes to send from `action_address`.
 * @param size_to_read The number of bytes to read into `data_read`.
 * @return `true` if the entire transaction (send and read) was successful, `false` otherwise.
 */
bool spi_send_and_read(
    FuriHalSpiBusHandle* spi,
    const uint8_t* action_address,
    uint8_t* data_read,
    size_t size_to_send,
    size_t size_to_read);

/**
 * @brief Deallocates and cleans up an SPI bus handle.
 *
 * This function releases the memory and resources associated with the
 * provided `FuriHalSpiBusHandle`. After calling this function, the `spi` pointer
 * will be invalid and should not be used again. It is crucial to call this function
 * to prevent memory leaks after the SPI bus handle is no longer needed.
 *
 * @param spi A pointer to the `FuriHalSpiBusHandle` to be freed.
 */
void spi_free(FuriHalSpiBusHandle* spi);

#endif
