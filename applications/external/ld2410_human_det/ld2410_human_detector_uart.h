#pragma once

#include <furi.h>
#include <furi_hal.h>
#include "ld2410_human_detector_parser.h"

typedef struct LD2410HumanDetectorUart LD2410HumanDetectorUart;

typedef void (*LD2410HumanDetectorUartCallback)(LD2410Data* data, void* context);

LD2410HumanDetectorUart* ld2410_human_detector_uart_alloc();
void ld2410_human_detector_uart_free(LD2410HumanDetectorUart* uart);

bool ld2410_human_detector_uart_start(LD2410HumanDetectorUart* uart);
void ld2410_human_detector_uart_stop(LD2410HumanDetectorUart* uart);

uint32_t ld2410_human_detector_uart_get_rx_bytes(LD2410HumanDetectorUart* uart);

void ld2410_human_detector_uart_set_handle_rx_data_cb(
    LD2410HumanDetectorUart* uart,
    LD2410HumanDetectorUartCallback callback,
    void* context);
