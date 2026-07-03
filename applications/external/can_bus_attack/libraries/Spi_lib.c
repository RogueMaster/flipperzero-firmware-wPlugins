#include "Spi_lib.h"

// Function to callback for the SPI to work
inline static void furi_hal_spi_bus_r_handle_event_callback(
    const FuriHalSpiBusHandle* handle,
    FuriHalSpiBusHandleEvent event,
    const LL_SPI_InitTypeDef* preset) {
    if(event == FuriHalSpiBusHandleEventInit) {
        furi_hal_gpio_write(handle->cs, true);
        furi_hal_gpio_init(handle->cs, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    } else if(event == FuriHalSpiBusHandleEventDeinit) {
        furi_hal_gpio_write(handle->cs, true);
        furi_hal_gpio_init(handle->cs, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    } else if(event == FuriHalSpiBusHandleEventActivate) {
        LL_SPI_Init(handle->bus->spi, (LL_SPI_InitTypeDef*)preset);
        LL_SPI_SetRxFIFOThreshold(handle->bus->spi, LL_SPI_RX_FIFO_TH_QUARTER);
        LL_SPI_Enable(handle->bus->spi);

        furi_hal_gpio_init_ex(
            handle->miso,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI1);
        furi_hal_gpio_init_ex(
            handle->mosi,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI1);
        furi_hal_gpio_init_ex(
            handle->sck,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI1);

        furi_hal_gpio_write(handle->cs, false);
    } else if(event == FuriHalSpiBusHandleEventDeactivate) {
        furi_hal_gpio_write(handle->cs, true);

        furi_hal_gpio_init(handle->miso, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_init(handle->mosi, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_init(handle->sck, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

        LL_SPI_Disable(handle->bus->spi);
    }
}

// Here is the CALLBACK
static void spi_bus_callback(const FuriHalSpiBusHandle* handle, FuriHalSpiBusHandleEvent event) {
    furi_hal_spi_bus_r_handle_event_callback(handle, event, SPEED_8MHZ);
}

//  This is to Init the SPI Communication
FuriHalSpiBusHandle* spi_alloc() {
    FuriHalSpiBusHandle* spi = malloc(sizeof(FuriHalSpiBusHandle));
    spi->bus = BUS;
    spi->callback = &spi_bus_callback;
    spi->cs = CS;
    spi->miso = MISO;
    spi->mosi = MOSI;
    spi->sck = SCK;
    return spi;
}

// Function to send data
bool spi_send(FuriHalSpiBusHandle* spi, uint8_t* buffer) {
    furi_hal_spi_acquire(spi);
    bool ret = furi_hal_spi_bus_tx(spi, buffer, sizeof(buffer), TIMEOUT_SPI);
    furi_hal_spi_release(spi);
    return ret;
}

// Function to read register
bool spi_send_and_read(FuriHalSpiBusHandle* spi, uint8_t* action_address, uint8_t* data_read) {
    furi_hal_spi_acquire(spi);
    bool ret =
        (furi_hal_spi_bus_tx(spi, action_address, sizeof(action_address), TIMEOUT_SPI) &&
         furi_hal_spi_bus_rx(spi, data_read, sizeof(data_read), TIMEOUT_SPI));
    furi_hal_spi_release(spi);
    return ret;
}
