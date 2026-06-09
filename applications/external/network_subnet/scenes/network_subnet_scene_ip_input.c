#include "network_subnet.h"
#include "../views/ip_input_view.h"
#include "../scenes/scenes.h"
#include "../core/subnet_math.h"

void network_subnet_scene_ip_input_on_enter(void* context) {
    FURI_LOG_D(NETWORK_SUBNET_TAG, "In on_enter of ip_input");
    furi_assert(context);
    NetworkSubnetApp* app = context;
    app->cidr = count_number_of_continuous_ones(ip_to_u32(app->subnet_mask));
    with_view_model(
        app->ip_input_view,
        IpInputViewModel * m,
        {
            uint8_t three_counter = 0;
            uint8_t octet_counter = 0;
            uint8_t hundred = 100;
            for(short i = 0; i < 12; i++) {
                m->octets_and_cidr_digits[i] = (short)(app->ip[octet_counter] / hundred) % 10;
                three_counter += 1;
                hundred = hundred / 10;
                if(three_counter == 3) {
                    octet_counter += 1;
                    hundred = 100;
                    three_counter = 0;
                }
            }
            FURI_LOG_D(NETWORK_SUBNET_TAG, "received cidr %d", app->cidr);
            m->octets_and_cidr_digits[12] = (int)app->cidr / 10;
            m->octets_and_cidr_digits[13] = (int)app->cidr % 10;
            m->active_digit = 0;
        },
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdIpInput);
}

bool network_subnet_scene_ip_input_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    NetworkSubnetApp* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case EventIpConfirmed:
            FURI_LOG_D(NETWORK_SUBNET_TAG, "received eventIpConfirmed event");
            with_view_model(
                app->ip_input_view,
                IpInputViewModel * m,
                {
                    uint8_t three_counter = 0;
                    uint8_t octet_counter = 0;
                    uint8_t temp_octet = 0;
                    uint8_t hundred = 100;
                    for(int i = 0; i < 14; i++) {
                        temp_octet = temp_octet + m->octets_and_cidr_digits[i] * hundred;
                        hundred = hundred / 10;
                        three_counter += 1;
                        if(three_counter == 3) {
                            hundred = 100;
                            app->ip[octet_counter] = temp_octet;
                            three_counter = 0;
                            FURI_LOG_D(
                                NETWORK_SUBNET_TAG,
                                "calculated octet %d to be %d",
                                octet_counter,
                                temp_octet);
                            octet_counter += 1;
                            temp_octet = 0;
                        }
                    }
                    app->cidr = m->octets_and_cidr_digits[12] * 10 + m->octets_and_cidr_digits[13];
                    u32_to_ip(cidr_to_mask(app->cidr), app->subnet_mask);
                    FURI_LOG_D(NETWORK_SUBNET_TAG, "calculated cidr %d", app->cidr);
                },
                false);
            scene_manager_next_scene(app->scene_manager, NetworkSubnetSceneResult);
            consumed = true;
            break;
        }
        break;
    default:
        consumed = false;
    }
    return consumed;
}
void network_subnet_scene_ip_input_on_exit(void* context) {
    furi_assert(context);
    NetworkSubnetApp* app = context;
    UNUSED(app);
}
