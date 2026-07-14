#include "lora_transmitter_i.h"
#include "lora_app.h"

static void lora_transmitter_init(void* context) {
    furi_assert(context);
    LoraTransmitter* transmitter = context;
    init_lora_config_default(&transmitter->model->lora_cfg);
}

static void _lora_transmitter_set_rf_test_config(LoraTransmitter* transmitter) {
    furi_assert(transmitter);
    char temp[256];
    LoraState previous_state = lora_state_manager_get_state(transmitter->state_manager);
    lora_state_manager_set_state(transmitter->state_manager, CONFIG);

    snprintf(
        temp,
        sizeof(temp),
        "AT+TEST=RFCFG,%ld.%d,SF%d,%ld,%d,%d,%d,%s,%s,%s\n",
        transmitter->model->lora_cfg.freq,
        canal_list[transmitter->model->lora_cfg.canal_idx],
        transmitter->model->lora_cfg.sf,
        bandwidth_list[transmitter->model->lora_cfg.bw_idx],
        transmitter->model->lora_cfg.tx_preamble,
        transmitter->model->lora_cfg.rx_preamble,
        transmitter->model->lora_cfg.power,
        transmitter->model->lora_cfg.with_crc ? "ON" : "OFF",
        transmitter->model->lora_cfg.is_iq_inverted ? "ON" : "OFF",
        transmitter->model->lora_cfg.with_public_lorawan ? "ON" : "OFF");

    transmitter->send_method(transmitter->context, temp, strlen(temp) + 1, true);
    lora_state_manager_set_state(
        transmitter->state_manager, previous_state); // Restore previous state
}

static void lora_transmitter_enter_test_mode(LoraTransmitter* transmitter) {
    lora_state_manager_set_state(transmitter->state_manager, CONFIG);
    transmitter->send_method(transmitter->context, "AT+MODE=TEST\n", 14, true);
    furi_delay_ms(1000);
}

static void _lora_transmitter_enter_receive_mode(LoraTransmitter* transmitter) {
    lora_state_manager_set_state(transmitter->state_manager, CONFIG);
    lora_transmitter_enter_test_mode(transmitter);
    transmitter->send_method(transmitter->context, "AT+TEST=RXLRPKT\n", 17, true);
    _lora_transmitter_set_rf_test_config(transmitter);
    lora_state_manager_set_state(transmitter->state_manager, RX);
}

static int32_t lora_transmitter_start(void* context) {
    LoraTransmitter* transmitter = context;

    uint32_t events;
    do {
        FURI_LOG_D("lora_transmitter_start", "Waiting for events");
        events = furi_thread_flags_wait(
            TransmitterEventEnterReceiveMode | TransmitterEventSetRFTestConfig |
                TransmitterEventExciting,
            FuriFlagWaitAny,
            FuriWaitForever);
        if(events & TransmitterEventEnterReceiveMode) {
            _lora_transmitter_enter_receive_mode(transmitter);
        }
        if(events & TransmitterEventSetRFTestConfig) {
            _lora_transmitter_set_rf_test_config(transmitter);
        }
    } while(!transmitter->should_exit);
    FURI_LOG_D("lora_transmitter_start", "Exiting lora_transmitter_start");
    return 0;
}

LoraTransmitter* lora_transmitter_alloc(
    void* context,
    LoraTransmitterMethod send_method,
    LoraTransmitterContextDestructor context_destructor,
    SetTransmitterThreadIdMethod set_thread_id_method) {
    furi_assert(context);
    LoraTransmitter* transmitter = malloc(sizeof(LoraTransmitter));
    transmitter->model = malloc(sizeof(LoraTransmitterModel));
    transmitter->context = context;
    transmitter->send_method = send_method;
    transmitter->context_destructor = context_destructor;

    transmitter->thread =
        furi_thread_alloc_ex("LoraTransmitter", 1024UL, lora_transmitter_start, transmitter);
    furi_thread_start(transmitter->thread);
    set_thread_id_method(transmitter->context, furi_thread_get_id(transmitter->thread));
    // Init
    lora_transmitter_init(transmitter);
    return transmitter;
}

void lora_transmitter_free(LoraTransmitter* transmitter) {
    transmitter->should_exit = true;
    // Signal that we want the transmitter to exit.  It may be doing other work.
    furi_thread_flags_set(furi_thread_get_id(transmitter->thread), TransmitterEventExciting);
    furi_thread_join(transmitter->thread);
    furi_thread_free(transmitter->thread);

    transmitter->context_destructor(transmitter->context);
    free(transmitter->model);
    free(transmitter);
}

void lora_transmitter_enter_receive_mode(LoraTransmitter* transmitter) {
    // Possibility to add arg to transmitter here
    furi_thread_flags_set(
        furi_thread_get_id(transmitter->thread), TransmitterEventEnterReceiveMode);
}

void lora_transmitter_set_state_manager(
    LoraTransmitter* transmitter,
    LoraStateManager* state_manager) {
    furi_assert(transmitter);
    furi_assert(state_manager);
    transmitter->state_manager = state_manager;
}

void lora_transmitter_otaa_join_procedure(LoraTransmitter* transmitter) {
    int join_attempts = 1;

    // Loop until the device is joined to the network
    while(lora_state_manager_get_state(transmitter->state_manager) != JOINED) {
        FURI_LOG_I("lora_transmitter_otaa_join_procedure", "Attempting to join the network...");
        // Send the join command
        transmitter->send_method(transmitter->context, "AT+JOIN_CMD\n", 13, false);
        furi_delay_ms(10000);

        if((lora_state_manager_get_state(transmitter->state_manager)) == JOINED) {
            // Device is joined, break the loop
            break;
        }

        join_attempts++;

        if(join_attempts > 3) {
            // If the device is not joined after 3 attempts, break the loop
            FURI_LOG_I("lora_transmitter_otaa_join_procedure", "Failed to join after 3 attempts");
            break;
        }

        FURI_LOG_I("lora_transmitter_otaa_join_procedure", "Join attempt %d", join_attempts);
    }
}

void lora_transmitter_setup_lorawan(LoraTransmitter* transmitter) {
    char temp[64] = {0};
    transmitter->send_method(transmitter->context, "AT+ID\n", 7, false);
    furi_delay_ms(1000);

    transmitter->send_method(transmitter->context, "AT+MODE=LWOTAA\n", 16, false);
    furi_delay_ms(1000);

    snprintf(temp, sizeof(temp), "AT+DR=%d\n", transmitter->model->lorawan_cfg.dr);
    transmitter->send_method(transmitter->context, temp, strlen(temp) + 1, false); // +1 for \0
    furi_delay_ms(1000);

    snprintf(temp, sizeof(temp), "AT+POWER=%d\n", transmitter->model->lorawan_cfg.tx_power);
    transmitter->send_method(transmitter->context, temp, strlen(temp) + 1, false);
    furi_delay_ms(1000);

    transmitter->send_method(transmitter->context, "AT+ADR=ON\n", strlen(temp) + 1, false);
    furi_delay_ms(1000);

    transmitter->send_method(transmitter->context, "AT+CLASS=A\n", strlen(temp) + 1, false);
    furi_delay_ms(1000);

    snprintf(temp, sizeof(temp), "AT+KEY=APPKEY,%s\n", transmitter->model->lorawan_cfg.appkey);
    transmitter->send_method(transmitter->context, temp, strlen(temp) + 1, false);
    furi_delay_ms(1000);

    lora_state_manager_set_state(transmitter->state_manager, CONFIG);
}

void lora_transmitter_send_cmsg(LoraTransmitter* transmitter, const char* msg) {
    char temp[64] = {0};
    snprintf(temp, sizeof(temp), "AT+CMSG=%s\n", msg);
    transmitter->send_method(transmitter->context, temp, strlen(temp) + 1, false);
    furi_delay_ms(1000);
}

void lora_transmitter_set_rf_test_config(LoraTransmitter* transmitter, LoraConfigModel* config) {
    transmitter->model->lora_cfg = *config;
    furi_thread_flags_set(
        furi_thread_get_id(transmitter->thread), TransmitterEventSetRFTestConfig);
}
