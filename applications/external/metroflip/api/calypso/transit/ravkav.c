#include "ravkav_lists.h"
#include "../../../metroflip_i.h"
#include "../../metroflip/metroflip_api.h"
#include "../calypso_util.h"
#include "ravkav_i.h"

const char* get_ravkav_issuer(int issuer) {
    if(RAVKAV_ISSUERS_LIST[issuer]) {
        return RAVKAV_ISSUERS_LIST[issuer];
    } else {
        // Return hex
        char* issuer_str = malloc(9 * sizeof(char));
        if(!issuer_str) {
            return "Unknown";
        }
        snprintf(issuer_str, 9, "0x%02X", issuer);
        return issuer_str;
    }
}

void show_ravkav_contract_info(RavKavCardContract* contract, FuriString* parsed_data) {
    // Core contract validity period
    if(contract->balance != 0.0f) {
        furi_string_cat_printf(parsed_data, "Balance: %.2f ILS\n", (double)contract->balance);
    }

    furi_string_cat_printf(parsed_data, "Valid from: ");
    locale_format_datetime_cat(parsed_data, &contract->start_date, false);
    if(contract->end_date_available) {
        furi_string_cat_printf(parsed_data, "\nto: ");
        locale_format_datetime_cat(parsed_data, &contract->end_date, false);
    }
    furi_string_cat_printf(parsed_data, "\n");

    // Issuer information
    furi_string_cat_printf(parsed_data, "Issuer: %s\n", get_ravkav_issuer(contract->provider));

    // Sale details
    furi_string_cat_printf(parsed_data, "Sold on: ");
    locale_format_datetime_cat(parsed_data, &contract->sale_date, false);
    furi_string_cat_printf(parsed_data, "\nSale device: %d\n", contract->sale_device);
    furi_string_cat_printf(parsed_data, "Sale number: %d\n", contract->sale_number);

    // Restriction details
    if(contract->restrict_code_available) {
        furi_string_cat_printf(parsed_data, "Restriction code: %d\n", contract->restrict_code);
    }
    if(contract->restrict_duration_available) {
        furi_string_cat_printf(
            parsed_data, "Restriction duration: \n%d minutes\n", contract->restrict_duration);
    }

    // Additional metadata
    furi_string_cat_printf(parsed_data, "Contract version: %d\n", contract->version);
    furi_string_cat_printf(parsed_data, "Journey interchanges flag: %d\n", contract->interchange);
}

void show_ravkav_environment_info(RavKavCardEnv* environment, FuriString* parsed_data) {
    // Validity information
    furi_string_cat_printf(parsed_data, "End of validity:\n");
    locale_format_datetime_cat(parsed_data, &environment->end_dt, false);
    furi_string_cat_printf(parsed_data, "\n");

    // Issue date
    furi_string_cat_printf(parsed_data, "Issue date:\n");
    locale_format_datetime_cat(parsed_data, &environment->issue_dt, false);
    furi_string_cat_printf(parsed_data, "\n");

    // Application details
    furi_string_cat_printf(parsed_data, "App version: %d\n", environment->app_version);
    furi_string_cat_printf(parsed_data, "App number: %d\n", environment->app_num);

    // Payment and network details
    furi_string_cat_printf(parsed_data, "Pay method: %d\n", environment->pay_method);
}

void show_ravkav_event_info(RavKavCardEvent* event, FuriString* parsed_data) {
    // Essential details first
    if(event->location_id_available) {
        furi_string_cat_printf(parsed_data, "Location:\n%d\n", event->location_id);
    }
    furi_string_cat_printf(parsed_data, "Time:\n");
    locale_format_datetime_cat(parsed_data, &event->time, true);
    furi_string_cat_printf(parsed_data, "\n");
    furi_string_cat_printf(parsed_data, "Service provider: %d\n", event->service_provider);

    // Fare and route-related information
    if(event->fare_code_available) {
        furi_string_cat_printf(parsed_data, "Fare Code: %d\n", event->fare_code);
    }
    if(event->route_number_available) {
        furi_string_cat_printf(parsed_data, "Route Number: %d\n", event->route_number);
    }
    if(event->debit_amount_available) {
        furi_string_cat_printf(parsed_data, "Debit Amount: %.2f\n", (double)event->debit_amount);
    }
    if(event->stop_en_route_available) {
        furi_string_cat_printf(parsed_data, "Stop en Route: %d\n", event->stop_en_route);
    }

    // Remaining event metadata
    furi_string_cat_printf(parsed_data, "Area ID: %d\n", event->area_id);
    furi_string_cat_printf(parsed_data, "Contract ID: %d\n", event->contract_id);
    furi_string_cat_printf(parsed_data, "Event Type: %d\n", event->type);
    furi_string_cat_printf(parsed_data, "Event Interchange Flag: %d\n", event->interchange_flag);
}
