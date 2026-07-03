// osm_logger.c — redirect the default entry to our real app() in src/app.c
#include <stdint.h>

extern int32_t app(void* p);

int32_t osm_logger_app(void* p) {
    return app(p);
}
