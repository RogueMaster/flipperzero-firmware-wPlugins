#include <datetime.h>
#include <stdbool.h>

#ifndef NAVIGO_I_H
#define NAVIGO_I_H

typedef struct {
    int transport_type;
    int transition;
    int service_provider;
    int station_group_id;
    int station_id;
    int station_sub_id;
    int location_gate;
    bool location_gate_available;
    int device;
    int door;
    int side;
    bool device_available;
    int route_number;
    bool route_number_available;
    int mission;
    bool mission_available;
    int vehicle_id;
    bool vehicle_id_available;
    int used_contract;
    bool used_contract_available;
    DateTime date;
} NavigoCardEvent;

typedef struct {
    int transport_type;
    int transition;
    int result;
    int service_provider;
    int station_group_id;
    int station_id;
    int station_sub_id;
    int device;
    bool device_available;
    int route_number;
    bool route_number_available;
    DateTime date;
} NavigoCardSpecialEvent;

typedef struct {
    int app_version;
    int country_num;
    int network_num;
    DateTime end_dt;
} NavigoCardEnv;

typedef struct {
    int card_status;
    int commercial_id;
} NavigoCardHolder;

typedef struct {
    int count;
    int relative_first_stamp_15mn;
    int struct_number;
    int last_load;
} NavigoCardContractCounter;

typedef struct {
    int tariff;
    int serial_number;
    bool serial_number_available;
    int pay_method;
    bool pay_method_available;
    double price_amount;
    bool price_amount_available;
    DateTime start_date;
    DateTime end_date;
    bool end_date_available;
    int zones[5];
    bool zones_available;
    DateTime sale_date;
    int sale_agent;
    int sale_device;
    int status;
    int authenticator;
    NavigoCardContractCounter counter;
    bool counter_present;
    bool present;
} NavigoCardContract;

typedef struct {
    NavigoCardEnv environment;
    NavigoCardHolder holder;
    NavigoCardContract contracts[4];
    NavigoCardEvent events[3];
    NavigoCardSpecialEvent special_events[3];
} NavigoCardData;

#endif // NAVIGO_I_H
