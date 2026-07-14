/**
 * @file miband_nfc_scene_config.h
 * @brief Scene configuration - Updated with new features
 */

// Main menu - entry point of the application
ADD_SCENE(miband_nfc, main_menu, MainMenu)

// File selection - browse and select NFC dump files
ADD_SCENE(miband_nfc, file_select, FileSelect)

// Magic card emulation - emulate blank template with 0xFF keys
ADD_SCENE(miband_nfc, magic_emulator, MagicEmulator)

// Scanner - detect and identify NFC cards
ADD_SCENE(miband_nfc, scanner, Scanner)

// Backup - automatic backup before write (NEW)
ADD_SCENE(miband_nfc, backup, Backup)

// Writer - write dump data to Mi Band NFC
ADD_SCENE(miband_nfc, writer, Writer)

// Magic saver - convert and save dump with magic keys
ADD_SCENE(miband_nfc, magic_saver, MagicSaver)

// Verify - read and compare data with original dump
ADD_SCENE(miband_nfc, verify, Verify)

// Difference viewer - show detailed verification differences (NEW)
ADD_SCENE(miband_nfc, diff_viewer, DiffViewer)

// UID Check - quick UID display with disk scanner (NEW)
ADD_SCENE(miband_nfc, uid_check, UidCheck)

// About - display help and usage information
ADD_SCENE(miband_nfc, about, About)

ADD_SCENE(miband_nfc, settings, Settings)
