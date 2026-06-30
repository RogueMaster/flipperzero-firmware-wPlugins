// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2024 KBEmbedded

#ifndef PRINTER_RECEIVE_H
#define PRINTER_RECEIVE_H

#pragma once

/**
 * Start a printer instance in receive mode
 *
 * Allows devices to send image data to the flipper
 *
 * @note Once printer_receive_start() is called, some printer instance settings
 * can no longer be modified, e.g. the Game Boy Link pinout.
 *
 * @param printer_handle Printer instance handle
 */
void printer_receive_start(void *printer_handle);

/**
 * Mark a received print as printed/completed
 *
 * Once a print command is actually received, the printer instance callback
 * is called with the print reason. The printer instance will continually
 * report that it is in the printing state until this function is called.
 * At which time, the image is considered printed, and this updated status
 * is returned on the next status check.
 *
 * @warning Once this function is called, the struct gb_image pointer given to the
 * printer instance callback should be considered no longer valid! It is advised
 * complete operations on the struct gb_image (or copy its contents to another
 * buffer) before calling this function.
 *
 * @param printer_handle Printer instance handle
 */
void printer_receive_print_complete(void *printer_handle);

#endif // PRINTER_RECEIVE_H
