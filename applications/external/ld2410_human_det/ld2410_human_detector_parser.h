#pragma once

#include <stdint.h>
#include <stdbool.h>

#define LD2410_FRAME_HEADER_1 0xF4
#define LD2410_FRAME_HEADER_2 0xF3
#define LD2410_FRAME_HEADER_3 0xF2
#define LD2410_FRAME_HEADER_4 0xF1
#define LD2410_FRAME_END_1    0xF8
#define LD2410_FRAME_END_2    0xF7
#define LD2410_FRAME_END_3    0xF6
#define LD2410_FRAME_END_4    0xF5

typedef enum {
    LD2410TargetStateNoTarget = 0x00,
    LD2410TargetStateMoving = 0x01,
    LD2410TargetStateStatic = 0x02,
    LD2410TargetStateBoth = 0x03,
} LD2410TargetState;

typedef struct {
    LD2410TargetState target_state;
    uint16_t move_distance_cm;
    uint8_t move_energy;
    uint16_t static_distance_cm;
    uint8_t static_energy;
    uint16_t detection_distance_cm;
} LD2410Data;

typedef struct {
    uint8_t buffer[64];
    uint8_t index;
    bool header_found;
} LD2410Parser;

void ld2410_human_detector_parser_init(LD2410Parser* parser);
bool ld2410_human_detector_parser_push_byte(
    LD2410Parser* parser,
    uint8_t byte,
    LD2410Data* out_data);
