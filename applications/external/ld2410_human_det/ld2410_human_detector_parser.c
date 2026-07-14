#include "ld2410_human_detector_parser.h"
#include <string.h>

void ld2410_human_detector_parser_init(LD2410Parser* parser) {
    memset(parser, 0, sizeof(LD2410Parser));
    parser->index = 0;
    parser->header_found = false;
}

// Unused checksum validation removed
/*
static bool ld2410_validate_checksum(const uint8_t* data, size_t len) {
    if(len < 4) return false;
    // Frame structure: Header(4) + Len(2) + Payload(N) + Check(1) + End(4)
    // Actually the logic is simpler for standard data output
    // Standard data: F4 F3 F2 F1 [Len_L Len_H] [DataType] [Data...] [Check] F8 F7 F6 F5

    // LD2410 manual: Check is last byte before End sequence?
    // Actually typically implementation just checks structure.
    // Let's implement basics.
    return true;
}
*/

bool ld2410_human_detector_parser_push_byte(
    LD2410Parser* parser,
    uint8_t byte,
    LD2410Data* out_data) {
    // 1. If header not found, look for F4 F3 F2 F1
    if(!parser->header_found) {
        // Shift buffer to keep looking
        // Simple state machine:
        // We can just keep a sliding window or fill checking last 4 bytes
        // But simplified logic:
        parser->buffer[parser->index++] = byte;

        if(parser->index >= 4) {
            if(parser->buffer[parser->index - 4] == LD2410_FRAME_HEADER_1 &&
               parser->buffer[parser->index - 3] == LD2410_FRAME_HEADER_2 &&
               parser->buffer[parser->index - 2] == LD2410_FRAME_HEADER_3 &&
               parser->buffer[parser->index - 1] == LD2410_FRAME_HEADER_4) {
                parser->header_found = true;
                // Reset index to 0 for payload reading or keep it?
                // The standard packet length is typically fixed or read from length bytes.
                // Let's reset buffer to hold the PAYLOAD starting after header
                parser->index = 0;
                return false;
            }
        }

        if(parser->index >= 63) {
            // Reset if too full and no header found
            parser->index = 0;
        }
        return false;
    }

    // 2. Header found. Now read length and data.
    // Byte 0,1 after header = Data Length (2 bytes, little endian)
    // Structure: Len(2) + DataType(1) + ... + Check(1) + End(4)

    parser->buffer[parser->index++] = byte;

    // Need at least 2 bytes to know length
    if(parser->index < 2) return false;

    // uint16_t info_len = parser->buffer[0] | (parser->buffer[1] << 8); // Unused
    // info_len might be useful for strict checking, but basic mode often relies on footer
    // UNUSED(info_len);
    // info_len is bytes of (Type + Head + Move + Static + .. + Checksum?)
    // Actually "Data Length = Head + Payload + Checksum" usually excluding header/tail?
    // Datasheet says: Length = Payload Length + 2 (DataType) + 1 (Check)?
    // Let's assume typical report packet size is fixed ~23 bytes usually.
    // The dataframe length in basic mode is defined.

    // Standard report:
    // 0: Data Len L
    // 1: Data Len H
    // 2: Data Type (0x02 = Engineering, 0x01 = Target info etc) -- usually 0x02 or 0x01?
    // 3: Head Header (0xAA)
    // 4: Target State
    // 5: Move dist L
    // 6: Move dist H
    // 7: Move energy
    // 8: Static dist L
    // 9: Static dist H
    // 10: Static energy
    // 11: Detect dist L
    // 12: Detect dist H
    // ...

    // To be safe, wait until we see the FOOTER F8 F7 F6 F5
    if(parser->index >= 4) {
        if(parser->buffer[parser->index - 4] == LD2410_FRAME_END_1 &&
           parser->buffer[parser->index - 3] == LD2410_FRAME_END_2 &&
           parser->buffer[parser->index - 2] == LD2410_FRAME_END_3 &&
           parser->buffer[parser->index - 1] == LD2410_FRAME_END_4) {
            // Full frame received.
            // Parse internal data
            // Typical payload starting at buffer[2] (after length)

            // Check Data Type: buffer[2]
            // We expect reports.

            // Simplified parsing for standard report mode
            // Offset relative to buffer[0]
            // buffer[0,1] = len
            // buffer[2] = DataType (usually 0x02 for common report?)
            // buffer[3] = Head (0xAA)
            // buffer[4] = Target State
            // buffer[5,6] = Move Dist
            // buffer[7] = Move Energy
            // buffer[8,9] = Static Dist
            // buffer[10] = Static Energy
            // buffer[11,12] = Detection Dist

            if(parser->buffer[3] == 0xAA) {
                if(out_data) {
                    out_data->target_state = parser->buffer[4];
                    out_data->move_distance_cm = parser->buffer[5] | (parser->buffer[6] << 8);
                    out_data->move_energy = parser->buffer[7];
                    out_data->static_distance_cm = parser->buffer[8] | (parser->buffer[9] << 8);
                    out_data->static_energy = parser->buffer[10];
                    out_data->detection_distance_cm = parser->buffer[11] |
                                                      (parser->buffer[12] << 8);
                }

                // Reset
                parser->header_found = false;
                parser->index = 0;
                return true;
            }

            // If invalid or other packet, reset
            parser->header_found = false;
            parser->index = 0;
            return false;
        }
    }

    // Safety break
    if(parser->index >= 63) {
        parser->header_found = false;
        parser->index = 0;
    }

    return false;
}
