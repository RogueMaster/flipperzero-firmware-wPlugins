/*
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-04 14:21:29
 * @Description: file content
 */
#pragma once

#include "subbrute_worker.h"

#include <lib/subghz/protocols/base.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/environment.h>

/**
 * @class SubBruteWorker
 * @brief Class representing a SubBruteWorker object.
 *
 * The SubBruteWorker class is responsible for performing sub-brute forcing tasks.
 * It manages the state, configuration and execution of the sub-brute forcing algorithm.
 */
struct SubBruteWorker {
    SubBruteWorkerState state; /**< State of the worker */
    volatile bool worker_running; /**< Worker running state */
    volatile bool initiated; /**< Initiated state */
    volatile bool transmit_mode; /**< Transmit mode */

    uint64_t step; /**< Current step */

    FuriThread* thread; /**< Thread */

    /** @see @c SubGhz service for more info */
    SubGhzEnvironment* environment; /**< Environment */
    SubGhzProtocolDecoderBase* decoder_result;
    SubGhzTransmitter* transmitter; /**< Transmitter */
    const char* protocol_name; /**< Name of the protocol */
    uint8_t tx_timeout_ms; /**< Timeout for transmit */
    const SubGhzDevice* radio_device; /**< Radio device */

    // Initiated values
    SubBruteAttacks attack; /**< Attack state */
    uint32_t frequency; /**< Frequency */
    FuriHalSubGhzPreset preset; /**< Preset */
    SubBruteFileProtocol file; /**< File protocol */
    uint8_t bits; /**< Number of bits in key */
    uint32_t te; /**< Time to wait for response */
    uint8_t repeat; /**< Number of extra repeats */
    uint8_t load_index; /**< Index of group to bruteforce in loaded file */
    uint64_t file_key; /**< Key from file */
    uint64_t max_value; /**< Max step */
    uint8_t opencode; /**< Opencode */
    bool two_bytes; /**< Two bytes key */

    // Manual transmit
    uint32_t last_time_tx_data; /**< Last time data was transmitted */

    // Callback for changed states
    SubBruteWorkerCallback callback; /**< Callback for changed states */
    void* context;
};

/**
 * @brief This function is the entry point for the sub-brute worker thread.
 *
 * @param context A pointer to the context data for the worker thread.
 * @return int32_t The return status of the worker thread.
 *
 * The sub-brute worker thread performs a sub-brute force operation based on the given context.
 * It takes the context as an input, processes the data, and returns a status code.
 * The function is used as an entry point for the worker thread.
 */
int32_t subbrute_worker_thread(void* context);

/**
 * @brief Transmits FlipperFormat using subGHz.
 *
 * This function transmits the specified FlipperFormat using the subGHz
 * protocol.
 *
 * @param instance The SubBruteWorker instance.
 * @param flipper_format Pointer to the FlipperFormat to be transmitted.
 */
void subbrute_worker_subghz_transmit(SubBruteWorker* instance, FlipperFormat* flipper_format);

/**
 * @brief Send a callback for a SubBruteWorker instance.
 *
 * This function is used to send a callback for the SubBruteWorker instance. The callback is triggered
 * when certain conditions are met during the worker's execution.
 *
 * @param instance The SubBruteWorker instance for which the callback is sent.
 *
 * @note This function does not return any values.
 */
void subbrute_worker_send_callback(SubBruteWorker* instance);
