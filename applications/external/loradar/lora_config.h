#pragma once

#include <furi.h>

extern const uint32_t bandwidth_list[];
extern const uint8_t canal_list[];

// Default values for LoRa configuration
#define DEFAULT_FREQ                (868) // Frequency in MHz
#define DEFAULT_CANAL_NUM           (0) // Canal
#define DEFAULT_SF                  (8) // Spreading Factor
#define DEFAULT_BW                  (125) // Bandwidth (kHz)
#define DEFAULT_TX_PREAMBLE         (8) // TX Preamble length
#define DEFAULT_RX_PREAMBLE         (8) // RX Preamble length
#define DEFAULT_POWER               (14) // Power level (dBm)
#define DEFAULT_WITH_CRC            (true) // CRC enabled
#define DEFAULT_IQ_INVERTED         (false) // IQ inversion disabled
#define DEFAULT_WITH_PUBLIC_LORAWAN (false) // Public LoRaWAN enabled

/**
 * @brief LoRa configuration Object
 */
typedef struct {
    uint32_t freq; // Frequency in MHz
    uint8_t canal_idx; // Canal index (see canal_list)
    uint8_t sf; // Spreading factor
    uint8_t bw_idx; // Bandwidth index (see bandwidth_list)
    uint8_t tx_preamble; // TX preamble length
    uint8_t rx_preamble; // RX preamble length
    uint8_t power; // Power level (dbm)
    bool with_crc; // CRC enabled/disabled
    bool is_iq_inverted; // IQ inversion enabled/disabled
    bool with_public_lorawan; // Public LoRaWAN enabled/disabled
} LoraConfigModel;

void init_lora_config_default(LoraConfigModel* cfg);
