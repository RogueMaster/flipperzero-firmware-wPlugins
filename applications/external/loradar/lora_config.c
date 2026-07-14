#include <lora_config.h>

const uint32_t bandwidth_list[] = {
    125,
    250,
    500,
};

const uint8_t canal_list[] = {
    1, // 868.1 is canal 0
    3, // 868.3 is canal 1
    5, // 868.5 is canal 2
};

void init_lora_config_default(LoraConfigModel* cfg) {
    cfg->freq = DEFAULT_FREQ;
    cfg->canal_idx = DEFAULT_CANAL_NUM;
    cfg->sf = DEFAULT_SF;

    for(size_t i = 0; i < sizeof(bandwidth_list) / sizeof(bandwidth_list[0]); i++) {
        if(bandwidth_list[i] == DEFAULT_BW) {
            cfg->bw_idx = i;
            break;
        }
    }

    cfg->tx_preamble = DEFAULT_TX_PREAMBLE;
    cfg->rx_preamble = DEFAULT_RX_PREAMBLE;
    cfg->power = DEFAULT_POWER;
    cfg->with_crc = DEFAULT_WITH_CRC;
    cfg->is_iq_inverted = DEFAULT_IQ_INVERTED;
    cfg->with_public_lorawan = DEFAULT_WITH_PUBLIC_LORAWAN;
}
