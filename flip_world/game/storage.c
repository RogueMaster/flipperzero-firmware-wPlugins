#include <game/storage.h>

bool save_uint32(const char *path_name, uint32_t value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%lu", value);
    return save_char(path_name, buffer);
}

// Helper function to save an int8_t
static bool save_int8(const char *path_name, int8_t value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    return save_char(path_name, buffer);
}

// Helper function to save a float
static bool save_float(const char *path_name, float value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.6f", (double)value); // Limit to 6 decimal places
    return save_char(path_name, buffer);
}
bool save_player_context(PlayerContext *player_context)
{
    if (!player_context)
    {
        FURI_LOG_E(TAG, "Invalid player context");
        return false;
    }

    // ensure the folders exist
    char directory_path[128];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world");
    Storage *storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data");
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data/player");
    storage_common_mkdir(storage, directory_path);
    furi_record_close(RECORD_STORAGE);

    // 1. Username (String)
    if (!save_char("player/username", player_context->username))
    {
        FURI_LOG_E(TAG, "Failed to save player username");
        return false;
    }

    // 2. Level (uint32_t)
    if (!save_uint32("player/level", player_context->level))
    {
        FURI_LOG_E(TAG, "Failed to save player level");
        return false;
    }

    // 3. XP (uint32_t)
    if (!save_uint32("player/xp", player_context->xp))
    {
        FURI_LOG_E(TAG, "Failed to save player xp");
        return false;
    }

    // 4. Health (uint32_t)
    if (!save_uint32("player/health", player_context->health))
    {
        FURI_LOG_E(TAG, "Failed to save player health");
        return false;
    }

    // 5. Strength (uint32_t)
    if (!save_uint32("player/strength", player_context->strength))
    {
        FURI_LOG_E(TAG, "Failed to save player strength");
        return false;
    }

    // 6. Max Health (uint32_t)
    if (!save_uint32("player/max_health", player_context->max_health))
    {
        FURI_LOG_E(TAG, "Failed to save player max health");
        return false;
    }

    // 7. Health Regen (uint32_t)
    if (!save_uint32("player/health_regen", player_context->health_regen))
    {
        FURI_LOG_E(TAG, "Failed to save player health regen");
        return false;
    }

    // 8. Elapsed Health Regen (float)
    if (!save_float("player/elapsed_health_regen", player_context->elapsed_health_regen))
    {
        FURI_LOG_E(TAG, "Failed to save player elapsed health regen");
        return false;
    }

    // 9. Attack Timer (float)
    if (!save_float("player/attack_timer", player_context->attack_timer))
    {
        FURI_LOG_E(TAG, "Failed to save player attack timer");
        return false;
    }

    // 10. Elapsed Attack Timer (float)
    if (!save_float("player/elapsed_attack_timer", player_context->elapsed_attack_timer))
    {
        FURI_LOG_E(TAG, "Failed to save player elapsed attack timer");
        return false;
    }

    // 11. Direction (enum PlayerDirection)
    {
        char direction_str[2];
        switch (player_context->direction)
        {
        case ENTITY_UP:
            strncpy(direction_str, "0", sizeof(direction_str));
            break;
        case ENTITY_DOWN:
            strncpy(direction_str, "1", sizeof(direction_str));
            break;
        case ENTITY_LEFT:
            strncpy(direction_str, "2", sizeof(direction_str));
            break;
        case ENTITY_RIGHT:
        default:
            strncpy(direction_str, "3", sizeof(direction_str));
            break;
        }
        direction_str[1] = '\0'; // Ensure null termination

        if (!save_char("player/direction", direction_str))
        {
            FURI_LOG_E(TAG, "Failed to save player direction");
            return false;
        }
    }

    // 12. State (enum PlayerState)
    {
        char state_str[2];
        switch (player_context->state)
        {
        case ENTITY_IDLE:
            strncpy(state_str, "0", sizeof(state_str));
            break;
        case ENTITY_MOVING:
            strncpy(state_str, "1", sizeof(state_str));
            break;
        case ENTITY_ATTACKING:
            strncpy(state_str, "2", sizeof(state_str));
            break;
        case ENTITY_ATTACKED:
            strncpy(state_str, "3", sizeof(state_str));
            break;
        case ENTITY_DEAD:
            strncpy(state_str, "4", sizeof(state_str));
            break;
        default:
            strncpy(state_str, "5", sizeof(state_str)); // Assuming '5' for unknown states
            break;
        }
        state_str[1] = '\0'; // Ensure null termination

        if (!save_char("player/state", state_str))
        {
            FURI_LOG_E(TAG, "Failed to save player state");
            return false;
        }
    }

    // 13. Start Position X (float)
    if (!save_float("player/start_position_x", player_context->start_position.x))
    {
        FURI_LOG_E(TAG, "Failed to save player start position x");
        return false;
    }

    // 14. Start Position Y (float)
    if (!save_float("player/start_position_y", player_context->start_position.y))
    {
        FURI_LOG_E(TAG, "Failed to save player start position y");
        return false;
    }

    // 15. dx (int8_t)
    if (!save_int8("player/dx", player_context->dx))
    {
        FURI_LOG_E(TAG, "Failed to save player dx");
        return false;
    }

    // 16. dy (int8_t)
    if (!save_int8("player/dy", player_context->dy))
    {
        FURI_LOG_E(TAG, "Failed to save player dy");
        return false;
    }

    return true;
}

static FuriString *player_context_json(PlayerContext *player_context, bool websocket)
{
    FuriString *json = furi_string_alloc();
    if (!json)
    {
        FURI_LOG_E(TAG, "Failed to allocate JSON string");
        return NULL;
    }

    furi_string_cat_str(json, "{");

    if (websocket)
    {
        // Minimal JSON for WebSocket (abbreviated, <128 characters)
        // "u": username
        furi_string_cat_str(json, "\"u\":\"");
        furi_string_cat_str(json, player_context->username);
        furi_string_cat_str(json, "\",");

        // "xp": experience
        furi_string_cat_str(json, "\"xp\":");
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%lu", player_context->xp);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, ",");

        // "h": health
        furi_string_cat_str(json, "\"h\":");
        snprintf(buffer, sizeof(buffer), "%lu", player_context->health);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, ",");

        // "ehr": elapsed health regen (1 decimal)
        furi_string_cat_str(json, "\"ehr\":");
        snprintf(buffer, sizeof(buffer), "%.1f", (double)player_context->elapsed_health_regen);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, ",");

        // "eat": elapsed attack timer (1 decimal)
        furi_string_cat_str(json, "\"eat\":");
        snprintf(buffer, sizeof(buffer), "%.1f", (double)player_context->elapsed_attack_timer);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, ",");

        // "d": direction (numeric code)
        furi_string_cat_str(json, "\"d\":");
        snprintf(buffer, sizeof(buffer), "%d", player_context->direction);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, ",");

        // "s": state (numeric code)
        furi_string_cat_str(json, "\"s\":");
        snprintf(buffer, sizeof(buffer), "%d", player_context->state);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, ",");

        // "sp": start position object with x and y (1 decimal)
        furi_string_cat_str(json, "\"sp\":{");
        furi_string_cat_str(json, "\"x\":");
        snprintf(buffer, sizeof(buffer), "%.1f", (double)player_context->start_position.x);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, ",\"y\":");
        snprintf(buffer, sizeof(buffer), "%.1f", (double)player_context->start_position.y);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, "}");
    }
    else
    {
        // Full JSON output (unchanged)
        // 1. Username
        furi_string_cat_str(json, "\"username\":\"");
        furi_string_cat_str(json, player_context->username);
        furi_string_cat_str(json, "\",");

        // 2. Level
        furi_string_cat_str(json, "\"level\":");
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%lu", player_context->level);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, ",");

        // 3. XP
        furi_string_cat_str(json, "\"xp\":");
        snprintf(buffer, sizeof(buffer), "%lu", player_context->xp);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, ",");

        // 4. Health
        furi_string_cat_str(json, "\"health\":");
        snprintf(buffer, sizeof(buffer), "%lu", player_context->health);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, ",");

        // 5. Strength
        furi_string_cat_str(json, "\"strength\":");
        snprintf(buffer, sizeof(buffer), "%lu", player_context->strength);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, ",");

        // 6. Max Health
        furi_string_cat_str(json, "\"max_health\":");
        snprintf(buffer, sizeof(buffer), "%lu", player_context->max_health);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, ",");

        // 7. Health Regen
        furi_string_cat_str(json, "\"health_regen\":");
        snprintf(buffer, sizeof(buffer), "%u", player_context->health_regen);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, ",");

        // 8. Elapsed Health Regen
        furi_string_cat_str(json, "\"elapsed_health_regen\":");
        snprintf(buffer, sizeof(buffer), "%.6f", (double)player_context->elapsed_health_regen);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, ",");

        // 9. Attack Timer
        furi_string_cat_str(json, "\"attack_timer\":");
        snprintf(buffer, sizeof(buffer), "%.6f", (double)player_context->attack_timer);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, ",");

        // 10. Elapsed Attack Timer
        furi_string_cat_str(json, "\"elapsed_attack_timer\":");
        snprintf(buffer, sizeof(buffer), "%.6f", (double)player_context->elapsed_attack_timer);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, ",");

        // 11. Direction (string representation)
        furi_string_cat_str(json, "\"direction\":");
        switch (player_context->direction)
        {
        case ENTITY_UP:
            furi_string_cat_str(json, "\"up\",");
            break;
        case ENTITY_DOWN:
            furi_string_cat_str(json, "\"down\",");
            break;
        case ENTITY_LEFT:
            furi_string_cat_str(json, "\"left\",");
            break;
        case ENTITY_RIGHT:
        default:
            furi_string_cat_str(json, "\"right\",");
            break;
        }

        // 12. State (string representation)
        furi_string_cat_str(json, "\"state\":");
        switch (player_context->state)
        {
        case ENTITY_IDLE:
            furi_string_cat_str(json, "\"idle\",");
            break;
        case ENTITY_MOVING:
            furi_string_cat_str(json, "\"moving\",");
            break;
        case ENTITY_ATTACKING:
            furi_string_cat_str(json, "\"attacking\",");
            break;
        case ENTITY_ATTACKED:
            furi_string_cat_str(json, "\"attacked\",");
            break;
        case ENTITY_DEAD:
            furi_string_cat_str(json, "\"dead\",");
            break;
        default:
            furi_string_cat_str(json, "\"unknown\",");
            break;
        }

        // 13. Start Position X
        furi_string_cat_str(json, "\"start_position_x\":");
        snprintf(buffer, sizeof(buffer), "%.6f", (double)player_context->start_position.x);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, ",");

        // 14. Start Position Y
        furi_string_cat_str(json, "\"start_position_y\":");
        snprintf(buffer, sizeof(buffer), "%.6f", (double)player_context->start_position.y);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, ",");

        // 15. dx
        furi_string_cat_str(json, "\"dx\":");
        snprintf(buffer, sizeof(buffer), "%d", player_context->dx);
        furi_string_cat_str(json, buffer);
        furi_string_cat_str(json, ",");

        // 16. dy
        furi_string_cat_str(json, "\"dy\":");
        snprintf(buffer, sizeof(buffer), "%d", player_context->dy);
        furi_string_cat_str(json, buffer);
    }

    furi_string_cat_str(json, "}");

    // For websocket, output only the minimal JSON (without extra wrapping)
    FuriString *json_data = furi_string_alloc();
    if (!json_data)
    {
        FURI_LOG_E(TAG, "Failed to allocate JSON string");
        furi_string_free(json);
        return NULL;
    }

    if (websocket)
    {
        furi_string_cat(json_data, json);
    }
    else
    {
        furi_string_cat_str(json_data, "{\"username\":\"");
        furi_string_cat_str(json_data, player_context->username);
        furi_string_cat_str(json_data, "\",\"game_stats\":");
        furi_string_cat(json_data, json);
        furi_string_cat_str(json_data, "}");
    }

    furi_string_free(json);
    return json_data;
}

bool save_player_context_api(PlayerContext *player_context, FlipperHTTP *fhttp)
{
    if (!player_context)
    {
        FURI_LOG_E(TAG, "Invalid player context");
        return false;
    }

    if (!fhttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL");
        return false;
    }

    FuriString *json_data = player_context_json(player_context, false);
    if (!json_data)
    {
        FURI_LOG_E(TAG, "Failed to create JSON data");
        return false;
    }

    // save the json_data to the API
    if (!flipper_http_request(fhttp, POST, "https://www.jblanked.com/flipper/api/user/update-game-stats/", "{\"Content-Type\": \"application/json\"}", furi_string_get_cstr(json_data)))
    {
        FURI_LOG_E(TAG, "Failed to save player context to API");
        furi_string_free(json_data);
        return false;
    }
    fhttp->state = RECEIVING;
    while (fhttp->state != IDLE)
    {
        furi_delay_ms(100);
    }
    furi_string_free(json_data);

    return true;
}

bool websocket_player_context(PlayerContext *player_context, FlipperHTTP *fhttp)
{
    if (!player_context)
    {
        FURI_LOG_E(TAG, "Invalid player context");
        return false;
    }

    if (!fhttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL");
        return false;
    }

    // create JSON for all the player context data
    FuriString *json = player_context_json(player_context, true);
    if (!json)
    {
        FURI_LOG_E(TAG, "Failed to create JSON data");
        return false;
    }

    // websocket session is already started, so just send json to esp32
    if (!flipper_http_send_data(fhttp, furi_string_get_cstr(json)))
    {
        FURI_LOG_E(TAG, "Failed to send player context to websocket");
        furi_string_free(json);
        return false;
    }
    furi_string_free(json);

    return true;
}

// Helper function to load an integer
static bool load_number(const char *path_name, int *value)
{
    if (!path_name || !value)
    {
        FURI_LOG_E(TAG, "Invalid arguments to load_number");
        return false;
    }

    char buffer[64];

    if (!load_char(path_name, buffer, sizeof(buffer)))
    {
        FURI_LOG_E(TAG, "Failed to load number from path: %s", path_name);
        return false;
    }

    *value = atoi(buffer);
    return true;
}

// Helper function to load a float
static bool load_float(const char *path_name, float *value)
{
    if (!path_name || !value)
    {
        FURI_LOG_E(TAG, "Invalid arguments to load_float");
        return false;
    }

    char buffer[64];
    if (!load_char(path_name, buffer, sizeof(buffer)))
    {
        FURI_LOG_E(TAG, "Failed to load float from path: %s", path_name);
        return false;
    }

    // check if the string is a valid float
    char *endptr;
    *value = strtof(buffer, &endptr);
    if (endptr == buffer)
    {
        FURI_LOG_E(TAG, "Failed to parse float from path: %s", path_name);
        return false;
    }

    return true;
}

// Helper function to load an int8_t
static bool load_int8(const char *path_name, int8_t *value)
{
    if (!path_name || !value)
    {
        FURI_LOG_E(TAG, "Invalid arguments to load_int8");
        return false;
    }

    char buffer[64];

    if (!load_char(path_name, buffer, sizeof(buffer)))
    {
        FURI_LOG_E(TAG, "Failed to load int8 from path: %s", path_name);
        return false;
    }

    long temp = strtol(buffer, NULL, 10);
    if (temp < INT8_MIN || temp > INT8_MAX)
    {
        FURI_LOG_E(TAG, "Value out of range for int8: %ld", temp);
        return false;
    }

    // check if the string is a valid int8
    char *endptr;
    *value = (int8_t)strtol(buffer, &endptr, 10);
    if (endptr == buffer)
    {
        FURI_LOG_E(TAG, "Failed to parse int8 from path: %s", path_name);
        return false;
    }
    return true;
}

// Helper function to load a uint32_t
bool load_uint32(const char *path_name, uint32_t *value)
{
    if (!path_name || !value)
    {
        FURI_LOG_E(TAG, "Invalid arguments to load_uint32");
        return false;
    }

    char buffer[64];
    if (!load_char(path_name, buffer, sizeof(buffer)))
    {
        FURI_LOG_E(TAG, "Failed to load uint32 from path: %s", path_name);
        return false;
    }

    // check if the string is a valid uint32
    char *endptr;
    *value = strtoul(buffer, &endptr, 10);
    if (endptr == buffer)
    {
        FURI_LOG_E(TAG, "Failed to parse uint32 from path: %s", path_name);
        return false;
    }

    return true;
}

bool load_player_context(PlayerContext *player_context)
{
    if (!player_context)
    {
        FURI_LOG_E(TAG, "Invalid player context");
        return false;
    }

    // 1. Username (String)
    if (!load_char("player/username", player_context->username, sizeof(player_context->username)))
    {
        FURI_LOG_E(TAG, "No data or parse error for username. Using default: 'Unknown'");
        memset(player_context->username, 0, sizeof(player_context->username));
        strncpy(player_context->username, "Unknown", sizeof(player_context->username) - 1);
    }

    // 2. Level (uint32_t)
    {
        uint32_t temp = 1; // Default
        if (!load_char("player/level", (char *)&temp, sizeof(temp)))
        {
            FURI_LOG_E(TAG, "No data or parse error for level. Using default: 1");
        }
        else
        {
            // char buffer[64];
            if (load_uint32("player/level", &temp))
            {
                player_context->level = temp;
            }
            else
            {
                FURI_LOG_E(TAG, "Failed to parse level. Using default: 1");
                player_context->level = 1;
            }
        }
        player_context->level = temp;
    }

    // 3. XP (uint32_t)
    {
        uint32_t temp = 0; // Default
        if (!load_uint32("player/xp", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for xp. Using default: 0");
            temp = 0;
        }
        player_context->xp = temp;
    }

    // 4. Health (uint32_t)
    {
        uint32_t temp = 100; // Default
        if (!load_uint32("player/health", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for health. Using default: 100");
            temp = 100;
        }
        player_context->health = temp;
    }

    // 5. Strength (uint32_t)
    {
        uint32_t temp = 10; // Default
        if (!load_uint32("player/strength", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for strength. Using default: 10");
            temp = 10;
        }
        player_context->strength = temp;
    }

    // 6. Max Health (uint32_t)
    {
        uint32_t temp = 100; // Default
        if (!load_uint32("player/max_health", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for max_health. Using default: 100");
            temp = 100;
        }
        player_context->max_health = temp;
    }

    // 7. Health Regen (uint32_t)
    {
        uint32_t temp = 1; // Default
        if (!load_uint32("player/health_regen", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for health_regen. Using default: 1");
            temp = 1;
        }
        player_context->health_regen = temp;
    }

    // 8. Elapsed Health Regen (float)
    {
        float temp = 0.0f; // Default
        if (!load_float("player/elapsed_health_regen", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for elapsed_health_regen. Using default: 0.0f");
            temp = 0.0f;
        }
        player_context->elapsed_health_regen = temp;
    }

    // 9. Attack Timer (float)
    {
        float temp = 0.1f; // Default
        if (!load_float("player/attack_timer", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for attack_timer. Using default: 0.1f");
            temp = 0.1f;
        }
        player_context->attack_timer = temp;
    }

    // 10. Elapsed Attack Timer (float)
    {
        float temp = 0.0f; // Default
        if (!load_float("player/elapsed_attack_timer", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for elapsed_attack_timer. Using default: 0.0f");
            temp = 0.0f;
        }
        player_context->elapsed_attack_timer = temp;
    }

    // 11. Direction (enum PlayerDirection)
    {
        int direction_int = 3; // Default to ENTITY_RIGHT
        if (!load_number("player/direction", &direction_int))
        {
            FURI_LOG_E(TAG, "No data or parse error for direction. Defaulting to ENTITY_RIGHT");
            direction_int = 3;
        }

        switch (direction_int)
        {
        case 0:
            player_context->direction = ENTITY_UP;
            break;
        case 1:
            player_context->direction = ENTITY_DOWN;
            break;
        case 2:
            player_context->direction = ENTITY_LEFT;
            break;
        case 3:
            player_context->direction = ENTITY_RIGHT;
            break;
        default:
            FURI_LOG_E(TAG, "Invalid direction value: %d. Defaulting to ENTITY_RIGHT", direction_int);
            player_context->direction = ENTITY_RIGHT;
            break;
        }
    }

    // 12. State (enum PlayerState)
    {
        int state_int = 0; // Default to ENTITY_IDLE
        if (!load_number("player/state", &state_int))
        {
            FURI_LOG_E(TAG, "No data or parse error for state. Defaulting to ENTITY_IDLE");
            state_int = 0;
        }

        switch (state_int)
        {
        case 0:
            player_context->state = ENTITY_IDLE;
            break;
        case 1:
            player_context->state = ENTITY_MOVING;
            break;
        case 2:
            player_context->state = ENTITY_ATTACKING;
            break;
        case 3:
            player_context->state = ENTITY_ATTACKED;
            break;
        case 4:
            player_context->state = ENTITY_DEAD;
            break;
        default:
            FURI_LOG_E(TAG, "Invalid state value: %d. Defaulting to ENTITY_IDLE", state_int);
            player_context->state = ENTITY_IDLE;
            break;
        }
    }

    // 13. Start Position X (float)
    {
        float temp = 192.0f; // Default
        if (!load_float("player/start_position_x", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for start_position_x. Using default: 192.0f");
            temp = 192.0f;
        }
        player_context->start_position.x = temp;
    }

    // 14. Start Position Y (float)
    {
        float temp = 96.0f; // Default
        if (!load_float("player/start_position_y", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for start_position_y. Using default: 96.0f");
            temp = 96.0f;
        }
        player_context->start_position.y = temp;
    }

    // 15. dx (int8_t)
    {
        int8_t temp = 1; // Default
        if (!load_int8("player/dx", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for dx. Using default: 1");
            temp = 1;
        }
        player_context->dx = temp;
    }

    // 16. dy (int8_t)
    {
        int8_t temp = 0; // Default
        if (!load_int8("player/dy", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for dy. Using default: 0");
            temp = 0;
        }
        player_context->dy = temp;
    }

    return true;
}
// loads from STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data/player/player_stats.json
// then gets each key-value pair and saves it as it's own file so it can be loaded separately using
// load_player_context
bool set_player_context()
{
    char file_path[256];
    snprintf(file_path, sizeof(file_path),
             STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data/player/player_stats.json");

    FuriString *player_stats = flipper_http_load_from_file(file_path);
    if (!player_stats)
    {
        FURI_LOG_E(TAG, "Failed to load player stats from file: %s", file_path);
        return false;
    }

    // Get the key one-by-one and save it to a separate file

    // 1. Username (String)
    FuriString *username = get_json_value_furi("username", player_stats);
    if (username)
    {
        save_char("player/username", furi_string_get_cstr(username));
        furi_string_free(username);
    }

    // 2. Level (uint32_t)
    FuriString *level = get_json_value_furi("level", player_stats);
    if (level)
    {
        save_uint32("player/level", atoi(furi_string_get_cstr(level)));
        furi_string_free(level);
    }

    // 3. XP (uint32_t)
    FuriString *xp = get_json_value_furi("xp", player_stats);
    if (xp)
    {
        save_uint32("player/xp", atoi(furi_string_get_cstr(xp)));
        furi_string_free(xp);
    }

    // 4. Health (uint32_t)
    FuriString *health = get_json_value_furi("health", player_stats);
    if (health)
    {
        save_uint32("player/health", atoi(furi_string_get_cstr(health)));
        furi_string_free(health);
    }

    // 5. Strength (uint32_t)
    FuriString *strength = get_json_value_furi("strength", player_stats);
    if (strength)
    {
        save_uint32("player/strength", atoi(furi_string_get_cstr(strength)));
        furi_string_free(strength);
    }

    // 6. Max Health (uint32_t)
    FuriString *max_health = get_json_value_furi("max_health", player_stats);
    if (max_health)
    {
        save_uint32("player/max_health", atoi(furi_string_get_cstr(max_health)));
        furi_string_free(max_health);
    }

    // 7. Health Regen (uint32_t)
    FuriString *health_regen = get_json_value_furi("health_regen", player_stats);
    if (health_regen)
    {
        save_uint32("player/health_regen", atoi(furi_string_get_cstr(health_regen)));
        furi_string_free(health_regen);
    }

    // 8. Elapsed Health Regen (float)
    FuriString *elapsed_health_regen = get_json_value_furi("elapsed_health_regen", player_stats);
    if (elapsed_health_regen)
    {
        save_float("player/elapsed_health_regen", strtof(furi_string_get_cstr(elapsed_health_regen), NULL));
        furi_string_free(elapsed_health_regen);
    }

    // 9. Attack Timer (float)
    FuriString *attack_timer = get_json_value_furi("attack_timer", player_stats);
    if (attack_timer)
    {
        save_float("player/attack_timer", strtof(furi_string_get_cstr(attack_timer), NULL));
        furi_string_free(attack_timer);
    }

    // 10. Elapsed Attack Timer (float)
    FuriString *elapsed_attack_timer = get_json_value_furi("elapsed_attack_timer", player_stats);
    if (elapsed_attack_timer)
    {
        save_float("player/elapsed_attack_timer", strtof(furi_string_get_cstr(elapsed_attack_timer), NULL));
        furi_string_free(elapsed_attack_timer);
    }

    // 11. Direction (enum PlayerDirection)
    FuriString *direction = get_json_value_furi("direction", player_stats);
    if (direction)
    {
        save_char("player/direction", furi_string_get_cstr(direction));
        furi_string_free(direction);
    }

    // 12. State (enum PlayerState)
    FuriString *state = get_json_value_furi("state", player_stats);
    if (state)
    {
        save_char("player/state", furi_string_get_cstr(state));
        furi_string_free(state);
    }

    // 13. Start Position X (float)
    FuriString *start_position_x = get_json_value_furi("start_position_x", player_stats);
    if (start_position_x)
    {
        save_float("player/start_position_x", strtof(furi_string_get_cstr(start_position_x), NULL));
        furi_string_free(start_position_x);
    }

    // 14. Start Position Y (float)
    FuriString *start_position_y = get_json_value_furi("start_position_y", player_stats);
    if (start_position_y)
    {
        save_float("player/start_position_y", strtof(furi_string_get_cstr(start_position_y), NULL));
        furi_string_free(start_position_y);
    }

    // 15. dx (int8_t)
    FuriString *dx = get_json_value_furi("dx", player_stats);
    if (dx)
    {
        save_int8("player/dx", atoi(furi_string_get_cstr(dx)));
        furi_string_free(dx);
    }

    // 16. dy (int8_t)
    FuriString *dy = get_json_value_furi("dy", player_stats);
    if (dy)
    {
        save_int8("player/dy", atoi(furi_string_get_cstr(dy)));
        furi_string_free(dy);
    }

    furi_string_free(player_stats);
    return true;
}

static inline void furi_string_remove_str(FuriString *string, const char *needle)
{
    furi_string_replace_str(string, needle, "", 0);
}

static FuriString *json_data(const FuriString *world_data, const char *key)
{
    size_t json_data_pos = furi_string_search_str(world_data, key, 0);
    if (json_data_pos == FURI_STRING_FAILURE)
    {
        FURI_LOG_E("Game", "Failed to find json_data in world data");

        return NULL;
    }

    size_t bracket_start = furi_string_search_char(world_data, '[', json_data_pos);
    if (bracket_start == FURI_STRING_FAILURE)
    {
        FURI_LOG_E("Game", "Failed to find start of json_data array");

        return NULL;
    }

    size_t bracket_end = furi_string_search_char(world_data, ']', bracket_start);
    if (bracket_end == FURI_STRING_FAILURE)
    {
        FURI_LOG_E("Game", "Failed to find end of json_data array");

        return NULL;
    }

    FuriString *json_data_str = furi_string_alloc();
    if (!json_data_str)
    {
        FURI_LOG_E("Game", "Failed to allocate json_data string");

        return NULL;
    }

    furi_string_cat_str(json_data_str, "{\"");
    furi_string_cat_str(json_data_str, key);
    furi_string_cat_str(json_data_str, "\":");

    {
        FuriString *temp_sub = furi_string_alloc();

        furi_string_set_strn(
            temp_sub,
            furi_string_get_cstr(world_data) + bracket_start,
            (bracket_end + 1) - bracket_start);

        furi_string_cat(json_data_str, temp_sub);
        furi_string_free(temp_sub);
    }

    furi_string_cat_str(json_data_str, "}");

    return json_data_str;
}

bool separate_world_data(char *id, FuriString *world_data)
{
    if (!id || !world_data)
    {
        FURI_LOG_E("Game", "Invalid parameters");
        return false;
    }
    FuriString *file_json_data = json_data(world_data, "json_data");
    if (!file_json_data || furi_string_size(file_json_data) == 0)
    {
        FURI_LOG_E("Game", "Failed to get json data in separate_world_data");
        if (file_json_data)
        {
            furi_string_free(file_json_data);
        }
        return false;
    }

    // Save file_json_data to disk
    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path),
             STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s", id);

    Storage *storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, directory_path);

    File *file = storage_file_alloc(storage);
    char file_path[256];
    snprintf(file_path, sizeof(file_path),
             STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s/%s_json_data.json",
             id, id);

    if (!storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS))
    {
        FURI_LOG_E("Game", "Failed to open file for writing: %s", file_path);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        furi_string_free(file_json_data);
        return false;
    }

    size_t data_size = furi_string_size(file_json_data);
    if (storage_file_write(file, furi_string_get_cstr(file_json_data), data_size) != data_size)
    {
        FURI_LOG_E("Game", "Failed to write json_data");
    }
    storage_file_close(file);

    furi_string_replace_at(file_json_data, 0, 1, "");
    furi_string_replace_at(file_json_data, furi_string_size(file_json_data) - 1, 1, "");
    // include the comma at the end of the json_data array
    furi_string_cat_str(file_json_data, ",");

    furi_string_remove_str(world_data, furi_string_get_cstr(file_json_data));
    furi_string_free(file_json_data);

    // save npc_data to disk
    FuriString *file_npc_data = json_data(world_data, "npc_data");
    if (!file_npc_data)
    {
        FURI_LOG_I("Game", "NPC data is not present");
        // not every map has npc_data, so we can skip this
    }
    else
    {

        snprintf(file_path, sizeof(file_path),
                 STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s/%s_npc_data.json",
                 id, id);

        if (!storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        {
            FURI_LOG_E("Game", "Failed to open file for writing: %s", file_path);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            furi_string_free(file_npc_data);
            return false;
        }

        data_size = furi_string_size(file_npc_data);
        if (storage_file_write(file, furi_string_get_cstr(file_npc_data), data_size) != data_size)
        {
            FURI_LOG_E("Game", "Failed to write npc_data");
        }
        storage_file_close(file);

        furi_string_replace_at(file_npc_data, 0, 1, "");
        furi_string_replace_at(file_npc_data, furi_string_size(file_npc_data) - 1, 1, "");
        // include the comma at the end of the npc_data array
        furi_string_cat_str(file_npc_data, ",");

        furi_string_remove_str(world_data, furi_string_get_cstr(file_npc_data));
        furi_string_free(file_npc_data);
    }

    // Save enemy_data to disk
    FuriString *file_enemy_data = json_data(world_data, "enemy_data");
    if (!file_enemy_data)
    {
        FURI_LOG_E("Game", "Failed to get enemy data");
        return false;
    }

    snprintf(file_path, sizeof(file_path),
             STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s/%s_enemy_data.json",
             id, id);

    if (!storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS))
    {
        FURI_LOG_E("Game", "Failed to open file for writing: %s", file_path);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        furi_string_free(file_enemy_data);
        return false;
    }

    data_size = furi_string_size(file_enemy_data);
    if (storage_file_write(file, furi_string_get_cstr(file_enemy_data), data_size) != data_size)
    {
        FURI_LOG_E("Game", "Failed to write enemy_data");
    }
    furi_string_free(file_enemy_data);

    // Clean up
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return true;
}
