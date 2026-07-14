#include "midi_parser.h"

#include <string.h>

#define MIDI_DEFAULT_TEMPO_US_PER_QUARTER (500000UL)

static MidiParserStatus midi_reader_read_exact(MidiParser* parser, uint8_t* data, size_t size) {
    if(!parser->reader.read) {
        return MidiParserStatusIoError;
    }

    return parser->reader.read(parser->reader.context, data, size) == size ?
               MidiParserStatusOk :
               MidiParserStatusTruncated;
}

static MidiParserStatus midi_reader_read_u8(MidiParser* parser, uint8_t* value) {
    return midi_reader_read_exact(parser, value, 1U);
}

static MidiParserStatus midi_reader_read_be16(MidiParser* parser, uint16_t* value) {
    uint8_t bytes[2];
    MidiParserStatus status = midi_reader_read_exact(parser, bytes, sizeof(bytes));
    if(status != MidiParserStatusOk) {
        return status;
    }
    *value = ((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1];
    return MidiParserStatusOk;
}

static MidiParserStatus midi_reader_read_be24(MidiParser* parser, uint32_t* value) {
    uint8_t bytes[3];
    MidiParserStatus status = midi_reader_read_exact(parser, bytes, sizeof(bytes));
    if(status != MidiParserStatusOk) {
        return status;
    }
    *value = ((uint32_t)bytes[0] << 16) | ((uint32_t)bytes[1] << 8) | (uint32_t)bytes[2];
    return MidiParserStatusOk;
}

static MidiParserStatus midi_reader_read_be32(MidiParser* parser, uint32_t* value) {
    uint8_t bytes[4];
    MidiParserStatus status = midi_reader_read_exact(parser, bytes, sizeof(bytes));
    if(status != MidiParserStatusOk) {
        return status;
    }
    *value = ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) |
             (uint32_t)bytes[3];
    return MidiParserStatusOk;
}

static MidiParserStatus midi_reader_skip(MidiParser* parser, uint32_t size) {
    if(size == 0U) {
        return MidiParserStatusOk;
    }
    if(!parser->reader.tell || !parser->reader.seek) {
        return MidiParserStatusIoError;
    }

    const uint32_t position = parser->reader.tell(parser->reader.context);
    if(UINT32_MAX - position < size) {
        return MidiParserStatusTruncated;
    }

    return parser->reader.seek(parser->reader.context, position + size) ?
               MidiParserStatusOk :
               MidiParserStatusTruncated;
}

static MidiParserStatus midi_read_varlen(MidiParser* parser, uint32_t limit, uint32_t* value) {
    uint32_t result = 0U;

    for(uint8_t index = 0U; index < 4U; index++) {
        if(parser->reader.tell && parser->reader.tell(parser->reader.context) >= limit) {
            return MidiParserStatusTruncated;
        }

        uint8_t byte = 0U;
        MidiParserStatus status = midi_reader_read_u8(parser, &byte);
        if(status != MidiParserStatusOk) {
            return status;
        }

        result = (result << 7) | (uint32_t)(byte & 0x7FU);
        if((byte & 0x80U) == 0U) {
            *value = result;
            return MidiParserStatusOk;
        }
    }

    return MidiParserStatusMalformedVarlen;
}

static MidiParserStatus
    midi_read_track_u8(MidiParser* parser, const MidiTrackCursor* track, uint8_t* value) {
    if(!parser->reader.tell) {
        return MidiParserStatusIoError;
    }
    if(parser->reader.tell(parser->reader.context) >= track->end) {
        return MidiParserStatusTruncated;
    }
    return midi_reader_read_u8(parser, value);
}

static MidiParserStatus
    midi_read_track_be24(MidiParser* parser, const MidiTrackCursor* track, uint32_t* value) {
    if(!parser->reader.tell) {
        return MidiParserStatusIoError;
    }
    const uint32_t position = parser->reader.tell(parser->reader.context);
    if(position > track->end || (track->end - position) < 3U) {
        return MidiParserStatusTruncated;
    }
    return midi_reader_read_be24(parser, value);
}

static MidiParserStatus
    midi_skip_track_bytes(MidiParser* parser, const MidiTrackCursor* track, uint32_t size) {
    if(!parser->reader.tell) {
        return MidiParserStatusIoError;
    }
    const uint32_t position = parser->reader.tell(parser->reader.context);
    if(position > track->end) {
        return MidiParserStatusTruncated;
    }
    if(size > (track->end - position)) {
        return MidiParserStatusTruncated;
    }
    return midi_reader_skip(parser, size);
}

static uint8_t midi_channel_data_length(uint8_t status) {
    switch(status & 0xF0U) {
    case 0xC0U:
    case 0xD0U:
        return 1U;
    default:
        return 2U;
    }
}

static void midi_parser_init_track(MidiTrackCursor* track, uint32_t start, uint32_t size) {
    memset(track, 0, sizeof(*track));
    track->start = start;
    track->end = start + size;
    track->position = start;
}

void midi_parser_init(MidiParser* parser) {
    if(parser) {
        memset(parser, 0, sizeof(*parser));
    }
}

MidiParserStatus midi_parser_open(MidiParser* parser, const MidiByteReader* reader) {
    if(!parser || !reader || !reader->read || !reader->seek || !reader->tell) {
        return MidiParserStatusInvalidArgument;
    }

    midi_parser_init(parser);
    parser->reader = *reader;
    return MidiParserStatusOk;
}

MidiParserStatus midi_parser_read_header(MidiParser* parser) {
    if(!parser) {
        return MidiParserStatusInvalidArgument;
    }

    uint8_t chunk_id[4];
    MidiParserStatus status = midi_reader_read_exact(parser, chunk_id, sizeof(chunk_id));
    if(status != MidiParserStatusOk) {
        return status;
    }
    if(memcmp(chunk_id, "MThd", 4U) != 0) {
        return MidiParserStatusBadMagic;
    }

    uint32_t header_size = 0U;
    status = midi_reader_read_be32(parser, &header_size);
    if(status != MidiParserStatusOk) {
        return status;
    }
    if(header_size < 6U) {
        return MidiParserStatusBadHeader;
    }

    status = midi_reader_read_be16(parser, &parser->header.format);
    if(status != MidiParserStatusOk) {
        return status;
    }
    status = midi_reader_read_be16(parser, &parser->header.track_count);
    if(status != MidiParserStatusOk) {
        return status;
    }
    status = midi_reader_read_be16(parser, &parser->header.division);
    if(status != MidiParserStatusOk) {
        return status;
    }

    if(header_size > 6U) {
        status = midi_reader_skip(parser, header_size - 6U);
        if(status != MidiParserStatusOk) {
            return status;
        }
    }

    if(parser->header.track_count == 0U) {
        return MidiParserStatusBadHeader;
    }
    if((parser->header.division & 0x8000U) != 0U) {
        const int8_t smpte_frames_code = (int8_t)(parser->header.division >> 8);
        const uint8_t ticks_per_frame = (uint8_t)(parser->header.division & 0xFFU);
        uint8_t frames_per_second = (uint8_t)(-smpte_frames_code);
        if(frames_per_second == 29U) {
            frames_per_second = 30U;
        }
        if(ticks_per_frame == 0U ||
           !(frames_per_second == 24U || frames_per_second == 25U || frames_per_second == 30U)) {
            return MidiParserStatusUnsupportedDivision;
        }

        parser->header.timing_mode = MidiTimingModeSmpte;
        parser->header.ticks_per_second = (uint32_t)frames_per_second * ticks_per_frame;
    } else {
        if(parser->header.division == 0U) {
            return MidiParserStatusBadHeader;
        }

        parser->header.timing_mode = MidiTimingModePpq;
        parser->header.ticks_per_quarter = parser->header.division;
    }

    if(parser->header.format > 2U) {
        return MidiParserStatusUnsupportedFormat;
    }

    while(parser->track_count < MIDI_PARSER_MAX_TRACKS) {
        if(parser->reader.eof && parser->reader.eof(parser->reader.context)) {
            break;
        }

        status = midi_reader_read_exact(parser, chunk_id, sizeof(chunk_id));
        if(status != MidiParserStatusOk) {
            break;
        }

        uint32_t chunk_size = 0U;
        status = midi_reader_read_be32(parser, &chunk_size);
        if(status != MidiParserStatusOk) {
            return status;
        }

        if(memcmp(chunk_id, "MTrk", 4U) == 0) {
            const uint32_t start = parser->reader.tell(parser->reader.context);
            if(UINT32_MAX - start < chunk_size) {
                return MidiParserStatusBadTrack;
            }

            midi_parser_init_track(&parser->tracks[parser->track_count], start, chunk_size);
            parser->track_count++;

            status = midi_reader_skip(parser, chunk_size);
            if(status != MidiParserStatusOk) {
                return status;
            }

            if(parser->track_count >= parser->header.track_count) {
                break;
            }
            continue;
        }

        status = midi_reader_skip(parser, chunk_size);
        if(status != MidiParserStatusOk) {
            return status;
        }
    }

    if(parser->track_count == 0U) {
        return MidiParserStatusBadTrack;
    }

    parser->header.partial_tracks = parser->header.track_count > parser->track_count;
    parser->header_loaded = true;
    return parser->reader.seek(parser->reader.context, parser->tracks[0].start) ?
               MidiParserStatusOk :
               MidiParserStatusIoError;
}

static MidiParserStatus
    midi_parser_read_track_event(MidiParser* parser, uint8_t track_index, MidiEvent* event) {
    memset(event, 0, sizeof(*event));
    event->type = MidiEventTypeNone;

    MidiTrackCursor* track = &parser->tracks[track_index];
    if(track->done || track->position >= track->end) {
        track->done = true;
        return MidiParserStatusEof;
    }

    if(!parser->reader.seek(parser->reader.context, track->position)) {
        return MidiParserStatusIoError;
    }

    uint32_t delta_ticks = 0U;
    MidiParserStatus status = midi_read_varlen(parser, track->end, &delta_ticks);
    if(status != MidiParserStatusOk) {
        return status;
    }

    if(UINT32_MAX - track->absolute_tick < delta_ticks) {
        return MidiParserStatusBadEvent;
    }
    track->absolute_tick += delta_ticks;

    uint8_t status_or_data = 0U;
    status = midi_read_track_u8(parser, track, &status_or_data);
    if(status != MidiParserStatusOk) {
        return status;
    }

    uint8_t status_byte = status_or_data;
    uint8_t first_data = 0U;
    bool has_first_data = false;

    if(status_or_data < 0x80U) {
        if(track->running_status == 0U) {
            return MidiParserStatusMissingRunningStatus;
        }
        status_byte = track->running_status;
        first_data = status_or_data;
        has_first_data = true;
    } else if(status_or_data < 0xF0U) {
        track->running_status = status_or_data;
    }

    event->absolute_tick = track->absolute_tick;
    event->delta_ticks = delta_ticks;
    event->raw_status = status_byte;

    if(status_byte == 0xFFU) {
        uint8_t meta_type = 0U;
        status = midi_read_track_u8(parser, track, &meta_type);
        if(status != MidiParserStatusOk) {
            return status;
        }

        uint32_t length = 0U;
        status = midi_read_varlen(parser, track->end, &length);
        if(status != MidiParserStatusOk) {
            return status;
        }

        if(meta_type == 0x2FU) {
            status = midi_skip_track_bytes(parser, track, length);
            if(status != MidiParserStatusOk) {
                return status;
            }
            track->done = true;
            event->type = MidiEventTypeEndOfTrack;
            return MidiParserStatusOk;
        }

        if(meta_type == 0x51U) {
            if(length == 3U) {
                status = midi_read_track_be24(parser, track, &event->tempo_us_per_quarter);
                if(status != MidiParserStatusOk) {
                    return status;
                }
                event->type = MidiEventTypeTempo;
                return MidiParserStatusOk;
            }

            status = midi_skip_track_bytes(parser, track, length);
            if(status != MidiParserStatusOk) {
                return status;
            }
            event->type = MidiEventTypeSkipped;
            return MidiParserStatusOk;
        }

        status = midi_skip_track_bytes(parser, track, length);
        if(status != MidiParserStatusOk) {
            return status;
        }
        event->type = MidiEventTypeSkipped;
        return MidiParserStatusOk;
    }

    if(status_byte == 0xF0U || status_byte == 0xF7U) {
        uint32_t length = 0U;
        status = midi_read_varlen(parser, track->end, &length);
        if(status != MidiParserStatusOk) {
            return status;
        }
        status = midi_skip_track_bytes(parser, track, length);
        if(status != MidiParserStatusOk) {
            return status;
        }
        event->type = MidiEventTypeSkipped;
        return MidiParserStatusOk;
    }

    if(status_byte < 0x80U || status_byte >= 0xF0U) {
        return MidiParserStatusBadEvent;
    }

    const uint8_t data_length = midi_channel_data_length(status_byte);
    uint8_t data[2] = {0U, 0U};

    if(has_first_data) {
        data[0] = first_data;
    } else {
        status = midi_read_track_u8(parser, track, &data[0]);
        if(status != MidiParserStatusOk) {
            return status;
        }
    }

    if(data[0] >= 0x80U) {
        return MidiParserStatusBadEvent;
    }

    if(data_length == 2U) {
        status = midi_read_track_u8(parser, track, &data[1]);
        if(status != MidiParserStatusOk) {
            return status;
        }
        if(data[1] >= 0x80U) {
            return MidiParserStatusBadEvent;
        }
    }

    event->channel = status_byte & 0x0FU;

    switch(status_byte & 0xF0U) {
    case 0x80U:
        event->type = MidiEventTypeNoteOff;
        event->note = data[0];
        event->velocity = data[1];
        break;
    case 0x90U:
        event->note = data[0];
        event->velocity = data[1];
        event->type = (data[1] == 0U) ? MidiEventTypeNoteOff : MidiEventTypeNoteOn;
        break;
    case 0xC0U:
        event->type = MidiEventTypeProgramChange;
        event->program = data[0];
        break;
    default:
        event->type = MidiEventTypeSkipped;
        break;
    }

    track->position = parser->reader.tell(parser->reader.context);
    return MidiParserStatusOk;
}

static MidiParserStatus midi_parser_fill_pending(MidiParser* parser, uint8_t track_index) {
    MidiTrackCursor* track = &parser->tracks[track_index];
    while(!track->done && !track->has_pending) {
        MidiEvent event;
        MidiParserStatus status = midi_parser_read_track_event(parser, track_index, &event);
        if(status == MidiParserStatusEof) {
            track->done = true;
            return MidiParserStatusOk;
        }
        if(status != MidiParserStatusOk) {
            track->done = true;
            return status;
        }

        track->position = parser->reader.tell(parser->reader.context);
        if(event.type == MidiEventTypeEndOfTrack) {
            track->done = true;
            if(parser->track_count == 1U) {
                track->pending = event;
                track->has_pending = true;
            }
            return MidiParserStatusOk;
        }

        track->pending = event;
        track->has_pending = true;
    }

    return MidiParserStatusOk;
}

MidiParserStatus midi_parser_next_event(MidiParser* parser, MidiEvent* event) {
    if(!parser || !event || !parser->header_loaded || parser->track_count == 0U) {
        return MidiParserStatusInvalidArgument;
    }

    for(uint8_t i = 0U; i < parser->track_count; i++) {
        if(!parser->tracks[i].has_pending && !parser->tracks[i].done) {
            MidiParserStatus status = midi_parser_fill_pending(parser, i);
            if(status != MidiParserStatusOk && parser->track_count == 1U) {
                return status;
            }
        }
    }

    bool found = false;
    uint8_t selected_track = 0U;
    uint32_t selected_tick = 0U;

    for(uint8_t i = 0U; i < parser->track_count; i++) {
        if(parser->tracks[i].has_pending) {
            const uint32_t tick = parser->tracks[i].pending.absolute_tick;
            if(!found || tick < selected_tick) {
                found = true;
                selected_tick = tick;
                selected_track = i;
            }
        }
    }

    if(!found) {
        return MidiParserStatusEof;
    }

    *event = parser->tracks[selected_track].pending;
    parser->tracks[selected_track].has_pending = false;
    event->delta_ticks = event->absolute_tick - parser->last_output_tick;
    parser->last_output_tick = event->absolute_tick;
    return MidiParserStatusOk;
}

void midi_parser_close(MidiParser* parser) {
    if(parser) {
        midi_parser_init(parser);
    }
}

const MidiHeader* midi_parser_get_header(const MidiParser* parser) {
    return parser ? &parser->header : NULL;
}

const char* midi_parser_status_string(MidiParserStatus status) {
    switch(status) {
    case MidiParserStatusOk:
        return "OK";
    case MidiParserStatusEof:
        return "End of file";
    case MidiParserStatusInvalidArgument:
        return "Invalid argument";
    case MidiParserStatusIoError:
        return "I/O error";
    case MidiParserStatusBadMagic:
        return "Not a MIDI file";
    case MidiParserStatusBadHeader:
        return "Bad MIDI header";
    case MidiParserStatusBadTrack:
        return "Missing or bad track";
    case MidiParserStatusUnsupportedFormat:
        return "Unsupported MIDI format";
    case MidiParserStatusUnsupportedDivision:
        return "Unsupported SMPTE timing";
    case MidiParserStatusMalformedVarlen:
        return "Malformed variable length value";
    case MidiParserStatusMissingRunningStatus:
        return "Missing running status";
    case MidiParserStatusBadEvent:
        return "Bad MIDI event";
    case MidiParserStatusTruncated:
        return "Truncated MIDI file";
    default:
        return "Unknown parser error";
    }
}
