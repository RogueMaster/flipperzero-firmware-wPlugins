#include <stdlib.h>
#include "intercode.h"
#include "../../metroflip/metroflip_api.h"

CalypsoApp* get_intercode_structure_env_holder() {
    CalypsoApp* IntercodeEnvHolderStructure = malloc(sizeof(CalypsoApp));
    if(!IntercodeEnvHolderStructure) {
        return NULL;
    }

    int app_elements_count = 3;

    IntercodeEnvHolderStructure->type = CALYPSO_APP_ENV_HOLDER;
    IntercodeEnvHolderStructure->container = malloc(sizeof(CalypsoContainerElement));
    IntercodeEnvHolderStructure->container->elements =
        malloc(app_elements_count * sizeof(CalypsoElement));
    IntercodeEnvHolderStructure->container->size = app_elements_count;

    IntercodeEnvHolderStructure->container->elements[0] = make_calypso_final_element(
        "EnvApplicationVersionNumber",
        6,
        "Numéro de version de l’application Billettique",
        CALYPSO_FINAL_TYPE_NUMBER);
    IntercodeEnvHolderStructure->container->elements[1] = make_calypso_bitmap_element(
        "Env",
        7,
        (CalypsoElement[]){
            make_calypso_final_element(
                "EnvNetworkId", 24, "Identification du réseau", CALYPSO_FINAL_TYPE_NUMBER),
            make_calypso_final_element(
                "EnvApplicationIssuerId",
                8,
                "Identification de l’émetteur de l’application",
                CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EnvApplicationValidityEndDate",
                14,
                "Date de fin de validité de l’application",
                CALYPSO_FINAL_TYPE_DATE),
            make_calypso_final_element(
                "EnvPayMethod", 11, "Code mode de paiement", CALYPSO_FINAL_TYPE_PAY_METHOD),
            make_calypso_final_element(
                "EnvAuthenticator",
                16,
                "Code de contrôle de l’intégrité des données",
                CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EnvSelectList",
                32,
                "Bitmap de tableau de paramètre multiple",
                CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_container_element(
                "EnvData",
                2,
                (CalypsoElement[]){
                    make_calypso_final_element(
                        "EnvDataCardStatus", 1, "Statut de la carte", CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "EnvData2", 0, "Données complémentaires", CALYPSO_FINAL_TYPE_UNKNOWN),
                }),
        });
    IntercodeEnvHolderStructure->container->elements[2] = make_calypso_bitmap_element(
        "Holder",
        8,
        (CalypsoElement[]){
            make_calypso_bitmap_element(
                "HolderName",
                2,
                (CalypsoElement[]){
                    make_calypso_final_element(
                        "HolderSurname", 85, "Nom du porteur", CALYPSO_FINAL_TYPE_STRING),
                    make_calypso_final_element(
                        "HolderForename",
                        85,
                        "Prénom de naissance du porteur",
                        CALYPSO_FINAL_TYPE_STRING),
                }),
            make_calypso_bitmap_element(
                "HolderBirth",
                2,
                (CalypsoElement[]){
                    make_calypso_final_element(
                        "HolderBirthDate", 32, "Date de naissance", CALYPSO_FINAL_TYPE_DATE),
                    make_calypso_final_element(
                        "HolderBirthPlace",
                        115,
                        "Lieu de naissance (23 caractères)",
                        CALYPSO_FINAL_TYPE_STRING),
                }),
            make_calypso_final_element(
                "HolderBirthName",
                85,
                "Nom de naissance du porteur (17 caractères)",
                CALYPSO_FINAL_TYPE_STRING),
            make_calypso_final_element(
                "HolderIdNumber", 32, "Identifiant Porteur", CALYPSO_FINAL_TYPE_NUMBER),
            make_calypso_final_element(
                "HolderCountryAlpha", 24, "Pays du titulaire", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "HolderCompany", 32, "Société du titulaire", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_bitmap_element(
                "HolderProfiles",
                4,
                (CalypsoElement[]){
                    make_calypso_bitmap_element(
                        "HolderProfileBitmap",
                        3,
                        (CalypsoElement[]){
                            make_calypso_final_element(
                                "HolderNetworkId", 24, "Réseau", CALYPSO_FINAL_TYPE_UNKNOWN),
                            make_calypso_final_element(
                                "HolderProfileNumber",
                                8,
                                "Numéro du statut",
                                CALYPSO_FINAL_TYPE_NUMBER),
                            make_calypso_final_element(
                                "HolderProfileDate",
                                14,
                                "Date de fin de validité du statut",
                                CALYPSO_FINAL_TYPE_DATE),
                        }),
                }),
            make_calypso_bitmap_element(
                "HolderData",
                12,
                (CalypsoElement[]){
                    make_calypso_final_element(
                        "HolderDataCardStatus", 4, "Type de carte", CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "HolderDataTeleReglement", 4, "Télérèglement", CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "HolderDataResidence", 17, "Ville du domicile", CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "HolderDataCommercialID", 6, "Produit carte", CALYPSO_FINAL_TYPE_NUMBER),
                    make_calypso_final_element(
                        "HolderDataWorkPlace", 17, "Lieu de travail", CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "HolderDataStudyPlace", 17, "Lieu d'étude", CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "HolderDataSaleDevice",
                        16,
                        "Numéro logique de SAM",
                        CALYPSO_FINAL_TYPE_NUMBER),
                    make_calypso_final_element(
                        "HolderDataAuthenticator", 16, "Signature", CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "HolderDataProfileStartDate1",
                        14,
                        "Date de début de validité du statut",
                        CALYPSO_FINAL_TYPE_DATE),
                    make_calypso_final_element(
                        "HolderDataProfileStartDate2",
                        14,
                        "Date de début de validité du statut",
                        CALYPSO_FINAL_TYPE_DATE),
                    make_calypso_final_element(
                        "HolderDataProfileStartDate3",
                        14,
                        "Date de début de validité du statut",
                        CALYPSO_FINAL_TYPE_DATE),
                    make_calypso_final_element(
                        "HolderDataProfileStartDate4",
                        14,
                        "Date de début de validité du statut",
                        CALYPSO_FINAL_TYPE_DATE),
                }),
        });

    return IntercodeEnvHolderStructure;
}

CalypsoApp* get_intercode_structure_contract() {
    CalypsoApp* IntercodeContractStructure = malloc(sizeof(CalypsoApp));
    if(!IntercodeContractStructure) {
        return NULL;
    }

    int app_elements_count = 1;

    IntercodeContractStructure->type = CALYPSO_APP_CONTRACT;
    IntercodeContractStructure->container = malloc(sizeof(CalypsoContainerElement));
    IntercodeContractStructure->container->elements =
        malloc(app_elements_count * sizeof(CalypsoElement));
    IntercodeContractStructure->container->size = app_elements_count;

    IntercodeContractStructure->container->elements[0] = make_calypso_bitmap_element(
        "Contract",
        20,
        (CalypsoElement[]){
            make_calypso_final_element(
                "ContractNetworkId", 24, "Identification du réseau", CALYPSO_FINAL_TYPE_UNKNOWN),

            make_calypso_final_element(
                "ContractProvider",
                8,
                "Identification de l’exploitant",
                CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "ContractTariff", 16, "Code tarif", CALYPSO_FINAL_TYPE_TARIFF),

            make_calypso_final_element(
                "ContractSerialNumber", 32, "Numéro TCN", CALYPSO_FINAL_TYPE_UNKNOWN),

            make_calypso_bitmap_element(
                "ContractCustomerInfoBitmap",
                2,
                (CalypsoElement[]){
                    make_calypso_final_element(
                        "ContractCustomerProfile",
                        6,
                        "Statut du titulaire ou Taux de réduction applicable",
                        CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "ContractCustomerNumber",
                        32,
                        "Numéro de client",
                        CALYPSO_FINAL_TYPE_UNKNOWN),
                }),

            make_calypso_bitmap_element(
                "ContractPassengerInfoBitmap",
                2,
                (CalypsoElement[]){
                    make_calypso_final_element(
                        "ContractPassengerClass",
                        8,
                        "Classe de service des voyageurs",
                        CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "ContractPassengerTotal",
                        8,
                        "Nombre total de voyageurs",
                        CALYPSO_FINAL_TYPE_UNKNOWN),
                }),

            make_calypso_final_element(
                "ContractVehicleClassAllowed",
                6,
                "Classes de véhicule autorisé",
                CALYPSO_FINAL_TYPE_UNKNOWN),

            make_calypso_final_element(
                "ContractPaymentPointer",
                32,
                "Pointeurs sur les événements de paiement",
                CALYPSO_FINAL_TYPE_UNKNOWN),

            make_calypso_final_element(
                "ContractPayMethod", 11, "Code mode de paiement", CALYPSO_FINAL_TYPE_PAY_METHOD),

            make_calypso_final_element(
                "ContractServices", 16, "Services autorisés", CALYPSO_FINAL_TYPE_UNKNOWN),

            make_calypso_final_element(
                "ContractPriceAmount", 16, "Montant total", CALYPSO_FINAL_TYPE_AMOUNT),

            make_calypso_final_element(
                "ContractPriceUnit", 16, "Code de monnaie", CALYPSO_FINAL_TYPE_UNKNOWN),

            make_calypso_bitmap_element(
                "ContractRestrictionBitmap",
                7,
                (CalypsoElement[]){
                    make_calypso_final_element(
                        "ContractRestrictStart", 11, "", CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "ContractRestrictEnd", 11, "", CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "ContractRestrictDay", 8, "", CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "ContractRestrictTimeCode", 8, "", CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "ContractRestrictCode",
                        8,
                        "Code de restriction",
                        CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "ContractRestrictProduct",
                        16,
                        "Produit de restriction",
                        CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "ContractRestrictLocation",
                        16,
                        "Référence du lieu de restriction",
                        CALYPSO_FINAL_TYPE_UNKNOWN),
                }),

            make_calypso_bitmap_element(
                "ContractValidityInfoBitmap",
                9,
                (CalypsoElement[]){
                    make_calypso_final_element(
                        "ContractValidityStartDate",
                        14,
                        "Date de début de validité",
                        CALYPSO_FINAL_TYPE_DATE),
                    make_calypso_final_element(
                        "ContractValidityStartTime",
                        11,
                        "Heure de début de validité",
                        CALYPSO_FINAL_TYPE_TIME),
                    make_calypso_final_element(
                        "ContractValidityEndDate",
                        14,
                        "Date de fin de validité",
                        CALYPSO_FINAL_TYPE_DATE),
                    make_calypso_final_element(
                        "ContractValidityEndTime",
                        11,
                        "Heure de fin de validité",
                        CALYPSO_FINAL_TYPE_TIME),
                    make_calypso_final_element(
                        "ContractValidityDuration",
                        8,
                        "Durée de validité",
                        CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "ContractValidityLimiteDate",
                        14,
                        "Date limite de première utilisation",
                        CALYPSO_FINAL_TYPE_DATE),
                    make_calypso_final_element(
                        "ContractValidityZones",
                        8,
                        "Numéros des zones autorisées",
                        CALYPSO_FINAL_TYPE_ZONES),
                    make_calypso_final_element(
                        "ContractValidityJourneys",
                        16,
                        "Nombre de voyages autorisés",
                        CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "ContractPeriodJourneys",
                        16,
                        "Nombre de voyages autorisés par période",
                        CALYPSO_FINAL_TYPE_UNKNOWN),
                }),

            make_calypso_bitmap_element(
                "ContractJourneyData",
                8,
                (CalypsoElement[]){
                    make_calypso_final_element(
                        "ContractJourneyOrigin",
                        16,
                        "Code lieu d’origine",
                        CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "ContractJourneyDestination",
                        16,
                        "Code lieu de destination",
                        CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "ContractJourneyRouteNumbers",
                        16,
                        "Numéros des lignes autorisées",
                        CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "ContractJourneyRouteVariants",
                        8,
                        "Variantes aux numéros des lignes autorisées",
                        CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "ContractJourneyRun", 16, "Référence du voyage", CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "ContractJourneyVia", 16, "Code lieu du via", CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "ContractJourneyDistance", 16, "Distance", CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "ContractJourneyInterchanges",
                        8,
                        "Nombre de correspondances autorisées",
                        CALYPSO_FINAL_TYPE_UNKNOWN),
                }),

            make_calypso_bitmap_element(
                "ContractSaleData",
                4,
                (CalypsoElement[]){
                    make_calypso_final_element(
                        "ContractValiditySaleDate", 14, "Date de vente", CALYPSO_FINAL_TYPE_DATE),
                    make_calypso_final_element(
                        "ContractValiditySaleTime", 11, "Heure de vente", CALYPSO_FINAL_TYPE_TIME),
                    make_calypso_final_element(
                        "ContractValiditySaleAgent",
                        8,
                        "Identification de l’exploitant de vente",
                        CALYPSO_FINAL_TYPE_SERVICE_PROVIDER),
                    make_calypso_final_element(
                        "ContractValiditySaleDevice",
                        16,
                        "Identification du terminal de vente",
                        CALYPSO_FINAL_TYPE_UNKNOWN),
                }),

            make_calypso_final_element(
                "ContractStatus", 8, "État du contrat", CALYPSO_FINAL_TYPE_UNKNOWN),

            make_calypso_final_element(
                "ContractLoyaltyPoints",
                16,
                "Nombre de points de fidélité",
                CALYPSO_FINAL_TYPE_NUMBER),

            make_calypso_final_element(
                "ContractAuthenticator",
                16,
                "Code de contrôle de l’intégrité des données",
                CALYPSO_FINAL_TYPE_UNKNOWN),

            make_calypso_final_element(
                "ContractData(0..255)", 0, "Données complémentaires", CALYPSO_FINAL_TYPE_UNKNOWN),
        });

    return IntercodeContractStructure;
}

CalypsoApp* get_intercode_structure_event() {
    CalypsoApp* IntercodeEventStructure = malloc(sizeof(CalypsoApp));
    if(!IntercodeEventStructure) {
        return NULL;
    }

    int app_elements_count = 3;

    IntercodeEventStructure->type = CALYPSO_APP_EVENT;
    IntercodeEventStructure->container = malloc(sizeof(CalypsoContainerElement));
    IntercodeEventStructure->container->elements =
        malloc(app_elements_count * sizeof(CalypsoElement));
    IntercodeEventStructure->container->size = app_elements_count;

    IntercodeEventStructure->container->elements[0] = make_calypso_final_element(
        "EventDateStamp", 14, "Date de l’événement", CALYPSO_FINAL_TYPE_DATE);

    IntercodeEventStructure->container->elements[1] = make_calypso_final_element(
        "EventTimeStamp", 11, "Heure de l’événement", CALYPSO_FINAL_TYPE_TIME);

    IntercodeEventStructure->container->elements[2] = make_calypso_bitmap_element(
        "EventBitmap",
        28,
        (CalypsoElement[]){
            make_calypso_final_element(
                "EventDisplayData", 8, "Données pour l’affichage", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element("EventNetworkId", 24, "Réseau", CALYPSO_FINAL_TYPE_NUMBER),
            make_calypso_final_element(
                "EventCode", 8, "Nature de l’événement", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventResult", 8, "Code Résultat", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventServiceProvider",
                8,
                "Identité de l’exploitant",
                CALYPSO_FINAL_TYPE_SERVICE_PROVIDER),
            make_calypso_final_element(
                "EventNotokCounter", 8, "Compteur événements anormaux", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventSerialNumber",
                24,
                "Numéro de série de l’événement",
                CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventDestination", 16, "Destination de l’usager", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventLocationId", 16, "Lieu de l’événement", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventLocationGate", 8, "Identification du passage", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventDevice", 16, "Identificateur de l’équipement", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventRouteNumber", 16, "Référence de la ligne", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventRouteVariant",
                8,
                "Référence d’une variante de la ligne",
                CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventJourneyRun", 16, "Référence de la mission", CALYPSO_FINAL_TYPE_NUMBER),
            make_calypso_final_element(
                "EventVehicleId", 16, "Identificateur du véhicule", CALYPSO_FINAL_TYPE_NUMBER),
            make_calypso_final_element(
                "EventVehicleClass", 8, "Type de véhicule utilisé", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventLocationType",
                5,
                "Type d’endroit (gare, arrêt de bus), ",
                CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventEmployee", 240, "Code de l’employé impliqué", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventLocationReference",
                16,
                "Référence du lieu de l’événement",
                CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventJourneyInterchanges",
                8,
                "Nombre de correspondances",
                CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventPeriodJourneys", 16, "Nombre de voyage effectué", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventTotalJourneys",
                16,
                "Nombre total de voyage autorisé",
                CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventJourneyDistance", 16, "Distance parcourue", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventPriceAmount",
                16,
                "Montant en jeu lors de l’événement",
                CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventPriceUnit", 16, "Unité de montant en jeu", CALYPSO_FINAL_TYPE_UNKNOWN),
            make_calypso_final_element(
                "EventContractPointer",
                5,
                "Référence du contrat concerné",
                CALYPSO_FINAL_TYPE_NUMBER),
            make_calypso_final_element(
                "EventAuthenticator", 16, "Code de sécurité", CALYPSO_FINAL_TYPE_UNKNOWN),

            make_calypso_bitmap_element(
                "EventData",
                5,
                (CalypsoElement[]){
                    make_calypso_final_element(
                        "EventDataDateFirstStamp",
                        14,
                        "Date de la première montée",
                        CALYPSO_FINAL_TYPE_DATE),
                    make_calypso_final_element(
                        "EventDataTimeFirstStamp",
                        11,
                        "Heure de la première montée",
                        CALYPSO_FINAL_TYPE_TIME),
                    make_calypso_final_element(
                        "EventDataSimulation",
                        1,
                        "Dernière validation (0=normal, 1=dégradé), ",
                        CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "EventDataTrip", 2, "Tronçon", CALYPSO_FINAL_TYPE_UNKNOWN),
                    make_calypso_final_element(
                        "EventDataRouteDirection", 2, "Sens", CALYPSO_FINAL_TYPE_UNKNOWN),
                }),
        });

    return IntercodeEventStructure;
}

CalypsoApp* get_intercode_structure_counter() {
    CalypsoApp* IntercodeCounterStructure = malloc(sizeof(CalypsoApp));
    if(!IntercodeCounterStructure) {
        return NULL;
    }

    int app_elements_count = 2;

    IntercodeCounterStructure->type = CALYPSO_APP_COUNTER;
    IntercodeCounterStructure->container = malloc(sizeof(CalypsoContainerElement));
    IntercodeCounterStructure->container->elements =
        malloc(app_elements_count * sizeof(CalypsoElement));
    IntercodeCounterStructure->container->size = app_elements_count;

    IntercodeCounterStructure->container->elements[0] = make_calypso_final_element(
        "CounterContractCount", 6, "Nombre de titres du carnet", CALYPSO_FINAL_TYPE_NUMBER);

    IntercodeCounterStructure->container->elements[1] = make_calypso_final_element(
        "CounterRelativeFirstStamp15mn",
        18,
        "Temps relatif de la première validation (au quart d'heure près)",
        CALYPSO_FINAL_TYPE_NUMBER);

    return IntercodeCounterStructure;
}

const char* get_intercode_string_transition_type(int transition) {
    switch(transition) {
    case 0x1:
        return "Entry (First validation)";
    case 0x2:
        return "Exit";
    case 0x3:
        return "Validation";
    case 0x4:
        return "Inspection";
    case 0x5:
        return "Test validation";
    case 0x6:
        return "Entry (Interchange)";
    case 0x7:
        return "Exit (Interchange)";
    case 0x9:
        return "Validation cancelled";
    case 0xA:
        return "Entry (Public road)";
    case 0xB:
        return "Exit (Public road)";
    case 0xD:
        return "Distribution";
    case 0xF:
        return "Invalidation";
    default: {
        char* transition_str = malloc(6 * sizeof(char));
        snprintf(transition_str, 6, "%d", transition);
        return transition_str;
    }
    }
}

const char* get_intercode_string_transport_type(int type) {
    switch(type) {
    case URBAN_BUS:
        return "Urban Bus";
    case INTERURBAN_BUS:
        return "Interurban Bus";
    case METRO:
        return "Metro";
    case TRAM:
        return "Tram";
    case COMMUTER_TRAIN:
        return "Train";
    case PARKING:
        return "Parking";
    default:
        return "Unknown";
    }
}

const char* get_intercode_string_pay_method(int pay_method) {
    switch(pay_method) {
    case 0x30:
        return "Apple Pay/Google Pay";
    case 0x80:
        return "Debit PME";
    case 0x90:
        return "Cash";
    case 0xA0:
        return "Mobility Check";
    case 0xB3:
        return "Payment Card";
    case 0xA4:
        return "Check";
    case 0xA5:
        return "Vacation Check";
    case 0xB7:
        return "Telepayment";
    case 0xD0:
        return "Remote Payment";
    case 0xD7:
        return "Voucher, Prepayment, Exchange Voucher, Travel Voucher";
    case 0xD9:
        return "Discount Voucher";
    default:
        return "Unknown";
    }
}

const char* get_intercode_string_event_result(int result) {
    switch(result) {
    case 0x0:
        return "OK";
    case 0x7:
        return "Transfer / Dysfunction";
    case 0x8:
        return "Disabled due to fraud";
    case 0x9:
        return "Disabled due to monetary fraud";
    case 0xA:
        return "Invalidation impossible";
    case 0x30:
        return "Double validation (Entry)";
    case 0x31:
        return "Invalid zone";
    case 0x32:
        return "Contract expired";
    case 0x33:
        return "Double validation (Exit)";
    default: {
        char* result_str = malloc(6 * sizeof(char));
        if(!result_str) {
            return "Unknown";
        }
        snprintf(result_str, 6, "%d", result);
        return result_str;
    }
    }
}

const char* get_intercode_string_version(int version) {
    // version is a 6 bits int
    // if the first 3 bits are 000, it's a 1.x version
    // if the first 3 bits are 001, it's a 2.x version
    // else, it's unknown
    int major = (version >> 3) & 0x07;
    if(major == 0) {
        return "Intercode I";
    } else if(major == 1) {
        return "Intercode II";
    }
    return "Unknown";
}

int get_intercode_string_subversion(int version) {
    // subversion is a 3 bits int
    return version & 0x07;
}

const char* get_intercode_string_holder_type(int card_status) {
    // b3 -> RFU
    // b2 -> linked to an organization
    // b1..b0 -> personalization status (0: anonymous, 1: identified, 2: personalized, 3: networkSpecific)
    int status = card_status & 0x03;
    switch(status) {
    case 0:
        return "Anonymous";
    case 1:
        return "Identified";
    case 2:
        return "Personalized";
    case 3:
        return "Network Specific";
    default:
        return "Unknown";
    }
}

bool is_intercode_string_holder_linked(int card_status) {
    // b3 -> RFU
    // b2 -> linked to an organization
    // b1..b0 -> personalization status (0: anonymous, 1: identified, 2: personalized, 3: networkSpecific)
    return card_status & 0x04;
}

const char* get_intercode_string_contract_status(int status) {
    switch(status) {
    case 0x0:
        return "Valid (never used)";
    case 0x1:
        return "Valid (used)";
    case 0x3:
        return "Renewal required";
    case 0xD:
        return "Not validable";
    case 0x13:
        return "Blocked";
    case 0x3F:
        return "Suspended";
    case 0x58:
        return "Invalid";
    case 0x7F:
        return "Refunded";
    case 0xFF:
        return "Erasable";
    default:
        return "Unknown";
    }
}
