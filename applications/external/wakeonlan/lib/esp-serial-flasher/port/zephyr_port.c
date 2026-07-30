/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT espressif_esp_loader

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/console/tty.h>
#include <zephyr/sys/printk.h>

#include <zephyr_port.h>
#include <esp_loader.h>
#include <esp_loader_io.h>

struct esp_loader_dev_config {
    esp_loader_config_t config;
    esp_loader_connect_args_t connect_args;
};

struct esp_loader_dev_data {
    zephyr_port_t interface;
    esp_loader_t loader;
};

static const char *const s_log_level_str[] = { "", "E", "W", "I", "D" };

static void zephyr_log(esp_loader_port_t *port, esp_loader_log_level_t level,
                       const char *fmt, va_list args)
{
    (void)port;
    printk("[%s] esf: ", s_log_level_str[level]);
    vprintk(fmt, args);
    printk("\n");
}

static void zephyr_log_hex(esp_loader_port_t *port, esp_loader_log_level_t level,
                           const char *label, const uint8_t *data, size_t size)
{
    (void)port;
    printk("[%s] esf: %s (%zu bytes):\n", s_log_level_str[level],
           label ? label : "hex", size);
    for (size_t i = 0; i < size; i++) {
        printk("%02x ", data[i]);
        if ((i + 1) % 16 == 0) {
            printk("\n");
        }
    }
    if (size % 16 != 0) {
        printk("\n");
    }
}

static esp_loader_error_t configure_tty(zephyr_port_t *p)
{
    if (tty_init(&p->_tty, p->config->uart_dev) < 0 ||
            tty_set_rx_buf(&p->_tty, p->_tty_rx_buf, sizeof(p->_tty_rx_buf)) < 0 ||
            tty_set_tx_buf(&p->_tty, p->_tty_tx_buf, sizeof(p->_tty_tx_buf)) < 0) {
        return ESP_LOADER_ERROR_FAIL;
    }
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t zephyr_port_init(esp_loader_port_t *port)
{
    zephyr_port_t *p = container_of(port, zephyr_port_t, port);
    const esp_loader_config_t *c = p->config;

    if (!device_is_ready(c->uart_dev)) {
        printk("[E] esf: ESP UART not ready\n");
        return ESP_LOADER_ERROR_FAIL;
    }

    if (!device_is_ready(c->boot_spec.port)) {
        printk("[E] esf: ESP boot GPIO not ready\n");
        return ESP_LOADER_ERROR_FAIL;
    }

    if (!device_is_ready(c->enable_spec.port)) {
        printk("[E] esf: ESP reset GPIO not ready\n");
        return ESP_LOADER_ERROR_FAIL;
    }

    if (gpio_pin_configure_dt(&c->boot_spec, GPIO_OUTPUT_INACTIVE) < 0) {
        printk("[E] esf: Failed to configure ESP boot GPIO\n");
        return ESP_LOADER_ERROR_FAIL;
    }
    if (gpio_pin_configure_dt(&c->enable_spec, GPIO_OUTPUT_INACTIVE) < 0) {
        printk("[E] esf: Failed to configure ESP reset GPIO\n");
        return ESP_LOADER_ERROR_FAIL;
    }

    return configure_tty(p);
}

static void zephyr_port_deinit(esp_loader_port_t *port)
{
    zephyr_port_t *p = container_of(port, zephyr_port_t, port);
    const esp_loader_config_t *c = p->config;

    /* Disabling UART interrupts allows target to reset properly */
    uart_irq_tx_disable(c->uart_dev);
    uart_irq_rx_disable(c->uart_dev);
    gpio_pin_configure_dt(&c->boot_spec, GPIO_DISCONNECTED);
    gpio_pin_configure_dt(&c->enable_spec, GPIO_DISCONNECTED);
}

static esp_loader_error_t zephyr_uart_read(esp_loader_port_t *port, uint8_t *data, const uint16_t size, const uint32_t timeout)
{
    zephyr_port_t *p = container_of(port, zephyr_port_t, port);
    const esp_loader_config_t *c = p->config;

    if (!device_is_ready(c->uart_dev) || data == NULL || size == 0) {
        return ESP_LOADER_ERROR_FAIL;
    }

    ssize_t total_read = 0;
    ssize_t remaining = size;

    tty_set_rx_timeout(&p->_tty, timeout);
    while (remaining > 0) {
        const uint16_t chunk_size = remaining < CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE ?
                                    remaining : CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE;
        ssize_t read = tty_read(&p->_tty, &data[total_read], chunk_size);
        if (read < 0) {
            return ESP_LOADER_ERROR_TIMEOUT;
        }
        total_read += read;
        remaining -= read;
    }
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t zephyr_uart_write(esp_loader_port_t *port, const uint8_t *data, const uint16_t size, const uint32_t timeout)
{
    zephyr_port_t *p = container_of(port, zephyr_port_t, port);
    const esp_loader_config_t *c = p->config;

    if (!device_is_ready(c->uart_dev) || data == NULL || size == 0) {
        return ESP_LOADER_ERROR_FAIL;
    }

    ssize_t total_written = 0;
    ssize_t remaining = size;

    tty_set_tx_timeout(&p->_tty, timeout);
    while (remaining > 0) {
        const uint16_t chunk_size = remaining < CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE ?
                                    remaining : CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE;
        ssize_t written = tty_write(&p->_tty, &data[total_written], chunk_size);
        if (written < 0) {
            return ESP_LOADER_ERROR_TIMEOUT;
        }
        total_written += written;
        remaining -= written;
    }
    return ESP_LOADER_SUCCESS;
}

static void zephyr_uart_delay_ms(esp_loader_port_t *port, uint32_t ms)
{
    (void)port;
    k_msleep(ms);
}

static void zephyr_uart_start_timer(esp_loader_port_t *port, uint32_t ms)
{
    zephyr_port_t *p = container_of(port, zephyr_port_t, port);
    p->_time_end = sys_timepoint_calc(Z_TIMEOUT_MS(ms)).tick;
}

static uint32_t zephyr_uart_remaining_time(esp_loader_port_t *port)
{
    zephyr_port_t *p = container_of(port, zephyr_port_t, port);
    int64_t remaining = k_ticks_to_ms_floor64(p->_time_end - k_uptime_ticks());

    return (remaining > 0) ? (uint32_t)remaining : 0;
}

static void zephyr_uart_reset_target(esp_loader_port_t *port)
{
    zephyr_port_t *p = container_of(port, zephyr_port_t, port);
    const esp_loader_config_t *c = p->config;

    gpio_pin_set_dt(&c->boot_spec, false);
    gpio_pin_set_dt(&c->enable_spec, true);
    k_msleep(CONFIG_SERIAL_FLASHER_RESET_HOLD_TIME_MS);
    gpio_pin_set_dt(&c->enable_spec, false);
}

static void zephyr_uart_enter_bootloader(esp_loader_port_t *port)
{
    zephyr_port_t *p = container_of(port, zephyr_port_t, port);
    const esp_loader_config_t *c = p->config;

    gpio_pin_set_dt(&c->boot_spec, true);
    gpio_pin_set_dt(&c->enable_spec, true);
    k_msleep(CONFIG_SERIAL_FLASHER_RESET_HOLD_TIME_MS);
    gpio_pin_set_dt(&c->enable_spec, false);
    k_msleep(CONFIG_SERIAL_FLASHER_BOOT_HOLD_TIME_MS);
    gpio_pin_set_dt(&c->boot_spec, false);
}

static esp_loader_error_t zephyr_uart_change_rate(esp_loader_port_t *port, uint32_t baudrate)
{
    zephyr_port_t *p = container_of(port, zephyr_port_t, port);
    const esp_loader_config_t *c = p->config;
    struct uart_config uart_config;

    if (uart_config_get(c->uart_dev, &uart_config) != 0) {
        return ESP_LOADER_ERROR_FAIL;
    }
    uart_config.baudrate = baudrate;

    if (uart_configure(c->uart_dev, &uart_config) != 0) {
        return ESP_LOADER_ERROR_FAIL;
    }
    /* bitrate-change can require tty re-configuration */
    return configure_tty(p);
}

static int esp_loader_dev_init(const struct device *dev)
{
    struct esp_loader_dev_data *data = dev->data;

    if (esp_loader_init_serial(&data->loader, (esp_loader_port_t *) &data->interface.port) != ESP_LOADER_SUCCESS) {
        return -EIO;
    }

    return 0;
}

/* clang-format off */
static const esp_loader_port_ops_t zephyr_uart_ops = {
    .init                     = zephyr_port_init,
    .deinit                   = zephyr_port_deinit,
    .enter_bootloader         = zephyr_uart_enter_bootloader,
    .reset_target             = zephyr_uart_reset_target,
    .start_timer              = zephyr_uart_start_timer,
    .remaining_time           = zephyr_uart_remaining_time,
    .delay_ms                 = zephyr_uart_delay_ms,
    .log                      = zephyr_log,
    .log_hex                  = zephyr_log_hex,
    .change_transmission_rate = zephyr_uart_change_rate,
    .write                    = zephyr_uart_write,
    .read                     = zephyr_uart_read,
};

#define ESP_LOADER_DEFINE(inst)                                                \
    static const struct esp_loader_dev_config esp_loader_dev_config_##inst = { \
        .config = {                                                            \
            .uart_dev = DEVICE_DT_GET(DT_INST_PHANDLE(inst, uart)),            \
            .enable_spec = GPIO_DT_SPEC_GET(DT_DRV_INST(inst), reset_gpios),   \
            .boot_spec = GPIO_DT_SPEC_GET(DT_DRV_INST(inst), boot_gpios),      \
            .baud_rate = DT_INST_PROP(inst, default_baudrate),                 \
            .baud_rate_high = DT_INST_PROP(inst, higher_baudrate),             \
        },                                                                     \
        .connect_args = {                                                      \
            .sync_timeout = DT_INST_PROP(inst, sync_timeout_ms),               \
            .trials = DT_INST_PROP(inst, num_trials),                          \
        }                                                                      \
    };                                                                         \
    static struct esp_loader_dev_data esp_loader_dev_data_##inst = {           \
        .interface = {                                                         \
            .port.ops = &zephyr_uart_ops,                                      \
            .config = &esp_loader_dev_config_##inst.config,                    \
        }                                                                      \
    };                                                                         \
    DEVICE_DT_INST_DEFINE(inst, esp_loader_dev_init, NULL,                     \
         &esp_loader_dev_data_##inst, &esp_loader_dev_config_##inst,           \
         POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, NULL);
/* clang-format on */

DT_INST_FOREACH_STATUS_OKAY(ESP_LOADER_DEFINE)

esp_loader_t *esp_loader_from_device(const struct device *dev)
{
    struct esp_loader_dev_data *data = dev->data;
    return (esp_loader_t *)&data->loader;
}

const esp_loader_config_t *esp_loader_config_from_device(const struct device *dev)
{
    const struct esp_loader_dev_config *c = dev->config;
    return (const esp_loader_config_t *)&c->config;
}

const esp_loader_connect_args_t *esp_loader_connect_args_from_device(const struct device *dev)
{
    const struct esp_loader_dev_config *cfg = dev->config;
    return (const esp_loader_connect_args_t *)&cfg->connect_args;
}

esp_loader_error_t esp_loader_host_baudrate(esp_loader_t *loader, uint32_t baudrate)
{
    if (!loader || !baudrate) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }
    return zephyr_uart_change_rate(loader->_port, baudrate);
}
