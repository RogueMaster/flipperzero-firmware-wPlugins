#ifndef MINE_SWEEPER_CONFIG_H
#define MINE_SWEEPER_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// Application tag for logging
#define TAG "Mine Sweeper Application"

// Storage Helper Defines
#define MINESWEEPER_SETTINGS_FILE_VERSION               3
#define MINESWEEPER_SETTINGS_FILE_VERSION_MIN_SUPPORTED 2
#define CONFIG_FILE_DIRECTORY_PATH                      EXT_PATH("apps_data/mine_sweeper_redux")
#define MINESWEEPER_SETTINGS_SAVE_PATH                  CONFIG_FILE_DIRECTORY_PATH "/mine_sweeper_redux.conf"
#define MINESWEEPER_SETTINGS_SAVE_PATH_TMP              MINESWEEPER_SETTINGS_SAVE_PATH ".tmp"
#define MINESWEEPER_SETTINGS_HEADER                     "Mine Sweeper Redux Config File"

#define MINESWEEPER_SETTINGS_KEY_WIDTH      "BoardWidth"
#define MINESWEEPER_SETTINGS_KEY_HEIGHT     "BoardHeight"
#define MINESWEEPER_SETTINGS_KEY_DIFFICULTY "BoardDifficulty"
#define MINESWEEPER_SETTINGS_KEY_FEEDBACK   "FeedbackEnabled"
#define MINESWEEPER_SETTINGS_KEY_WRAP       "WrapEnabled"
#define MINESWEEPER_SETTINGS_KEY_SOLVABLE   "EnsureSolvable"

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // MINE_SWEEPER_CONFIG_H
