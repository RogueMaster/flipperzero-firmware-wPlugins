#include "virtual_portal.h"

#include <furi_hal.h>
#include <stm32wbxx_ll_dma.h>

#include "audio/wav_player_hal.h"
#include "string.h"

#define TAG "VirtualPortal"

#define BLOCK_SIZE 16

#define PORTAL_SIDE_RING 0
#define PORTAL_SIDE_RIGHT 0
#define PORTAL_SIDE_TRAP 1
#define PORTAL_SIDE_LEFT 2

const NotificationSequence sequence_set_backlight = {
    &message_display_backlight_on,
    &message_do_not_reset,
    NULL,
};
const NotificationSequence sequence_set_leds = {
    &message_red_0,
    &message_blue_0,
    &message_green_0,
    &message_do_not_reset,
    NULL,
};

static float lerp(float start, float end, float t) {
    return start + (end - start) * t;
}

static void wav_player_dma_isr(void* ctx) {
    VirtualPortal* virtual_portal = (VirtualPortal*)ctx;
    // half of transfer
    if (LL_DMA_IsActiveFlag_HT1(DMA1)) {
        LL_DMA_ClearFlag_HT1(DMA1);
        // fill first half of buffer
        for (int i = 0; i < SAMPLES_COUNT / 2; i++) {
            if (!virtual_portal->count) {
                virtual_portal->audio_buffer[i] = 0;
                continue;
            }
            virtual_portal->audio_buffer[i] = *virtual_portal->tail;
            if (++virtual_portal->tail == virtual_portal->end) {
                virtual_portal->tail = virtual_portal->current_audio_buffer;
            }
            virtual_portal->count--;
        }
    }

    // transfer complete
    if (LL_DMA_IsActiveFlag_TC1(DMA1)) {
        LL_DMA_ClearFlag_TC1(DMA1);
        // fill second half of buffer
        for (int i = SAMPLES_COUNT / 2; i < SAMPLES_COUNT; i++) {
            if (!virtual_portal->count) {
                virtual_portal->audio_buffer[i] = 0;
                continue;
            }
            virtual_portal->audio_buffer[i] = *virtual_portal->tail;
            if (++virtual_portal->tail == virtual_portal->end) {
                virtual_portal->tail = virtual_portal->current_audio_buffer;
            }
            virtual_portal->count--;
        }
    }
}

void virtual_portal_tick(void* ctx) {
    VirtualPortal* virtual_portal = (VirtualPortal*)ctx;
    (void)virtual_portal;
    VirtualPortalLed* led = &virtual_portal->right;
    if (!led->running) {
        return;
    }
    uint32_t elapsed = furi_get_tick() - led->start_time;
    if (elapsed < led->delay) {
        float t_phase = fminf((float)elapsed / (float)led->delay, 1);

        if (led->two_phase) {
            if (led->current_phase == 0) {
                // Phase 1: Increase channels that need to go up, hold others constant
                if (led->target_r > led->last_r) {
                    led->r = lerp(led->last_r, led->target_r, t_phase);
                }
                if (led->target_g > led->last_g) {
                    led->g = lerp(led->last_g, led->target_g, t_phase);
                }
                if (led->target_b > led->last_b) {
                    led->b = lerp(led->last_b, led->target_b, t_phase);
                }
            } else {
                // Phase 2: Decrease channels that need to go down
                if (led->target_r < led->last_r) {
                    led->r = lerp(led->last_r, led->target_r, t_phase);
                }
                if (led->target_g < led->last_g) {
                    led->g = lerp(led->last_g, led->target_g, t_phase);
                }
                if (led->target_b < led->last_b) {
                    led->b = lerp(led->last_b, led->target_b, t_phase);
                }
            }
        } else {
            // Simple one-phase transition: all channels change together
            led->r = lerp(led->last_r, led->target_r, t_phase);
            led->g = lerp(led->last_g, led->target_g, t_phase);
            led->b = lerp(led->last_b, led->target_b, t_phase);
        }

        furi_hal_light_set(LightRed, led->r);
        furi_hal_light_set(LightGreen, led->g);
        furi_hal_light_set(LightBlue, led->b);
    } else if (led->two_phase && led->current_phase == 0) {
        // Move to phase 2 - save the current state as our "last" values for phase 2
        led->last_r = led->r;
        led->last_g = led->g;
        led->last_b = led->b;
        led->start_time = furi_get_tick();
        led->current_phase++;
    } else {
        // Transition complete - set final values
        led->r = led->target_r;
        led->g = led->target_g;
        led->b = led->target_b;
        furi_hal_light_set(LightRed, led->r);
        furi_hal_light_set(LightGreen, led->g);
        furi_hal_light_set(LightBlue, led->b);
        led->running = false;
    }
}

void queue_led_command(VirtualPortal* virtual_portal, int side, uint8_t r, uint8_t g, uint8_t b, uint16_t duration) {
    VirtualPortalLed* led = &virtual_portal->left;
    switch (side) {
        case PORTAL_SIDE_RIGHT:
            led = &virtual_portal->right;
            break;
        case PORTAL_SIDE_TRAP:
            led = &virtual_portal->trap;
            break;
        case PORTAL_SIDE_LEFT:
            led = &virtual_portal->left;
            break;
    }

    // Store current values as last values
    led->last_r = led->r;
    led->last_g = led->g;
    led->last_b = led->b;

    // Set target values
    led->target_r = r;
    led->target_g = g;
    led->target_b = b;

    if (duration) {
        // Determine if we need a two-phase transition
        bool increasing = (r > led->last_r) || (g > led->last_g) || (b > led->last_b);
        bool decreasing = (r < led->last_r) || (g < led->last_g) || (b < led->last_b);
        led->two_phase = increasing && decreasing;

        // Set up transition parameters
        led->start_time = furi_get_tick();
        if (led->two_phase) {
            // If two-phase, each phase gets half the duration
            led->delay = duration / 2;
        } else {
            led->delay = duration;
        }

        // Start in phase 0
        led->current_phase = 0;
        led->running = true;
    } else {
        // Immediate change, no transition
        if (side == PORTAL_SIDE_RIGHT) {
            led->r = r;
            led->g = g;
            led->b = b;
            furi_hal_light_set(LightRed, r);
            furi_hal_light_set(LightGreen, g);
            furi_hal_light_set(LightBlue, b);
        }
        led->running = false;
    }
}

VirtualPortal* virtual_portal_alloc(NotificationApp* notifications) {
    VirtualPortal* virtual_portal = malloc(sizeof(VirtualPortal));
    virtual_portal->notifications = notifications;

    notification_message(virtual_portal->notifications, &sequence_set_backlight);
    notification_message(virtual_portal->notifications, &sequence_set_leds);

    for (int i = 0; i < POF_TOKEN_LIMIT; i++) {
        virtual_portal->tokens[i] = pof_token_alloc();
    }
    virtual_portal->sequence_number = 0;
    virtual_portal->active = false;
    virtual_portal->volume = 20.0f;

    virtual_portal->led_timer = furi_timer_alloc(virtual_portal_tick,
                                                 FuriTimerTypePeriodic, virtual_portal);
    virtual_portal->head = virtual_portal->current_audio_buffer;
    virtual_portal->tail = virtual_portal->current_audio_buffer;
    virtual_portal->end = &virtual_portal->current_audio_buffer[SAMPLES_COUNT_BUFFERED];

    furi_timer_start(virtual_portal->led_timer, 10);

    if (furi_hal_speaker_acquire(1000)) {
        wav_player_speaker_init(8000);
        wav_player_dma_init((uint32_t)virtual_portal->audio_buffer, SAMPLES_COUNT);

        furi_hal_interrupt_set_isr(FuriHalInterruptIdDma1Ch1, wav_player_dma_isr, virtual_portal);

        wav_player_dma_start();
    }

    return virtual_portal;
}

void virtual_portal_set_type(VirtualPortal* virtual_portal, PoFType type) {
    virtual_portal->type = type;
}

void virtual_portal_cleanup(VirtualPortal* virtual_portal) {
    notification_message(virtual_portal->notifications, &sequence_reset_rgb);
    notification_message(virtual_portal->notifications, &sequence_display_backlight_on);
}

void virtual_portal_free(VirtualPortal* virtual_portal) {
    for (int i = 0; i < POF_TOKEN_LIMIT; i++) {
        pof_token_free(virtual_portal->tokens[i]);
        virtual_portal->tokens[i] = NULL;
    }
    furi_timer_stop(virtual_portal->led_timer);
    furi_timer_free(virtual_portal->led_timer);
    if (furi_hal_speaker_is_mine()) {
        furi_hal_speaker_release();
        wav_player_speaker_stop();
        wav_player_dma_stop();
    }
    wav_player_hal_deinit();
    furi_hal_interrupt_set_isr(FuriHalInterruptIdDma1Ch1, NULL, NULL);

    free(virtual_portal);
}

void virtual_portal_set_leds(uint8_t r, uint8_t g, uint8_t b) {
    furi_hal_light_set(LightRed, r);
    furi_hal_light_set(LightGreen, g);
    furi_hal_light_set(LightBlue, b);
}
void virtual_portal_set_backlight(uint8_t brightness) {
    furi_hal_light_set(LightBacklight, brightness);
}

void virtual_portal_load_token(VirtualPortal* virtual_portal, PoFToken* pof_token) {
    furi_assert(pof_token);
    FURI_LOG_D(TAG, "virtual_portal_load_token");
    PoFToken* target = NULL;
    uint8_t empty[4] = {0, 0, 0, 0};

    // first try to "reload" to the same slot it used before based on UID
    for (int i = 0; i < POF_TOKEN_LIMIT; i++) {
        if (memcmp(virtual_portal->tokens[i]->UID, pof_token->UID, sizeof(pof_token->UID)) == 0) {
            // Found match
            if (virtual_portal->tokens[i]->loaded) {
                // already loaded, no-op
                return;
            } else {
                FURI_LOG_D(TAG, "Found matching UID at index %d", i);
                target = virtual_portal->tokens[i];
                break;
            }
        }
    }

    // otherwise load into first slot with no set UID
    if (target == NULL) {
        for (int i = 0; i < POF_TOKEN_LIMIT; i++) {
            if (memcmp(virtual_portal->tokens[i]->UID, empty, sizeof(empty)) == 0) {
                FURI_LOG_D(TAG, "Found empty UID at index %d", i);
                // By definition an empty UID slot would not be loaded, so I'm not checking.  Fight me.
                target = virtual_portal->tokens[i];
                break;
            }
        }
    }

    // Re-use first unloaded slot
    if (target == NULL) {
        for (int i = 0; i < POF_TOKEN_LIMIT; i++) {
            if (virtual_portal->tokens[i]->loaded == false) {
                FURI_LOG_D(TAG, "Re-using previously used slot %d", i);
                target = virtual_portal->tokens[i];
                break;
            }
        }
    }

    if (target == NULL) {
        FURI_LOG_W(TAG, "Failed to find slot to token into");
        return;
    }
    furi_assert(target);

    // TODO: make pof_token_copy()
    target->change = pof_token->change;
    target->loaded = pof_token->loaded;
    memcpy(target->dev_name, pof_token->dev_name, sizeof(pof_token->dev_name));
    memcpy(target->UID, pof_token->UID, sizeof(pof_token->UID));

    furi_string_set(target->load_path, pof_token->load_path);

    const NfcDeviceData* data = nfc_device_get_data(pof_token->nfc_device, NfcProtocolMfClassic);
    nfc_device_set_data(target->nfc_device, NfcProtocolMfClassic, data);
}

uint8_t virtual_portal_next_sequence(VirtualPortal* virtual_portal) {
    if (virtual_portal->sequence_number == 0xff) {
        virtual_portal->sequence_number = 0;
    }
    return virtual_portal->sequence_number++;
}

int virtual_portal_activate(VirtualPortal* virtual_portal, uint8_t* message, uint8_t* response) {
    FURI_LOG_D(TAG, "process %c", message[0]);
    virtual_portal->active = message[1] != 0;

    response[0] = message[0];
    response[1] = message[1];
    response[2] = 0xFF;
    response[3] = 0x77;
    return 4;
}

int virtual_portal_reset(VirtualPortal* virtual_portal, uint8_t* message, uint8_t* response) {
    FURI_LOG_D(TAG, "process %c", message[0]);

    virtual_portal->active = false;
    // virtual_portal->sequence_number = 0;
    for (int i = 0; i < POF_TOKEN_LIMIT; i++) {
        if (virtual_portal->tokens[i]->loaded) {
            virtual_portal->tokens[i]->change = true;
        }
    }

    uint8_t index = 0;
    response[index++] = 'R';
    response[index++] = 0x02;  // Trap Team Xbox One
    response[index++] = 0x27;  // Trap Team Xbox One

    // response[index++] = 0x02; // Swap Force 3DS
    // response[index++] = 0x02; // Swap Force 3DS
    return index;
}

int virtual_portal_status(VirtualPortal* virtual_portal, uint8_t* response) {
    response[0] = 'S';

    bool update = false;
    for (size_t i = 0; i < POF_TOKEN_LIMIT; i++) {
        // Can't use bit_lib since it uses the opposite endian
        if (virtual_portal->tokens[i]->loaded) {
            response[1 + i / 4] |= 1 << ((i % 4) * 2 + 0);
        }
        if (virtual_portal->tokens[i]->change) {
            update = true;
            response[1 + i / 4] |= 1 << ((i % 4) * 2 + 1);
        }

        virtual_portal->tokens[i]->change = false;
    }
    response[5] = virtual_portal_next_sequence(virtual_portal);
    response[6] = 1;

    // Let me know when a status that actually has a change is sent
    if (update) {
        char display[33] = {0};
        memset(display, 0, sizeof(display));
        for (size_t i = 0; i < BLOCK_SIZE; i++) {
            snprintf(display + (i * 2), sizeof(display), "%02x", response[i]);
        }
        FURI_LOG_I(TAG, "> S %s", display);
    }

    return 7;
}

int virtual_portal_send_status(VirtualPortal* virtual_portal, uint8_t* response) {
    if (virtual_portal->active) {
        return virtual_portal_status(virtual_portal, response);
    }
    return 0;
}

// 4d01ff0000d0077d6c2a77a400000000
int virtual_portal_m(VirtualPortal* virtual_portal, uint8_t* message, uint8_t* response) {
    // Activate speaker for any non-zero value in the range 01-FF
    virtual_portal->speaker = (message[1] != 0);
    if (virtual_portal->speaker) {
        if (!virtual_portal->playing_audio) {
            wav_player_speaker_start();
        }
        virtual_portal->count = 0;
        virtual_portal->head = virtual_portal->tail = virtual_portal->current_audio_buffer;
        virtual_portal->playing_audio = true;
    }
    /*
    char display[33] = {0};
    for(size_t i = 0; i < BLOCK_SIZE; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", message[i]);
    }
    FURI_LOG_I(TAG, "M %s", display);
    */

    size_t index = 0;
    response[index++] = 'M';
    // Always respond with 01 if active, 00 if not
    response[index++] = virtual_portal->speaker ? 0x01 : 0x00;
    response[index++] = 0x00;
    response[index++] = virtual_portal->m;
    g72x_init_state(&virtual_portal->state);
    return index;
}

int virtual_portal_l(VirtualPortal* virtual_portal, uint8_t* message) {
    UNUSED(virtual_portal);

    /*
    char display[33] = {0};
    memset(display, 0, sizeof(display));
    for(size_t i = 0; i < BLOCK_SIZE; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", message[i]);
    }
    FURI_LOG_I(TAG, "L %s", display);
    */

    uint8_t side = message[1];  // 0: left, 2: right
    uint8_t brightness = 0;
    switch (side) {
        case 0:
        case 2:
            queue_led_command(virtual_portal, side, message[2], message[3], message[4], 0);
            break;
        case 1:
            brightness = message[2];
            virtual_portal_set_backlight(brightness);
            break;
        case 3:
            brightness = 0xff;
            virtual_portal_set_backlight(brightness);
            break;
    }
    return 0;
}

int virtual_portal_j(VirtualPortal* virtual_portal, uint8_t* message, uint8_t* response) {
    /*
    char display[33] = {0};
    memset(display, 0, sizeof(display));
    for(size_t i = 0; i < BLOCK_SIZE; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", message[i]);
    }
    FURI_LOG_I(TAG, "J %s", display);
    */

    uint8_t side = message[1];
    uint16_t delay = message[6] << 8 | message[5];

    queue_led_command(virtual_portal, side, message[2], message[3], message[4], delay);

    // Delay response
    // furi_delay_ms(delay); // causes issues
    // UNUSED(delay);

    // https://marijnkneppers.dev/posts/reverse-engineering-skylanders-toys-to-life-mechanics/
    size_t index = 0;
    response[index++] = 'J';
    return index;
}

int virtual_portal_query(VirtualPortal* virtual_portal, uint8_t* message, uint8_t* response) {
    int index = message[1];
    int blockNum = message[2];
    int arrayIndex = index & 0x0f;
    FURI_LOG_I(TAG, "Query %d %d", arrayIndex, blockNum);

    PoFToken* pof_token = virtual_portal->tokens[arrayIndex];
    if (!pof_token->loaded) {
        response[0] = 'Q';
        response[1] = 0x00 | arrayIndex;
        response[2] = blockNum;
        return 3;
    }
    NfcDevice* nfc_device = pof_token->nfc_device;
    const MfClassicData* data = nfc_device_get_data(nfc_device, NfcProtocolMfClassic);
    const MfClassicBlock block = data->block[blockNum];

    response[0] = 'Q';
    response[1] = 0x10 | arrayIndex;
    response[2] = blockNum;
    memcpy(response + 3, block.data, BLOCK_SIZE);
    return 3 + BLOCK_SIZE;
}

int virtual_portal_write(VirtualPortal* virtual_portal, uint8_t* message, uint8_t* response) {
    int index = message[1];
    int blockNum = message[2];
    int arrayIndex = index & 0x0f;

    char display[33] = {0};
    for (size_t i = 0; i < BLOCK_SIZE; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", message[3 + i]);
    }
    FURI_LOG_I(TAG, "Write %d %d %s", arrayIndex, blockNum, display);

    PoFToken* pof_token = virtual_portal->tokens[arrayIndex];
    if (!pof_token->loaded) {
        response[0] = 'W';
        response[1] = 0x00 | arrayIndex;
        response[2] = blockNum;
        return 3;
    }

    NfcDevice* nfc_device = pof_token->nfc_device;

    MfClassicData* data = mf_classic_alloc();
    nfc_device_copy_data(nfc_device, NfcProtocolMfClassic, data);
    MfClassicBlock* block = &data->block[blockNum];

    memcpy(block->data, message + 3, BLOCK_SIZE);
    nfc_device_set_data(nfc_device, NfcProtocolMfClassic, data);

    mf_classic_free(data);

    nfc_device_save(nfc_device, furi_string_get_cstr(pof_token->load_path));

    response[0] = 'W';
    response[1] = 0x10 | arrayIndex;
    response[2] = blockNum;
    return 3;
}

// HID portals use send 8000hz 16 bit signed PCM samples
void virtual_portal_process_audio(
    VirtualPortal* virtual_portal,
    uint8_t* message,
    uint8_t len) {
    for (size_t i = 0; i < len; i += 2) {
        int16_t int_16 =
            (((int16_t)message[i + 1] << 8) + ((int16_t)message[i]));

        float data = ((float)int_16 / 256.0);
        data /= UINT8_MAX / 2;  // scale -1..1

        data *= virtual_portal->volume;  // volume
        data = tanhf(data);              // hyperbolic tangent limiter

        data *= UINT8_MAX / 2;  // scale -128..127
        data += UINT8_MAX / 2;  // to unsigned

        if (data < 0) {
            data = 0;
        }

        if (data > 255) {
            data = 255;
        }
        *virtual_portal->head = data;
        virtual_portal->count++;
        if (++virtual_portal->head == virtual_portal->end) {
            virtual_portal->head = virtual_portal->current_audio_buffer;
        }
    }
}

// 360 portals didn't have the bandwith, so they use CCITT G.721 ADPCM coding
// to encode the audio so it uses less bandwith.
void virtual_portal_process_audio_360(
    VirtualPortal* virtual_portal,
    uint8_t* message,
    uint8_t len) {
    for (size_t i = 0; i < len; i++) {
        int16_t int_16 = (int16_t)g721_decoder(message[i], &virtual_portal->state);

        float data = ((float)int_16 / 256.0);
        data /= UINT8_MAX / 2;  // scale -1..1

        data *= virtual_portal->volume;  // volume
        data = tanhf(data);              // hyperbolic tangent limiter

        data *= UINT8_MAX / 2;  // scale -128..127
        data += UINT8_MAX / 2;  // to unsigned

        if (data < 0) {
            data = 0;
        }

        if (data > 255) {
            data = 255;
        }
        *virtual_portal->head = data;
        virtual_portal->count++;
        if (++virtual_portal->head == virtual_portal->end) {
            virtual_portal->head = virtual_portal->current_audio_buffer;
        }

        int_16 = (int16_t)g721_decoder(message[i] >> 4, &virtual_portal->state);

        data = ((float)int_16 / 256.0);
        data /= UINT8_MAX / 2;  // scale -1..1

        data *= virtual_portal->volume;  // volume
        data = tanhf(data);              // hyperbolic tangent limiter

        data *= UINT8_MAX / 2;  // scale -128..127
        data += UINT8_MAX / 2;  // to unsigned

        if (data < 0) {
            data = 0;
        }

        if (data > 255) {
            data = 255;
        }
        *virtual_portal->head = data;
        virtual_portal->count++;
        if (++virtual_portal->head == virtual_portal->end) {
            virtual_portal->head = virtual_portal->current_audio_buffer;
        }
    }
}

// 32 byte message, 32 byte response;
int virtual_portal_process_message(
    VirtualPortal* virtual_portal,
    uint8_t* message,
    uint8_t* response) {
    memset(response, 0, 32);
    switch (message[0]) {
        case 'A':
            return virtual_portal_activate(virtual_portal, message, response);
        case 'C':  // Ring color R G B
            queue_led_command(virtual_portal, PORTAL_SIDE_RING, message[1], message[2], message[3], 0);
            return 0;
        case 'J':
            // https://github.com/flyandi/flipper_zero_rgb_led
            return virtual_portal_j(virtual_portal, message, response);
        case 'L':
            return virtual_portal_l(virtual_portal, message);
        case 'M':
            return virtual_portal_m(virtual_portal, message, response);
        case 'Q':  // Query
            if (!virtual_portal->active) {
                return 0;  // No response if portal is not active
            }
            return virtual_portal_query(virtual_portal, message, response);
        case 'R':
            return virtual_portal_reset(virtual_portal, message, response);
        case 'S':  // Status
            if (!virtual_portal->active) {
                return 0;  // No response if portal is not active
            }
            return virtual_portal_status(virtual_portal, response);
        case 'V':
            virtual_portal->m = message[3];
            return 0;
        case 'W':  // Write
            if (!virtual_portal->active) {
                return 0;  // No response if portal is not active
            }
            return virtual_portal_write(virtual_portal, message, response);
        case 'Z':
            return 0;
        default:
            FURI_LOG_W(TAG, "Unhandled command %c", message[0]);
            return 0;  // No response
    }

    return 0;
}