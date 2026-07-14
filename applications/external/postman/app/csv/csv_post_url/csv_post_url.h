#pragma once

#include "../../app.h"
#include "../../structs.h"
#include <furi.h>
#include <storage/storage.h>

#define POST_URL_CSV_PATH APP_DATA_PATH("post_url.csv")

/**
 * @brief Initialize the CSV file for POST URLs.
 * 
 * @param app Pointer to the App structure.
 * @return true if initialization was successful, false otherwise.
 */
bool init_csv_post_url(App* app);

/**
 * @brief Synchronize POST URLs from CSV file to memory.
 * 
 * @param app Pointer to the App structure.
 * @return true if synchronization was successful, false otherwise.
 */
bool sync_csv_post_url_to_mem(App* app);

/**
 * @brief Write a POST URL to the CSV file.
 * 
 * @param app Pointer to the App structure.
 * @param post_url Pointer to the PostUrlList structure to be written.
 * @return true if writing was successful, false otherwise.
 */
bool write_post_url_to_csv(App* app, const PostUrlList* post_url);

/**
 * @brief Delete a POST URL from the CSV file.
 * 
 * @param app Pointer to the App structure.
 * @param url The URL to be deleted.
 * @return true if deletion was successful, false otherwise.
 */
bool delete_post_url_from_csv(App* app, const char* url);
