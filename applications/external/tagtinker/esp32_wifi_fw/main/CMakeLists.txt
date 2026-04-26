# TagTinker WiFi cloud-renderer - thin firmware. All drawing and API
# integration lives in cloud-plugins/ (Cloudflare Worker); this binary
# is just a UART <-> HTTPS bridge.
idf_component_register(
    SRCS
        "main.c"
        "wifi_link.c"
        "wifi_net.c"
        "cloud_client.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        esp_wifi
        esp_netif
        esp_event
        esp_http_client
        esp-tls
        driver
        json
        shared
)
