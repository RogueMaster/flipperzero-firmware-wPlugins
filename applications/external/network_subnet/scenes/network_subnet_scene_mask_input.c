#include "network_subnet.h"
#include "../views/mask_input_view.h"
#include "../scenes/scenes.h"

void network_subnet_scene_mask_input_on_enter(void* context) {
    FURI_LOG_D(NETWORK_SUBNET_TAG, "In on_enter of mask_input");
    furi_assert(context);
    NetworkSubnetApp* app = context;
    with_view_model(
        app->mask_input_view,
        MaskInputViewModel * m,
        {
            uint8_t three_counter = 0;
            uint8_t octet_counter = 0;
            uint8_t hundred = 100;
            for(short i = 0; i < 12; i++) {
                if(three_counter == 3) {
                    octet_counter += 1;
                    hundred = 100;
                    three_counter = 0;
                }
                m->octect_digits[i] = (short)(app->subnet_mask[octet_counter] / hundred) % 10;
                three_counter += 1;
                hundred = hundred / 10;
            }
            m->active_digit = 0;
        },
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdMaskInput);
}

bool network_subnet_scene_mask_input_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    NetworkSubnetApp* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case EventMaskConfirmed:
            with_view_model(
                app->mask_input_view,
                MaskInputViewModel * m,
                {
                    uint8_t three_counter = 0;
                    uint8_t octet_counter = 0;
                    uint8_t temp_octet = 0;
                    uint8_t hundred = 100;
                    for(int i = 0; i < 12; i++) {
                        temp_octet = temp_octet + m->octect_digits[i] * hundred;
                        hundred = hundred / 10;
                        three_counter += 1;
                        if(three_counter == 3) {
                            hundred = 100;
                            app->subnet_mask[octet_counter] = temp_octet;
                            three_counter = 0;
                            FURI_LOG_D(
                                NETWORK_SUBNET_TAG,
                                "calculated subnet %d to be %d",
                                octet_counter,
                                temp_octet);
                            octet_counter += 1;
                            temp_octet = 0;
                        }
                    }
                },
                true);
            scene_manager_next_scene(app->scene_manager, NetworkSubnetSceneIpInput);
            consumed = true;
            break;
        }
        break;
    default:
        consumed = false;
    }
    return consumed;
}
void network_subnet_scene_mask_input_on_exit(void* context) {
    furi_assert(context);
    NetworkSubnetApp* app = context;
    UNUSED(app);
}
