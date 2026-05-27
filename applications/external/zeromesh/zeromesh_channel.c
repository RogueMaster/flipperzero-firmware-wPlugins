#include "zeromesh_channel.h"
#include "zeromesh_history.h"

static const char* channel_names[] = {
    "Primary",
    "Channel 1",
    "Channel 2",
    "Channel 3",
    "Channel 4",
    "Channel 5",
    "Channel 6",
    "Channel 7",
};

void channel_init(ZeroMeshApp* app) {
    if(!app) return;
    
    app->current_channel = 0;
    app->num_channels = 3;
}

void channel_next(ZeroMeshApp* app) {
    if(!app) return;
    
    app->current_channel = (app->current_channel + 1) % app->num_channels;
    
    char status_msg[64];
    snprintf(status_msg, sizeof(status_msg), "Channel: %s", channel_names[app->current_channel]);
    set_status(app, status_msg);
    
    log_line(app, "Switched to %s", channel_names[app->current_channel]);
}

void channel_set(ZeroMeshApp* app, uint8_t channel) {
    if(!app || channel >= MAX_CHANNELS) return;
    
    app->current_channel = channel;
    if(channel >= app->num_channels) {
        app->num_channels = channel + 1;
    }
}

const char* channel_get_name(uint8_t channel) {
    if(channel >= MAX_CHANNELS) return "Unknown";
    return channel_names[channel];
}