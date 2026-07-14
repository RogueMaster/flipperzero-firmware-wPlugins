
#include "BluetoothA2DPSinkQueued.h"

#if IS_VALID_PLATFORM

void BluetoothA2DPSinkQueued::bt_i2s_task_start_up(void) {
    ESP_LOGI(BT_APP_TAG, "ringbuffer data empty! mode changed: RINGBUFFER_MODE_PREFETCHING");
    ringbuffer_mode = RINGBUFFER_MODE_PREFETCHING;
    if ((s_i2s_write_semaphore = xSemaphoreCreateBinary()) == nullptr) {
        ESP_LOGE(BT_APP_TAG, "%s, Semaphore create failed", __func__);
        return;
    }
    if ((s_ringbuf_i2s = xRingbufferCreate(i2s_ringbuffer_size, RINGBUF_TYPE_BYTEBUF)) == nullptr) {
        ESP_LOGE(BT_APP_TAG, "%s, ringbuffer create failed", __func__);
        return;
    }
    //xTaskCreate(bt_i2s_task_handler, "BtI2STask", 2048, nullptr, configMAX_PRIORITIES - 3, &s_bt_i2s_task_handle);
    BaseType_t result = xTaskCreatePinnedToCore(ccall_i2s_task_handler, "BtI2STask", i2s_stack_size, nullptr, i2s_task_priority, &s_bt_i2s_task_handle, task_core);
    if (result!=pdPASS){
        ESP_LOGE(BT_AV_TAG, "xTaskCreatePinnedToCore");
    } else {
        ESP_LOGI(BT_AV_TAG, "BtI2STask Started");
    }
}

void BluetoothA2DPSinkQueued::bt_i2s_task_shut_down(void) {
    if (s_bt_i2s_task_handle) {
        vTaskDelete(s_bt_i2s_task_handle);
        s_bt_i2s_task_handle = nullptr;
    }
    if (s_ringbuf_i2s) {
        vRingbufferDelete(s_ringbuf_i2s);
        s_ringbuf_i2s = nullptr;
    }
    if (s_i2s_write_semaphore) {
        vSemaphoreDelete(s_i2s_write_semaphore);
        s_i2s_write_semaphore = nullptr;
    }

    ESP_LOGI(BT_AV_TAG, "BtI2STask shutdown");
}

/* NEW I2S Task & ring buffer */

void BluetoothA2DPSinkQueued::i2s_task_handler(void *arg) {
    uint8_t *data = nullptr;
    size_t item_size = 0;
    /**
     * The total length of DMA buffer of I2S is:
     * `dma_frame_num * dma_desc_num * i2s_channel_num * i2s_data_bit_width / 8`.
     * Transmit `dma_frame_num * dma_desc_num` bytes to DMA is trade-off.
     */
    is_starting = true;

    while (true) {
        if (is_starting){
            // wait for ringbuffer to be filled
            if (pdTRUE != xSemaphoreTake(s_i2s_write_semaphore, portMAX_DELAY)){
                continue;
            }
            is_starting = false;
        }
        // xSemaphoreTake was succeeding here, so we have the buffer filled up
        item_size = 0;

        // receive data from ringbuffer and write it to I2S DMA transmit buffer 
        data = (uint8_t *)xRingbufferReceiveUpTo(s_ringbuf_i2s, &item_size, (TickType_t)pdMS_TO_TICKS(i2s_ticks), i2s_write_size_upto);
        if (item_size == 0) {
            if (ringbuffer_mode != RINGBUFFER_MODE_PREFETCHING) {
                ESP_LOGI(BT_APP_TAG, "ringbuffer underflowed! mode changed: RINGBUFFER_MODE_PREFETCHING");
                ringbuffer_mode = RINGBUFFER_MODE_PREFETCHING;
            }
            continue;
        } 

        // if i2s is not active we just consume the buffer w/o output
        if (is_i2s_active && is_output){
            size_t written = i2s_write_data(data, item_size);
            ESP_LOGD(BT_AV_TAG, "i2s_task_handler: %d->%d", item_size, written);
            if (written==0){
                ESP_LOGE(BT_APP_TAG, "i2s_write_data failed %d->%d", item_size, written);
                continue;
            }
        }

        vRingbufferReturnItem(s_ringbuf_i2s, (void *)data);
        delay_ms(5);
    }
}

size_t BluetoothA2DPSinkQueued::write_audio(const uint8_t *data, size_t size)
{
    size_t item_size = 0;
    BaseType_t done = pdFALSE;

    // This should not really happen!
    if (!is_i2s_active){
        ESP_LOGW(BT_APP_TAG, "i2s is not active: we try to activate it");
        out->begin();
        delay_ms(200);
    }

    if (ringbuffer_mode == RINGBUFFER_MODE_DROPPING) {
        ESP_LOGW(BT_APP_TAG, "ringbuffer is full, drop this packet!");
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
        vRingbufferGetInfo(s_ringbuf_i2s, nullptr, nullptr, nullptr, nullptr, &item_size);
#else
        vRingbufferGetInfo(s_ringbuf_i2s, nullptr, nullptr, nullptr, &item_size);
#endif
        if (item_size <= i2s_ringbuffer_prefetch_size()) {
            ESP_LOGI(BT_APP_TAG, "ringbuffer data decreased! mode changed: RINGBUFFER_MODE_PROCESSING");
            ringbuffer_mode = RINGBUFFER_MODE_PROCESSING;
        }
        return 0;
    }

    done = xRingbufferSend(s_ringbuf_i2s, (void *)data, size, (TickType_t)0);

    if (!done) {
        ESP_LOGW(BT_APP_TAG, "ringbuffer overflowed, ready to decrease data! mode changed: RINGBUFFER_MODE_DROPPING");
        ringbuffer_mode = RINGBUFFER_MODE_DROPPING;
    }

    if (ringbuffer_mode == RINGBUFFER_MODE_PREFETCHING) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
        vRingbufferGetInfo(s_ringbuf_i2s, nullptr, nullptr, nullptr, nullptr, &item_size);
#else
        vRingbufferGetInfo(s_ringbuf_i2s, nullptr, nullptr, nullptr, &item_size);
#endif

        if (item_size >= i2s_ringbuffer_prefetch_size()) {
            ESP_LOGI(BT_APP_TAG, "ringbuffer data increased! mode changed: RINGBUFFER_MODE_PROCESSING");
            ringbuffer_mode = RINGBUFFER_MODE_PROCESSING;
            if (pdFALSE == xSemaphoreGive(s_i2s_write_semaphore)) {
                ESP_LOGE(BT_APP_TAG, "semphore give failed");
            }
        }
    }

    return done ? size : 0;
}

#endif // platform
