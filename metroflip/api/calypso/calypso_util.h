#include <stdbool.h>
#include "calypso_i.h"

#ifndef CALYPSO_UTIL_H
#define CALYPSO_UTIL_H

typedef enum {
    CALYPSO_APP_ENV_HOLDER,
    CALYPSO_APP_CONTRACT,
    CALYPSO_APP_EVENT,
    CALYPSO_APP_COUNTER,
} CalypsoAppType;

typedef enum {
    CALYPSO_FINAL_TYPE_UNKNOWN,
    CALYPSO_FINAL_TYPE_NUMBER,
    CALYPSO_FINAL_TYPE_DATE,
    CALYPSO_FINAL_TYPE_TIME,
    CALYPSO_FINAL_TYPE_PAY_METHOD,
    CALYPSO_FINAL_TYPE_AMOUNT,
    CALYPSO_FINAL_TYPE_SERVICE_PROVIDER,
    CALYPSO_FINAL_TYPE_ZONES,
    CALYPSO_FINAL_TYPE_TARIFF,
    CALYPSO_FINAL_TYPE_NETWORK_ID,
    CALYPSO_FINAL_TYPE_TRANSPORT_TYPE,
    CALYPSO_FINAL_TYPE_CARD_STATUS,
    CALYPSO_FINAL_TYPE_STRING,
} CalypsoFinalType;

typedef enum {
    CALYPSO_ELEMENT_TYPE_REPEATER,
    CALYPSO_ELEMENT_TYPE_CONTAINER,
    CALYPSO_ELEMENT_TYPE_BITMAP,
    CALYPSO_ELEMENT_TYPE_FINAL
} CalypsoElementType;

typedef struct CalypsoFinalElement_t CalypsoFinalElement;
typedef struct CalypsoBitmapElement_t CalypsoBitmapElement;
typedef struct CalypsoContainerElement_t CalypsoContainerElement;
typedef struct CalypsoRepeaterElement_t CalypsoRepeaterElement;

typedef struct {
    CalypsoElementType type;
    union {
        CalypsoFinalElement* final;
        CalypsoBitmapElement* bitmap;
        CalypsoContainerElement* container;
        CalypsoRepeaterElement* repeater;
    };
} CalypsoElement;

struct CalypsoFinalElement_t {
    char key[36];
    int size;
    char label[64];
    CalypsoFinalType final_type;
};

struct CalypsoBitmapElement_t {
    char key[36];
    int size;
    CalypsoElement* elements;
};

struct CalypsoContainerElement_t {
    char key[36];
    int size;
    CalypsoElement* elements;
};

struct CalypsoRepeaterElement_t {
    char key[36];
    int size;
    CalypsoElement element;
};

typedef struct {
    CalypsoAppType type;
    CalypsoContainerElement* container;
} CalypsoApp;

CalypsoElement make_calypso_final_element(
    const char* key,
    int size,
    const char* label,
    CalypsoFinalType final_type);

CalypsoElement make_calypso_bitmap_element(const char* key, int size, CalypsoElement* elements);

CalypsoElement make_calypso_container_element(const char* key, int size, CalypsoElement* elements);

CalypsoElement make_calypso_repeater_element(const char* key, int size, CalypsoElement element);

int* get_bitmap_positions(const char* binary_string, int* count);

int is_bit_present(int* positions, int count, int bit);

// Calypso known Card types

const char* get_country_string(int country_num);

#endif // CALYPSO_UTIL_H
