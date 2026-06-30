#include "run/run.hpp"
#include "app.hpp"

FlipDownloaderRun::FlipDownloaderRun() : isLoadingNextApps{false}
{
    // Initialize download queue
    downloadQueueSize = 0;
    currentDownloadIndex = 0;
    isProcessingQueue = false;

    // Initialize download state
    isDownloadStarted = false;
    isDownloading = false;
    isDownloadComplete = false;
    idleCheckCounter = 0;

    // Initialize GitHub download state
    isGitHubDownloading = false;
    isGitHubDownloadComplete = false;
    github_download_delay_counter = 0;
}

FlipDownloaderRun::~FlipDownloaderRun()
{
    // nothing to do
}

void FlipDownloaderRun::clearDownloadQueue()
{
    downloadQueueSize = 0;
    currentDownloadIndex = 0;
    isProcessingQueue = false;
}

uint8_t FlipDownloaderRun::countAppsInCategory(FlipperAppCategory category)
{
    char savePath[128];
    snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/%s.json", APP_ID, getAppCategory(category));
    FuriString *appInfo = flipper_http_load_from_file(savePath);
    if (!appInfo || furi_string_size(appInfo) == 0)
    {
        return 0;
    }

    const char *jsonText = furi_string_get_cstr(appInfo);
    uint8_t count = 0;

    // Find the opening bracket of the array using manual search
    const char *arrayStart = nullptr;
    const char *pos = jsonText;
    while (*pos != '\0')
    {
        if (*pos == '[')
        {
            arrayStart = pos;
            break;
        }
        pos++;
    }
    if (!arrayStart)
    {
        furi_string_free(appInfo);
        return 0;
    }

    const char *current = arrayStart + 1;

    // Count objects in the array
    while (*current)
    {
        // Skip whitespace
        while (*current && (*current == ' ' || *current == '\t' || *current == '\n' || *current == '\r' || *current == ','))
        {
            current++;
        }

        // Check if we've reached the end of the array
        if (*current == ']')
        {
            break;
        }

        // Look for opening brace
        if (*current == '{')
        {
            count++;

            // Skip this entire object
            int braceCount = 1;
            current++; // Move past the opening brace

            while (*current && braceCount > 0)
            {
                if (*current == '"')
                {
                    // Skip string content completely
                    current++;
                    while (*current && *current != '"')
                    {
                        if (*current == '\\' && *(current + 1))
                        {
                            current += 2; // Skip escaped character
                        }
                        else
                        {
                            current++;
                        }
                    }
                    if (*current == '"')
                    {
                        current++; // Skip closing quote
                    }
                }
                else if (*current == '{')
                {
                    braceCount++;
                    current++;
                }
                else if (*current == '}')
                {
                    braceCount--;
                    current++;
                }
                else
                {
                    current++;
                }
            }
        }
        else
        {
            current++;
        }
    }

    furi_string_free(appInfo);
    return count;
}

int FlipDownloaderRun::countChar(const char *s, char c)
{
    int count = 0;
    for (; *s; s++)
    {
        if (*s == c)
        {
            count++;
        }
    }
    return count;
}

bool FlipDownloaderRun::createDirectory(const char *dirPath)
{
    if (!dirPath || dirPath[0] == '\0')
    {
        return false;
    }

    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    if (!storage)
    {
        return false;
    }

    if (!storage_common_exists(storage, dirPath) && storage_common_mkdir(storage, dirPath) != FSE_OK)
    {
        FURI_LOG_E(TAG, "Failed to create directory %s", dirPath);
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    furi_record_close(RECORD_STORAGE);
    return true;
}

void FlipDownloaderRun::deleteFile(const char *filePath)
{
    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    storage_simply_remove_recursive(storage, filePath);
    furi_record_close(RECORD_STORAGE);
}

bool FlipDownloaderRun::downloadApp(const char *appId, const char *buildId, const char *category)
{
    FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(appContext);
    furi_check(app);
    isDownloading = false;
    isDownloadComplete = false;
    idleCheckCounter = 0;
    snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps/%s/%s.fap", category, appId);
    uint8_t target = furi_hal_version_get_hw_target();
    uint16_t api_major, api_minor;
    furi_hal_info_get_api_version(&api_major, &api_minor);
    char url[256];
    snprintf(url, sizeof(url), "https://catalog.flipperzero.one/api/v0/application/version/%s/build/compatible?target=f%d&api=%d.%d", buildId, target, api_major, api_minor);
    return app->httpDownloadFile(savePath, url);
}

bool FlipDownloaderRun::downloadCategoryInfo(FlipperAppCategory category, uint8_t iteration)
{
    FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(appContext);
    furi_check(app);
    isDownloading = false;
    isDownloadComplete = false;
    idleCheckCounter = 0;
    setCategorySavePath(category);
    deleteFile(savePath);
    char url[256];
    snprintf(url, sizeof(url), "https://catalog.flipperzero.one/api/v0/0/application?limit=%d&is_latest_release_version=true&offset=%d&sort_by=updated_at&sort_order=-1&category_id=%s", MAX_RECEIVED_APPS, iteration, getAppCategoryId(category));
    return app->httpDownloadFile(savePath, url);
}

bool FlipDownloaderRun::downloadFile(FlipDownloaderDownloadLink link)
{
    FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(appContext);
    furi_check(app);
    isDownloadStarted = true;
    isDownloading = true;
    hasDownloadTransitioned = false;
    isDownloadComplete = false;
    setSavePath(link);
    switch (link)
    {
        // Marauder (ESP32)
    case DownloadLinkFirmwareMarauderLink1:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/FZEEFlasher/fzeeflasher.github.io/main/resources/STATIC/M/FLIPDEV/esp32_marauder.ino.bootloader.bin");
    case DownloadLinkFirmwareMarauderLink2:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/FZEEFlasher/fzeeflasher.github.io/main/resources/STATIC/M/FLIPDEV/esp32_marauder.ino.partitions.bin");
    case DownloadLinkFirmwareMarauderLink3:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/fzeeflasher.github.io/main/resources/CURRENT/esp32_marauder_v1_9_0_20251213_flipper.bin");
    // Black Magic (ESP32)
    case DownloadLinkFirmwareBlackMagicLink1:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/FZEEFlasher/fzeeflasher.github.io/main/resources/STATIC/BM/bootloader.bin");
    case DownloadLinkFirmwareBlackMagicLink2:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/FZEEFlasher/fzeeflasher.github.io/main/resources/STATIC/BM/partition-table.bin");
    case DownloadLinkFirmwareBlackMagicLink3:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/FZEEFlasher/fzeeflasher.github.io/main/resources/STATIC/BM/blackmagic.bin");
        // FlipperHTTP (WiFi Devboard)
    case DownloadLinkFlipperHTTPWiFiDevboardLink1:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/WiFi%20Developer%20Board%20(ESP32S2)/flipper_http_bootloader.bin");
    case DownloadLinkFlipperHTTPWiFiDevboardLink2:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/WiFi%20Developer%20Board%20(ESP32S2)/flipper_http_firmware_a.bin");
    case DownloadLinkFlipperHTTPWiFiDevboardLink3:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/WiFi%20Developer%20Board%20(ESP32S2)/flipper_http_partitions.bin");
        // FlipperHTTP (ESP32-C3)
    case DownloadLinkFlipperHTTPESP32C3Link1:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-C3/flipper_http_bootloader_esp32_c3.bin");
    case DownloadLinkFlipperHTTPESP32C3Link2:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-C3/flipper_http_firmware_a_esp32_c3.bin");
    case DownloadLinkFlipperHTTPESP32C3Link3:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-C3/flipper_http_partitions_esp32_c3.bin");
        // FlipperHTTP (ESP32-C5)
    case DownloadLinkFlipperHTTPESP32C5Link1:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-C5/flipper_http_bootloader_esp32_c5.bin");
    case DownloadLinkFlipperHTTPESP32C5Link2:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-C5/flipper_http_firmware_a_esp32_c5.bin");
    case DownloadLinkFlipperHTTPESP32C5Link3:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-C5/flipper_http_partitions_esp32_c5.bin");
        // FlipperHTTP (ESP32-C6)
    case DownloadLinkFlipperHTTPESP32C6Link1:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-C6/flipper_http_bootloader_esp32_c6.bin");
    case DownloadLinkFlipperHTTPESP32C6Link2:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-C6/flipper_http_firmware_a_esp32_c6.bin");
    case DownloadLinkFlipperHTTPESP32C6Link3:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-C6/flipper_http_partitions_esp32_c6.bin");
        // FlipperHTTP (ESP32-Cam)
    case DownloadLinkFlipperHTTPESP32CamLink1:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-Cam/flipper_http_bootloader_esp32_cam.bin");
    case DownloadLinkFlipperHTTPESP32CamLink2:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-Cam/flipper_http_firmware_a_esp32_cam.bin");
    case DownloadLinkFlipperHTTPESP32CamLink3:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-Cam/flipper_http_partitions_esp32_cam.bin");
        // FlipperHTTP (ESP32-S3)
    case DownloadLinkFlipperHTTPESP32S3Link1:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-S3/flipper_http_bootloader_esp32_s3.bin");
    case DownloadLinkFlipperHTTPESP32S3Link2:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-S3/flipper_http_firmware_a_esp32_s3.bin");
    case DownloadLinkFlipperHTTPESP32S3Link3:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-S3/flipper_http_partitions_esp32_s3.bin");
        // FlipperHTTP (ESP32-WROOM)
    case DownloadLinkFlipperHTTPESP32WROOMLink1:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-WROOM/flipper_http_bootloader_esp32_wroom.bin");
    case DownloadLinkFlipperHTTPESP32WROOMLink2:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-WROOM/flipper_http_firmware_a_esp32_wroom.bin");
    case DownloadLinkFlipperHTTPESP32WROOMLink3:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-WROOM/flipper_http_partitions_esp32_wroom.bin");
        // FlipperHTTP (ESP32-WROVER)
    case DownloadLinkFlipperHTTPESP32WROVERLink1:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-WROVER/flipper_http_bootloader_esp32_wrover.bin");
    case DownloadLinkFlipperHTTPESP32WROVERLink2:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-WROVER/flipper_http_firmware_a_esp32_wrover.bin");
    case DownloadLinkFlipperHTTPESP32WROVERLink3:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/ESP32-WROVER/flipper_http_partitions_esp32_wrover.bin");
        // FlipperHTTP (PicoCalc W)
    case DownloadLinkFlipperHTTPPicoCalcWLink1:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/PicoCalc/flipper_http_picocalc_w.uf2");
        // FlipperHTTP (PicoCalc 2W)
    case DownloadLinkFlipperHTTPPicoCalc2WLink1:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/PicoCalc/flipper_http_picocalc_2w.uf2");
        // FlipperHTTP (VGM)
    case DownloadLinkVGMFlipperHTTP:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/FlipperHTTP/main/Video%20Game%20Module/C%2B%2B/flipper_http_vgm_c%2B%2B.uf2");
        // Picoware (VGM)
    case DownloadLinkVGMPicoware:
        return app->httpDownloadFile(savePath, "https://raw.githubusercontent.com/jblanked/Picoware/main/builds/ArduinoIDE/Picoware-VGM.uf2");
    default:
        return false;
    }
}

void FlipDownloaderRun::drawApps(Canvas *canvas)
{
    canvas_clear(canvas);

    // Get current app info
    std::unique_ptr<FlipperAppInfo> appInfo = getAppInfo(currentCategory, selectedIndexApps);

    if (!appInfo)
    {
        currentView = RunViewCatalog;
        currentCategory = CategoryUnknown;
        return;
    }

    // Draw app name on the first line
    canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
    canvas_draw_str(canvas, 5, 15, appInfo->app_name);

    // Draw app description starting from the second line with word wrap
    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);

    // Simple word wrapping for description
    const char *description = appInfo->app_description;
    int line_y = 28;
    int max_width = 118;
    int current_x = 5;
    char word_buffer[32];
    int word_index = 0;
    bool new_line = true;

    for (int i = 0; description[i] != '\0' && line_y < 55; i++)
    {
        if (description[i] == ' ' || description[i] == '\n' || description[i + 1] == '\0')
        {
            // End of word, check if it fits
            if (description[i + 1] == '\0' && description[i] != ' ')
            {
                word_buffer[word_index++] = description[i];
            }
            word_buffer[word_index] = '\0';

            int word_width = canvas_string_width(canvas, word_buffer);

            if (current_x + word_width > max_width && !new_line)
            {
                // Move to next line
                line_y += 10;
                current_x = 5;
                new_line = true;
            }

            if (line_y < 55)
            {
                canvas_draw_str(canvas, current_x, line_y, word_buffer);
                current_x += word_width + canvas_string_width(canvas, " ");
                new_line = false;
            }

            word_index = 0;
        }
        else
        {
            word_buffer[word_index++] = description[i];
        }
    }

    // Draw navigation indicators at the bottom
    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    uint8_t actualAppCount = countAppsInCategory(currentCategory);
    char nav_info[32];
    snprintf(nav_info, sizeof(nav_info), "App %d/%d", selectedIndexApps + 1, actualAppCount);
    canvas_draw_str(canvas, 80, 60, nav_info);
}

void FlipDownloaderRun::drawDownloadProgress(Canvas *canvas)
{
    FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(appContext);
    furi_check(app);
    canvas_clear(canvas);
    switch (app->getHttpState())
    {
    case IDLE:
        // First, check if we just finished a download (highest priority)
        // Only consider download complete if we've actually seen SENDING/RECEIVING state
        if (isDownloading && hasDownloadTransitioned)
        {
            // Download just completed - we're in IDLE state but isDownloading is still true
            // Now set isDownloading = false and isDownloadComplete = true
            isDownloading = false;
            hasDownloadTransitioned = false; // Reset for next download

            // Check if we're processing a queue
            if (isProcessingQueue)
            {
                currentDownloadIndex++;

                // Check if there are more downloads in the queue
                if (currentDownloadIndex >= downloadQueueSize)
                {
                    // All downloads in queue are complete
                    isDownloadComplete = true;
                    isDownloadStarted = false;
                    isProcessingQueue = false;
                }
                else
                {
                    // More files to download
                    isDownloadComplete = false;
                }
            }
            else
            {
                // Single download or not in queue mode - mark as complete
                isDownloadComplete = true;
                isDownloadStarted = false;
            }

            // If download is truly complete, show completion message
            if (isDownloadComplete)
            {
                canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
                canvas_draw_str(canvas, 0, 10, "Download complete!");
                if (loading)
                {
                    loading->stop();
                    loading.reset();
                }
            }

            idleCheckCounter = 0;
        }
        // Check if we need to start the next download in queue
        else if (isProcessingQueue && !isDownloading && currentDownloadIndex < downloadQueueSize)
        {
            // Start the next download in the queue
            processDownloadQueue();
        }
        // Show final completion message
        else if (isDownloadComplete && !isDownloading)
        {
            canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);

            // Check if we were downloading an individual app
            if (isDownloadingIndividualApp)
            {
                canvas_draw_str(canvas, 0, 10, "App downloaded!");
            }
            // Check if we were downloading category info
            else if (currentCategory != CategoryUnknown)
            {
                // Check if there are any apps available in this category
                if (hasAppsAvailable(currentCategory))
                {
                    canvas_draw_str(canvas, 0, 10, "Loading apps...");
                    // Automatically transition to Apps view after a brief moment
                    if (loading)
                    {
                        loading->stop();
                        loading.reset();
                    }
                    // Transition to Apps view
                    currentView = RunViewApps;
                    isDownloadComplete = false;
                    if (isLoadingNextApps)
                    {
                        isLoadingNextApps = false;
                        selectedIndexApps = 0; // Reset to first app of new batch
                    }
                    return; // Early return to avoid showing completion message
                }
                else
                {
                    canvas_draw_str(canvas, 0, 10, "No apps available");
                    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
                    canvas_draw_str(canvas, 0, 60, "Press Back to return");
                }
            }
            else
            {
                canvas_draw_str(canvas, 0, 10, "Download complete!");
            }

            if (loading)
            {
                loading->stop();
                loading.reset();
            }
        }
        // Show fetching message only if no download has started yet
        else if (!isDownloadStarted)
        {
            // If we have a category set, we might be waiting for a download to complete
            if (currentCategory != CategoryUnknown)
            {
                // Give it some time before checking if file exists (allow network/file operations to complete)
                idleCheckCounter++;
                if (idleCheckCounter > 3) // Wait a few cycles before checking
                {
                    char checkPath[128];
                    snprintf(checkPath, sizeof(checkPath), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/%s.json", APP_ID, getAppCategory(currentCategory));
                    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
                    if (storage_file_exists(storage, checkPath))
                    {
                        isDownloadComplete = true;
                        idleCheckCounter = 0; // Reset counter
                    }
                    furi_record_close(RECORD_STORAGE);
                }
            }

            canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
            canvas_draw_str(canvas, 0, 10, "Fetching...");
            if (loading)
            {
                loading->stop();
                loading.reset();
            }
        }
        break;
    case INACTIVE:
        canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
        canvas_draw_str(canvas, 0, 10, "Board not connected...");
        if (loading)
        {
            loading->stop();
            loading.reset();
        }
        break;
    case ISSUE:
        canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
        canvas_draw_str(canvas, 0, 10, "An issue occurred...");
        if (loading)
        {
            loading->stop();
            loading.reset();
        }
        break;
    case RECEIVING:
    case SENDING:
    {
        // Mark that we've actually started receiving/sending - the download has truly begun
        hasDownloadTransitioned = true;
        // Ensure isDownloading is true while actively downloading
        isDownloading = true;
        isDownloadComplete = false;
        if (!loading)
        {
            loading = std::make_unique<Loading>(canvas);
        }
        loading->animate();

        // Show current file progress if processing queue
        if (isProcessingQueue)
        {
            canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
            char fileProgressText[64];
            snprintf(fileProgressText, sizeof(fileProgressText), "File %d/%d", currentDownloadIndex + 1, downloadQueueSize);
            canvas_draw_str(canvas, 5, 15, fileProgressText);
        }

        // Show download progress (bytes received / total bytes)
        size_t bytesReceived = app->getBytesReceived();
        size_t contentLength = app->getContentLength();

        canvas_set_font_custom(canvas, FONT_SIZE_SMALL);

        if (contentLength > 0)
        {
            char progressText[64];
            snprintf(progressText, sizeof(progressText), "%zu/%zu bytes", bytesReceived, contentLength);
            canvas_draw_str(canvas, 5, 57, progressText);

            // Draw progress bar
            int progressBarWidth = 118;
            int progressBarHeight = 4;
            int progressBarX = 5;
            int progressBarY = 59;

            // Draw progress bar background
            canvas_draw_frame(canvas, progressBarX, progressBarY, progressBarWidth, progressBarHeight);

            // Draw progress bar fill
            if (contentLength > 0)
            {
                int fillWidth = (int)((float)bytesReceived / (float)contentLength * (progressBarWidth - 2));
                if (fillWidth > 0)
                {
                    canvas_draw_box(canvas, progressBarX + 1, progressBarY + 1, fillWidth, progressBarHeight - 2);
                }
            }
        }
        else if (bytesReceived > 0)
        {
            char progressText[32];
            snprintf(progressText, sizeof(progressText), "%zu bytes", bytesReceived);
            canvas_draw_str(canvas, 5, 57, progressText);
        }

        break;
    }
    default:
        break;
    };
}

void FlipDownloaderRun::drawGitHubProgress(Canvas *canvas)
{
    FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(appContext);
    furi_check(app);
    canvas_clear(canvas);

    switch (app->getHttpState())
    {
    case IDLE:
        if (!isGitHubDownloadComplete && !isGitHubDownloading)
        {
            // Check if we're in the repository info fetching phase
            if (github_total_files == 0)
            {
                // Give it some time before checking if file exists
                idleCheckCounter++;
                if (idleCheckCounter > 10)
                {
                    char checkPath[256];
                    snprintf(checkPath, sizeof(checkPath), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s-%s-info.json", APP_ID, github_author, github_repo);
                    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
                    if (storage_file_exists(storage, checkPath))
                    {
                        if (githubParseRepositoryInfo(github_author, github_repo))
                        {
                            furi_delay_ms(10);

                            // Load the file count
                            char *fileCountPath = (char *)malloc(256); // Use heap allocation
                            if (!fileCountPath)
                            {
                                FURI_LOG_E(TAG, "Failed to allocate memory for fileCountPath");
                                return;
                            }
                            snprintf(fileCountPath, 256, STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s/%s/file_count.txt", APP_ID, github_author, github_repo);
                            FuriString *fileCountStr = flipper_http_load_from_file(fileCountPath);
                            free(fileCountPath); // Free immediately after use
                            if (fileCountStr)
                            {
                                github_total_files = atoi(furi_string_get_cstr(fileCountStr));
                                furi_string_free(fileCountStr);
                                github_current_file = 0;
                                idleCheckCounter = 0;
                                // Download will be started on the next frame in the main IDLE case logic (helps with overflow)

                                // If no files found, immediately mark as complete
                                if (github_total_files == 0)
                                {
                                    isGitHubDownloadComplete = true;
                                    FURI_LOG_E(TAG, "No files to download, marking as complete immediately");
                                }
                            }
                            else
                            {
                                FURI_LOG_E(TAG, "Failed to load file count after successful parsing");
                                // Set to error state to avoid infinite "Fetching repo info..."
                                github_total_files = -1;
                            }
                        }
                        else
                        {
                            FURI_LOG_E(TAG, "Repository parsing failed");
                            github_total_files = -1;
                        }
                    }
                    furi_record_close(RECORD_STORAGE);
                }
            }
            else if (github_current_file < github_total_files && !isGitHubDownloading)
            {
                // delay before starting next download
                if (github_download_delay_counter < 10)
                {
                    github_download_delay_counter++;
                }
                else
                {
                    // Start next file download
                    if (!githubDownloadRepositoryFile(github_author, github_repo, github_current_file))
                    {
                        // If download failed to start, move to next file
                        github_current_file++;
                        FURI_LOG_E(TAG, "Failed to start download of file %d, moving to next (now at %d)", github_current_file, github_current_file + 1);
                        github_download_delay_counter = 0; // Reset delay for retry
                    }
                }
            }
            else if (github_current_file >= github_total_files)
            {
                // All files downloaded (or no files to download)
                isGitHubDownloadComplete = true;
            }

            canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
            if (github_total_files == -1)
            {
                canvas_draw_str(canvas, 0, 10, "Parsing failed!");
                canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
                canvas_draw_str(canvas, 0, 25, "Check repository URL");
                canvas_draw_str(canvas, 0, 60, "Press Back to return");
            }
            else if (github_total_files == 0)
            {
                // Check if parsing was completed by looking for file count file
                Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
                char *fileCountPath = (char *)malloc(256); // Use heap allocation
                bool parsingComplete = false;

                if (fileCountPath)
                {
                    snprintf(fileCountPath, 256, STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s/%s/file_count.txt", APP_ID, github_author, github_repo);
                    parsingComplete = storage_file_exists(storage, fileCountPath);
                    free(fileCountPath);
                }
                furi_record_close(RECORD_STORAGE);

                // Still fetching
                canvas_draw_str(canvas, 0, 10, "Fetching repo info...");
                canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
                // exit if it's been too long
                if (parsingComplete && idleCheckCounter > 500)
                {
                    canvas_draw_str(canvas, 0, 25, "Timeout! Press Back");
                    canvas_draw_str(canvas, 0, 60, "to return to menu");
                    isGitHubDownloadComplete = true; // Mark as complete to exit
                }
            }
            else
            {
                canvas_draw_str(canvas, 0, 10, "Starting downloads...");
                canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
                char statusText[64];
                snprintf(statusText, sizeof(statusText), "File %d/%d", github_current_file + 1, github_total_files);
                canvas_draw_str(canvas, 0, 25, statusText);
            }
        }
        else if (isGitHubDownloadComplete)
        {
            canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
            canvas_draw_str(canvas, 0, 10, "Download complete!");
            canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
            char statusText[64];
            snprintf(statusText, sizeof(statusText), "Downloaded %d files", github_current_file);
            canvas_draw_str(canvas, 0, 25, statusText);
            canvas_draw_str(canvas, 0, 60, "Press Back to return");
        }
        else if (isGitHubDownloading)
        {
            // Download just completed (transitioned from RECEIVING to IDLE)
            isGitHubDownloading = false;
            idleCheckCounter = 0;

            // Check if this was the repo info download (github_total_files == 0)
            if (github_total_files != 0)
            {
                // This was an individual file download
                github_current_file++;

                // If we've downloaded all files, mark as complete
                if (github_current_file >= github_total_files)
                {
                    isGitHubDownloadComplete = true;
                    FURI_LOG_I(TAG, "All GitHub files downloaded successfully! Final count: %d", github_current_file);
                }
                else
                {
                    FURI_LOG_I(TAG, "More files to download: %d remaining", github_total_files - github_current_file);
                    github_download_delay_counter = 0;
                }
            }
        }
        break;

    case INACTIVE:
        canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
        canvas_draw_str(canvas, 0, 10, "Board not connected...");
        break;

    case ISSUE:
        canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
        canvas_draw_str(canvas, 0, 10, "An issue occurred...");
        break;

    case RECEIVING:
    case SENDING:
    {
        isGitHubDownloading = true;
        isGitHubDownloadComplete = false;
        if (!loading)
        {
            loading = std::make_unique<Loading>(canvas);
        }
        loading->animate();

        canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
        if (github_total_files > 0)
        {
            char progressText[64];
            snprintf(progressText, sizeof(progressText), "File %d/%d", github_current_file + 1, github_total_files);
            canvas_draw_str(canvas, 5, 15, progressText);
        }
        else
        {
            canvas_draw_str(canvas, 5, 15, "Fetching repo...");
        }

        // Show download progress (bytes received / total bytes)
        size_t bytesReceived = app->getBytesReceived();
        size_t contentLength = app->getContentLength();

        canvas_set_font_custom(canvas, FONT_SIZE_SMALL);

        if (contentLength > 0)
        {
            char progressText[64];
            snprintf(progressText, sizeof(progressText), "%zu/%zu bytes", bytesReceived, contentLength);
            canvas_draw_str(canvas, 5, 57, progressText);

            // Draw progress bar
            int progressBarWidth = 118;
            int progressBarHeight = 4;
            int progressBarX = 5;
            int progressBarY = 59;

            // Draw progress bar background
            canvas_draw_frame(canvas, progressBarX, progressBarY, progressBarWidth, progressBarHeight);

            // Draw progress bar fill
            if (contentLength > 0)
            {
                int fillWidth = (int)((float)bytesReceived / (float)contentLength * (progressBarWidth - 2));
                if (fillWidth > 0)
                {
                    canvas_draw_box(canvas, progressBarX + 1, progressBarY + 1, fillWidth, progressBarHeight - 2);
                }
            }
        }
        else if (bytesReceived > 0)
        {
            char progressText[32];
            snprintf(progressText, sizeof(progressText), "%zu bytes", bytesReceived);
            canvas_draw_str(canvas, 5, 57, progressText);
        }

        break;
    }
    default:
        break;
    }
}

void FlipDownloaderRun::drawMainMenu(Canvas *canvas)
{
    const char *menuItems[5] = {"App Catalog", "ESP32 Firmware", "FlipperHTTP Firmware", "VGM Firmware", "GitHub Repo"};
    drawMenu(canvas, selectedIndexMain, menuItems, 5);
}

void FlipDownloaderRun::drawMenu(Canvas *canvas, uint8_t selectedIndex, const char **menuItems, uint8_t menuCount)
{
    canvas_clear(canvas);

    // Draw title
    canvas_set_font_custom(canvas, FONT_SIZE_LARGE);
    const char *title = "FlipDownloader";
    int title_width = canvas_string_width(canvas, title);
    int title_x = (128 - title_width) / 2; // Center the title
    canvas_draw_str(canvas, title_x, 12, title);

    // Draw underline for title
    canvas_draw_line(canvas, title_x, 14, title_x + title_width, 14);

    // Draw decorative horizontal pattern
    for (int i = 0; i < 128; i += 4)
    {
        canvas_draw_dot(canvas, i, 18);
    }

    // Menu items - horizontal scrolling, show one at a time
    canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);

    // Display current menu item centered
    const char *currentItem = menuItems[selectedIndex];
    int text_width = canvas_string_width(canvas, currentItem);
    int text_x = (128 - text_width) / 2;
    int menu_y = 40; // Centered vertically

    // Draw main selection box for current item
    canvas_draw_rbox(canvas, 10, menu_y - 8, 108, 16, 4);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, text_x, menu_y + 4, currentItem);
    canvas_set_color(canvas, ColorBlack);

    // Draw navigation arrows
    if (selectedIndex > 0)
    {
        canvas_draw_str(canvas, 2, menu_y + 4, "<");
    }
    if (selectedIndex < (menuCount - 1))
    {
        canvas_draw_str(canvas, 122, menu_y + 4, ">");
    }

    // Draw page indicator dots
    int dots_spacing = 6;
    int dots_start_x = (128 - (menuCount * dots_spacing)) / 2; // Center dots with spacing
    for (int i = 0; i < menuCount; i++)
    {
        int dot_x = dots_start_x + (i * dots_spacing);
        if (i == selectedIndex)
        {
            canvas_draw_box(canvas, dot_x, menu_y + 12, 4, 4); // Filled dot for current
        }
        else
        {
            canvas_draw_frame(canvas, dot_x, menu_y + 12, 4, 4); // Empty dot for others
        }
    }

    // Draw decorative bottom pattern
    for (int i = 0; i < 128; i += 4)
    {
        canvas_draw_dot(canvas, i, 58);
    }
}

void FlipDownloaderRun::drawMenuESP32(Canvas *canvas)
{
    const char *menuItems[2] = {"Black Magic", "Marauder"};
    drawMenu(canvas, selectedIndexESP32, menuItems, 2);
}

void FlipDownloaderRun::drawMenuFlipperHTTP(Canvas *canvas)
{
    const char *menuItems[10] = {"WiFi Devboard", "ESP32-C3", "ESP32-C5", "ESP32-C6", "ESP32-Cam", "ESP32-S3", "ESP32-WROOM", "ESP32-WROVER", "PicoCalc W", "PicoCalc 2W"};
    drawMenu(canvas, selectedIndexFlipperHTTP, menuItems, 10);
}

void FlipDownloaderRun::drawMenuCatalog(Canvas *canvas)
{
    const char *menuItems[11] = {"Bluetooth", "Games", "GPIO",
                                 "Infrared", "IButton", "Media", "NFC",
                                 "RFID", "SubGHz", "Tools", "USB"};
    drawMenu(canvas, selectedIndexCatalog, menuItems, 11);
}

void FlipDownloaderRun::drawMenuVGM(Canvas *canvas)
{
    const char *menuItems[2] = {"FlipperHTTP", "Picoware"};
    drawMenu(canvas, selectedIndexVGM, menuItems, 2);
}

const char *FlipDownloaderRun::findNthArrayObject(const char *text, uint8_t index)
{
    const char *pos = text;
    uint8_t objectCount = 0;

    // Find the opening bracket of the array using manual search
    const char *arrayStart = nullptr;
    const char *searchPos = pos;
    while (*searchPos != '\0')
    {
        if (*searchPos == '[')
        {
            arrayStart = searchPos;
            break;
        }
        searchPos++;
    }
    if (!arrayStart)
    {
        FURI_LOG_E(TAG, "No array found in JSON");
        return nullptr;
    }

    pos = arrayStart + 1;

    // Find all object opening braces and count them
    while (*pos)
    {
        // Skip whitespace
        while (*pos && (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r' || *pos == ','))
        {
            pos++;
        }

        // Check if we've reached the end of the array
        if (*pos == ']')
        {
            break;
        }

        // Look for opening brace
        if (*pos == '{')
        {
            if (objectCount == index)
            {
                return pos;
            }

            // Skip this entire object
            int braceCount = 1;
            pos++; // Move past the opening brace

            while (*pos && braceCount > 0)
            {
                if (*pos == '"')
                {
                    // Skip string content completely
                    pos++;
                    while (*pos && *pos != '"')
                    {
                        if (*pos == '\\' && *(pos + 1))
                        {
                            pos += 2; // Skip escaped character
                        }
                        else
                        {
                            pos++;
                        }
                    }
                    if (*pos == '"')
                    {
                        pos++; // Skip closing quote
                    }
                }
                else if (*pos == '{')
                {
                    braceCount++;
                    pos++;
                }
                else if (*pos == '}')
                {
                    braceCount--;
                    pos++;
                }
                else
                {
                    pos++;
                }
            }

            objectCount++;
        }
        else
        {
            pos++;
        }
    }

    FURI_LOG_E(TAG, "Failed to find object at index %d, only found %d objects", index, objectCount);
    return nullptr;
}

const char *FlipDownloaderRun::findStringValue(const char *text, const char *key, char *buffer, size_t bufferSize)
{
    // Input validation
    if (!text || !key || !buffer || bufferSize <= 1)
    {
        FURI_LOG_E(TAG, "Invalid parameters to findStringValue");
        return nullptr;
    }

    // Manual key length check
    size_t keyLen = 0;
    while (keyLen < 60 && key[keyLen] != '\0')
        keyLen++;
    if (keyLen == 0 || keyLen >= 60)
    {
        FURI_LOG_E(TAG, "Invalid key length in findStringValue");
        return nullptr;
    }

    // Manual search for key pattern
    char searchKey[64];
    int ret = snprintf(searchKey, sizeof(searchKey), "\"%s\":", key);
    if (ret <= 0 || ret >= (int)sizeof(searchKey))
    {
        FURI_LOG_E(TAG, "Failed to format search key");
        return nullptr;
    }

    size_t searchKeyLen = keyLen + 3; // "key": pattern
    const char *keyPos = nullptr;
    const char *searchPos = text;

    while (*searchPos)
    {
        bool found = true;
        for (size_t i = 0; i < searchKeyLen && searchPos[i] != '\0'; i++)
        {
            if (searchPos[i] != searchKey[i])
            {
                found = false;
                break;
            }
        }
        if (found)
        {
            keyPos = searchPos;
            break;
        }
        searchPos++;
    }

    if (!keyPos)
    {
        return nullptr;
    }

    // Move past the key and find the opening quote
    const char *valueStart = keyPos + searchKeyLen;

    // Manual bounds checking without strlen
    size_t remaining = 1000; // Conservative estimate for remaining text

    while (*valueStart && (*valueStart == ' ' || *valueStart == '\t') && remaining > 0)
    {
        valueStart++;
        remaining--;
    }

    if (*valueStart != '"' || remaining == 0)
    {
        return nullptr;
    }
    valueStart++; // Skip opening quote
    remaining--;

    // Find the closing quote
    const char *valueEnd = valueStart;
    while (*valueEnd && *valueEnd != '"' && remaining > 0)
    {
        if (*valueEnd == '\\' && *(valueEnd + 1) && remaining > 1)
        {
            valueEnd += 2; // Skip escaped characters
            remaining -= 2;
        }
        else
        {
            valueEnd++;
            remaining--;
        }
    }

    if (*valueEnd != '"')
    {
        return nullptr;
    }

    // Copy the value to buffer
    size_t valueLen = valueEnd - valueStart;
    if (valueLen >= bufferSize)
    {
        valueLen = bufferSize - 1;
    }

    if (valueLen > 0)
    {
        memcpy(buffer, valueStart, valueLen);
    }
    buffer[valueLen] = '\0';

    return buffer;
}

const char *FlipDownloaderRun::getAppCategory(FlipperAppCategory category)
{
    switch (category)
    {
    case CategoryBluetooth:
        return "Bluetooth";
    case CategoryGames:
        return "Games";
    case CategoryGPIO:
        return "GPIO";
    case CategoryInfrared:
        return "Infrared";
    case CategoryIButton:
        return "iButton";
    case CategoryMedia:
        return "Media";
    case CategoryNFC:
        return "NFC";
    case CategoryRFID:
        return "RFID";
    case CategorySubGHz:
        return "Sub-GHz";
    case CategoryTools:
        return "Tools";
    case CategoryUSB:
        return "USB";
    default:
        return NULL;
    }
}

FlipperAppCategory FlipDownloaderRun::getAppCategory(const char *category)
{
    if (strcmp(category, "Bluetooth") == 0)
        return CategoryBluetooth;
    if (strcmp(category, "Games") == 0)
        return CategoryGames;
    if (strcmp(category, "GPIO") == 0)
        return CategoryGPIO;
    if (strcmp(category, "Infrared") == 0)
        return CategoryInfrared;
    if (strcmp(category, "iButton") == 0)
        return CategoryIButton;
    if (strcmp(category, "Media") == 0)
        return CategoryMedia;
    if (strcmp(category, "NFC") == 0)
        return CategoryNFC;
    if (strcmp(category, "RFID") == 0)
        return CategoryRFID;
    if (strcmp(category, "Sub-GHz") == 0)
        return CategorySubGHz;
    if (strcmp(category, "Tools") == 0)
        return CategoryTools;
    if (strcmp(category, "USB") == 0)
        return CategoryUSB;
    return CategoryUnknown;
}

const char *FlipDownloaderRun::getAppCategoryId(const char *category)
{
    if (strcmp(category, "Bluetooth") == 0)
        return "64a69817effe1f448a4053b4";
    if (strcmp(category, "Games") == 0)
        return "64971d11be1a76c06747de2f";
    if (strcmp(category, "GPIO") == 0)
        return "64971d106617ba37a4bc79b9";
    if (strcmp(category, "Infrared") == 0)
        return "64971d106617ba37a4bc79b6";
    if (strcmp(category, "iButton") == 0)
        return "64971d11be1a76c06747de29";
    if (strcmp(category, "Media") == 0)
        return "64971d116617ba37a4bc79bc";
    if (strcmp(category, "NFC") == 0)
        return "64971d10be1a76c06747de26";
    if (strcmp(category, "RFID") == 0)
        return "64971d10577d519190ede5c2";
    if (strcmp(category, "Sub-GHz") == 0)
        return "64971d0f6617ba37a4bc79b3";
    if (strcmp(category, "Tools") == 0)
        return "64971d11577d519190ede5c5";
    if (strcmp(category, "USB") == 0)
        return "64971d11be1a76c06747de2c";
    return NULL;
}

const char *FlipDownloaderRun::getAppCategoryId(FlipperAppCategory category)
{
    switch (category)
    {
    case CategoryBluetooth:
        return "64a69817effe1f448a4053b4";
    case CategoryGames:
        return "64971d11be1a76c06747de2f";
    case CategoryGPIO:
        return "64971d106617ba37a4bc79b9";
    case CategoryInfrared:
        return "64971d106617ba37a4bc79b6";
    case CategoryIButton:
        return "64971d11be1a76c06747de29";
    case CategoryMedia:
        return "64971d116617ba37a4bc79bc";
    case CategoryNFC:
        return "64971d10be1a76c06747de26";
    case CategoryRFID:
        return "64971d10577d519190ede5c2";
    case CategorySubGHz:
        return "64971d0f6617ba37a4bc79b3";
    case CategoryTools:
        return "64971d11577d519190ede5c5";
    case CategoryUSB:
        return "64971d11be1a76c06747de2c";
    default:
        return NULL;
    }
}

bool FlipDownloaderRun::githubDownloadRepositoryFile(const char *author, const char *repo, int fileIndex)
{
    FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(appContext);
    if (!app || !author || !repo || fileIndex < 0)
    {
        return false;
    }

    // Helper function to find character manually
    auto findChar = [](const char *str, char target, size_t maxLen) -> const char *
    {
        for (size_t i = 0; i < maxLen && str[i] != '\0'; i++)
        {
            if (str[i] == target)
            {
                return &str[i];
            }
        }
        return nullptr;
    };

    // Helper function to find substring manually
    auto findSubstring = [](const char *haystack, const char *needle, size_t haystackLen, size_t needleLen) -> const char *
    {
        if (needleLen == 0 || needleLen > haystackLen)
            return nullptr;

        for (size_t i = 0; i <= haystackLen - needleLen; i++)
        {
            bool match = true;
            for (size_t j = 0; j < needleLen; j++)
            {
                if (haystack[i + j] != needle[j])
                {
                    match = false;
                    break;
                }
            }
            if (match)
            {
                return &haystack[i];
            }
        }
        return nullptr;
    };

    // Use heap allocation to prevent stack overflow
    char *s_filePath = (char *)malloc(256);
    char *s_chunkBuffer = (char *)malloc(512);
    char *s_url = (char *)malloc(96);
    char *s_name = (char *)malloc(48);
    char *s_path = (char *)malloc(128);

    if (!s_filePath || !s_chunkBuffer || !s_url || !s_name || !s_path)
    {
        FURI_LOG_E(TAG, "Failed to allocate heap buffers");
        if (s_filePath)
            free(s_filePath);
        if (s_chunkBuffer)
            free(s_chunkBuffer);
        if (s_url)
            free(s_url);
        if (s_name)
            free(s_name);
        if (s_path)
            free(s_path);
        return false;
    }

    // Build file path with length check
    int pathLen = snprintf(s_filePath, 256,
                           STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s/%s/all_files.txt",
                           APP_ID, author, repo);
    if (pathLen >= 256)
    {
        FURI_LOG_E(TAG, "Path too long");
        free(s_filePath);
        free(s_chunkBuffer);
        free(s_url);
        free(s_name);
        free(s_path);
        return false;
    }

    // Search patterns
    const char *urlPattern = "\"url\":\"";
    const char *namePattern = "\"name\":\"";
    const size_t urlPatternLen = 7;  // Manual count: "url":"
    const size_t namePatternLen = 8; // Manual count: "name":"

    int foundInstances = 0;
    uint8_t iteration = 0;
    const uint8_t MAX_ITERATIONS = 100;
    size_t currentOffset = 0;
    size_t nextChunkOffset = 0;
    const size_t CHUNK_SIZE = 511;
    const size_t OVERLAP_SIZE = 64; // Overlap to prevent object splitting

    // Load chunks and search for the fileIndex-th instance
    while (iteration < MAX_ITERATIONS)
    {
        memset(s_chunkBuffer, 0, 512);

        // Use nextChunkOffset if set from previous object, otherwise use currentOffset
        size_t readOffset = (nextChunkOffset > 0) ? nextChunkOffset : currentOffset;

        if (!loadFileChunk(s_filePath, s_chunkBuffer, CHUNK_SIZE, readOffset / CHUNK_SIZE))
        {
            FURI_LOG_E(TAG, "End of file or read error at iteration %d, offset %zu", iteration, readOffset);
            break;
        }

        currentOffset = readOffset;

        // Search for url/name pairs in this chunk
        const char *searchPos = s_chunkBuffer;
        bool foundObjectInChunk = false;

        while (searchPos && (searchPos - s_chunkBuffer) < 512)
        {
            // Find URL pattern using manual search
            const char *urlStart = findSubstring(searchPos, urlPattern, 512 - (searchPos - s_chunkBuffer), urlPatternLen);
            if (!urlStart)
            {
                break; // No more URL patterns in this chunk
            }

            // Find corresponding name pattern after URL using manual search
            const char *nameStart = findSubstring(urlStart, namePattern, 512 - (urlStart - s_chunkBuffer), namePatternLen);
            if (!nameStart)
            {
                // Name might be in next chunk, try next iteration
                break;
            }

            // Calculate absolute position for this object in the file
            size_t objectEndPos = currentOffset + (nameStart - s_chunkBuffer) + namePatternLen;

            // Find the end of the name value to get complete object size
            const char *nameValueStart = nameStart + namePatternLen;
            const char *nameEnd = findChar(nameValueStart, '"', 512 - (nameValueStart - s_chunkBuffer));
            if (nameEnd)
            {
                objectEndPos = currentOffset + (nameEnd - s_chunkBuffer) + 1;
            }

            foundObjectInChunk = true;

            // Check if this is the instance we want
            if (foundInstances == fileIndex)
            {
                // Extract URL
                const char *urlValueStart = urlStart + urlPatternLen;
                const char *urlEnd = findChar(urlValueStart, '"', 512 - (urlValueStart - s_chunkBuffer));
                if (!urlEnd)
                {
                    FURI_LOG_E(TAG, "URL end not found");
                    free(s_filePath);
                    free(s_chunkBuffer);
                    free(s_url);
                    free(s_name);
                    free(s_path);
                    return false;
                }

                size_t urlLen = urlEnd - urlValueStart;
                if (urlLen >= 96)
                {
                    FURI_LOG_E(TAG, "URL too long: %zu", urlLen);
                    free(s_filePath);
                    free(s_chunkBuffer);
                    free(s_url);
                    free(s_name);
                    free(s_path);
                    return false;
                }
                memcpy(s_url, urlValueStart, urlLen);
                s_url[urlLen] = '\0';

                // Extract name
                const char *nameValueStart = nameStart + namePatternLen;
                const char *nameEnd = findChar(nameValueStart, '"', 512 - (nameValueStart - s_chunkBuffer));
                if (!nameEnd)
                {
                    FURI_LOG_E(TAG, "Name end not found");
                    free(s_filePath);
                    free(s_chunkBuffer);
                    free(s_url);
                    free(s_name);
                    free(s_path);
                    return false;
                }

                size_t nameLen = nameEnd - nameValueStart;
                if (nameLen >= 48)
                {
                    FURI_LOG_E(TAG, "Name too long: %zu", nameLen);
                    free(s_filePath);
                    free(s_chunkBuffer);
                    free(s_url);
                    free(s_name);
                    free(s_path);
                    return false;
                }
                memcpy(s_name, nameValueStart, nameLen);
                s_name[nameLen] = '\0';

                // Build download path
                int pathLen = snprintf(s_path, 128,
                                       STORAGE_EXT_PATH_PREFIX "/apps_data/%s/%s/%s/%s",
                                       APP_ID, author, repo, s_name);
                if (pathLen >= 128)
                {
                    FURI_LOG_E(TAG, "Download path too long");
                    free(s_filePath);
                    free(s_chunkBuffer);
                    free(s_url);
                    free(s_name);
                    free(s_path);
                    return false;
                }

                // Create directory structure for the file if it contains subdirectories
                char *dirPath = (char *)malloc(128);
                if (dirPath)
                {
                    // Safely copy path
                    size_t pathLen = 0;
                    bool copySuccess = true;

                    while (pathLen < 127 && s_path[pathLen] != '\0')
                    {
                        dirPath[pathLen] = s_path[pathLen];
                        pathLen++;
                    }

                    // Check if we hit the limit (potential overflow)
                    if (pathLen >= 127 || s_path[pathLen] != '\0')
                    {
                        copySuccess = false; // Path too long, skip directory creation
                    }

                    if (copySuccess && pathLen > 0)
                    {
                        dirPath[pathLen] = '\0'; // Null terminate

                        // Find the last slash using a simple backwards loop
                        int lastSlashIndex = -1;
                        for (int i = pathLen - 1; i >= 0; i--)
                        {
                            if (dirPath[i] == '/')
                            {
                                lastSlashIndex = i;
                                break;
                            }
                        }

                        if (lastSlashIndex >= 0)
                        {
                            dirPath[lastSlashIndex] = '\0'; // Remove filename, keeping only directory path
                            createDirectory(dirPath);
                        }
                    }
                    free(dirPath);
                }

                // Set state and download
                isGitHubDownloading = true;
                isGitHubDownloadComplete = false;
                idleCheckCounter = 0;

                // Simple cleanup and download
                deleteFile(s_path);
                bool result = app->httpDownloadFile(s_path, s_url);

                if (!result)
                {
                    isGitHubDownloading = false;
                }

                // Clean up heap allocations before returning
                free(s_filePath);
                free(s_chunkBuffer);
                free(s_url);
                free(s_name);
                free(s_path);

                return result;
            }

            // Move to next potential match and set next chunk offset
            foundInstances++;
            searchPos = nameStart + namePatternLen;

            // Set next chunk offset to continue from end of this object
            nextChunkOffset = objectEndPos;
        }

        // Smart chunk advancement
        if (!foundObjectInChunk)
        {
            // No complete objects found, advance with overlap to avoid splitting
            if (nextChunkOffset > 0)
            {
                currentOffset = nextChunkOffset;
                nextChunkOffset = 0; // Reset for next iteration
            }
            else
            {
                // Advance by chunk size minus overlap to prevent object splitting
                currentOffset += (CHUNK_SIZE - OVERLAP_SIZE);
            }
        }
        else
        {
            // Objects found, nextChunkOffset already set by the loop above
            currentOffset = nextChunkOffset;
            nextChunkOffset = 0; // Reset for next iteration
        }

        iteration++;
    }

    FURI_LOG_E(TAG, "File index %d not found after %d iterations", fileIndex, iteration);

    // Clean up heap allocations before returning
    free(s_filePath);
    free(s_chunkBuffer);
    free(s_url);
    free(s_name);
    free(s_path);

    return false;
}

bool FlipDownloaderRun::githubFetchRepositoryInfo(const char *author, const char *repo)
{
    // Verify parameters before use
    if (!author || !repo)
    {
        FURI_LOG_E(TAG, "NULL parameters in githubFetchRepositoryInfo");
        return false;
    }

    FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(appContext);
    furi_check(app);
    char *dir = (char *)malloc(256); // Move to heap to reduce stack usage
    if (!dir)
    {
        FURI_LOG_E(TAG, "Failed to allocate directory buffer");
        return false;
    }

    // info path: /ext/apps_data/flip_downloader/data/author-repo-info.json (dev info)
    snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s-%s-info.json", APP_ID, author, repo);

    // create a data directory for the author: /ext/apps_data/flip_downloader/data/author (dev info)
    snprintf(dir, 256, STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s", APP_ID, author);
    createDirectory(dir);

    // create a directory for the actual repo info: /ext/apps_data/flip_downloader/data/author/repo (dev info)
    snprintf(dir, 256, STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s/%s", APP_ID, author, repo);
    createDirectory(dir);

    // create a directory for the actual repo info: /ext/apps_data/flip_downloader/author (user info)
    snprintf(dir, 256, STORAGE_EXT_PATH_PREFIX "/apps_data/%s/%s", APP_ID, author);
    createDirectory(dir);

    // create a directory for the actual repo info: /ext/apps_data/flip_downloader/author/repo (user info)
    snprintf(dir, 256, STORAGE_EXT_PATH_PREFIX "/apps_data/%s/%s/%s", APP_ID, author, repo);
    createDirectory(dir);

    // get the contents of the repo (let's use the same dir char from above)
    snprintf(dir, 256, "https://api.github.com/repos/%s/%s/git/trees/HEAD?recursive=1", author, repo);
    deleteFile(savePath); // Ensure we start with a clean slate
    bool result = app->httpDownloadFile(savePath, dir);
    free(dir);
    return result;
}

bool FlipDownloaderRun::githubParseRepositoryInfo(const char *author, const char *repo)
{
    // Basic parameter validation
    if (!author || !repo || !appContext)
    {
        FURI_LOG_E(TAG, "Invalid parameters or appContext");
        return false;
    }

    if (savePath[0] == '\0') // Check if first char is null instead of using strlen
    {
        FURI_LOG_E(TAG, "savePath not initialized");
        return false;
    }

    int fileCount = 0;

    // Use heap-allocated buffers to avoid stack overflow
    char *fileSavePath = (char *)malloc(128);
    char *fileJson = (char *)malloc(512);
    char *pathBuffer = (char *)malloc(128);
    char *chunkBuffer = (char *)malloc(128);

    if (!fileSavePath || !fileJson || !pathBuffer || !chunkBuffer)
    {
        FURI_LOG_E(TAG, "Failed to allocate heap buffers");
        if (fileSavePath)
            free(fileSavePath);
        if (fileJson)
            free(fileJson);
        if (pathBuffer)
            free(pathBuffer);
        if (chunkBuffer)
            free(chunkBuffer);
        return false;
    }

    const size_t CHUNK_SIZE = 127;
    uint8_t iteration = 0;
    const char *searchPattern = "\"path\":\"";
    const size_t patternLen = 8; // Manual count: "path":"
    size_t currentOffset = 0;
    const size_t OVERLAP_SIZE = 32; // Overlap to prevent object splitting

    // Helper function to find character manually
    auto findChar = [](const char *str, char target, size_t maxLen) -> const char *
    {
        for (size_t i = 0; i < maxLen && str[i] != '\0'; i++)
        {
            if (str[i] == target)
            {
                return &str[i];
            }
        }
        return nullptr;
    };

    // Helper function to find substring manually
    auto findSubstring = [](const char *haystack, const char *needle, size_t haystackLen, size_t needleLen) -> const char *
    {
        if (needleLen == 0 || needleLen > haystackLen)
            return nullptr;

        for (size_t i = 0; i <= haystackLen - needleLen; i++)
        {
            bool match = true;
            for (size_t j = 0; j < needleLen; j++)
            {
                if (haystack[i + j] != needle[j])
                {
                    match = false;
                    break;
                }
            }
            if (match)
            {
                return &haystack[i];
            }
        }
        return nullptr;
    };

    // Helper function to get string length manually
    auto getStringLen = [](const char *str, size_t maxLen) -> size_t
    {
        for (size_t i = 0; i < maxLen; i++)
        {
            if (str[i] == '\0')
            {
                return i;
            }
        }
        return maxLen;
    };

    // Use a single buffer to collect all file info, then save once at the end
    // Allocate a smaller initial size and expand as needed to reduce memory usage
    FuriString *allFilesInfo = furi_string_alloc_set_str("");
    if (!allFilesInfo)
    {
        FURI_LOG_E(TAG, "Failed to allocate file collection string");
        free(fileSavePath);
        free(fileJson);
        free(pathBuffer);
        free(chunkBuffer);
        return false;
    }

    // Store processed file paths to avoid duplicates
    FuriString *processedPaths = furi_string_alloc_set_str("");
    if (!processedPaths)
    {
        FURI_LOG_E(TAG, "Failed to allocate processed paths string");
        furi_string_free(allFilesInfo);
        free(fileSavePath);
        free(fileJson);
        free(pathBuffer);
        free(chunkBuffer);
        return false;
    }

    // Search for "path":"value" patterns throughout the file using chunked reading
    while (fileCount < MAX_GITHUB_REPO_FILES)
    {
        if (!loadFileChunk(savePath, chunkBuffer, CHUNK_SIZE, currentOffset / CHUNK_SIZE))
        {
            FURI_LOG_E(TAG, "End of file or read error at iteration %d, offset %zu", iteration, currentOffset);
            break;
        }

        // Ensure buffer is null-terminated and doesn't exceed allocated size
        chunkBuffer[127] = '\0'; // chunkBuffer is 128 bytes, so index 127 is the last valid position
        size_t chunkLen = getStringLen(chunkBuffer, 127);

        if (chunkLen == 0)
        {
            FURI_LOG_E(TAG, "Empty chunk at iteration %d, breaking", iteration);
            break;
        }

        // Search for all "path":" patterns in this chunk
        const char *searchPos = chunkBuffer;
        const char *lastProcessedPos = chunkBuffer; // Track last processed position for chunk advancement
        const char *patternMatch = NULL;
        bool foundObjectInChunk = false;

        while (fileCount < MAX_GITHUB_REPO_FILES)
        {
            patternMatch = findSubstring(searchPos, searchPattern, chunkLen - (searchPos - chunkBuffer), patternLen);
            if (!patternMatch)
                break;

            // Move past the pattern to the start of the value
            const char *valueStart = patternMatch + patternLen;

            // Find the end of the path value (closing quote) using manual search
            const char *valueEnd = findChar(valueStart, '"', 127 - (valueStart - chunkBuffer));
            if (!valueEnd)
            {
                FURI_LOG_E(TAG, "No closing quote found for path value, continuing");
                searchPos = patternMatch + patternLen; // Continue searching
                lastProcessedPos = searchPos;          // Update last processed position
                continue;
            }

            // Calculate path length first
            size_t pathLen = valueEnd - valueStart;
            if (pathLen == 0 || pathLen >= 127) // pathBuffer is 128 bytes
            {
                FURI_LOG_E(TAG, "Invalid path length %zu, skipping", pathLen);
                searchPos = valueEnd + 1;
                lastProcessedPos = searchPos; // Update last processed position
                continue;
            }

            // Extract the path
            memcpy(pathBuffer, valueStart, pathLen);
            pathBuffer[pathLen] = '\0';

            // Check if this is a file (blob) and not a directory (tree)
            // GitHub API structure: {"path":"somefile.txt","mode":"100644","type":"blob","sha":"...","size":123,"url":"..."}
            // Look for "type":"blob" AFTER the current "path" pattern
            const char *typeSearchStart = valueEnd + 1;    // Start searching after the path value
            const char *typeSearchEnd = chunkBuffer + 127; // End of current chunk

            // Find the next "type": pattern after this path
            const char *typePattern = "\"type\":\"";
            const size_t typePatternLen = 8; // Manual count: "type":"

            bool isFile = false;
            const char *typeMatch = findSubstring(typeSearchStart, typePattern, typeSearchEnd - typeSearchStart, typePatternLen);

            if (typeMatch)
            {
                // Check if the type value is "blob" (file) or "tree" (directory)
                const char *typeValueStart = typeMatch + typePatternLen;

                // Check if we have "blob" at this position
                if ((typeValueStart + 4) <= typeSearchEnd &&
                    typeValueStart[0] == 'b' &&
                    typeValueStart[1] == 'l' &&
                    typeValueStart[2] == 'o' &&
                    typeValueStart[3] == 'b')
                {
                    isFile = true;
                }
            }
            else
            {
                // If we can't find the type in this chunk, try to look in a wider context
                // This handles cases where the type might be split across chunks
                FURI_LOG_E(TAG, "Type pattern not found in current chunk for path: %s", pathBuffer);

                // For now, assume it's a file if the path has a file extension
                // This is a fallback for chunk boundary issues
                const char *lastDot = findChar(pathBuffer, '.', pathLen);
                if (lastDot && (pathLen - (lastDot - pathBuffer)) <= 5) // Reasonable extension length
                {
                    isFile = true;
                    FURI_LOG_I(TAG, "Using extension-based detection for file: %s", pathBuffer);
                }
            }

            // Mark that we found an object in this chunk
            foundObjectInChunk = true;

            if (!isFile)
            {
                searchPos = valueEnd + 1;
                lastProcessedPos = searchPos; // Update last processed position
                continue;
            }

            // Check for duplicates
            char *searchPath = (char *)malloc(140); // Buffer for search pattern: |pathBuffer|
            if (!searchPath)
            {
                FURI_LOG_E(TAG, "Failed to allocate searchPath buffer");
                searchPos = valueEnd + 1;
                lastProcessedPos = searchPos; // Update last processed position
                continue;
            }
            snprintf(searchPath, 140, "|%s|", pathBuffer);

            if (furi_string_search_str(processedPaths, searchPath) != FURI_STRING_FAILURE)
            {
                FURI_LOG_E(TAG, "Skipping duplicate file: %s", pathBuffer);
                free(searchPath);
                searchPos = valueEnd + 1;
                lastProcessedPos = searchPos; // Update last processed position
                continue;
            }

            // Add to processed paths
            furi_string_cat_str(processedPaths, searchPath);
            free(searchPath);

            // Create the file info JSON and add to collection
            snprintf(fileJson, 512, "{\"url\":\"https://raw.githubusercontent.com/%s/%s/HEAD/%s\",\"name\":\"%s\"}\n",
                     author, repo, pathBuffer, pathBuffer);

            furi_string_cat_str(allFilesInfo, fileJson);

            fileCount++;

            // Continue searching from after this match
            searchPos = valueEnd + 1;
            lastProcessedPos = searchPos; // Update last processed position
        }

        // Smart chunk advancement
        if (!foundObjectInChunk)
        {
            // No complete objects found, advance by chunk size minus overlap to prevent object splitting
            currentOffset += (CHUNK_SIZE - OVERLAP_SIZE);
        }
        else
        {
            // Objects found, advance to continue from where we left off in this chunk
            // Use the absolute position of the last processed match
            currentOffset = currentOffset + (lastProcessedPos - chunkBuffer);
        }

        iteration++;

        // Safety check to prevent infinite loop
        if (iteration > 200)
        {
            FURI_LOG_W(TAG, "File too large, stopping search at iteration %d", iteration);
            break;
        }
    }

    // Now save all files at once
    snprintf(fileSavePath, 128, STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s/%s/all_files.txt", APP_ID, author, repo);
    saveCharWithPath(fileSavePath, furi_string_get_cstr(allFilesInfo));

    // Clean up collection string
    furi_string_free(allFilesInfo);
    furi_string_free(processedPaths);

    // Save the file count
    snprintf(fileSavePath, 128, STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s/%s/file_count.txt", APP_ID, author, repo);
    char fileCountStr[16];
    snprintf(fileCountStr, sizeof(fileCountStr), "%d", fileCount);
    saveCharWithPath(fileSavePath, fileCountStr);

    // Clean up heap-allocated buffers
    free(fileSavePath);
    free(fileJson);
    free(pathBuffer);
    free(chunkBuffer);

    return true;
}

std::unique_ptr<FlipperAppInfo> FlipDownloaderRun::getAppInfo(FlipperAppCategory category, uint8_t index)
{
    char savePath[128];
    snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/%s.json", APP_ID, getAppCategory(category));
    FuriString *appInfo = flipper_http_load_from_file(savePath);
    if (!appInfo || furi_string_size(appInfo) == 0)
    {
        FURI_LOG_E(APP_ID, "Failed to load app info from %s", savePath);
        return nullptr;
    }

    std::unique_ptr<FlipperAppInfo> appInfoPtr = std::make_unique<FlipperAppInfo>();
    const char *jsonText = furi_string_get_cstr(appInfo);

    // Find the nth object in the array
    const char *objectStart = findNthArrayObject(jsonText, index);
    if (!objectStart)
    {
        FURI_LOG_E(TAG, "Failed to find object at index %d.", index);
        furi_string_free(appInfo);
        return nullptr;
    }

    // Find the end of this object
    const char *objectEnd = objectStart + 1;
    int braceCount = 1;
    while (*objectEnd && braceCount > 0)
    {
        if (*objectEnd == '{')
        {
            braceCount++;
        }
        else if (*objectEnd == '}')
        {
            braceCount--;
        }
        else if (*objectEnd == '"')
        {
            // Skip string content
            objectEnd++;
            while (*objectEnd && *objectEnd != '"')
            {
                if (*objectEnd == '\\' && *(objectEnd + 1))
                {
                    objectEnd += 2;
                }
                else
                {
                    objectEnd++;
                }
            }
        }
        if (*objectEnd)
            objectEnd++;
    }

    // Create a temporary buffer for this object
    size_t objectLen = objectEnd - objectStart;
    char *objectText = (char *)malloc(objectLen + 1);
    if (!objectText)
    {
        FURI_LOG_E(TAG, "Failed to allocate memory for object text.");
        furi_string_free(appInfo);
        return nullptr;
    }
    strncpy(objectText, objectStart, objectLen);
    objectText[objectLen] = '\0';

    // Extract app_id (alias)
    char valueBuffer[256];
    if (!findStringValue(objectText, "alias", valueBuffer, sizeof(valueBuffer)))
    {
        FURI_LOG_E(TAG, "Failed to get app_id for index %d.", index);
        free(objectText);
        furi_string_free(appInfo);
        return nullptr;
    }
    strncpy(appInfoPtr->app_id, valueBuffer, MAX_ID_LENGTH - 1);
    appInfoPtr->app_id[MAX_ID_LENGTH - 1] = '\0';

    // Find current_version object using manual search
    const char *currentVersionStart = nullptr;
    const char *searchTarget = "\"current_version\":";
    const size_t targetLen = 18; // Manual count of "current_version": (18 chars not 19 lol)
    const char *searchPos = objectText;

    while (*searchPos != '\0')
    {
        bool match = true;
        for (size_t i = 0; i < targetLen; i++)
        {
            if (searchPos[i] != searchTarget[i])
            {
                match = false;
                break;
            }
        }
        if (match)
        {
            currentVersionStart = searchPos;
            break;
        }
        searchPos++;
    }

    if (!currentVersionStart)
    {
        FURI_LOG_E(TAG, "Failed to find current_version for index %d.", index);
        free(objectText);
        furi_string_free(appInfo);
        return nullptr;
    }

    // Move to the opening brace of current_version object using manual search
    const char *braceSearchPos = currentVersionStart;
    while (*braceSearchPos != '\0')
    {
        if (*braceSearchPos == '{')
        {
            currentVersionStart = braceSearchPos;
            break;
        }
        braceSearchPos++;
    }
    if (*braceSearchPos == '\0') // Didn't find opening brace
    {
        FURI_LOG_E(TAG, "Failed to find current_version object for index %d.", index);
        free(objectText);
        furi_string_free(appInfo);
        return nullptr;
    }

    // Extract app_name (name)
    if (!findStringValue(currentVersionStart, "name", valueBuffer, sizeof(valueBuffer)))
    {
        FURI_LOG_E(TAG, "Failed to get app_name for index %d.", index);
        free(objectText);
        furi_string_free(appInfo);
        return nullptr;
    }
    strncpy(appInfoPtr->app_name, valueBuffer, MAX_APP_NAME_LENGTH - 1);
    appInfoPtr->app_name[MAX_APP_NAME_LENGTH - 1] = '\0';

    // Extract app_description (short_description)
    if (!findStringValue(currentVersionStart, "short_description", valueBuffer, sizeof(valueBuffer)))
    {
        FURI_LOG_E(TAG, "Failed to get app_description for index %d.", index);
        free(objectText);
        furi_string_free(appInfo);
        return nullptr;
    }
    strncpy(appInfoPtr->app_description, valueBuffer, MAX_APP_DESCRIPTION_LENGTH - 1);
    appInfoPtr->app_description[MAX_APP_DESCRIPTION_LENGTH - 1] = '\0';

    // Extract app_version (version)
    if (!findStringValue(currentVersionStart, "version", valueBuffer, sizeof(valueBuffer)))
    {
        FURI_LOG_E(TAG, "Failed to get app_version for index %d.", index);
        free(objectText);
        furi_string_free(appInfo);
        return nullptr;
    }
    strncpy(appInfoPtr->app_version, valueBuffer, MAX_APP_VERSION_LENGTH - 1);
    appInfoPtr->app_version[MAX_APP_VERSION_LENGTH - 1] = '\0';

    // Extract app_build_id (_id)
    if (!findStringValue(currentVersionStart, "_id", valueBuffer, sizeof(valueBuffer)))
    {
        FURI_LOG_E(TAG, "Failed to get _id for index %d.", index);
        free(objectText);
        furi_string_free(appInfo);
        return nullptr;
    }
    strncpy(appInfoPtr->app_build_id, valueBuffer, MAX_ID_LENGTH - 1);
    appInfoPtr->app_build_id[MAX_ID_LENGTH - 1] = '\0';

    free(objectText);
    furi_string_free(appInfo);
    return appInfoPtr;
}

bool FlipDownloaderRun::hasAppsAvailable(FlipperAppCategory category)
{
    char savePath[128];
    snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/%s.json", APP_ID, getAppCategory(category));
    FuriString *appInfo = flipper_http_load_from_file(savePath);
    if (!appInfo || furi_string_size(appInfo) == 0)
    {
        return false;
    }

    const char *jsonText = furi_string_get_cstr(appInfo);

    // Find the opening bracket of the array using manual search
    const char *arrayStart = nullptr;
    const char *searchPos = jsonText;
    while (*searchPos != '\0')
    {
        if (*searchPos == '[')
        {
            arrayStart = searchPos;
            break;
        }
        searchPos++;
    }
    if (!arrayStart)
    {
        furi_string_free(appInfo);
        return false;
    }

    // Check if there's at least one object in the array
    const char *pos = arrayStart + 1;
    while (*pos && (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r'))
    {
        pos++;
    }

    bool hasApps = (*pos == '{'); // If we find an opening brace, there's at least one app

    furi_string_free(appInfo);
    return hasApps;
}

bool FlipDownloaderRun::init(void *appContext)
{
    if (!appContext)
    {
        FURI_LOG_E("FlipDownloaderRun", "App context is null");
        return false;
    }
    this->appContext = appContext;
    savePath[0] = '\0';
    github_author[0] = '\0';
    github_repo[0] = '\0';
    github_current_file = 0;
    github_total_files = 0;

    // Initialize download queue
    clearDownloadQueue();

    return true;
}

bool FlipDownloaderRun::loadFileChunk(const char *filePath, char *buffer, size_t sizeOfChunk, uint8_t iteration)
{

    if (!filePath || !buffer || sizeOfChunk == 0)
    {
        FURI_LOG_E(TAG, "Invalid parameters for loadFileChunk");
        return false;
    }

    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    if (!storage)
    {
        FURI_LOG_E(TAG, "Failed to open storage record");
        return false;
    }

    File *file = storage_file_alloc(storage);
    if (!file)
    {
        FURI_LOG_E(TAG, "Failed to allocate file");
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Open the file for reading
    if (!storage_file_open(file, filePath, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        FURI_LOG_E(TAG, "Failed to open file for reading: %s", filePath);
        return false;
    }

    // Change the current access position in a file.
    if (!storage_file_seek(file, iteration * sizeOfChunk, true))
    {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        FURI_LOG_E(TAG, "Failed to seek file: %s", filePath);
        return false;
    }

    // Check whether the current access position is at the end of the file.
    if (storage_file_eof(file))
    {
        FURI_LOG_E(TAG, "End of file reached: %s", filePath);
        return false;
    }

    // Read data into the buffer
    size_t read_count = storage_file_read(file, buffer, sizeOfChunk);
    if (storage_file_get_error(file) != FSE_OK)
    {
        FURI_LOG_E(TAG, "Error reading from file.");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // ensure we don't go out of bounds
    if (read_count > 0 && read_count < sizeOfChunk)
    {
        buffer[read_count] = '\0'; // Null-terminate after the last read character
    }
    else if (read_count >= sizeOfChunk && sizeOfChunk > 0)
    {
        buffer[sizeOfChunk - 1] = '\0'; // Use last byte for null terminator
    }
    else
    {
        buffer[0] = '\0'; // Empty buffer
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return read_count > 0;
}

bool FlipDownloaderRun::loadFileChunkFromOffset(const char *filePath, char *buffer, size_t bufferSize, size_t offset)
{
    if (!filePath || !buffer || bufferSize == 0)
    {
        FURI_LOG_E(TAG, "Invalid parameters for loadFileChunkFromOffset");
        return false;
    }

    // Additional safety checks
    if (filePath[0] == '\0' || bufferSize > 8192) // Reasonable size limit
    {
        FURI_LOG_E(TAG, "Invalid file path or buffer size");
        return false;
    }

    // Open the storage record
    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    if (!storage)
    {
        FURI_LOG_E(TAG, "Failed to open storage record");
        return false;
    }

    File *file = storage_file_alloc(storage);
    if (!file)
    {
        FURI_LOG_E(TAG, "Failed to allocate file");
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Open the file for reading
    if (!storage_file_open(file, filePath, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        FURI_LOG_E(TAG, "Failed to open file for reading: %s", filePath);
        return false;
    }

    // Seek to the specified offset
    if (!storage_file_seek(file, offset, true))
    {
        FURI_LOG_E(TAG, "Failed to seek to offset %zu in file: %s", offset, filePath);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Check if we're at end of file
    if (storage_file_eof(file))
    {
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Read data into the buffer
    size_t read_count = storage_file_read(file, buffer, bufferSize - 1); // -1 for null terminator
    if (storage_file_get_error(file) != FSE_OK)
    {
        FURI_LOG_E(TAG, "Error reading from file at offset %zu", offset);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Null-terminate the buffer and add safety check
    if (read_count < bufferSize)
    {
        buffer[read_count] = '\0';
    }
    else
    {
        buffer[bufferSize - 1] = '\0';
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return read_count > 0;
}

void FlipDownloaderRun::processDownloadQueue()
{
    if (!isProcessingQueue || currentDownloadIndex >= downloadQueueSize)
    {
        // Queue is complete
        isProcessingQueue = false;
        isDownloadComplete = true;
        return;
    }

    // Wait until the current download is finished before starting the next one
    if (isDownloading)
    {
        return;
    }

    FlipDownloaderDownloadLink currentLink = downloadQueue[currentDownloadIndex];

    // Download the current file
    downloadFile(currentLink);
}

void FlipDownloaderRun::queueDownload(FlipDownloaderDownloadLink link)
{
    if (downloadQueueSize < 10) // Prevent overflow
    {
        downloadQueue[downloadQueueSize] = link;
        downloadQueueSize++;
    }
}

bool FlipDownloaderRun::saveCharWithPath(const char *fullPath, const char *value)
{
    if (!value)
    {
        return false;
    }

    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    File *file = storage_file_alloc(storage);

    // Open the file in write mode
    if (!storage_file_open(file, fullPath, FSAM_WRITE, FSOM_CREATE_ALWAYS))
    {
        FURI_LOG_E(HTTP_TAG, "Failed to open file for writing: %s", fullPath);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Write the data to the file
    size_t data_size = 0;
    while (value[data_size] != '\0')
        data_size++; // Manual length calculation
    data_size += 1;  // Include null terminator
    if (storage_file_write(file, value, data_size) != data_size)
    {
        FURI_LOG_E(HTTP_TAG, "Failed to append data to file");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return true;
}

void FlipDownloaderRun::startDownloadQueue()
{
    if (downloadQueueSize > 0)
    {
        currentDownloadIndex = 0;
        isProcessingQueue = true;
        isDownloading = false;
        isDownloadComplete = false;
        processDownloadQueue();
    }
}

bool FlipDownloaderRun::startTextInput(uint32_t view)
{
    if (!keyboard)
    {
        keyboard = std::make_unique<Keyboard>();
    }
    else
    {
        keyboard.reset();
        keyboard = std::make_unique<Keyboard>();
    }

    if (view == RunViewGitHubAuthor)
    {
        // Clear the author buffer for fresh input
        github_author[0] = '\0';
        currentView = RunViewGitHubAuthor;
    }
    else if (view == RunViewGitHubRepo)
    {
        // Clear the repo buffer for fresh input
        github_repo[0] = '\0';
        currentView = RunViewGitHubRepo;
    }

    return true;
}

void FlipDownloaderRun::startGitHubDownload()
{
    // Initialize GitHub download state
    isGitHubDownloading = false;
    isGitHubDownloadComplete = false;
    github_current_file = 0;
    github_total_files = 0;
    github_download_delay_counter = 0;

    // Manual length calculation
    size_t author_len = 0;
    while (author_len < sizeof(github_author) && github_author[author_len] != '\0')
        author_len++;
    size_t repo_len = 0;
    while (repo_len < sizeof(github_repo) && github_repo[repo_len] != '\0')
        repo_len++;

    if (author_len == 0 || repo_len == 0)
    {
        FURI_LOG_E(APP_ID, "GitHub author or repo is empty");
        currentView = RunViewMainMenu;
        return;
    }

    currentView = RunViewGitHubProgress;
    github_current_file = 0;
    github_total_files = 0;
    isDownloading = false;
    isDownloadComplete = false;
    idleCheckCounter = 0;

    // Start by fetching repository info
    githubFetchRepositoryInfo(github_author, github_repo);
}

void FlipDownloaderRun::setCategorySavePath(FlipperAppCategory category)
{
    snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/%s.json", APP_ID, getAppCategory(category));
}

void FlipDownloaderRun::setSavePath(FlipDownloaderDownloadLink link)
{
    uint8_t firmwareType = 0; // 0 for ESP32 and 1 for VGM

    switch (link)
    {
    case DownloadLinkVGMFlipperHTTP:
    case DownloadLinkVGMPicoware:
        firmwareType = 1; // VGM firmware
        break;
    default:
        firmwareType = 0; // ESP32 firmware
        break;
    }

    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    char directory_path[128];

    // make initial directories if needed
    if (firmwareType == 0) // save in ESP32 flasher directory (esp32 firmware)
    {
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher");
        storage_common_mkdir(storage, directory_path);
    }
    else if (firmwareType == 1) // install in app_data directory (vgm firmware)
    {
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/vgm");
        storage_common_mkdir(storage, directory_path);
    }

    // make final directories and return the full path
    switch (link)
    {
        // Marauder (ESP32)
    case DownloadLinkFirmwareMarauderLink1:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/Marauder");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/Marauder/esp32_marauder.ino.bootloader.bin");
        break;
    case DownloadLinkFirmwareMarauderLink2:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/Marauder");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/Marauder/esp32_marauder.ino.partitions.bin");
        break;
    case DownloadLinkFirmwareMarauderLink3:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/Marauder");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/Marauder/esp32_marauder_v1_6_2_20250531_flipper.bin");
        break;
        // Black Magic (ESP32)
    case DownloadLinkFirmwareBlackMagicLink1:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/Black Magic");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/Black Magic/bootloader.bin");
        break;
    case DownloadLinkFirmwareBlackMagicLink2:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/Black Magic");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/Black Magic/partition-table.bin");
        break;
    case DownloadLinkFirmwareBlackMagicLink3:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/Black Magic");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/Black Magic/blackmagic.bin");
        break;
        // FlipperHTTP (WiFi Devboard)
    case DownloadLinkFlipperHTTPWiFiDevboardLink1:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/WiFi-Devboard");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/WiFi-Devboard/flipper_http_bootloader.bin");
        break;
    case DownloadLinkFlipperHTTPWiFiDevboardLink2:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/WiFi-Devboard");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/WiFi-Devboard/flipper_http_firmware_a.bin");
        break;
    case DownloadLinkFlipperHTTPWiFiDevboardLink3:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/WiFi-Devboard");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/WiFi-Devboard/flipper_http_partitions.bin");
        break;
        // FlipperHTTP (ESP32-C3)
    case DownloadLinkFlipperHTTPESP32C3Link1:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-C3");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-C3/flipper_http_bootloader_esp32_c3.bin");
        break;
    case DownloadLinkFlipperHTTPESP32C3Link2:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-C3");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-C3/flipper_http_firmware_a_esp32_c3.bin");
        break;
    case DownloadLinkFlipperHTTPESP32C3Link3:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-C3");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-C3/flipper_http_partitions_esp32_c3.bin");
        break;
        // FlipperHTTP (ESP32-C5)
    case DownloadLinkFlipperHTTPESP32C5Link1:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-C5");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-C5/flipper_http_bootloader_esp32_c5.bin");
        break;
    case DownloadLinkFlipperHTTPESP32C5Link2:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-C5");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-C5/flipper_http_firmware_a_esp32_c5.bin");
        break;
    case DownloadLinkFlipperHTTPESP32C5Link3:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-C5");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-C5/flipper_http_partitions_esp32_c5.bin");
        break;
        // FlipperHTTP (ESP32-C6)
    case DownloadLinkFlipperHTTPESP32C6Link1:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-C6");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-C6/flipper_http_bootloader_esp32_c6.bin");
        break;
    case DownloadLinkFlipperHTTPESP32C6Link2:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-C6");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-C6/flipper_http_firmware_a_esp32_c6.bin");
        break;
    case DownloadLinkFlipperHTTPESP32C6Link3:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-C6");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-C6/flipper_http_partitions_esp32_c6.bin");
        break;
        // FlipperHTTP (ESP32-Cam)
    case DownloadLinkFlipperHTTPESP32CamLink1:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-Cam");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-Cam/flipper_http_bootloader_esp32_cam.bin");
        break;
    case DownloadLinkFlipperHTTPESP32CamLink2:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-Cam");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-Cam/flipper_http_firmware_a_esp32_cam.bin");
        break;
    case DownloadLinkFlipperHTTPESP32CamLink3:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-Cam");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-Cam/flipper_http_partitions_esp32_cam.bin");
        break;
        // FlipperHTTP (ESP32-S3)
    case DownloadLinkFlipperHTTPESP32S3Link1:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-S3");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-S3/flipper_http_bootloader_esp32_s3.bin");
        break;
    case DownloadLinkFlipperHTTPESP32S3Link2:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-S3");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-S3/flipper_http_firmware_a_esp32_s3.bin");
        break;
    case DownloadLinkFlipperHTTPESP32S3Link3:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-S3");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-S3/flipper_http_partitions_esp32_s3.bin");
        break;
        // FlipperHTTP (ESP32-WROOM)
    case DownloadLinkFlipperHTTPESP32WROOMLink1:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-WROOM");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-WROOM/flipper_http_bootloader_esp32_wroom.bin");
        break;
    case DownloadLinkFlipperHTTPESP32WROOMLink2:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-WROOM");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-WROOM/flipper_http_firmware_a_esp32_wroom.bin");
        break;
    case DownloadLinkFlipperHTTPESP32WROOMLink3:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-WROOM");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-WROOM/flipper_http_partitions_esp32_wroom.bin");
        break;
        // FlipperHTTP (ESP32-WROVER)
    case DownloadLinkFlipperHTTPESP32WROVERLink1:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-WROVER");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-WROVER/flipper_http_bootloader_esp32_wrover.bin");
        break;
    case DownloadLinkFlipperHTTPESP32WROVERLink2:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-WROVER");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-WROVER/flipper_http_firmware_a_esp32_wrover.bin");
        break;
    case DownloadLinkFlipperHTTPESP32WROVERLink3:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-WROVER");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/ESP32-WROVER/flipper_http_partitions_esp32_wrover.bin");
        break;
        // FlipperHTTP (PicoCalc W)
    case DownloadLinkFlipperHTTPPicoCalcWLink1:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/PicoCalcW");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/PicoCalcW/flipper_http_picocalc_w.uf2");
        break;
        // FlipperHTTP (PicoCalc 2W)
    case DownloadLinkFlipperHTTPPicoCalc2WLink1:
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP");
        storage_common_mkdir(storage, directory_path);
        snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/PicoCalc2W");
        storage_common_mkdir(storage, directory_path);
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/esp_flasher/FlipperHTTP/PicoCalc2W/flipper_http_picocalc_2w.uf2");
        break;
        // FlipperHTTP (VGM)
    case DownloadLinkVGMFlipperHTTP:
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/vgm/flipper_http_vgm_c++.uf2");
        break;
        // Picoware (VGM)
    case DownloadLinkVGMPicoware:
        snprintf(savePath, sizeof(savePath), STORAGE_EXT_PATH_PREFIX "/apps_data/vgm/Picoware-VGM.uf2");
        break;
    default:
        break;
    }

    furi_record_close(RECORD_STORAGE);
}

void FlipDownloaderRun::updateDraw(Canvas *canvas)
{
    switch (currentView)
    {
    case RunViewMainMenu:
        drawMainMenu(canvas);
        break;
    case RunViewCatalog:
        drawMenuCatalog(canvas);
        break;
    case RunViewESP32:
        drawMenuESP32(canvas);
        break;
    case RunViewFlipperHTTP:
        drawMenuFlipperHTTP(canvas);
        break;
    case RunViewVGM:
        drawMenuVGM(canvas);
        break;
    case RunViewGitHubAuthor:
        if (keyboard)
        {
            keyboard->draw(canvas, "Author:");
        }
        break;
    case RunViewGitHubRepo:
        if (keyboard)
        {
            keyboard->draw(canvas, "Repo:");
        }
        break;
    case RunViewGitHubProgress:
        drawGitHubProgress(canvas);
        break;
    case RunViewDownloadProgress:
        drawDownloadProgress(canvas);
        break;
    case RunViewApps:
        drawApps(canvas);
        break;
    default:
        break;
    }
}

void FlipDownloaderRun::updateInput(InputEvent *event)
{
    if (event->type == InputTypeShort || event->type == InputTypeLong)
    {
        //  download progress view - only allow back button
        if (currentView == RunViewDownloadProgress)
        {
            switch (event->key)
            {
            case InputKeyBack:
                // Clear the download queue when going back
                clearDownloadQueue();

                // Return to appropriate view based on what was being downloaded
                if (isLoadingNextApps)
                {
                    isLoadingNextApps = false;
                    currentView = RunViewApps;
                }
                else if (isDownloadingIndividualApp)
                {
                    isDownloadingIndividualApp = false;
                    isDownloadComplete = false;
                    currentView = RunViewApps;
                }
                else if (currentCategory != CategoryUnknown)
                {
                    currentView = RunViewCatalog;
                    currentCategory = CategoryUnknown;
                    isDownloadComplete = false;
                }
                else
                {
                    currentView = RunViewMainMenu;
                }
                break;
            default:
                // Ignore all other inputs during download
                break;
            }
            return;
        }

        // Get current selected index and menu count based on current view
        uint8_t *currentSelectedIndex = 0;
        uint8_t menuCount = 0;

        switch (currentView)
        {
        case RunViewMainMenu:
            currentSelectedIndex = &selectedIndexMain;
            menuCount = 5;
            break;
        case RunViewCatalog:
            currentSelectedIndex = &selectedIndexCatalog;
            menuCount = 11;
            break;
        case RunViewESP32:
            currentSelectedIndex = &selectedIndexESP32;
            menuCount = 2;
            break;
        case RunViewFlipperHTTP:
            currentSelectedIndex = &selectedIndexFlipperHTTP;
            menuCount = 10;
            break;
        case RunViewVGM:
            currentSelectedIndex = &selectedIndexVGM;
            menuCount = 2;
            break;
        case RunViewGitHubAuthor:
        case RunViewGitHubRepo:
        case RunViewGitHubProgress:
            // Special handling for GitHub views
            break;
        case RunViewApps:
            currentSelectedIndex = &selectedIndexApps;
            menuCount = MAX_RECEIVED_APPS;
            break;
        default:
            return; // Invalid view
        }

        //  GitHub views
        if (currentView == RunViewGitHubAuthor ||
            currentView == RunViewGitHubRepo || currentView == RunViewGitHubProgress)
        {
            if (currentView == RunViewGitHubAuthor || currentView == RunViewGitHubRepo)
            {
                // Virtual keyboard input handling
                if (keyboard)
                {
                    switch (event->key)
                    {
                    case InputKeyBack:
                        if (currentView == RunViewGitHubRepo)
                        {
                            currentView = RunViewGitHubAuthor;
                        }
                        else if (currentView == RunViewGitHubAuthor)
                        {
                            currentView = RunViewMainMenu;
                        }
                        break;

                    default:
                        if (keyboard)
                        {
                            if (keyboard->handleInput(event))
                            {
                                // Handle successful input
                                if (currentView == RunViewGitHubAuthor)
                                {
                                    // Move to repo input
                                    snprintf(github_author, sizeof(github_author), "%s", keyboard->getText());
                                    currentView = RunViewGitHubRepo;
                                    startTextInput(RunViewGitHubRepo);
                                }
                                else if (currentView == RunViewGitHubRepo)
                                {
                                    // Start download
                                    snprintf(github_repo, sizeof(github_repo), "%s", keyboard->getText());
                                    startGitHubDownload();
                                }
                            }
                        }
                        break;
                    };
                }
            }
            else if (currentView == RunViewGitHubProgress)
            {
                switch (event->key)
                {
                case InputKeyBack:
                    currentView = RunViewMainMenu;
                    github_current_file = 0;
                    github_total_files = 0;
                    isGitHubDownloading = false;
                    isGitHubDownloadComplete = false;
                    break;
                default:
                    break;
                }
            }
            return;
        }

        switch (event->key)
        {
        case InputKeyLeft:
        case InputKeyRight:
            // Only handle left/right for horizontal menu views (not Apps view)
            if (currentView != RunViewApps && currentView != RunViewGitHubAuthor &&
                currentView != RunViewGitHubRepo && currentView != RunViewGitHubProgress)
            {
                if (event->key == InputKeyLeft)
                {
                    if (*currentSelectedIndex > 0)
                    {
                        (*currentSelectedIndex)--;
                    }
                    else
                    {
                        *currentSelectedIndex = menuCount - 1; // Wrap to last item
                    }
                }
                else // InputKeyRight
                {
                    if (*currentSelectedIndex < (menuCount - 1))
                    {
                        (*currentSelectedIndex)++;
                    }
                    else
                    {
                        *currentSelectedIndex = 0; // Wrap to first item
                    }
                }
            }
            break;
        case InputKeyUp:
        case InputKeyDown:
            // Handle vertical navigation
            if (currentView == RunViewApps)
            {
                // Special handling for apps view with pagination
                if (event->key == InputKeyUp)
                {
                    if (selectedIndexApps > 0)
                    {
                        selectedIndexApps--;
                    }
                }
                else // InputKeyDown
                {
                    if (selectedIndexApps < (MAX_RECEIVED_APPS - 1))
                    {
                        selectedIndexApps++;
                    }
                    else
                    {
                        // At the end of current batch, load next batch
                        if (!isLoadingNextApps)
                        {
                            isLoadingNextApps = true;
                            currentAppIteration++;
                            currentView = RunViewDownloadProgress;
                            downloadCategoryInfo(currentCategory, currentAppIteration * MAX_RECEIVED_APPS);
                        }
                    }
                }
            }
            else
            {
                // Also allow up/down for navigation in other views
                if (event->key == InputKeyUp)
                {
                    if (*currentSelectedIndex > 0)
                    {
                        (*currentSelectedIndex)--;
                    }
                    else
                    {
                        *currentSelectedIndex = menuCount - 1;
                    }
                }
                else
                {
                    if (*currentSelectedIndex < (menuCount - 1))
                    {
                        (*currentSelectedIndex)++;
                    }
                    else
                    {
                        *currentSelectedIndex = 0;
                    }
                }
            }
            break;
        case InputKeyOk:
            // Handle selection based on current view and selectedIndex
            switch (currentView)
            {
            case RunViewMainMenu:
                switch (selectedIndexMain)
                {
                case 0: // App Catalog
                    currentView = RunViewCatalog;
                    break;
                case 1: // ESP32 Firmware
                    currentView = RunViewESP32;
                    break;
                case 2: // FlipperHTTP Firmware
                    currentView = RunViewFlipperHTTP;
                    break;
                case 3: // VGM Firmware
                    currentView = RunViewVGM;
                    break;
                case 4: // GitHub Repo
                    currentView = RunViewGitHubAuthor;
                    startTextInput(RunViewGitHubAuthor);
                    break;
                }
                break;
            case RunViewESP32:
                switch (selectedIndexESP32)
                {
                case 0:                                    // Black Magic - download 3 files
                    currentView = RunViewDownloadProgress; // Switch to download view
                    clearDownloadQueue();
                    queueDownload(DownloadLinkFirmwareBlackMagicLink1);
                    queueDownload(DownloadLinkFirmwareBlackMagicLink2);
                    queueDownload(DownloadLinkFirmwareBlackMagicLink3);
                    startDownloadQueue();
                    break;
                case 1:                                    // Marauder - download 3 files
                    currentView = RunViewDownloadProgress; // Switch to download view
                    clearDownloadQueue();
                    queueDownload(DownloadLinkFirmwareMarauderLink1);
                    queueDownload(DownloadLinkFirmwareMarauderLink2);
                    queueDownload(DownloadLinkFirmwareMarauderLink3);
                    startDownloadQueue();
                    break;
                }
                break;
            case RunViewFlipperHTTP:
                switch (selectedIndexFlipperHTTP) // {"WiFi Devboard", "ESP32-C3", "ESP32-C5", "ESP32-C6", "ESP32-Cam", "ESP32-S3", "ESP32-WROOM", "ESP32-WROVER", "PicoCalc W", "PicoCalc 2W"};
                {
                case 0:                                    // FlipperHTTP (WiFi Devboard)
                    currentView = RunViewDownloadProgress; // Switch to download view
                    clearDownloadQueue();
                    queueDownload(DownloadLinkFlipperHTTPWiFiDevboardLink1);
                    queueDownload(DownloadLinkFlipperHTTPWiFiDevboardLink2);
                    queueDownload(DownloadLinkFlipperHTTPWiFiDevboardLink3);
                    startDownloadQueue();
                    break;
                case 1:                                    // FlipperHTTP (ESP32-C3)
                    currentView = RunViewDownloadProgress; // Switch to download view
                    clearDownloadQueue();
                    queueDownload(DownloadLinkFlipperHTTPESP32C3Link1);
                    queueDownload(DownloadLinkFlipperHTTPESP32C3Link2);
                    queueDownload(DownloadLinkFlipperHTTPESP32C3Link3);
                    startDownloadQueue();
                    break;
                case 2:                                    // FlipperHTTP (ESP32-C5)
                    currentView = RunViewDownloadProgress; // Switch to download view
                    clearDownloadQueue();
                    queueDownload(DownloadLinkFlipperHTTPESP32C5Link1);
                    queueDownload(DownloadLinkFlipperHTTPESP32C5Link2);
                    queueDownload(DownloadLinkFlipperHTTPESP32C5Link3);
                    startDownloadQueue();
                    break;
                case 3:                                    // FlipperHTTP (ESP32-C6)
                    currentView = RunViewDownloadProgress; // Switch to download view
                    clearDownloadQueue();
                    queueDownload(DownloadLinkFlipperHTTPESP32C6Link1);
                    queueDownload(DownloadLinkFlipperHTTPESP32C6Link2);
                    queueDownload(DownloadLinkFlipperHTTPESP32C6Link3);
                    startDownloadQueue();
                    break;
                case 4:                                    // FlipperHTTP (ESP32-Cam)
                    currentView = RunViewDownloadProgress; // Switch to download view
                    clearDownloadQueue();
                    queueDownload(DownloadLinkFlipperHTTPESP32CamLink1);
                    queueDownload(DownloadLinkFlipperHTTPESP32CamLink2);
                    queueDownload(DownloadLinkFlipperHTTPESP32CamLink3);
                    startDownloadQueue();
                    break;
                case 5:                                    // FlipperHTTP (ESP32-S3)
                    currentView = RunViewDownloadProgress; // Switch to download view
                    clearDownloadQueue();
                    queueDownload(DownloadLinkFlipperHTTPESP32S3Link1);
                    queueDownload(DownloadLinkFlipperHTTPESP32S3Link2);
                    queueDownload(DownloadLinkFlipperHTTPESP32S3Link3);
                    startDownloadQueue();
                    break;
                case 6:                                    // FlipperHTTP (ESP32-WROOM)
                    currentView = RunViewDownloadProgress; // Switch to download view
                    clearDownloadQueue();
                    queueDownload(DownloadLinkFlipperHTTPESP32WROOMLink1);
                    queueDownload(DownloadLinkFlipperHTTPESP32WROOMLink2);
                    queueDownload(DownloadLinkFlipperHTTPESP32WROOMLink3);
                    startDownloadQueue();
                    break;
                case 7:                                    // FlipperHTTP (ESP32-WROVER)
                    currentView = RunViewDownloadProgress; // Switch to download view
                    clearDownloadQueue();
                    queueDownload(DownloadLinkFlipperHTTPESP32WROVERLink1);
                    queueDownload(DownloadLinkFlipperHTTPESP32WROVERLink2);
                    queueDownload(DownloadLinkFlipperHTTPESP32WROVERLink3);
                    startDownloadQueue();
                    break;
                case 8:                                    // FlipperHTTP (PicoCalc W) - download 1 file
                    currentView = RunViewDownloadProgress; // Switch to download view
                    clearDownloadQueue();
                    downloadFile(DownloadLinkFlipperHTTPPicoCalcWLink1);
                    break;
                case 9:                                    // FlipperHTTP (PicoCalc 2W) - download 1 file
                    currentView = RunViewDownloadProgress; // Switch to download view
                    clearDownloadQueue();
                    downloadFile(DownloadLinkFlipperHTTPPicoCalc2WLink1);
                    break;
                default:
                    break;
                }
                break;
            case RunViewVGM:
                switch (selectedIndexVGM)
                {
                case 0:                                    // FlipperHTTP - download 1 file
                    currentView = RunViewDownloadProgress; // Switch to download view
                    downloadFile(DownloadLinkVGMFlipperHTTP);
                    break;
                case 1:                                    // Picoware - download 1 file
                    currentView = RunViewDownloadProgress; // Switch to download view
                    downloadFile(DownloadLinkVGMPicoware);
                    break;
                }
                break;
            case RunViewCatalog:
                // Map selectedIndexCatalog to FlipperAppCategory
                FlipperAppCategory selectedCategory;
                switch (selectedIndexCatalog)
                {
                case 0:
                    selectedCategory = CategoryBluetooth;
                    break;
                case 1:
                    selectedCategory = CategoryGames;
                    break;
                case 2:
                    selectedCategory = CategoryGPIO;
                    break;
                case 3:
                    selectedCategory = CategoryInfrared;
                    break;
                case 4:
                    selectedCategory = CategoryIButton;
                    break;
                case 5:
                    selectedCategory = CategoryMedia;
                    break;
                case 6:
                    selectedCategory = CategoryNFC;
                    break;
                case 7:
                    selectedCategory = CategoryRFID;
                    break;
                case 8:
                    selectedCategory = CategorySubGHz;
                    break;
                case 9:
                    selectedCategory = CategoryTools;
                    break;
                case 10:
                    selectedCategory = CategoryUSB;
                    break;
                default:
                    selectedCategory = CategoryUnknown;
                    break;
                }

                if (selectedCategory != CategoryUnknown)
                {
                    currentCategory = selectedCategory;
                    currentAppIteration = 0;
                    selectedIndexApps = 0;
                    isLoadingNextApps = false;
                    currentView = RunViewDownloadProgress;
                    downloadCategoryInfo(currentCategory, 0);
                }
                break;
            case RunViewApps:
                // Download the selected app
                {
                    std::unique_ptr<FlipperAppInfo> appInfo = getAppInfo(currentCategory, selectedIndexApps);
                    if (appInfo)
                    {
                        const char *categoryStr = getAppCategory(currentCategory);
                        if (categoryStr)
                        {
                            currentView = RunViewDownloadProgress;
                            isDownloadingIndividualApp = true;
                            downloadApp(appInfo->app_id, appInfo->app_build_id, categoryStr);
                        }
                    }
                }
                break;
            }
            break;
        case InputKeyBack:
            if (currentView == RunViewApps)
            {
                // Return to catalog from apps view
                currentView = RunViewCatalog;
                currentCategory = CategoryUnknown;
            }
            else if (currentView != RunViewMainMenu)
            {
                // Return to main menu from sub-views
                currentView = RunViewMainMenu;
                if (isDownloading)
                {
                    isDownloading = false;
                    isDownloadComplete = false;
                }
            }
            else
            {
                // Return to previous screen/menu from main menu
                shouldReturnToMenu = true;
            }
            break;
        default:
            break;
        }
    }
}
