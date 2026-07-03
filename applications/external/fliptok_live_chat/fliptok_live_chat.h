#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_bt.h>
#include <bt/bt_service/bt.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <notification/notification_messages.h>
#include <input/input.h>
#include <storage/storage.h>

#define TAG                   "FlipTokLive"
#define BT_SERIAL_BUFFER_SIZE 256

#define MAX_MESSAGES 8
#define USERNAME_LEN 17
#define MESSAGE_LEN  65

#define TIKTOK_PACKET_SIZE (1 + USERNAME_LEN + MESSAGE_LEN)

typedef enum {
    TikTokMsgTypeChat = 0,
    TikTokMsgTypeLike = 1,
    TikTokMsgTypeGift = 2,
    TikTokMsgTypeFollow = 3,
} TikTokMsgType;

#pragma pack(push, 1)
typedef struct {
    uint8_t type;
    char username[USERNAME_LEN];
    char message[MESSAGE_LEN];
} TikTokMessage;
#pragma pack(pop)

typedef enum {
    BtStateWaiting,
    BtStateReceiving,
    BtStateLost,
} BtState;

typedef enum {
    SceneSplash,
    SceneChat,
} AppScene;

typedef struct {
    TikTokMessage items[MAX_MESSAGES];
    uint8_t count;
    uint8_t head;
    int8_t scroll;
} MessageBuffer;

typedef struct {
    Bt* bt;
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    NotificationApp* notification;
    void* ble_serial_profile;

    BtState bt_state;
    AppScene scene;
    MessageBuffer msgs;
    uint32_t last_packet;
    bool show_splash;
} TikTokApp;
