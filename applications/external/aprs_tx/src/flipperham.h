#pragma once

#include <furi.h>
#include <stdint.h>

#define TXT_LEN       68
#define TXT_N         16
#define POS_LEN       16
#define CALL_LEN      10
#define APRS_PATH_LEN 9
#define CALL_N        32
#define FREQ_N        4
#define HAM_N         8

#define CARRIER_HZ 433250000UL
#define MY_CALL    "FL1PER"
#define MY_TOCALL  "APZFLP"

#define CFG_DIR       "/ext/apps_data/aprstx"
#define CFG_FILE      "/ext/apps_data/aprstx/cfg.bin"
#define CALLBOOK_DIR  "/ext/ham"
#define CALLBOOK_FILE "/ext/ham/callbook.txt"
#define MY_CALLS_FILE "/ext/ham/my-callsigns.txt"

typedef struct {
    uint8_t encoding_index;
    uint8_t rf_mod;
    uint8_t rf_dev;
    uint8_t tx_freq_index;
    uint8_t dst_ssid;
    uint8_t repeat_n;
    uint8_t ham_index;
    uint16_t leadin_ms;
    uint16_t preamble_ms;

    char bulletin[TXT_N][TXT_LEN];
    char status[TXT_N][TXT_LEN];
    char message[TXT_N][TXT_LEN];
    char calls[CALL_N][CALL_LEN];
    char pos_name[TXT_N][TXT_LEN];
    char pos_lat[TXT_N][POS_LEN];
    char pos_lon[TXT_N][POS_LEN];
    uint32_t freq[FREQ_N];

    uint8_t bulletin_used[TXT_N];
    uint8_t status_used[TXT_N];
    uint8_t message_used[TXT_N];
    uint8_t calls_used[CALL_N];
    uint8_t pos_used[TXT_N];
    uint8_t freq_used[FREQ_N];

    uint8_t bulletin_n;
    uint8_t status_n;
    uint8_t message_n;
    uint8_t calls_n;
    uint8_t pos_n;
    uint8_t freq_n;
    uint8_t aprs_path_index;
    char aprs_path_edit[APRS_PATH_LEN];
    uint8_t debug_tx;
} FlipperHamCfg;

enum {
    FlipperHamViewMenu = 0,
    FlipperHamViewSend,
    FlipperHamViewSettings,
    FlipperHamViewBulletin,
    FlipperHamViewStatus,
    FlipperHamViewMessage,
    FlipperHamViewMessageEdit,
    FlipperHamViewPosition,
    FlipperHamViewSsid,
    FlipperHamViewCall,
    FlipperHamViewBook,
    FlipperHamViewC2,
    FlipperHamViewTextInput,
    FlipperHamViewFreq,
    FlipperHamViewFreqEdit,
    FlipperHamViewPosEdit,
    FlipperHamViewPosAction,
    FlipperHamViewHam,
    FlipperHamViewHamTx,
    FlipperHamViewReadme,
    FlipperHamViewSplash,
};

enum {
    FlipperHamMenuIndexSend = 0,
    FlipperHamMenuIndexSettings,
    FlipperHamMenuIndexCallbook,
    FlipperHamMenuIndexHam,
    FlipperHamMenuIndexReadme,
};

enum {
    FlipperHamSendIndexMessage = 0,
    FlipperHamSendIndexPosition,
    FlipperHamSendIndexStatus,
    FlipperHamSendIndexBulletin,
};

enum {
    FlipperHamSettingsIndexFreq = 0,
    FlipperHamSettingsIndexBaud,
    FlipperHamSettingsIndexAprsPath,
    FlipperHamSettingsIndexProfile,
    FlipperHamSettingsIndexDeviation,
    FlipperHamSettingsIndexRepeat,
    FlipperHamSettingsIndexLeadin,
    FlipperHamSettingsIndexPreamble,
    FlipperHamSettingsIndexCustomPath,
    FlipperHamSettingsIndexDebug,
};

enum {
    FlipperHamBulletinIndexAdd = 0,
    FlipperHamBulletinIndexBase = 100,
};

enum {
    FlipperHamStatusIndexAdd = 0,
    FlipperHamStatusIndexBase = 200,
};

enum {
    FlipperHamCallIndexAdd = 0,
    FlipperHamCallIndexBase = 300,
};

enum {
    FlipperHamMessageIndexAdd = 0,
    FlipperHamMessageIndexBase = 400,
};

enum {
    FlipperHamPositionIndexAdd = 0,
    FlipperHamPositionIndexBase = 450,
};

enum {
    FlipperHamPosEditIndexName = 0,
    FlipperHamPosEditIndexLat,
    FlipperHamPosEditIndexLon,
    FlipperHamPosEditIndexDelete,
};

enum {
    FlipperHamBookIndexAdd = 0,
    FlipperHamBookIndexBase = 500,
};

enum {
    FlipperHamC2IndexEdit = 0,
    FlipperHamC2IndexDelete,
    FlipperHamC2IndexCopy,
};

enum {
    FlipperHamFreqIndexAdd = 0,
    FlipperHamFreqIndexBase = 600,
};

int32_t flipperham_app(void* p);
