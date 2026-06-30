#include <stdlib.h>
#include "../../metroflip/metroflip_api.h"

CalypsoApp* get_ravkav_contract_structure() {
    /*
    En1545FixedInteger("Version", 3),
    En1545FixedInteger.date(CONTRACT_START),
    En1545FixedInteger(CONTRACT_PROVIDER, 8),
    En1545FixedInteger(CONTRACT_TARIFF, 11),
    En1545FixedInteger.date(CONTRACT_SALE),
    En1545FixedInteger(CONTRACT_SALE_DEVICE, 12),
    En1545FixedInteger("ContractSaleNumber", 10),
    En1545FixedInteger(CONTRACT_INTERCHANGE, 1),
    En1545Bitmap(
            En1545FixedInteger(CONTRACT_UNKNOWN_A, 5),
            En1545FixedInteger(CONTRACT_RESTRICT_CODE, 5),
            En1545FixedInteger("ContractRestrictDuration", 6),
            En1545FixedInteger.date(CONTRACT_END),
            En1545FixedInteger(CONTRACT_DURATION, 8),
            En1545FixedInteger(CONTRACT_UNKNOWN_B, 32),
            En1545FixedInteger(CONTRACT_UNKNOWN_C, 6),
            En1545FixedInteger(CONTRACT_UNKNOWN_D, 32),
            En1545FixedInteger(CONTRACT_UNKNOWN_E, 32)
    )
    */
    CalypsoApp* RavKavContractStructure = malloc(sizeof(CalypsoApp));

    if(!RavKavContractStructure) {
        return NULL;
    }

    int app_elements_count = 9;

    RavKavContractStructure->type = CALYPSO_APP_CONTRACT;
    RavKavContractStructure->container = malloc(sizeof(CalypsoContainerElement));
    RavKavContractStructure->container->elements =
        malloc(app_elements_count * sizeof(CalypsoElement));
    RavKavContractStructure->container->size = app_elements_count;

    RavKavContractStructure->container->elements[0] =
        make_calypso_final_element("ContractVersion", 3, "Version", CALYPSO_FINAL_TYPE_NUMBER);
    RavKavContractStructure->container->elements[1] =
        make_calypso_final_element("ContractStartDate", 14, "Start Date", CALYPSO_FINAL_TYPE_DATE);
    RavKavContractStructure->container->elements[2] =
        make_calypso_final_element("ContractProvider", 8, "Provider", CALYPSO_FINAL_TYPE_UNKNOWN);
    RavKavContractStructure->container->elements[3] =
        make_calypso_final_element("ContractTariff", 11, "Tariff", CALYPSO_FINAL_TYPE_TARIFF);
    RavKavContractStructure->container->elements[4] =
        make_calypso_final_element("ContractSaleDate", 14, "Sale Date", CALYPSO_FINAL_TYPE_DATE);
    RavKavContractStructure->container->elements[5] = make_calypso_final_element(
        "ContractSaleDevice", 12, "Sale Device", CALYPSO_FINAL_TYPE_UNKNOWN);
    RavKavContractStructure->container->elements[6] = make_calypso_final_element(
        "ContractSaleNumber", 10, "Sale Number", CALYPSO_FINAL_TYPE_UNKNOWN);
    RavKavContractStructure->container->elements[7] = make_calypso_final_element(
        "ContractInterchange", 1, "Contract Interchange", CALYPSO_FINAL_TYPE_UNKNOWN);
    RavKavContractStructure->container->elements[8] = make_calypso_bitmap_element(
        "Contract",
        9,
        (CalypsoElement[]){
            make_calypso_final_element(
                "ContractUnknownA", 5, "Unknown A", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "ContractRestrictCode", 5, "Restrict Code", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "ContractRestrictDuration", 6, "Restrict Duration", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element("ContractEndDate", 14, "End Date", CALYPSO_FINAL_TYPE_DATE),
            make_calypso_final_element(
                "ContractDuration", 8, "Duration", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "ContractUnknownB", 32, "Unknown B", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "ContractUnknownC", 6, "Unknown C", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "ContractUnknownD", 32, "Unknown D", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "ContractUnknownE", 32, "Unknown E", CALYPSO_FINAL_TYPE_UNKNOWN),

        });

    return RavKavContractStructure;
}

CalypsoApp* get_ravkav_env_holder_structure() {
    /*
    En1545FixedInteger(ENV_VERSION_NUMBER, 3),
    En1545FixedInteger(ENV_NETWORK_ID, 20),
    En1545FixedInteger(ENV_UNKNOWN_A, 26),
    En1545FixedInteger.date(ENV_APPLICATION_ISSUE),
    En1545FixedInteger.date(ENV_APPLICATION_VALIDITY_END),
    En1545FixedInteger("PayMethod", 3),
    En1545FixedInteger.dateBCD(HOLDER_BIRTH_DATE),
    En1545FixedHex(ENV_UNKNOWN_B, 44),
    En1545FixedInteger(HOLDER_ID_NUMBER, 30)
    */

    CalypsoApp* RavKavEnvHolderStructure = malloc(sizeof(CalypsoApp));
    if(!RavKavEnvHolderStructure) {
        return NULL;
    }

    int app_elements_count = 8;

    RavKavEnvHolderStructure->type = CALYPSO_APP_ENV_HOLDER;
    RavKavEnvHolderStructure->container = malloc(sizeof(CalypsoContainerElement));
    RavKavEnvHolderStructure->container->elements =
        malloc(app_elements_count * sizeof(CalypsoElement));
    RavKavEnvHolderStructure->container->size = app_elements_count;

    RavKavEnvHolderStructure->container->elements[0] = make_calypso_final_element(
        "EnvApplicationVersionNumber",
        3,
        "Numéro de version de l’application Billettique",
        CALYPSO_FINAL_TYPE_NUMBER);
    RavKavEnvHolderStructure->container->elements[1] = make_calypso_final_element(
        "EnvCountry", 12, "Identification du réseau", CALYPSO_FINAL_TYPE_NUMBER);
    RavKavEnvHolderStructure->container->elements[2] =
        make_calypso_final_element("EnvIssuer", 8, "Issuer", CALYPSO_FINAL_TYPE_NUMBER);
    RavKavEnvHolderStructure->container->elements[3] = make_calypso_final_element(
        "EnvApplicationNumber", 26, "Application Number", CALYPSO_FINAL_TYPE_NUMBER);
    RavKavEnvHolderStructure->container->elements[4] =
        make_calypso_final_element("EnvDateOfIssue", 14, "Date of issue", CALYPSO_FINAL_TYPE_DATE);
    RavKavEnvHolderStructure->container->elements[5] =
        make_calypso_final_element("EnvEndValidity", 14, "End Validity", CALYPSO_FINAL_TYPE_DATE);
    RavKavEnvHolderStructure->container->elements[6] =
        make_calypso_final_element("   ", 3, "Payment method", CALYPSO_FINAL_TYPE_PAY_METHOD),
    RavKavEnvHolderStructure->container->elements[7] = make_calypso_final_element(
        "EnvBCDDate", 32, "Birth Date", CALYPSO_FINAL_TYPE_DATE); // might do this one later

    return RavKavEnvHolderStructure;
}

CalypsoApp* get_ravkav_event_structure() {
    /*
    En1545FixedInteger("EventVersion", 3),
    En1545FixedInteger(EVENT_SERVICE_PROVIDER, 8),
    En1545FixedInteger(EVENT_CONTRACT_POINTER, 4),
    En1545FixedInteger(EVENT_CODE, 8),
    En1545FixedInteger.dateTime(EVENT),
    En1545FixedInteger("EventTransferFlag", 1),
    En1545FixedInteger.dateTime(EVENT_FIRST_STAMP),
    En1545FixedInteger("EventContractPrefs", 32),
    En1545Bitmap(
            En1545FixedInteger(EVENT_LOCATION_ID, 16),
            En1545FixedInteger(EVENT_ROUTE_NUMBER, 16),
            En1545FixedInteger("StopEnRoute", 8),
            En1545FixedInteger(EVENT_UNKNOWN_A, 12),
            En1545FixedInteger(EVENT_VEHICLE_ID, 14),
            En1545FixedInteger(EVENT_UNKNOWN_B, 4),
            En1545FixedInteger(EVENT_UNKNOWN_C, 8)
    ),
    En1545Bitmap(
            En1545Container(
                    En1545FixedInteger("RouteSystem", 10),
                    En1545FixedInteger("FareCode", 8),
                    En1545FixedInteger(EVENT_PRICE_AMOUNT, 16)
            ),
            En1545FixedInteger(EVENT_UNKNOWN_D, 32),
            En1545FixedInteger(EVENT_UNKNOWN_E, 32)
    )
    */
    //  EventTime EventVehicle
    CalypsoApp* RavKavEventStructure = malloc(sizeof(CalypsoApp));

    if(!RavKavEventStructure) {
        return NULL;
    }

    int app_elements_count = 11;

    RavKavEventStructure->type = CALYPSO_APP_EVENT;
    RavKavEventStructure->container = malloc(sizeof(CalypsoContainerElement));
    RavKavEventStructure->container->elements =
        malloc(app_elements_count * sizeof(CalypsoElement));
    RavKavEventStructure->container->size = app_elements_count;

    RavKavEventStructure->container->elements[0] =
        make_calypso_final_element("EventVersion", 3, "Version Number", CALYPSO_FINAL_TYPE_NUMBER);
    RavKavEventStructure->container->elements[1] = make_calypso_final_element(
        "EventServiceProvider", 8, "Service provider", CALYPSO_FINAL_TYPE_UNKNOWN);
    RavKavEventStructure->container->elements[2] = make_calypso_final_element(
        "EventContractID", 4, "Contract ID", CALYPSO_FINAL_TYPE_UNKNOWN);
    RavKavEventStructure->container->elements[3] =
        make_calypso_final_element("EventAreaID", 4, "Area ID", CALYPSO_FINAL_TYPE_UNKNOWN);
    RavKavEventStructure->container->elements[4] =
        make_calypso_final_element("EventType", 4, "Event Type", CALYPSO_FINAL_TYPE_UNKNOWN);
    RavKavEventStructure->container->elements[5] =
        make_calypso_final_element("EventTime", 30, "Event Time", CALYPSO_FINAL_TYPE_DATE);
    RavKavEventStructure->container->elements[6] = make_calypso_final_element(
        "EventInterchangeFlag", 1, "Event Interchange flag", CALYPSO_FINAL_TYPE_UNKNOWN);
    RavKavEventStructure->container->elements[7] = make_calypso_final_element(
        "EventFirstTime", 30, "Event First Time", CALYPSO_FINAL_TYPE_DATE);
    RavKavEventStructure->container->elements[8] =
        make_calypso_final_element("EventUnknownA", 32, "Unknown A", CALYPSO_FINAL_TYPE_UNKNOWN);
    RavKavEventStructure->container->elements[9] = make_calypso_bitmap_element(
        "Location",
        7,
        (CalypsoElement[]){
            make_calypso_final_element(
                "EventLocationID", 16, "Location ID", CALYPSO_FINAL_TYPE_NUMBER),
            make_calypso_final_element(
                "EVentRouteNumber", 16, "Route Number", CALYPSO_FINAL_TYPE_NUMBER),
            make_calypso_final_element(
                "EventStopEnRoute", 8, "Stop En Route", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventUnknownB", 12, "Unknown B", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventVehicleID", 14, "Vehicle ID", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element("EventUnknownC", 4, "Unknown C", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventUnknownD", 8, "Unknown D", CALYPSO_FINAL_TYPE_UNKNOWN)});
    RavKavEventStructure->container->elements[10] = make_calypso_bitmap_element(
        "EventExtension",
        3,
        (CalypsoElement[]){
            make_calypso_container_element(
                "EventData",
                3,
                (CalypsoElement[]){
                    make_calypso_final_element(
                        "EventRouteSystem", 10, "Route System", CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "EventfareCode", 8, "fare Code", CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "EventDebitAmount", 16, "Debit Amount", CALYPSO_FINAL_TYPE_UNKNOWN),
                }),
            make_calypso_final_element(
                "EventUnknownE", 32, "Unknown C", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventUnknownF", 32, "Unknown D", CALYPSO_FINAL_TYPE_UNKNOWN),
        });
    return RavKavEventStructure;
}
