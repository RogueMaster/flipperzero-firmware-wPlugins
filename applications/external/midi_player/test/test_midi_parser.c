#include "midi_parser.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const uint8_t* data;
    size_t size;
    size_t position;
} TestReader;

static size_t test_reader_read(void* context, uint8_t* data, size_t size) {
    TestReader* reader = context;
    const size_t remaining = reader->size - reader->position;
    const size_t read_size = size < remaining ? size : remaining;
    memcpy(data, reader->data + reader->position, read_size);
    reader->position += read_size;
    return read_size;
}

static bool test_reader_seek(void* context, uint32_t position) {
    TestReader* reader = context;
    if(position > reader->size) {
        return false;
    }
    reader->position = position;
    return true;
}

static uint32_t test_reader_tell(void* context) {
    TestReader* reader = context;
    return (uint32_t)reader->position;
}

static bool test_reader_eof(void* context) {
    TestReader* reader = context;
    return reader->position >= reader->size;
}

static void put_be16(uint8_t* output, uint16_t value) {
    output[0] = (uint8_t)(value >> 8);
    output[1] = (uint8_t)value;
}

static void put_be32(uint8_t* output, uint32_t value) {
    output[0] = (uint8_t)(value >> 24);
    output[1] = (uint8_t)(value >> 16);
    output[2] = (uint8_t)(value >> 8);
    output[3] = (uint8_t)value;
}

static size_t put_header(uint8_t* output, uint16_t format, uint16_t tracks, uint16_t division) {
    memcpy(output, "MThd", 4U);
    put_be32(output + 4U, 6U);
    put_be16(output + 8U, format);
    put_be16(output + 10U, tracks);
    put_be16(output + 12U, division);
    return 14U;
}

static size_t begin_track(uint8_t* output, size_t offset) {
    memcpy(output + offset, "MTrk", 4U);
    put_be32(output + offset + 4U, 0U);
    return offset + 8U;
}

static void finish_track(uint8_t* output, size_t track_start, size_t track_end) {
    put_be32(output + track_start - 4U, (uint32_t)(track_end - track_start));
}

static MidiParserStatus parse_header(const uint8_t* data, size_t size, MidiParser* parser) {
    static TestReader test_reader;
    test_reader.data = data;
    test_reader.size = size;
    test_reader.position = 0U;

    MidiByteReader reader = {
        .read = test_reader_read,
        .seek = test_reader_seek,
        .tell = test_reader_tell,
        .eof = test_reader_eof,
        .context = &test_reader,
    };

    MidiParserStatus status = midi_parser_open(parser, &reader);
    if(status != MidiParserStatusOk) {
        return status;
    }
    return midi_parser_read_header(parser);
}

static void expect_status(
    const char* name,
    MidiParserStatus actual,
    MidiParserStatus expected,
    unsigned* failures) {
    if(actual != expected) {
        printf(
            "FAIL %s: expected %s, got %s\n",
            name,
            midi_parser_status_string(expected),
            midi_parser_status_string(actual));
        (*failures)++;
    }
}

static void expect_u32(const char* name, uint32_t actual, uint32_t expected, unsigned* failures) {
    if(actual != expected) {
        printf("FAIL %s: expected %" PRIu32 ", got %" PRIu32 "\n", name, expected, actual);
        (*failures)++;
    }
}

static void expect_event(
    const char* name,
    const MidiEvent* event,
    MidiEventType type,
    uint32_t tick,
    uint8_t note,
    unsigned* failures) {
    if(event->type != type || event->absolute_tick != tick || event->note != note) {
        printf(
            "FAIL %s: unexpected event type=%d tick=%" PRIu32 " note=%u\n",
            name,
            event->type,
            event->absolute_tick,
            event->note);
        (*failures)++;
    }
}

static void test_valid_format0(unsigned* failures) {
    uint8_t midi[128];
    size_t offset = put_header(midi, 0U, 1U, 96U);
    offset = begin_track(midi, offset);
    const size_t track_start = offset;

    const uint8_t track[] = {
        0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20, 0x00, 0x90, 0x3C, 0x40, 0x60, 0x40,
        0x40, 0x60, 0x80, 0x3C, 0x00, 0x00, 0x40, 0x00, 0x00, 0xFF, 0x2F, 0x00,
    };
    memcpy(midi + offset, track, sizeof(track));
    offset += sizeof(track);
    finish_track(midi, track_start, offset);

    MidiParser parser;
    expect_status(
        "valid header", parse_header(midi, offset, &parser), MidiParserStatusOk, failures);
    const MidiHeader* header = midi_parser_get_header(&parser);
    expect_u32("ticks per quarter", header->ticks_per_quarter, 96U, failures);

    MidiEvent event;
    expect_status(
        "tempo event status",
        midi_parser_next_event(&parser, &event),
        MidiParserStatusOk,
        failures);
    expect_event("tempo event", &event, MidiEventTypeTempo, 0U, 0U, failures);
    expect_u32("tempo value", event.tempo_us_per_quarter, 500000U, failures);

    expect_status(
        "note on C", midi_parser_next_event(&parser, &event), MidiParserStatusOk, failures);
    expect_event("note on C event", &event, MidiEventTypeNoteOn, 0U, 60U, failures);
    expect_u32("note on C velocity", event.velocity, 64U, failures);

    expect_status(
        "running note on E", midi_parser_next_event(&parser, &event), MidiParserStatusOk, failures);
    expect_event("running note on E event", &event, MidiEventTypeNoteOn, 96U, 64U, failures);

    expect_status(
        "note off C", midi_parser_next_event(&parser, &event), MidiParserStatusOk, failures);
    expect_event("note off C event", &event, MidiEventTypeNoteOff, 192U, 60U, failures);

    expect_status(
        "running note off E",
        midi_parser_next_event(&parser, &event),
        MidiParserStatusOk,
        failures);
    expect_event("running note off E event", &event, MidiEventTypeNoteOff, 192U, 64U, failures);

    expect_status(
        "end of track", midi_parser_next_event(&parser, &event), MidiParserStatusOk, failures);
    expect_event("end event", &event, MidiEventTypeEndOfTrack, 192U, 0U, failures);
}

static void test_bad_magic(unsigned* failures) {
    const uint8_t midi[] = {'N', 'O', 'P', 'E'};
    MidiParser parser;
    expect_status(
        "bad magic", parse_header(midi, sizeof(midi), &parser), MidiParserStatusBadMagic, failures);
}

static void test_format1_merge(unsigned* failures) {
    uint8_t midi[128];
    size_t offset = put_header(midi, 1U, 2U, 96U);

    offset = begin_track(midi, offset);
    size_t track_start = offset;
    const uint8_t tempo_track[] = {
        0x00,
        0xFF,
        0x51,
        0x03,
        0x07,
        0xA1,
        0x20,
        0x00,
        0xFF,
        0x2F,
        0x00,
    };
    memcpy(midi + offset, tempo_track, sizeof(tempo_track));
    offset += sizeof(tempo_track);
    finish_track(midi, track_start, offset);

    offset = begin_track(midi, offset);
    track_start = offset;
    const uint8_t note_track[] = {
        0x00,
        0x90,
        0x3C,
        0x40,
        0x60,
        0x80,
        0x3C,
        0x00,
        0x00,
        0xFF,
        0x2F,
        0x00,
    };
    memcpy(midi + offset, note_track, sizeof(note_track));
    offset += sizeof(note_track);
    finish_track(midi, track_start, offset);

    MidiParser parser;
    expect_status(
        "format 1 header", parse_header(midi, offset, &parser), MidiParserStatusOk, failures);

    MidiEvent event;
    expect_status(
        "format 1 tempo", midi_parser_next_event(&parser, &event), MidiParserStatusOk, failures);
    expect_event("format 1 tempo event", &event, MidiEventTypeTempo, 0U, 0U, failures);

    expect_status(
        "format 1 note on", midi_parser_next_event(&parser, &event), MidiParserStatusOk, failures);
    expect_event("format 1 note on event", &event, MidiEventTypeNoteOn, 0U, 60U, failures);

    expect_status(
        "format 1 note off", midi_parser_next_event(&parser, &event), MidiParserStatusOk, failures);
    expect_event("format 1 note off event", &event, MidiEventTypeNoteOff, 96U, 60U, failures);

    expect_status(
        "format 1 eof", midi_parser_next_event(&parser, &event), MidiParserStatusEof, failures);
}

static void test_smpte_timing(unsigned* failures) {
    uint8_t midi[40];
    size_t offset = put_header(midi, 0U, 1U, 0xE250U);
    offset = begin_track(midi, offset);
    const size_t track_start = offset;
    const uint8_t track[] = {0x00, 0xFF, 0x2F, 0x00};
    memcpy(midi + offset, track, sizeof(track));
    offset += sizeof(track);
    finish_track(midi, track_start, offset);

    MidiParser parser;
    expect_status(
        "smpte timing", parse_header(midi, offset, &parser), MidiParserStatusOk, failures);
    const MidiHeader* header = midi_parser_get_header(&parser);
    expect_u32("smpte mode", header->timing_mode, MidiTimingModeSmpte, failures);
    expect_u32("smpte ticks per second", header->ticks_per_second, 2400U, failures);
}

static void test_zero_division(unsigned* failures) {
    uint8_t midi[32];
    const size_t size = put_header(midi, 0U, 1U, 0U);
    MidiParser parser;
    expect_status(
        "zero division", parse_header(midi, size, &parser), MidiParserStatusBadHeader, failures);
}

static void test_malformed_varlen(unsigned* failures) {
    uint8_t midi[64];
    size_t offset = put_header(midi, 0U, 1U, 96U);
    offset = begin_track(midi, offset);
    const size_t track_start = offset;
    const uint8_t track[] = {0x81, 0x80, 0x80, 0x80, 0x00};
    memcpy(midi + offset, track, sizeof(track));
    offset += sizeof(track);
    finish_track(midi, track_start, offset);

    MidiParser parser;
    expect_status(
        "varlen header", parse_header(midi, offset, &parser), MidiParserStatusOk, failures);
    MidiEvent event;
    expect_status(
        "overlong varlen",
        midi_parser_next_event(&parser, &event),
        MidiParserStatusMalformedVarlen,
        failures);
}

static void test_unknown_meta_and_sysex_skip(unsigned* failures) {
    uint8_t midi[96];
    size_t offset = put_header(midi, 0U, 1U, 96U);
    offset = begin_track(midi, offset);
    const size_t track_start = offset;
    const uint8_t track[] = {
        0x00,
        0xFF,
        0x7F,
        0x02,
        0x01,
        0x02,
        0x00,
        0xF0,
        0x03,
        0x01,
        0x02,
        0x03,
        0x00,
        0xFF,
        0x2F,
        0x00,
    };
    memcpy(midi + offset, track, sizeof(track));
    offset += sizeof(track);
    finish_track(midi, track_start, offset);

    MidiParser parser;
    expect_status(
        "skip header", parse_header(midi, offset, &parser), MidiParserStatusOk, failures);
    MidiEvent event;
    expect_status(
        "meta skip status", midi_parser_next_event(&parser, &event), MidiParserStatusOk, failures);
    expect_event("meta skip event", &event, MidiEventTypeSkipped, 0U, 0U, failures);
    expect_status(
        "sysex skip status", midi_parser_next_event(&parser, &event), MidiParserStatusOk, failures);
    expect_event("sysex skip event", &event, MidiEventTypeSkipped, 0U, 0U, failures);
}

static void test_missing_running_status(unsigned* failures) {
    uint8_t midi[64];
    size_t offset = put_header(midi, 0U, 1U, 96U);
    offset = begin_track(midi, offset);
    const size_t track_start = offset;
    const uint8_t track[] = {0x00, 0x3C, 0x40};
    memcpy(midi + offset, track, sizeof(track));
    offset += sizeof(track);
    finish_track(midi, track_start, offset);

    MidiParser parser;
    expect_status(
        "missing running header",
        parse_header(midi, offset, &parser),
        MidiParserStatusOk,
        failures);
    MidiEvent event;
    expect_status(
        "missing running event",
        midi_parser_next_event(&parser, &event),
        MidiParserStatusMissingRunningStatus,
        failures);
}

static void test_truncated_track(unsigned* failures) {
    uint8_t midi[64];
    size_t offset = put_header(midi, 0U, 1U, 96U);
    memcpy(midi + offset, "MTrk", 4U);
    put_be32(midi + offset + 4U, 10U);
    offset += 8U;
    const uint8_t track[] = {0x00, 0x90, 0x3C};
    memcpy(midi + offset, track, sizeof(track));
    offset += sizeof(track);

    MidiParser parser;
    expect_status(
        "truncated header",
        parse_header(midi, offset, &parser),
        MidiParserStatusTruncated,
        failures);
}

int main(void) {
    unsigned failures = 0U;

    test_valid_format0(&failures);
    test_bad_magic(&failures);
    test_format1_merge(&failures);
    test_smpte_timing(&failures);
    test_zero_division(&failures);
    test_malformed_varlen(&failures);
    test_unknown_meta_and_sysex_skip(&failures);
    test_missing_running_status(&failures);
    test_truncated_track(&failures);

    if(failures != 0U) {
        printf("%u parser test(s) failed\n", failures);
        return 1;
    }

    printf("All parser tests passed\n");
    return 0;
}
