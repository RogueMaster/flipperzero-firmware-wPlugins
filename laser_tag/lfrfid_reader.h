#pragma once

/**
* @file lfrfid_reader.h
* @brief EM4100 tag reader, inspired by applications/main/lfrfid/lfrfid_cli.c
* @details This file contains the declaration of the LFRFIDReader structure and its functions. You typically allocate a new LFRFIDReader, set the tag detection callback, start the reader. The tag detection callback is called each time a tag is detected. Once you are done, you stop the reader and free it.
* @author CodeAllNight (MrDerekJamison)
*/

#include <furi.h>

typedef struct LFRFIDReader LFRFIDReader;

/**
 * @brief Callback function for tag detection.
 * @param data Tag data.
 * @param length Tag data length.
 * @param context Callback context.
 */
typedef void (*LFRFIDReaderTagCallback)(uint8_t* data, uint8_t length, void* context);

/**
 * @brief Allocates a new LFRFIDReader.
 * @return LFRFIDReader* Pointer to the allocated LFRFIDReader.
 */
LFRFIDReader* lfrfid_reader_alloc();

/**
 * @brief Sets the tag detection callback.
 * @param reader LFRFIDReader to set the callback for.
 * @param requested_protocol Requested protocol, e.g. "EM4100".
 * @param callback Callback function.
 * @param context Callback context.
 */
void lfrfid_reader_set_tag_callback(
    LFRFIDReader* reader,
    char* requested_protocol,
    LFRFIDReaderTagCallback callback,
    void* context);

/**
 * @brief Starts the LFRFIDReader.
 * @param reader LFRFIDReader to start.
 */
void lfrfid_reader_start(LFRFIDReader* reader);

/**
 * @brief Stops the LFRFIDReader.
 * @param reader LFRFIDReader to stop.
 */
void lfrfid_reader_stop(LFRFIDReader* reader);

/**
 * @brief Frees the LFRFIDReader.
 * @param reader LFRFIDReader to free.
 */
void lfrfid_reader_free(LFRFIDReader* reader);
