#include <web_crawler.h>
#include <alloc/web_crawler_alloc.h>
/**
 * @brief      Entry point for the WebCrawler application.
 * @param      p  Input parameter - unused
 * @return     0 to indicate success, -1 on failure
 */
int32_t web_crawler_app(void *p)
{
    UNUSED(p);

    WebCrawlerApp *app = web_crawler_app_alloc();
    if (!app)
    {
        FURI_LOG_E(TAG, "Failed to allocate WebCrawlerApp");
        return -1;
    }

    // check if board is connected (Derek Jamison)
    FlipperHTTP *fhttp = flipper_http_alloc();
    if (!fhttp)
    {
        easy_flipper_dialog("FlipperHTTP Error", "The UART is likely busy.\nEnsure you have the correct\nflash for your board then\nrestart your Flipper Zero.");
        return -1;
    }

    if (!flipper_http_ping(fhttp))
    {
        FURI_LOG_E(TAG, "Failed to ping the device");
        flipper_http_free(fhttp);
        return -1;
    }

    // Try to wait for pong response.
    uint32_t counter = 10;
    while (fhttp->state == INACTIVE && --counter > 0)
    {
        FURI_LOG_D(TAG, "Waiting for PONG");
        furi_delay_ms(100); // this causes a BusFault
    }

    flipper_http_free(fhttp);
    if (counter == 0)
    {
        easy_flipper_dialog("FlipperHTTP Error", "Ensure your WiFi Developer\nBoard or Pico W is connected\nand the latest FlipperHTTP\nfirmware is installed.");
    }

    // Run the application
    view_dispatcher_run(app->view_dispatcher);

    // Free resources after the application loop ends
    web_crawler_app_free(app);

    return 0;
}