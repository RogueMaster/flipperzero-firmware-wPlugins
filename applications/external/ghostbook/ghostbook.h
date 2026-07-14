/*
 * GhostBook v0.6.0
 * Secure encrypted contacts for Flipper Zero
 * 
 * Original Author: Digi
 * Created: December 2025
 * License: MIT
 * 
 * If you fork this, keep this header intact.
 * https://github.com/digitard/ghostbook
 */

#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_random.h>
#include <furi_hal_nfc.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/popup.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_listener.h>
#include <stdlib.h>
#include <string.h>

#define TAG "GhostBook"

// File paths
#define GHOSTBOOK_FOLDER       APP_DATA_PATH("")
#define GHOSTBOOK_AUTH_FILE    APP_DATA_PATH(".auth")
#define GHOSTBOOK_PROFILE      APP_DATA_PATH("profile.enc")
#define GHOSTBOOK_CONTACTS_DIR APP_DATA_PATH("contacts")

// Security settings
#define MIN_PASSCODE_LENGTH     6
#define MAX_PASSCODE_LENGTH     10
#define DEFAULT_PASSCODE_LENGTH 6
#define DEFAULT_MAX_ATTEMPTS    5
#define SALT_LENGTH             16
#define KEY_LENGTH              32
#define IV_LENGTH               16
#define KEY_ITERATIONS          10000

// Field lengths
#define GHOST_HANDLE_LEN   32
#define GHOST_NAME_LEN     48
#define GHOST_EMAIL_LEN    64
#define GHOST_DISCORD_LEN  40
#define GHOST_SIGNAL_LEN   20
#define GHOST_TELEGRAM_LEN 40
#define GHOST_NOTES_LEN    128

// Button codes for passcode (matches Flipper input)
typedef enum {
    PasscodeUp = 0,
    PasscodeDown,
    PasscodeLeft,
    PasscodeRight,
    PasscodeOk,
    PasscodeBack,
} PasscodeButton;

// Scenes
typedef enum {
    GhostSceneLock, // Passcode entry
    GhostSceneSecuritySetup, // First-time security level selection
    GhostSceneWipeSetup, // First-time wipe threshold selection
    GhostSceneSetup, // First-time passcode setup
    GhostSceneSetupConfirm, // Confirm new passcode
    GhostSceneMain,
    GhostSceneView,
    GhostSceneEdit,
    GhostSceneEditHandle,
    GhostSceneEditName,
    GhostSceneEditEmail,
    GhostSceneEditDiscord,
    GhostSceneEditSignal,
    GhostSceneEditTelegram,
    GhostSceneEditNotes,
    GhostSceneFlair,
    GhostSceneShare,
    GhostSceneReceive,
    GhostSceneContacts,
    GhostSceneContactView,
    GhostSceneExport,
    GhostSceneAbout,
    GhostSceneWiped, // Show wipe message
    GhostSceneNum,
} GhostScene;

// Views
typedef enum {
    GhostViewSubmenu,
    GhostViewWidget,
    GhostViewTextInput,
    GhostViewPopup,
    GhostViewPasscode, // Custom passcode view
} GhostView;

// Flair types
typedef enum {
    GhostFlairNone = 0,
    GhostFlairSkull,
    GhostFlairGhost,
    GhostFlairMatrix,
    GhostFlairGlitch,
    GhostFlairLock,
    GhostFlairEye,
    GhostFlairBolt,
    GhostFlairStar,
    GhostFlairNum,
} GhostFlair;

// Ghost card data
typedef struct {
    char handle[GHOST_HANDLE_LEN];
    char name[GHOST_NAME_LEN];
    char email[GHOST_EMAIL_LEN];
    char discord[GHOST_DISCORD_LEN];
    char signal[GHOST_SIGNAL_LEN];
    char telegram[GHOST_TELEGRAM_LEN];
    char notes[GHOST_NOTES_LEN];
    GhostFlair flair;
} GhostCard;

// Contact entry
typedef struct {
    GhostCard card;
    char filename[64];
} GhostContact;

// Authentication data (stored hashed)
typedef struct {
    uint8_t salt[SALT_LENGTH];
    uint8_t passcode_hash[32]; // Hash of salt+passcode
    uint8_t failed_attempts;
    uint8_t passcode_length; // 6-10 buttons
    uint8_t max_attempts; // 3, 5, 7, or 10
    bool initialized;
} GhostAuth;

// Main app context
typedef struct {
    // System
    Gui* gui;
    Storage* storage;
    NotificationApp* notif;
    ViewDispatcher* view;

    // GUI modules
    Submenu* submenu;
    Widget* widget;
    TextInput* text_input;
    Popup* popup;
    View* passcode_view;

    // NFC
    Nfc* nfc;
    NfcListener* nfc_listener;
    NfcPoller* nfc_poller;
    FuriThread* nfc_thread;
    bool nfc_active;
    GhostCard received_card;
    bool card_received;

    // Security
    GhostAuth auth;
    uint8_t passcode_input[MAX_PASSCODE_LENGTH];
    uint8_t passcode_pos;
    uint8_t setup_passcode[MAX_PASSCODE_LENGTH]; // For setup confirmation
    uint8_t setup_length; // Selected length during setup
    uint8_t setup_max_attempts; // Selected wipe threshold during setup
    uint8_t derived_key[KEY_LENGTH]; // Derived from passcode
    bool unlocked;

    // State
    GhostScene current_scene;

    // Data
    GhostCard my_card;
    GhostContact* contacts;
    uint32_t contact_count;
    uint32_t selected_contact;
    char text_buf[GHOST_NOTES_LEN];
    uint8_t edit_field;
} GhostApp;

// Crypto functions
void ghost_derive_key(
    const uint8_t* passcode,
    uint8_t passcode_len,
    const uint8_t* salt,
    uint8_t* key_out);
bool ghost_encrypt_data(
    const uint8_t* key,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* cipher,
    size_t* cipher_len);
bool ghost_decrypt_data(
    const uint8_t* key,
    const uint8_t* cipher,
    size_t cipher_len,
    uint8_t* plain,
    size_t* plain_len);

// NFC functions
void ghost_nfc_share_start(GhostApp* app);
void ghost_nfc_share_stop(GhostApp* app);
void ghost_nfc_receive_start(GhostApp* app);
void ghost_nfc_receive_stop(GhostApp* app);

// Auth functions
bool ghost_auth_load(GhostApp* app);
bool ghost_auth_save(GhostApp* app);
bool ghost_auth_verify(GhostApp* app, const uint8_t* passcode);
void ghost_auth_set(GhostApp* app, const uint8_t* passcode, uint8_t length, uint8_t max_attempts);
void ghost_wipe_all_data(GhostApp* app);

// Utility
const char* ghost_flair_name(GhostFlair f);
const char* ghost_flair_icon(GhostFlair f);
const char* ghost_button_name(PasscodeButton b);

// Card operations
void ghost_card_init(GhostCard* c);
bool ghost_save_profile(GhostApp* app);
bool ghost_load_profile(GhostApp* app);
bool ghost_save_contact(GhostApp* app, GhostCard* c);
bool ghost_load_contacts(GhostApp* app);
void ghost_free_contacts(GhostApp* app);

// Export
bool ghost_export_vcard(GhostApp* app, GhostCard* c, const char* path);
bool ghost_export_all_vcard(GhostApp* app, const char* path);
bool ghost_export_csv(GhostApp* app, const char* path);

// Scene handlers
void ghost_scene_lock_enter(void* ctx);
bool ghost_scene_lock_event(void* ctx, SceneManagerEvent e);
void ghost_scene_lock_exit(void* ctx);

void ghost_scene_security_setup_enter(void* ctx);
bool ghost_scene_security_setup_event(void* ctx, SceneManagerEvent e);
void ghost_scene_security_setup_exit(void* ctx);

void ghost_scene_wipe_setup_enter(void* ctx);
bool ghost_scene_wipe_setup_event(void* ctx, SceneManagerEvent e);
void ghost_scene_wipe_setup_exit(void* ctx);

void ghost_scene_setup_enter(void* ctx);
bool ghost_scene_setup_event(void* ctx, SceneManagerEvent e);
void ghost_scene_setup_exit(void* ctx);

void ghost_scene_setup_confirm_enter(void* ctx);
bool ghost_scene_setup_confirm_event(void* ctx, SceneManagerEvent e);
void ghost_scene_setup_confirm_exit(void* ctx);

void ghost_scene_main_enter(void* ctx);
bool ghost_scene_main_event(void* ctx, SceneManagerEvent e);
void ghost_scene_main_exit(void* ctx);

void ghost_scene_view_enter(void* ctx);
bool ghost_scene_view_event(void* ctx, SceneManagerEvent e);
void ghost_scene_view_exit(void* ctx);

void ghost_scene_edit_enter(void* ctx);
bool ghost_scene_edit_event(void* ctx, SceneManagerEvent e);
void ghost_scene_edit_exit(void* ctx);

void ghost_scene_text_enter(void* ctx);
bool ghost_scene_text_event(void* ctx, SceneManagerEvent e);
void ghost_scene_text_exit(void* ctx);

void ghost_scene_flair_enter(void* ctx);
bool ghost_scene_flair_event(void* ctx, SceneManagerEvent e);
void ghost_scene_flair_exit(void* ctx);

void ghost_scene_share_enter(void* ctx);
bool ghost_scene_share_event(void* ctx, SceneManagerEvent e);
void ghost_scene_share_exit(void* ctx);

void ghost_scene_receive_enter(void* ctx);
bool ghost_scene_receive_event(void* ctx, SceneManagerEvent e);
void ghost_scene_receive_exit(void* ctx);

void ghost_scene_contacts_enter(void* ctx);
bool ghost_scene_contacts_event(void* ctx, SceneManagerEvent e);
void ghost_scene_contacts_exit(void* ctx);

void ghost_scene_contact_view_enter(void* ctx);
bool ghost_scene_contact_view_event(void* ctx, SceneManagerEvent e);
void ghost_scene_contact_view_exit(void* ctx);

void ghost_scene_export_enter(void* ctx);
bool ghost_scene_export_event(void* ctx, SceneManagerEvent e);
void ghost_scene_export_exit(void* ctx);

void ghost_scene_about_enter(void* ctx);
bool ghost_scene_about_event(void* ctx, SceneManagerEvent e);
void ghost_scene_about_exit(void* ctx);

void ghost_scene_wiped_enter(void* ctx);
bool ghost_scene_wiped_event(void* ctx, SceneManagerEvent e);
void ghost_scene_wiped_exit(void* ctx);

// Passcode view
View* ghost_passcode_view_alloc(GhostApp* app);
void ghost_passcode_view_free(View* view);
void ghost_passcode_view_reset(GhostApp* app);
void ghost_passcode_view_set_mode(GhostApp* app, const char* header, bool show_attempts);
