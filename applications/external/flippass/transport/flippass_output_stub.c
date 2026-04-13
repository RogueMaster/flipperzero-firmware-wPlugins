#include "../flippass.h"

const char* flippass_output_transport_name(FlipPassOutputTransport transport) {
    return transport == FlipPassOutputTransportBluetooth ? "Bluetooth" : "USB";
}

bool flippass_output_type_string(App* app, FlipPassOutputTransport transport, const char* text) {
    UNUSED(app);
    UNUSED(transport);
    UNUSED(text);
    return false;
}

bool flippass_output_type_login(
    App* app,
    FlipPassOutputTransport transport,
    const char* username,
    const char* password) {
    UNUSED(app);
    UNUSED(transport);
    UNUSED(username);
    UNUSED(password);
    return false;
}

bool flippass_output_type_vault_ref(
    App* app,
    FlipPassOutputTransport transport,
    KDBXVault* vault,
    const KDBXFieldRef* ref) {
    UNUSED(app);
    UNUSED(transport);
    UNUSED(vault);
    UNUSED(ref);
    return false;
}

bool flippass_output_type_login_refs(
    App* app,
    FlipPassOutputTransport transport,
    KDBXVault* vault,
    const KDBXFieldRef* username_ref,
    const KDBXFieldRef* password_ref) {
    UNUSED(app);
    UNUSED(transport);
    UNUSED(vault);
    UNUSED(username_ref);
    UNUSED(password_ref);
    return false;
}

bool flippass_output_type_autotype(
    App* app,
    FlipPassOutputTransport transport,
    const KDBXEntry* entry) {
    UNUSED(app);
    UNUSED(transport);
    UNUSED(entry);
    return false;
}

void flippass_output_release_all(App* app) {
    UNUSED(app);
}

bool flippass_output_bluetooth_is_connected(const App* app) {
    UNUSED(app);
    return false;
}

bool flippass_output_bluetooth_is_advertising(const App* app) {
    UNUSED(app);
    return false;
}

bool flippass_output_bluetooth_advertise(App* app) {
    UNUSED(app);
    return false;
}

void flippass_output_bluetooth_get_name(char* buffer, size_t size) {
    if(buffer != NULL && size > 0U) {
        buffer[0] = '\0';
    }
}

void flippass_output_cleanup(App* app) {
    UNUSED(app);
}
