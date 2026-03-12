# Changelog

All notable changes to TonUINO Writer will be documented in this file.

## [2.0.0] - 2025-12-31

**Added**
- Complete SceneManager refactoring for stable navigation
- Cancelable READ and WRITE operations with dedicated Cancel button
- Non-blocking NFC operations using FuriThread
- Menu position memory - returns to last selected item
- Application icon (10x10px speaker/box design)
- Proper scene lifecycle management
- All 11 TonUINO play modes support
- Rapid Write mode for quick multi-card creation
- Card preview before writing
- Read existing TonUINO cards
- About screen with version information

**Changed**
- Migrated from ViewDispatcher-only to SceneManager + ViewDispatcher pattern
- NFC operations now non-blocking with 100ms tick polling
- Widget views properly reset in each scene
- Improved error handling and user feedback
- Better visual feedback during NFC operations

**Fixed**
- Global state elimination (removed g_mode_selection_app)
- Widget view reuse conflicts
- Back button stability across all scenes
- Mode selection navigation issues
- 4+ second UI freeze during NFC operations
- Inconsistent back button handling
- Magic number view IDs replaced with semantic enums

**Technical**
- Version: 2.0.0
- Build: 79
- Stack size: 2KB
- Category: NFC
- Compatible with: Official and Unleashed firmware

## [1.0.0] - Initial Release

**Added**
- Basic TonUINO card writing functionality
- Folder and mode selection
- Special values configuration
- MIFARE Classic 1K support
