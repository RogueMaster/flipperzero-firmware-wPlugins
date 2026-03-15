#include "opus_lists.h"
#include "../../../metroflip_i.h"
#include "../../metroflip/metroflip_api.h"
#include "../calypso_util.h"
#include "opus_i.h"

const char* get_opus_service_provider(int provider) {
    switch(provider) {
    case 0x01:
    case 0x02:
        return "STM";
    case 0x03:
        return "RTL";
    case 0x04:
        return "RTM";
    case 0x05:
        return "RTC";
    case 0x06:
        return "STL";
    case 0x10:
        return "STLevis";
    case 0x20:
        return "Chrono";
    default: {
        char* provider_str = malloc(10 * sizeof(char));
        if(!provider_str) {
            return "Unknown";
        }
        snprintf(provider_str, 10, "0x%02X", provider);
        return provider_str;
    }
    }
}

const char* get_opus_transport_type(int location_id) {
    switch(location_id) {
    case 0x65:
        return "Bus";
    case 0xc9:
        return "Metro";
    default: {
        char* location_str = malloc(9 * sizeof(char));
        if(!location_str) {
            return "Unknown";
        }
        snprintf(location_str, 9, "0x%02X", location_id);
        return location_str;
    }
    }
}

const char* get_opus_transport_line(int route_number) {
    if(OPUS_LINES_LIST[route_number]) {
        return OPUS_LINES_LIST[route_number];
    } else {
        // Return hex
        char* route_str = malloc(9 * sizeof(char));
        if(!route_str) {
            return "Unknown";
        }
        snprintf(route_str, 9, "0x%02X", route_number);
        return route_str;
    }
}

const char* get_opus_tariff(int tariff) {
    switch(tariff) {
    case 0xb1:
        return "Monthly";
    case 0xb2:
    case 0xc9:
        return "Weekly";
    case 0x1c7:
        return "Single Trips";
    case 0xa34:
        return "Monthly Student";
    case 0xa3e:
        return "Weekly";
    default: {
        char* tariff_str = malloc(9 * sizeof(char));
        if(!tariff_str) {
            return "Unknown";
        }
        snprintf(tariff_str, 9, "0x%02X", tariff);
        return tariff_str;
    }
    }
}

void show_opus_event_info(
    OpusCardEvent* event,
    OpusCardContract* contracts,
    FuriString* parsed_data) {
    UNUSED(contracts);
    furi_string_cat_printf(
        parsed_data,
        "%s %s\n",
        get_opus_transport_type(event->location_id),
        get_opus_transport_line(event->route_number));
    furi_string_cat_printf(
        parsed_data, "Transporter: %s\n", get_opus_service_provider(event->service_provider));
    furi_string_cat_printf(parsed_data, "Result: %d\n", event->result);
    furi_string_cat_printf(
        parsed_data,
        "Contract: %d - %s\n",
        event->used_contract,
        get_opus_tariff(contracts[event->used_contract - 1].tariff));
    furi_string_cat_printf(parsed_data, "Simulation: %s\n", event->simulation ? "true" : "false");
    furi_string_cat_printf(parsed_data, "Date: ");
    locale_format_datetime_cat(parsed_data, &event->date, true);
    furi_string_cat_printf(parsed_data, "\nFirst stamp: ");
    locale_format_datetime_cat(parsed_data, &event->first_stamp_date, true);
    furi_string_cat_printf(parsed_data, "\n");
}

void show_opus_contract_info(OpusCardContract* contract, FuriString* parsed_data) {
    furi_string_cat_printf(parsed_data, "Type: %s\n", get_opus_tariff(contract->tariff));
    furi_string_cat_printf(
        parsed_data, "Provider: %s\n", get_opus_service_provider(contract->provider));
    furi_string_cat_printf(parsed_data, "Valid from: ");
    locale_format_datetime_cat(parsed_data, &contract->start_date, false);
    furi_string_cat_printf(parsed_data, "\nto: ");
    locale_format_datetime_cat(parsed_data, &contract->end_date, false);
    furi_string_cat_printf(parsed_data, "\nSold on: ");
    locale_format_datetime_cat(parsed_data, &contract->sale_date, true);
    furi_string_cat_printf(
        parsed_data, "\nSales Agent: %s\n", get_opus_service_provider(contract->sale_agent));
    if(contract->inhibition) {
        furi_string_cat_printf(parsed_data, "Contract inhibited\n");
    }
    furi_string_cat_printf(parsed_data, "Used: %s\n", contract->used ? "true" : "false");
}

void show_opus_environment_info(
    OpusCardEnv* environment,
    OpusCardHolder* holder,
    FuriString* parsed_data) {
    furi_string_cat_printf(parsed_data, "App Version: %d\n", environment->app_version);
    furi_string_cat_printf(
        parsed_data, "Country: %s\n", get_country_string(environment->country_num));
    furi_string_cat_printf(
        parsed_data,
        "Network: %s\n",
        get_network_string(guess_card_type(environment->country_num, environment->network_num)));
    furi_string_cat_printf(parsed_data, "End of validity:\n");
    locale_format_datetime_cat(parsed_data, &environment->end_dt, false);
    furi_string_cat_printf(parsed_data, "\nIssuer: %d\n", environment->issuer_id);
    furi_string_cat_printf(
        parsed_data, "Card status: %s\n", environment->card_status ? "true" : "false");
    furi_string_cat_printf(
        parsed_data,
        "Card utilisation: %s\n",
        environment->card_utilisation ? "Used" : "Not used");
    furi_string_cat_printf(parsed_data, "Holder birth date: ");
    locale_format_datetime_cat(parsed_data, &holder->birth_date, false);
    furi_string_cat_printf(parsed_data, "\n");
}
