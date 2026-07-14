#include <datetime.h>
#include <stdbool.h>

#ifndef OPUS_I_H
#define OPUS_I_H

typedef struct {
    int service_provider;
    int result;
    int route_number;
    int route_direction;
    int location_id;
    int used_contract;
    bool simulation;
    DateTime date;
    DateTime first_stamp_date;
} OpusCardEvent;

typedef struct {
    int app_version;
    int country_num;
    int network_num;
    int issuer_id;
    bool card_status;
    bool card_utilisation;
    DateTime end_dt;
} OpusCardEnv;

typedef struct {
    int number;
    DateTime date;
} OpusCardHolderProfile;

typedef struct {
    DateTime birth_date;
    OpusCardHolderProfile profiles[4];
    int language;
} OpusCardHolder;

typedef struct {
    int provider;
    int tariff;
    DateTime start_date;
    DateTime end_date;
    int sale_agent;
    DateTime sale_date;
    bool inhibition;
    bool used;
    bool present;
} OpusCardContract;

typedef struct {
    OpusCardEnv environment;
    OpusCardHolder holder;
    OpusCardContract contracts[4];
    OpusCardEvent events[3];
} OpusCardData;

#endif // OPUS_I_H
