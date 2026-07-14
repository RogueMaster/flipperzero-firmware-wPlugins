#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIDI_PARSER_MAX_TRACKS 32U

typedef enum {
    MidiParserStatusOk = 0,
    MidiParserStatusEof,
    MidiParserStatusInvalidArgument,
    MidiParserStatusIoError,
    MidiParserStatusBadMagic,
    MidiParserStatusBadHeader,
    MidiParserStatusBadTrack,
    MidiParserStatusUnsupportedFormat,
    MidiParserStatusUnsupportedDivision,
    MidiParserStatusMalformedVarlen,
    MidiParserStatusMissingRunningStatus,
    MidiParserStatusBadEvent,
    MidiParserStatusTruncated,
} MidiParserStatus;

typedef enum {
    MidiEventTypeNone = 0,
    MidiEventTypeNoteOn,
    MidiEventTypeNoteOff,
    MidiEventTypeTempo,
    MidiEventTypeProgramChange,
    MidiEventTypeEndOfTrack,
    MidiEventTypeSkipped,
} MidiEventType;

typedef enum {
    MidiTimingModePpq = 0,
    MidiTimingModeSmpte,
} MidiTimingMode;

typedef size_t (*MidiReaderRead)(void* context, uint8_t* data, size_t size);
typedef bool (*MidiReaderSeek)(void* context, uint32_t position);
typedef uint32_t (*MidiReaderTell)(void* context);
typedef bool (*MidiReaderEof)(void* context);

typedef struct {
    MidiReaderRead read;
    MidiReaderSeek seek;
    MidiReaderTell tell;
    MidiReaderEof eof;
    void* context;
} MidiByteReader;

typedef struct {
    uint16_t format;
    uint16_t track_count;
    uint16_t division;
    MidiTimingMode timing_mode;
    uint16_t ticks_per_quarter;
    uint32_t ticks_per_second;
    bool partial_tracks;
} MidiHeader;

typedef struct {
    MidiEventType type;
    uint32_t absolute_tick;
    uint32_t delta_ticks;
    uint32_t tempo_us_per_quarter;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
    uint8_t program;
    uint8_t raw_status;
} MidiEvent;

typedef struct {
    uint32_t start;
    uint32_t end;
    uint32_t position;
    uint32_t absolute_tick;
    uint8_t running_status;
    bool done;
    bool has_pending;
    MidiEvent pending;
} MidiTrackCursor;

typedef struct {
    MidiByteReader reader;
    MidiHeader header;
    MidiTrackCursor tracks[MIDI_PARSER_MAX_TRACKS];
    uint8_t track_count;
    uint32_t last_output_tick;
    bool primed;
    bool header_loaded;
} MidiParser;

void midi_parser_init(MidiParser* parser);
MidiParserStatus midi_parser_open(MidiParser* parser, const MidiByteReader* reader);
MidiParserStatus midi_parser_read_header(MidiParser* parser);
MidiParserStatus midi_parser_next_event(MidiParser* parser, MidiEvent* event);
void midi_parser_close(MidiParser* parser);

const MidiHeader* midi_parser_get_header(const MidiParser* parser);
const char* midi_parser_status_string(MidiParserStatus status);

#ifdef __cplusplus
}
#endif
