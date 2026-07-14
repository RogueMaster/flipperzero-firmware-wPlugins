#include <datetime.h>
#include <stdbool.h>

#ifndef RAVKAV_I_H
#define RAVKAV_I_H

typedef struct {
    int version;
    int provider;
    int tariff;
    int sale_device;
    int sale_number;
    DateTime start_date;
    DateTime sale_date;
    int interchange;

    //bitmap elements
    int restrict_code;
    int restrict_duration;
    DateTime end_date;
    bool restrict_code_available;
    bool restrict_duration_available;
    bool end_date_available;

    bool present;
    float balance;
} RavKavCardContract;

typedef struct {
    int event_version;
    int service_provider;
    int contract_id;
    int area_id;
    int type;
    DateTime time;
    int interchange_flag;

    int location_id;
    bool location_id_available;
    int route_number;
    bool route_number_available;
    int stop_en_route;
    bool stop_en_route_available;
    int vehicle_id;
    bool vehicle_id_available;

    int fare_code;
    bool fare_code_available;
    float debit_amount;
    bool debit_amount_available;
} RavKavCardEvent;

typedef struct {
    int app_version;
    int network_num;
    int app_num;
    int pay_method;
    DateTime end_dt;
    DateTime issue_dt;
} RavKavCardEnv;

typedef struct {
    RavKavCardEnv environment;
    RavKavCardContract contracts[4];
    RavKavCardEvent events[3];
} RavKavCardData;

#endif // RAVKAV_I_H
