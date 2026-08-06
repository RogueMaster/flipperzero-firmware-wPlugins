#ifndef CAPTURE_MODULE_H_
#define CAPTURE_MODULE_H_

#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>

/**
 * @brief Creates a complete file path for a new PCAP file.
 *
 * This function combines a base path and a filename to create a
 * full path string for saving a PCAP file. It ensures the path is correctly
 * formatted and ready for file system operations.
 *
 * @param complete_path A pointer to a `FuriString` where the complete path will be stored.
 * @param PATH The base directory path where the file should be saved.
 * @param name The desired name of the PCAP file (e.g., "capture.pcap").
 */
void create_pcap_name(FuriString* complete_path, const char* PATH, const char* name);

/**
 * @brief Initializes a PCAP capture file for writing.
 *
 * This function opens or creates a new file at the specified filename,
 * writes the global PCAP header, and prepares the file for subsequent packet writes.
 * The file handle is returned and must be used for all further operations.
 *
 * @param file A pointer to the `File` object to be initialized.
 * @param filename The complete path to the PCAP file.
 * @return `true` on success, `false` if the file could not be opened or initialized.
 */
bool pcap_capture_init(File* file, const char* filename);

/**
 * @brief Appends a single network packet to an open PCAP file.
 *
 * This function writes a PCAP packet header and the raw packet data
 * to the file. It is designed to be called for each packet captured during a
 * network session.
 *
 * @param file A pointer to the initialized `File` object.
 * @param packet A pointer to the raw packet data.
 * @param packet_len The length of the raw packet data in bytes.
 * @return `true` on success, `false` if there was a write error.
 */
bool pcap_capture_add_packet(File* file, const uint8_t* packet, uint32_t packet_len);

/**
 * @brief Synchronizes the PCAP file with the storage medium.
 *
 * This function forces all buffered data to be written to the
 * physical storage (e.g., SD card). It's useful to call periodically during a
 * long capture or before closing the file to ensure all data is saved.
 *
 * @param file A pointer to the initialized `File` object.
 * @return `true` on success, `false` on a sync error.
 */
bool pcap_capture_sync(File* file);

/**
 * @brief Closes a PCAP file and releases resources.
 *
 * This function closes the open file handle and cleans up any
 * associated resources. It should always be called when a capture is finished
 * to ensure data integrity and prevent file system errors.
 *
 * @param file A pointer to the `File` object to be closed.
 */
void pcap_close(File* file);

/**
 * @brief Initializes a PCAP file for reading.
 *
 * This function opens an existing PCAP file, reads and validates
 * its global header, and prepares the file for sequential or random packet access.
 *
 * @param file A pointer to the `File` object to be initialized.
 * @param filename The complete path to the PCAP file.
 * @return The total number of packets found in the file, or `0` if the file
 * is invalid or could not be opened.
 */
size_t pcap_reader_init(File* file, const char* filename);

/**
 * @brief Retrieves a specific packet from a PCAP file.
 *
 * This function seeks to a specific packet within the file and reads its
 * data into the provided buffer. This allows for random access to packets without
 * having to read the entire file sequentially.
 *
 * @param file A pointer to the initialized `File` object.
 * @param packet A pointer to a buffer where the packet data will be copied.
 * @param packet_capacity Size of `packet` in bytes. Frames whose recorded
 *        orig_len exceeds this are clamped — protects against malicious or
 *        corrupt PCAPs writing past the caller's buffer (F0.5e fix).
 * @param packet_position The byte offset of the packet record in the file.
 * @return The length of the retrieved packet in bytes (≤ packet_capacity),
 * or `0` if the packet could not be read.
 */
size_t pcap_get_specific_packet(
    File* file,
    uint8_t* packet,
    size_t packet_capacity,
    uint32_t packet_position);

/**
 * @brief Scans a PCAP file to locate all packet positions.
 *
 * This function reads through the entire PCAP file, identifies the
 * starting position of each packet, and stores these positions in the provided
 * `positions` array. This is a preliminary step for enabling random access to packets.
 *
 * @param file A pointer to the `File` object.
 * @param filename The complete path to the PCAP file.
 * @param positions A pointer to a `uint64_t` array where the file offset of each packet will be stored.
 * @param max_positions Capacity of the `positions` array. F0.7 — added so
 *        pcap_scan can stop before overflowing on large captures.
 * @return The total number of packets found in the file (capped at max_positions).
 */
uint32_t pcap_scan(File* file, const char* filename, uint64_t* positions, uint32_t max_positions);

#endif
