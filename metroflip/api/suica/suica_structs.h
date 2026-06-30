#include <datetime.h>
#include <stdbool.h>
#include <furi.h>

#define SUICA_MAX_HISTORY_ENTRIES 0x15
typedef enum {
    SuicaHistoryNull,
    SuicaHistoryTopUp,
    SuicaHistoryBus,
    SuicaHistoryPosAndTaxi,
    SuicaHistoryVendingMachine,
    SuicaHistoryHappyBirthday,
    SuicaHistoryTrain
} SuicaHistoryType;

typedef struct {
    View* view_history;
    FuriTimer* timer;
    FuriString* suica_file_data;
} SuicaContext;
