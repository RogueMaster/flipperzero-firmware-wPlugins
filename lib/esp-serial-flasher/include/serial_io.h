/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#error "serial_io.h has been removed in v2. The free-function port API \
(loader_port_write, loader_port_read, loader_port_change_transmission_rate, …) \
no longer exists. Port implementations must now provide an esp_loader_ *_port_ops_t \
vtable and include esp_loader_io.h instead. \
See docs / migration - v1 - to - v2.md Section 4 for upgrade instructions."
