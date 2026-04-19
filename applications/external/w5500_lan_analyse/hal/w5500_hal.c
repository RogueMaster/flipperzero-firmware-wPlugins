#include "w5500_hal.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_spi.h>
#include <furi_hal_gpio.h>
#include <furi_hal_power.h>

#include <wizchip_conf.h>
#include <socket.h>
#include <w5500.h>

#define TAG "W5500_HAL"

/* GPIO pins for CS and RESET */
static const GpioPin gpio_cs = {.port = GPIOA, .pin = LL_GPIO_PIN_4};
static const GpioPin gpio_reset = {.port = GPIOC, .pin = LL_GPIO_PIN_3};

/* SPI acquired flag */
static bool spi_acquired = false;

/* --- SPI callback functions for WIZnet driver --- */

static void w5500_cs_select(void) {
    furi_hal_gpio_write(&gpio_cs, false);
}

static void w5500_cs_deselect(void) {
    furi_hal_gpio_write(&gpio_cs, true);
}

static uint8_t w5500_spi_read_byte(void) {
    uint8_t byte;
    furi_hal_spi_bus_rx(&furi_hal_spi_bus_handle_external, &byte, 1, 1000);
    return byte;
}

static void w5500_spi_write_byte(uint8_t byte) {
    furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_external, &byte, 1, 1000);
}

static void w5500_spi_read_burst(uint8_t* buf, uint16_t len) {
    furi_hal_spi_bus_rx(&furi_hal_spi_bus_handle_external, buf, len, 1000);
}

static void w5500_spi_write_burst(uint8_t* buf, uint16_t len) {
    furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_external, buf, len, 1000);
}

bool w5500_hal_init(void) {
    FURI_LOG_I(TAG, "Initializing W5500 HAL");

    /* Acquire SPI bus first — if it hangs here, no resources are leaked */
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);
    spi_acquired = true;

    /* Enable OTG power for the W5500 module (after SPI, so deinit can clean up) */
    furi_hal_power_enable_otg();
    furi_delay_ms(300);

    /* Configure CS pin: output open-drain, default high (deselected) */
    furi_hal_gpio_write(&gpio_cs, true);
    furi_hal_gpio_init(&gpio_cs, GpioModeOutputOpenDrain, GpioPullNo, GpioSpeedVeryHigh);

    /* Configure RESET pin: output open-drain, default high (not reset) */
    furi_hal_gpio_write(&gpio_reset, true);
    furi_hal_gpio_init(&gpio_reset, GpioModeOutputOpenDrain, GpioPullNo, GpioSpeedVeryHigh);

    /* Register SPI callbacks with WIZnet driver */
    reg_wizchip_spi_cbfunc(w5500_spi_read_byte, w5500_spi_write_byte);
    reg_wizchip_spiburst_cbfunc(w5500_spi_read_burst, w5500_spi_write_burst);
    reg_wizchip_cs_cbfunc(w5500_cs_select, w5500_cs_deselect);

    FURI_LOG_I(TAG, "W5500 HAL initialized");
    return true;
}

void w5500_hal_deinit(void) {
    FURI_LOG_I(TAG, "Deinitializing W5500 HAL");

    /* Close any open sockets (safe even if chip not initialized) */
    if(spi_acquired) {
        for(uint8_t i = 0; i < 8; i++) {
            close(i);
        }
        furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
        spi_acquired = false;
    }

    /* Reset GPIO pins to analog (safe to call unconditionally) */
    furi_hal_gpio_init(&gpio_cs, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_reset, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    /* Disable OTG power (ref-counted, safe if already disabled) */
    if(furi_hal_power_is_otg_enabled()) {
        furi_hal_power_disable_otg();
    }

    FURI_LOG_I(TAG, "W5500 HAL deinitialized");
}

void w5500_hal_hw_reset(void) {
    FURI_LOG_I(TAG, "Hardware reset W5500");

    /* Pull RESET low for 50ms, then high */
    furi_hal_gpio_write(&gpio_reset, false);
    furi_delay_ms(50);
    furi_hal_gpio_write(&gpio_reset, true);
    furi_delay_ms(100);

    FURI_LOG_I(TAG, "Hardware reset complete");
}

bool w5500_hal_chip_init(void) {
    FURI_LOG_I(TAG, "Initializing W5500 chip");

    /*
     * FIFO buffer sizes for 8 sockets (TX/RX in KB each).
     * Must be powers of 2 (0,1,2,4,8,16). Total = 16KB each.
     * Socket 0: 8KB MACRAW, 1-2: 2KB general tools,
     * 3: 1KB PXE DHCP, 4: 2KB PXE TFTP (blksize up to 1468),
     * 5: 1KB HTTP DL, 6-7: unused.
     */
    uint8_t rx_sizes[8] = {8, 2, 2, 1, 2, 1, 0, 0};
    uint8_t tx_sizes[8] = {8, 2, 2, 1, 2, 1, 0, 0};
    uint8_t fifo_sizes[2][8];
    memcpy(fifo_sizes[0], tx_sizes, 8);
    memcpy(fifo_sizes[1], rx_sizes, 8);

    if(ctlwizchip(CW_INIT_WIZCHIP, (void*)fifo_sizes) == -1) {
        FURI_LOG_E(TAG, "W5500 chip init failed (CW_INIT_WIZCHIP)");
        return false;
    }

    FURI_LOG_I(TAG, "W5500 chip initialized successfully");
    return true;
}

bool w5500_hal_check_version(void) {
    /* W5500 VERSIONR should return 0x04 */
    uint8_t ver = getVERSIONR();
    FURI_LOG_I(TAG, "W5500 VERSIONR = 0x%02X", ver);
    return (ver == 0x04);
}

void w5500_hal_set_mac(const uint8_t mac[6]) {
    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info);
    memcpy(net_info.mac, mac, 6);
    wizchip_setnetinfo(&net_info);
    setSHAR((uint8_t*)mac);
    FURI_LOG_I(
        TAG,
        "MAC set: %02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
}

void w5500_hal_get_mac(uint8_t mac[6]) {
    getSHAR(mac);
}

void w5500_hal_set_net_info(
    const uint8_t ip[4],
    const uint8_t subnet[4],
    const uint8_t gateway[4],
    const uint8_t dns[4]) {
    wiz_NetInfo net_info;
    wizchip_getnetinfo(&net_info);
    memcpy(net_info.ip, ip, 4);
    memcpy(net_info.sn, subnet, 4);
    memcpy(net_info.gw, gateway, 4);
    memcpy(net_info.dns, dns, 4);
    net_info.dhcp = NETINFO_STATIC;
    wizchip_setnetinfo(&net_info);
}

bool w5500_hal_get_link_status(void) {
    uint8_t link = PHY_LINK_OFF;
    if(ctlwizchip(CW_GET_PHYLINK, (void*)&link) == -1) {
        FURI_LOG_E(TAG, "Failed to read PHY link status");
        return false;
    }
    return (link != PHY_LINK_OFF);
}

void w5500_hal_get_phy_info(bool* link_up, uint8_t* speed, uint8_t* duplex) {
    /*
     * PHYCFGR register (0x002E) layout for W5500:
     *   Bit 0: LNK  (1 = link up)
     *   Bit 1: SPD  (1 = 100Mbps, 0 = 10Mbps)
     *   Bit 2: DPX  (1 = full duplex, 0 = half duplex)
     *   Bit 3: OPMDC[0]
     *   Bit 4: OPMDC[1]
     *   Bit 5: OPMDC[2]
     *   Bit 6: OPMD (0 = SW config, 1 = HW config)
     *   Bit 7: RST  (write 0 to reset PHY, auto-set to 1)
     */
    uint8_t phycfgr = getPHYCFGR();
    FURI_LOG_D(TAG, "PHYCFGR = 0x%02X", phycfgr);

    if(link_up) *link_up = (phycfgr & PHYCFGR_LNK_MASK) != 0;
    if(speed) *speed = (phycfgr & PHYCFGR_SPD_MASK) ? 1 : 0;
    if(duplex) *duplex = (phycfgr & PHYCFGR_DPX_MASK) ? 1 : 0;
}

bool w5500_hal_open_macraw(void) {
    FURI_LOG_I(TAG, "Opening MACRAW socket (Socket 0)");

    /* Close Socket 0 first if it was open */
    close(W5500_MACRAW_SOCKET);
    furi_delay_ms(10);

    /*
     * Open Socket 0 in MACRAW mode.
     * Sn_MR = 0x04 (MACRAW).
     * The second parameter (port) is not used for MACRAW but must be non-zero.
     * MFEN=0 means accept all frames (not just matching our MAC).
     *
     * We use socket() from WIZnet driver which sets Sn_MR and opens the socket.
     * The flag Sn_MR_MFEN (0x80) controls MAC filter; we do NOT set it.
     */
    int8_t ret = socket(W5500_MACRAW_SOCKET, Sn_MR_MACRAW, 0, 0);
    if(ret != W5500_MACRAW_SOCKET) {
        FURI_LOG_E(TAG, "Failed to open MACRAW socket: %d", ret);
        return false;
    }

    FURI_LOG_I(TAG, "MACRAW socket opened successfully");
    return true;
}

void w5500_hal_close_macraw(void) {
    close(W5500_MACRAW_SOCKET);
    FURI_LOG_I(TAG, "MACRAW socket closed");
}

uint16_t w5500_hal_macraw_recv(uint8_t* buf, uint16_t buf_size) {
    /* Check if there is data available in Socket 0 RX buffer */
    uint16_t rx_size = getSn_RX_RSR(W5500_MACRAW_SOCKET);
    if(rx_size == 0) {
        return 0;
    }

    /*
     * In MACRAW mode, recvfrom() handles the 2-byte packet info header
     * automatically. The addr/port params are ignored for MACRAW.
     */
    uint8_t dummy_ip[4];
    uint16_t dummy_port;
    int32_t ret = recvfrom(W5500_MACRAW_SOCKET, buf, buf_size, dummy_ip, &dummy_port);
    if(ret <= 0) {
        return 0;
    }

    return (uint16_t)ret;
}

uint16_t w5500_hal_macraw_send(const uint8_t* buf, uint16_t len) {
    /*
     * In MACRAW mode, sendto() sends a raw Ethernet frame.
     * The addr/port params are ignored for MACRAW.
     */
    uint8_t dummy_ip[4] = {0, 0, 0, 0};
    int32_t ret = sendto(W5500_MACRAW_SOCKET, (uint8_t*)buf, len, dummy_ip, 0);
    if(ret <= 0) {
        FURI_LOG_E(TAG, "MACRAW send failed: %ld", ret);
        return 0;
    }
    return (uint16_t)ret;
}
