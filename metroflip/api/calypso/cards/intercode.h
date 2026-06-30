#include "../calypso_util.h"

#ifndef INTERCODE_STRUCTURES_H
#define INTERCODE_STRUCTURES_H

const char* get_intercode_string_transition_type(int transition);

const char* get_intercode_string_transport_type(int type);

const char* get_intercode_string_pay_method(int pay_method);

const char* get_intercode_string_event_result(int result);

const char* get_intercode_string_version(int version);

int get_intercode_string_subversion(int version);

const char* get_intercode_string_holder_type(int card_status);

bool is_intercode_string_holder_linked(int card_status);

const char* get_intercode_string_contract_status(int status);

typedef enum {
    URBAN_BUS = 1,
    INTERURBAN_BUS = 2,
    METRO = 3,
    TRAM = 4,
    COMMUTER_TRAIN = 5,
    WATERBORNE_VEHICLE = 6,
    TOLL = 7,
    PARKING = 8,
    TAXI = 9,
    HIGH_SPEED_TRAIN = 10,
    RURAL_BUS = 11,
    EXPRESS_COMMUTER_TRAIN = 12,
    PARA_TRANSIT = 13,
    SELF_DRIVE_VEHICLE = 14,
    COACH = 15,
    LOCOMOTIVE = 16,
    POWERED_MOTOR_VEHICLE = 17,
    TRAILER = 18,
    REGIONAL_TRAIN = 19,
    INTER_CITY = 20,
    FUNICULAR = 21,
    CABLE_CAR = 22,
    SELF_SERVICE_BICYCLE = 23,
    CAR_SHARING = 24,
    CAR_POOLING = 25,
} INTERCODE_TRANSPORT_TYPE;

typedef enum {
    ENTRY = 1,
    EXIT = 2,
    PASSAGE = 3,
    CHECKPOINT_INSPECTION = 4,
    AUTONOMOUS = 5,
    INTERCHANGE = 6,
    VALIDATION = 7,
    PRESENCE_DETECTED = 8,
} INTERCODE_USER_ACTION;

#endif // INTERCODE_STRUCTURES_H
