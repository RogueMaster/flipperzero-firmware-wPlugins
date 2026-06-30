#ifndef WEB_CRAWLER_I_H
#define WEB_CRAWLER_I_H

#include <web_crawler.h>
#include <callback/web_crawler_callback.h>
#include <flip_storage/web_crawler_storage.h>

/**
 * @brief      Function to allocate resources for the WebCrawlerApp.
 * @return     Pointer to the initialized WebCrawlerApp, or NULL on failure.
 */
WebCrawlerApp *web_crawler_app_alloc();

#endif // WEB_CRAWLER_I_H
