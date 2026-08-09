#include "capture_module.h"

// PCAP global header structure
typedef struct pcap_global_header {
    uint32_t magic_number; // Magic number (0xA1B2C3D4 in big endian)
    uint16_t version_major; // Major version number (2)
    uint16_t version_minor; // Minor version number (4)
    int32_t thiszone; // GMT to local correction (0)
    uint32_t sigfigs; // Accuracy of timestamps (0)
    uint32_t snaplen; // Max length of captured packets
    uint32_t network; // Data link type (1 = Ethernet)
} pcap_global_header_t;

// PCAP packet header structure
typedef struct pcap_packet_header {
    uint32_t ts_sec; // Timestamp seconds
    uint32_t ts_usec; // Timestamp microseconds
    uint32_t incl_len; // Number of octets of packet saved in file
    uint32_t orig_len; // Actual length of packet
} pcap_packet_header_t;

// Base timestamp reference (to be initialized on first use)
static uint32_t base_timestamp_sec = 0;
static uint32_t base_tick = 0;

void create_pcap_name(FuriString* complete_path, const char* PATH, const char* name) {
    furi_string_reset(complete_path);
    furi_string_cat_printf(complete_path, "%s", PATH);
    furi_string_cat_printf(complete_path, "/");
    furi_string_cat_printf(complete_path, "%s", name);
    furi_string_cat_printf(complete_path, ".pcap");
}

bool pcap_capture_init(File* file, const char* filename) {
    // F0.7 — use the SDK's Unix timestamp helper. The previous code
    // approximated months as 30 days and ignored leap years, producing
    // PCAP timestamps that drifted by days/weeks vs wall clock and
    // confused Wireshark's relative-time display.
    base_timestamp_sec = furi_hal_rtc_get_timestamp();

    // Store the current tick for relative time calculation
    base_tick = furi_get_tick();

    // Create the file
    if(!storage_file_open(file, filename, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        return false;
    }

    // Write PCAP header
    pcap_global_header_t header;
    header.magic_number = 0xA1B2C3D4;
    header.version_major = 2;
    header.version_minor = 4;
    header.thiszone = 0;
    header.sigfigs = 0;
    header.snaplen = 65535;
    header.network = 1; // Ethernet

    // Write header to file
    if(storage_file_write(file, &header, sizeof(header)) != sizeof(header)) {
        storage_file_close(file);
        return false;
    }

    // Flush to ensure header is written
    storage_file_sync(file);

    return true;
}

bool pcap_capture_add_packet(File* file, const uint8_t* packet, uint32_t packet_len) {
    // Create packet header
    pcap_packet_header_t packet_header;

    // Calculate time elapsed since base timestamp
    uint32_t current_tick = furi_get_tick();
    uint32_t elapsed_ticks = current_tick - base_tick;

    // Convert ticks to milliseconds (Flipper OS uses 1000 ticks per second)
    uint32_t elapsed_ms = elapsed_ticks;

    // Calculate seconds and microseconds
    uint32_t elapsed_sec = elapsed_ms / 1000;
    uint32_t elapsed_usec = (elapsed_ms % 1000) * 1000; // Convert remaining ms to us

    // Add to base timestamp
    packet_header.ts_sec = base_timestamp_sec + elapsed_sec;
    packet_header.ts_usec = elapsed_usec;

    packet_header.incl_len = packet_len;
    packet_header.orig_len = packet_len;

    // Write packet header
    if(storage_file_write(file, &packet_header, sizeof(packet_header)) != sizeof(packet_header)) {
        return false;
    }

    // Write packet data
    if(storage_file_write(file, packet, packet_len) != packet_len) {
        return false;
    }

    return true;
}

bool pcap_capture_sync(File* file) {
    if(!file) return false;
    return storage_file_sync(file);
}

void pcap_close(File* file) {
    if(file) {
        storage_file_sync(file);
        storage_file_close(file);
    }
}

size_t pcap_reader_init(File* file, const char* filename) {
    // Open file
    if(!storage_file_open(file, filename, FSAM_READ, FSOM_OPEN_EXISTING)) {
        return 0;
    }

    pcap_global_header_t header;
    size_t bytes_read = storage_file_read(file, &header, sizeof(header));

    // Check if we read the complete header and magic number is correct
    if(bytes_read != sizeof(header) || header.magic_number != 0xA1B2C3D4) {
        storage_file_close(file);
        return 0;
    }

    return bytes_read;
}

size_t pcap_get_specific_packet(
    File* file,
    uint8_t* packet,
    size_t packet_capacity,
    uint32_t packet_position) {
    if(!file || !packet || packet_capacity == 0 || !storage_file_is_open(file)) return 0;

    // Sync the file
    if(!storage_file_sync(file)) return 0;

    // Set the position on the file
    if(!storage_file_seek(file, packet_position, true)) return 0;

    pcap_packet_header_t packet_header;

    // Read packet header
    if(storage_file_read(file, &packet_header, sizeof(packet_header)) != sizeof(packet_header))
        return 0;

    // F0.5e — clamp orig_len to caller's buffer size. The PCAP record
    // header is untrusted input (file may be corrupt or hand-crafted),
    // so a record claiming e.g. orig_len=0xFFFFFFFF would otherwise
    // silently overflow the caller's frame buffer.
    uint32_t to_read = packet_header.orig_len;
    if(to_read > packet_capacity) to_read = packet_capacity;

    // Read the packet data
    if(storage_file_read(file, packet, to_read) != to_read) return 0;

    return to_read;
}

uint32_t pcap_scan(File* file, const char* filename, uint64_t* positions, uint32_t max_positions) {
    // Counter and positions
    uint32_t counter = 0;

    // Get the first position
    if(!storage_file_open(file, filename, FSAM_READ, FSOM_OPEN_EXISTING)) {
        return 0;
    }

    // Get the total size of the file
    uint64_t file_size = storage_file_size(file);

    // This is for the pcap header field
    pcap_global_header_t header;

    // This part is to set the first position with bytes_read
    size_t bytes_read = storage_file_read(file, &header, sizeof(header));

    // Check if we read the complete header and magic number is correct
    if(bytes_read != sizeof(header) || header.magic_number != 0xA1B2C3D4) {
        storage_file_close(file);
        return 0;
    }

    // This variable will use to get the positions
    uint64_t position = 0;

    // For the packets
    pcap_packet_header_t packet_header;

    while(bytes_read > 0) {
        // Add the value for the position with bytes_read
        position = position + bytes_read;

        // F0.7 — bounds check; pre-fix this could write past
        // packet_positions[2000] on captures with > 2000 frames.
        if(counter >= max_positions) break;

        // Add the position
        positions[counter] = position;
        counter++;

        // Read the per-packet header, then skip the body via seek
        // (F0.5e — pre-fix this read into a 1518-byte stack buffer,
        // which a malicious orig_len could overflow). We only need
        // the offsets here; the body is read on demand by
        // pcap_get_specific_packet, which clamps to the caller's
        // buffer.
        bytes_read = storage_file_read(file, &packet_header, sizeof(packet_header));
        if(bytes_read != sizeof(packet_header)) {
            break;
        }

        if(packet_header.orig_len == 0) {
            break;
        }

        uint64_t cursor = storage_file_tell(file);
        if(!storage_file_seek(file, (uint32_t)(cursor + packet_header.orig_len), true)) {
            break;
        }
        bytes_read += packet_header.orig_len;

        if(storage_file_tell(file) >= file_size) {
            break;
        }
    }

    storage_file_close(file);

    return counter;
}
