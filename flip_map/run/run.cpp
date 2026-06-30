#include "run/run.hpp"
#include "app.hpp"
#include "flip_map_icons.h"

FlipMapRun::FlipMapRun(void *appContext) : appContext(appContext), currentView(FlipMapRunViewLogin), inCountryView(true), inputHeld(false),
                                           lastInput(InputKeyMAX), locationStatus(LocationNotStarted), loginStatus(LoginNotStarted), mapDataStatus(MapDataNotStarted),
                                           menuIndex(0), menuMaxIndex(0), menuStartIndex(0),
                                           registrationStatus(RegistrationNotStarted), shouldDebounce(false), shouldReturnToMenu(false)
{
    char *loginStatusStr = (char *)malloc(32);
    if (loginStatusStr)
    {
        FlipMapApp *app = static_cast<FlipMapApp *>(appContext);
        if (app && app->loadChar("login_status", loginStatusStr, 32))
        {
            if (strcmp(loginStatusStr, "success") == 0)
            {
                loginStatus = LoginSuccess;
                locationStatus = LocationWaiting;
                currentView = FlipMapRunViewLocation;
                userRequest(RequestTypeLocationUpdate);
            }
            else
            {
                loginStatus = LoginNotStarted;
                currentView = FlipMapRunViewLogin;
            }
        }
        free(loginStatusStr);
    }
}

FlipMapRun::~FlipMapRun()
{
    // nothing to do
}

void FlipMapRun::debounceInput()
{
    static uint8_t debounceCounter = 0;
    if (shouldDebounce)
    {
        lastInput = InputKeyMAX;
        debounceCounter++;
        if (debounceCounter < 2)
        {
            return;
        }
        debounceCounter = 0;
        shouldDebounce = false;
        inputHeld = false;
    }
}

void FlipMapRun::drawLocationView(Canvas *canvas)
{
    canvas_set_font(canvas, FontPrimary);
    static bool loadingStarted = false;
    switch (locationStatus)
    {
    case LocationWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Syncing...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            FlipMapApp *app = static_cast<FlipMapApp *>(appContext);
            if (app->getHttpState() == ISSUE)
            {
                locationStatus = LocationRequestError;
                return;
            }
            char response[256];
            if (app && app->loadChar("location_status", response, sizeof(response)))
            {
                if (strstr(response, "[ERROR]") == NULL)
                {
                    locationStatus = LocationSuccess;
                    currentView = FlipMapRunViewMapData;
                    mapDataStatus = MapDataWaiting;
                    // Reset menu state for map data
                    menuIndex = 0;
                    menuStartIndex = 0;
                    inCountryView = true;
                    userRequest(RequestTypeMapData);
                }
                else
                {
                    locationStatus = LocationRequestError;
                }
            }
            else
            {
                locationStatus = LocationRequestError;
            }
        }
        break;
    case LocationSuccess:
        canvas_draw_str(canvas, 0, 10, "Location successful!");
        canvas_draw_str(canvas, 0, 20, "Press OK to continue.");
        break;
    case LocationCredentialsMissing:
        canvas_draw_str(canvas, 0, 10, "Missing credentials!");
        canvas_draw_str(canvas, 0, 20, "Please set your username");
        canvas_draw_str(canvas, 0, 30, "and password in the app.");
        break;
    case LocationRequestError:
        canvas_draw_str(canvas, 0, 10, "Location request failed!");
        canvas_draw_str(canvas, 0, 20, "Check your network and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;
    case LocationNotStarted:
        locationStatus = LocationWaiting;
        userRequest(RequestTypeLocationUpdate);
        break;
    default:
        FURI_LOG_E(TAG, "Unknown location status");
        canvas_draw_str(canvas, 0, 10, "Syncing...");
        break;
    }
}

void FlipMapRun::drawLoginView(Canvas *canvas)
{
    canvas_set_font(canvas, FontPrimary);
    static bool loadingStarted = false;
    switch (loginStatus)
    {
    case LoginWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Logging in...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            FlipMapApp *app = static_cast<FlipMapApp *>(appContext);
            if (app->getHttpState() == ISSUE)
            {
                loginStatus = LoginRequestError;
                return;
            }
            char response[256];
            if (app && app->loadChar("login", response, sizeof(response)))
            {
                if (strstr(response, "[SUCCESS]") != NULL)
                {
                    loginStatus = LoginSuccess;
                    app->saveChar("login_status", "success");
                    locationStatus = LocationWaiting;
                    currentView = FlipMapRunViewLocation;
                    userRequest(RequestTypeLocationUpdate);
                }
                else if (strstr(response, "User not found") != NULL)
                {
                    loginStatus = LoginNotStarted;
                    currentView = FlipMapRunViewRegistration;
                    registrationStatus = RegistrationWaiting;
                    userRequest(RequestTypeRegistration);
                }
                else if (strstr(response, "Incorrect password") != NULL)
                {
                    loginStatus = LoginWrongPassword;
                }
                else if (strstr(response, "Username or password is empty.") != NULL)
                {
                    loginStatus = LoginCredentialsMissing;
                }
                else
                {
                    loginStatus = LoginRequestError;
                }
            }
            else
            {
                loginStatus = LoginRequestError;
            }
        }
        break;
    case LoginSuccess:
        canvas_draw_str(canvas, 0, 10, "Login successful!");
        canvas_draw_str(canvas, 0, 20, "Press OK to continue.");
        break;
    case LoginCredentialsMissing:
        canvas_draw_str(canvas, 0, 10, "Missing credentials!");
        canvas_draw_str(canvas, 0, 20, "Please set your username");
        canvas_draw_str(canvas, 0, 30, "and password in the app.");
        break;
    case LoginRequestError:
        canvas_draw_str(canvas, 0, 10, "Login request failed!");
        canvas_draw_str(canvas, 0, 20, "Check your network and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;
    case LoginWrongPassword:
        canvas_draw_str(canvas, 0, 10, "Wrong password!");
        canvas_draw_str(canvas, 0, 20, "Please check your password");
        canvas_draw_str(canvas, 0, 30, "and try again.");
        break;
    case LoginNotStarted:
        loginStatus = LoginWaiting;
        userRequest(RequestTypeLogin);
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Logging in...");
        break;
    }
}

void FlipMapRun::drawMapDataMenu(Canvas *canvas)
{
    canvas_clear(canvas);

    FlipMapApp *app = static_cast<FlipMapApp *>(appContext);
    char *response = (char *)malloc(2048);
    if (!app || !app->loadChar("map_data", response, 2048))
    {
        canvas_draw_str(canvas, 0, 10, "Error loading map data!");
        free(response);
        return;
    }

    // Parse total users
    char *total_users = get_json_value("total_users", response);
    if (!total_users)
    {
        canvas_draw_str(canvas, 0, 10, "Error parsing map data!");
        free(response);
        return;
    }

    // Draw title with current view indicator
    canvas_set_font(canvas, FontPrimary);
    char title[32];
    snprintf(title, sizeof(title), "FlipMap - %s", inCountryView ? "Countries" : "Cities");
    int title_width = canvas_string_width(canvas, title);
    int title_x = (128 - title_width) / 2;
    canvas_draw_str(canvas, title_x, 10, title);

    // Draw underline for title
    canvas_draw_line(canvas, title_x, 12, title_x + title_width, 12);

    // Draw decorative top pattern
    for (int i = 0; i < 128; i += 6)
    {
        canvas_draw_dot(canvas, i, 16);
    }

    // Draw total users info in top-right corner
    canvas_set_font(canvas, FontSecondary);
    char users_text[16];
    snprintf(users_text, sizeof(users_text), "Users: %s", total_users);
    int users_width = canvas_string_width(canvas, users_text);
    canvas_draw_str(canvas, 128 - users_width - 2, 63, users_text);

    // Count items for the current view
    uint8_t currentItemCount = 0;
    const char *arrayKey = inCountryView ? "countries" : "cities";

    for (uint8_t i = 0; i < 50; i++) // Check up to 50 items
    {
        char *item = get_json_array_value(arrayKey, i, response);
        if (!item)
        {
            break;
        }
        currentItemCount++;
        free(item);
    }

    if (currentItemCount == 0)
    {
        // No items found
        canvas_set_font(canvas, FontPrimary);
        char no_items[32];
        snprintf(no_items, sizeof(no_items), "No %s found", inCountryView ? "countries" : "cities");
        int no_items_width = canvas_string_width(canvas, no_items);
        int no_items_x = (128 - no_items_width) / 2;
        canvas_draw_str(canvas, no_items_x, 35, no_items);
    }
    else
    {
        // Update menu bounds
        menuMaxIndex = currentItemCount - 1;

        // Adjust scroll if needed
        if (menuIndex > menuMaxIndex)
        {
            menuIndex = menuMaxIndex;
        }

        // Display parameters
        const uint8_t maxDisplayItems = 4;
        const uint8_t itemStartY = 22;
        const uint8_t itemHeight = 10;

        // Calculate display window
        if (menuIndex < menuStartIndex)
        {
            menuStartIndex = menuIndex;
        }
        else if (menuIndex >= menuStartIndex + maxDisplayItems)
        {
            menuStartIndex = menuIndex - maxDisplayItems + 1;
        }

        // Display items with better formatting
        canvas_set_font(canvas, FontSecondary);
        uint8_t displayCount = 0;
        for (uint8_t i = menuStartIndex; i < currentItemCount && displayCount < maxDisplayItems; i++)
        {
            char *item = get_json_array_value(arrayKey, i, response);
            if (!item)
            {
                break;
            }

            char *itemName = get_json_value("name", item);
            char *itemCount = get_json_value("count", item);

            if (itemName && itemCount)
            {
                // Draw selection box for current item
                int item_y = itemStartY + (displayCount * itemHeight);
                if (i == menuIndex)
                {
                    // Draw filled selection box with rounded corners
                    canvas_draw_rbox(canvas, 2, item_y - 8, 124, itemHeight - 1, 2);
                    canvas_set_color(canvas, ColorWhite);

                    // Add selection indicator arrow
                    canvas_draw_str(canvas, 4, item_y - 1, ">");
                }

                // Format the item text with better spacing
                char itemText[128];
                int name_width = canvas_string_width(canvas, itemName);
                int max_name_width = 85; // Leave space for count

                if (name_width > max_name_width)
                {
                    // Truncate name if too long
                    char truncated_name[64];
                    strncpy(truncated_name, itemName, sizeof(truncated_name) - 4);
                    truncated_name[sizeof(truncated_name) - 4] = '\0';
                    snprintf(itemText, sizeof(itemText), "%s...", truncated_name);
                }
                else
                {
                    snprintf(itemText, sizeof(itemText), "%s", itemName);
                }

                // Draw item name (adjust position if selected to account for arrow)
                int text_x = (i == menuIndex) ? 14 : 6;
                canvas_draw_str(canvas, text_x, item_y, itemText);

                // Draw count aligned to the right
                char count_text[16];
                snprintf(count_text, sizeof(count_text), "(%s)", itemCount);
                int count_width = canvas_string_width(canvas, count_text);
                canvas_draw_str(canvas, 122 - count_width, item_y, count_text);

                if (i == menuIndex)
                {
                    canvas_set_color(canvas, ColorBlack);
                }
            }

            free(itemName);
            free(itemCount);
            free(item);
            displayCount++;
        }

        // Draw scroll indicators if needed
        if (currentItemCount > maxDisplayItems)
        {
            // Up arrow
            if (menuStartIndex > 0)
            {
                canvas_draw_str(canvas, 60, 20, "^");
            }

            // Down arrow
            if (menuStartIndex + maxDisplayItems < currentItemCount)
            {
                canvas_draw_str(canvas, 60, 62, "v");
            }
        }
    }

    // Draw navigation hints at bottom
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 18, 63, "Switch");
    canvas_draw_icon(canvas, 2, 56, &I_ButtonLeft_4x7);
    canvas_draw_icon(canvas, 10, 56, &I_ButtonRight_4x7);

    // Draw decorative bottom pattern
    for (int i = 0; i < 128; i += 6)
    {
        canvas_draw_dot(canvas, i, 48);
    }

    // Draw side decorations
    canvas_draw_line(canvas, 0, 18, 0, 46);
    canvas_draw_line(canvas, 127, 18, 127, 46);

    free(total_users);
    free(response);
}

void FlipMapRun::drawMapDataView(Canvas *canvas)
{
    canvas_set_font(canvas, FontPrimary);
    static bool loadingStarted = false;
    switch (mapDataStatus)
    {
    case MapDataWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Locating...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            FlipMapApp *app = static_cast<FlipMapApp *>(appContext);
            if (app->getHttpState() == ISSUE)
            {
                mapDataStatus = MapDataRequestError;
                return;
            }
            char *response = (char *)malloc(2048);
            if (app && app->loadChar("map_data", response, 2048))
            {
                if (strstr(response, "[ERROR]") == NULL)
                {
                    mapDataStatus = MapDataSuccess;
                }
                else
                {
                    mapDataStatus = MapDataRequestError;
                }
            }
            else
            {
                mapDataStatus = MapDataRequestError;
            }
            free(response);
        }
        break;
    case MapDataSuccess:
    {
        drawMapDataMenu(canvas);
        break;
    }
    case MapDataNotStarted:
        canvas_draw_str(canvas, 0, 10, "Map data not started.");
        canvas_draw_str(canvas, 0, 20, "Please try again later.");
        break;
    case MapDataRequestError:
        canvas_draw_str(canvas, 0, 10, "Map data request failed!");
        canvas_draw_str(canvas, 0, 20, "Check your network and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;
    case MapDataParseError:
        canvas_draw_str(canvas, 0, 10, "Error parsing map data!");
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Fetching map data...");
        break;
    };
}

void FlipMapRun::drawRegistrationView(Canvas *canvas)
{
    canvas_set_font(canvas, FontPrimary);
    static bool loadingStarted = false;
    switch (registrationStatus)
    {
    case RegistrationWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Registering...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            FlipMapApp *app = static_cast<FlipMapApp *>(appContext);
            if (app->getHttpState() == ISSUE)
            {
                registrationStatus = RegistrationRequestError;
                return;
            }
            char response[256];
            if (app && app->loadChar("register", response, sizeof(response)))
            {
                if (strstr(response, "[SUCCESS]") != NULL)
                {
                    registrationStatus = RegistrationSuccess;
                    locationStatus = LocationWaiting;
                    currentView = FlipMapRunViewLocation;
                    userRequest(RequestTypeLocationUpdate);
                }
                else if (strstr(response, "Username or password not provided") != NULL)
                {
                    registrationStatus = RegistrationCredentialsMissing;
                }
                else if (strstr(response, "User already exists") != NULL)
                {
                    registrationStatus = RegistrationUserExists;
                }
                else
                {
                    registrationStatus = RegistrationRequestError;
                }
            }
            else
            {
                registrationStatus = RegistrationRequestError;
            }
        }
        break;
    case RegistrationSuccess:
        canvas_draw_str(canvas, 0, 10, "Registration successful!");
        canvas_draw_str(canvas, 0, 20, "Press OK to continue.");
        break;
    case RegistrationCredentialsMissing:
        canvas_draw_str(canvas, 0, 10, "Missing credentials!");
        canvas_draw_str(canvas, 0, 20, "Please set your username");
        canvas_draw_str(canvas, 0, 30, "and password in the app.");
        break;
    case RegistrationRequestError:
        canvas_draw_str(canvas, 0, 10, "Registration request failed!");
        canvas_draw_str(canvas, 0, 20, "Check your network and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Registering...");
        break;
    }
}

bool FlipMapRun::httpRequestIsFinished()
{
    FlipMapApp *app = static_cast<FlipMapApp *>(appContext);
    if (!app)
    {
        FURI_LOG_E(TAG, "httpRequestIsFinished: App context is NULL");
        return true;
    }
    auto state = app->getHttpState();
    return state == IDLE || state == ISSUE || state == INACTIVE;
}

void FlipMapRun::updateDraw(Canvas *canvas)
{
    canvas_clear(canvas);

    switch (currentView)
    {
    case FlipMapRunViewLogin:
        drawLoginView(canvas);
        break;
    case FlipMapRunViewRegistration:
        drawRegistrationView(canvas);
        break;
    case FlipMapRunViewMapData:
        drawMapDataView(canvas);
        break;
    case FlipMapRunViewLocation:
        drawLocationView(canvas);
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Unknown view");
        break;
    }
}

void FlipMapRun::updateInput(InputEvent *event)
{
    lastInput = event->key;
    debounceInput();

    // Handle input based on current view
    if (currentView == FlipMapRunViewMapData && mapDataStatus == MapDataSuccess)
    {
        switch (lastInput)
        {
        case InputKeyUp:
            if (menuIndex > 0)
            {
                menuIndex--;
                shouldDebounce = true;
            }
            break;
        case InputKeyDown:
            if (menuIndex < menuMaxIndex)
            {
                menuIndex++;
                shouldDebounce = true;
            }
            break;
        case InputKeyLeft:
            if (!inCountryView)
            {
                inCountryView = true;
                menuIndex = 0;
                menuStartIndex = 0;
                shouldDebounce = true;
            }
            break;
        case InputKeyRight:
            if (inCountryView)
            {
                inCountryView = false;
                menuIndex = 0;
                menuStartIndex = 0;
                shouldDebounce = true;
            }
            break;
        case InputKeyOk:
            // maybe in next release we can add functionality to
            // select/view details of a specific country/city
            break;
        case InputKeyBack:
            shouldReturnToMenu = true;
            break;
        default:
            break;
        }
    }
    else
    {
        switch (lastInput)
        {
        case InputKeyBack:
            // return to menu
            shouldReturnToMenu = true;
            break;
        default:
            // nothing else for now
            // but we could use it to move in the map
            break;
        }
    }
}

void FlipMapRun::userRequest(RequestType requestType)
{
    FlipMapApp *app = static_cast<FlipMapApp *>(appContext);
    furi_check(app);

    // Allocate memory for credentials
    char *username = (char *)malloc(64);
    char *password = (char *)malloc(64);
    if (!username || !password)
    {
        FURI_LOG_E(TAG, "userRequest: Failed to allocate memory for credentials");
        if (username)
            free(username);
        if (password)
            free(password);
        return;
    }

    // Load credentials from storage
    bool credentialsLoaded = true;
    if (!app->loadChar("user_name", username, 64, "flipper_http"))
    {
        FURI_LOG_E(TAG, "Failed to load user_name");
        credentialsLoaded = false;
    }
    if (!app->loadChar("user_pass", password, 64, "flipper_http"))
    {
        FURI_LOG_E(TAG, "Failed to load user_pass");
        credentialsLoaded = false;
    }

    if (!credentialsLoaded)
    {
        switch (requestType)
        {
        case RequestTypeLogin:
            loginStatus = LoginCredentialsMissing;
            break;
        case RequestTypeRegistration:
            registrationStatus = RegistrationCredentialsMissing;
            break;
        case RequestTypeMapData:
            mapDataStatus = MapDataCredentialsMissing;
            break;
        case RequestTypeLocationUpdate:
            locationStatus = LocationCredentialsMissing;
            break;
        default:
            FURI_LOG_E(TAG, "Unknown request type: %d", requestType);
            loginStatus = LoginRequestError;
            registrationStatus = RegistrationRequestError;
            mapDataStatus = MapDataRequestError;
            locationStatus = LocationRequestError;
            break;
        }
        free(username);
        free(password);
        return;
    }

    // Create JSON payload for login/registration
    char *payload = (char *)malloc(256);
    if (!payload)
    {
        FURI_LOG_E(TAG, "userRequest: Failed to allocate memory for payload");
        free(username);
        free(password);
        return;
    }
    snprintf(payload, 256, "{\"username\":\"%s\",\"password\":\"%s\"}", username, password);

    switch (requestType)
    {
    case RequestTypeLogin:
        if (!app->httpRequestAsync("login.txt",
                                   "https://www.jblanked.com/flipper/api/user/login/",
                                   POST, "{\"Content-Type\":\"application/json\"}", payload))
        {
            loginStatus = LoginRequestError;
        }
        break;
    case RequestTypeRegistration:
        if (!app->httpRequestAsync("register.txt",
                                   "https://www.jblanked.com/flipper/api/user/register/",
                                   POST, "{\"Content-Type\":\"application/json\"}", payload))
        {
            registrationStatus = RegistrationRequestError;
        }
        break;
    case RequestTypeMapData:
    {
        char *authHeaders = (char *)malloc(256);
        if (!authHeaders)
        {
            FURI_LOG_E(TAG, "userRequest: Failed to allocate memory for authHeaders");
            mapDataStatus = MapDataRequestError;
            return;
        }
        snprintf(authHeaders, 256, "{\"Content-Type\":\"application/json\", \"username\":\"%s\", \"password\":\"%s\"}", username, password);
        if (!app->httpRequestAsync("map_data.txt",
                                   "https://www.jblanked.com/flipper/api/map/",
                                   GET, authHeaders, nullptr))
        {
            mapDataStatus = MapDataRequestError;
        }
        free(authHeaders);
        break;
    }
    case RequestTypeLocationUpdate:
    {
        char *authHeaders = (char *)malloc(256);
        char *url = (char *)malloc(256);
        char *locationStatusStr = (char *)malloc(16);
        if (!authHeaders && !url && !locationStatusStr)
        {
            if (authHeaders)
                free(authHeaders);
            if (url)
                free(url);
            if (locationStatusStr)
                free(locationStatusStr);
            FURI_LOG_E(TAG, "userRequest: Failed to allocate memory for location update");
            locationStatus = LocationRequestError;
            return;
        }
        if (!app->loadChar("location_status", locationStatusStr, 16))
        {
            snprintf(locationStatusStr, 16, "Disabled");
        }
        snprintf(url, 256, "https://www.jblanked.com/flipper/api/user/location/update/%s/%s/", username, locationStatusStr);
        snprintf(authHeaders, 256, "{\"Content-Type\":\"application/json\", \"username\":\"%s\", \"password\":\"%s\"}", username, password);
        if (!app->httpRequestAsync("location_request.txt", url, GET, authHeaders, nullptr))
        {
            locationStatus = LocationRequestError;
        }
        free(authHeaders);
        free(url);
        free(locationStatusStr);
        break;
    }
    default:
        FURI_LOG_E(TAG, "Unknown request type: %d", requestType);
        loginStatus = LoginRequestError;
        registrationStatus = RegistrationRequestError;
        mapDataStatus = MapDataRequestError;
        locationStatus = LocationRequestError;
        break;
    }

    free(username);
    free(password);
    free(payload);
}