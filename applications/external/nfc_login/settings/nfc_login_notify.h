#pragma once

#include "../nfc_login_app.h"

typedef enum {
    NfcLoginNotifySuccess,
    NfcLoginNotifyError,
} NfcLoginNotifyType;

void nfc_login_notify(App* app, NfcLoginNotifyType type);
