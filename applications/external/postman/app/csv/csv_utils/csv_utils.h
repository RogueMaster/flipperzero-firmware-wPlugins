#ifndef CSV_UTILS_H
#define CSV_UTILS_H

#include "../../app.h"

typedef enum {
    StateTypeGet,
    StateTypePost,
    StateTypeBuildHttp,
} StateType;

/**
 * @brief Read a line from the file.
 * 
 * @param file Pointer to the File structure.
 * @param str_result FuriString to store the read line.
 * @return true if a line was read successfully, false otherwise.
 */
bool read_line_from_file(File* file, FuriString* str_result);

/**
 * @brief Find url entry in the csv file 
 * 
 * @param file Pointer to the File structure.
 * @param str_result FuriString to store the read line.
 * @return true if a line was read successfully, false otherwise.
 */
bool url_in_csv(App* app, const char* url, StateType state_type);

#endif // CSV_UTILS_H
