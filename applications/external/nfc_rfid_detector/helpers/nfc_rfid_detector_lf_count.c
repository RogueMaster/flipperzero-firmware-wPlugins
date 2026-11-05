#include "nfc_rfid_detector_lf_count.h"

#include <furi.h>
#include <string.h>
#include <stm32wbxx_ll_dma.h>

// The firmware's LF field detector counts the field's edges in TIM2. Every 100 ms (one gate), DMA2 channel 1
// copies the count out, and channel 2 then resets the counter to zero. Edges that arrive between the copy and the
// reset are lost. This app turns channel 2 off and points channel 1 at a ring buffer. The counter then runs without
// resets, and each gate's count is the difference between two consecutive copies.
// These defines assume the firmware's DMA2 channel numbers. Update them if the firmware changes those.
#define LF_DMA       DMA2
#define LF_DMA_LATCH LL_DMA_CHANNEL_1
#define LF_DMA_RESET LL_DMA_CHANNEL_2

#define NFC_RFID_DETECTOR_LF_RING 128 // gates that can arrive between reads

struct NfcRfidDetectorLfCount {
    uint32_t ring[NFC_RFID_DETECTOR_LF_RING]; // the field count (TIM2), copied by DMA at each gate
    uint16_t read_index;
    bool have_previous_copy;
    uint32_t previous_copy;
    NfcRfidDetectorLfMeter* meter;
};

NfcRfidDetectorLfCount* nfc_rfid_detector_lf_count_alloc(void) {
    NfcRfidDetectorLfCount* count = malloc(sizeof(NfcRfidDetectorLfCount));
    memset(count, 0, sizeof(NfcRfidDetectorLfCount));
    count->meter = nfc_rfid_detector_lf_meter_alloc();
    return count;
}

void nfc_rfid_detector_lf_count_free(NfcRfidDetectorLfCount* count) {
    furi_assert(count);
    nfc_rfid_detector_lf_meter_free(count->meter);
    free(count);
}

void nfc_rfid_detector_lf_count_start(NfcRfidDetectorLfCount* count) {
    furi_assert(count);
    // restart counting, but keep the last result on screen (e.g. coming back from Config)
    memset(count->ring, 0, sizeof(count->ring));
    count->read_index = 0;
    count->have_previous_copy = false;
    nfc_rfid_detector_lf_meter_hold(count->meter);
    FURI_CRITICAL_ENTER();
    LL_DMA_DisableChannel(LF_DMA, LF_DMA_RESET);
    LL_DMA_DisableChannel(LF_DMA, LF_DMA_LATCH);
    LL_DMA_SetMemoryAddress(LF_DMA, LF_DMA_LATCH, (uint32_t)count->ring);
    LL_DMA_SetMemoryIncMode(LF_DMA, LF_DMA_LATCH, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetDataLength(LF_DMA, LF_DMA_LATCH, NFC_RFID_DETECTOR_LF_RING);
    LL_DMA_EnableChannel(LF_DMA, LF_DMA_LATCH);
    FURI_CRITICAL_EXIT();
}

void nfc_rfid_detector_lf_count_read(
    NfcRfidDetectorLfCount* count,
    NfcRfidDetectorLfMeterResult* result) {
    furi_assert(count);
    furi_assert(result);
    // write_index is the entry the DMA writes next. The entries from read_index up to it are complete.
    uint16_t write_index =
        (NFC_RFID_DETECTOR_LF_RING - LL_DMA_GetDataLength(LF_DMA, LF_DMA_LATCH)) %
        NFC_RFID_DETECTOR_LF_RING;
    // read the ring up to the DMA's current position, not past it
    __DMB();
    while(count->read_index != write_index) {
        uint32_t copy = count->ring[count->read_index];
        count->read_index = (count->read_index + 1) % NFC_RFID_DETECTOR_LF_RING;
        if(count->have_previous_copy) {
            nfc_rfid_detector_lf_meter_feed(count->meter, copy - count->previous_copy);
        }
        count->previous_copy = copy;
        count->have_previous_copy = true;
    }
    nfc_rfid_detector_lf_meter_get_result(count->meter, result);
}

void nfc_rfid_detector_lf_count_restart(NfcRfidDetectorLfCount* count) {
    furi_assert(count);
    nfc_rfid_detector_lf_meter_restart(count->meter);
}
