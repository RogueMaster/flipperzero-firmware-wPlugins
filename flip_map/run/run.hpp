#pragma once
#include "easy_flipper/easy_flipper.h"
#include "flipper_http/flipper_http.h"
#include "loading/loading.hpp"

class FlipMapApp;

typedef enum
{
    FlipMapRunViewLogin = 0,        // login view
    FlipMapRunViewRegistration = 1, // registration view
    FlipMapRunViewMapData = 2,      // map data view
    FlipMapRunViewLocation = 3      // location view
} FlipMapRunView;

typedef enum
{
    LoginCredentialsMissing = -1, // Credentials missing
    LoginSuccess = 0,             // Login successful
    LoginUserNotFound = 1,        // User not found
    LoginWrongPassword = 2,       // Wrong password
    LoginWaiting = 3,             // Waiting for response
    LoginNotStarted = 4,          // Login not started
    LoginRequestError = 5,        // Request error
} LoginStatus;

typedef enum
{
    RegistrationCredentialsMissing = -1, // Credentials missing
    RegistrationSuccess = 0,             // Registration successful
    RegistrationUserExists = 1,          // User already exists
    RegistrationRequestError = 2,        // Request error
    RegistrationNotStarted = 3,          // Registration not started
    RegistrationWaiting = 4,             // Waiting for response
} RegistrationStatus;

typedef enum
{
    MapDataCredentialsMissing = -1, // Credentials missing
    MapDataNotStarted = 0,          // Map data not started
    MapDataWaiting = 1,             // Waiting for map data response
    MapDataSuccess = 2,             // Map data fetched successfully
    MapDataParseError = 3,          // Error parsing map data
    MapDataRequestError = 4,        // Error in map data request
} MapDataStatus;

typedef enum
{
    LocationCredentialsMissing = -1, // Credentials missing
    LocationNotStarted = 0,          // Location not started
    LocationWaiting = 1,             // Waiting for location response
    LocationSuccess = 2,             // Location fetched successfully
    LocationParseError = 3,          // Error parsing location data
    LocationRequestError = 4,        // Error in location request
} LocationStatus;

typedef enum
{
    RequestTypeLogin = 0,         // Request login (login the user)
    RequestTypeRegistration = 1,  // Request registration (register the user)
    RequestTypeMapData = 2,       // Request map data (fetch map data)
    RequestTypeLocationUpdate = 3 // Request location update (update user location)
} RequestType;

class FlipMapRun
{
    void *appContext;                      // reference to the app context
    FlipMapRunView currentView;            // current view of the social run
    bool inCountryView;                    // flag to track if we're viewing countries or cities
    bool inputHeld;                        // flag to check if input is held
    InputKey lastInput;                    // last input key pressed
    std::unique_ptr<Loading> loading;      // loading animation instance
    LocationStatus locationStatus;         // current location status
    LoginStatus loginStatus;               // current login status
    MapDataStatus mapDataStatus;           // current map data status
    uint8_t menuIndex;                     // current menu index
    uint8_t menuMaxIndex;                  // maximum menu index
    uint8_t menuStartIndex;                // starting index for display
    RegistrationStatus registrationStatus; // current registration status
    bool shouldDebounce;                   // flag to debounce input
    bool shouldReturnToMenu;               // Flag to signal return to menu
    //
    void debounceInput();                      // debounce input to prevent multiple triggers
    void drawLocationView(Canvas *canvas);     // draw the location view
    void drawLoginView(Canvas *canvas);        // draw the login view
    void drawMapDataMenu(Canvas *canvas);      // draw the scrollable map data menu
    void drawMapDataView(Canvas *canvas);      // draw the map data view
    void drawRegistrationView(Canvas *canvas); // draw the registration view
    bool httpRequestIsFinished();              // check if the HTTP request is finished
    void userRequest(RequestType requestType); // Send a user request to the server based on the request type
public:
    FlipMapRun(void *appContext);
    ~FlipMapRun();
    //
    bool isActive() const { return shouldReturnToMenu == false; } // Check if the run is active
    void updateDraw(Canvas *canvas);                              // update and draw the run
    void updateInput(InputEvent *event);                          // update input for the run
};
