#include "chameleon_animation_view.h"
#include <furi.h>
#include <gui/elements.h>

#define ANIMATION_FPS         8
#define ANIMATION_DURATION_MS 3000

struct ChameleonAnimationView {
    View* view;
    FuriTimer* timer;
    ChameleonAnimationViewCallback callback;
    void* callback_context;
    ChameleonAnimationType animation_type;
};

typedef struct {
    uint8_t frame;
    bool running;
    ChameleonAnimationType type;
} ChameleonAnimationViewModel;

// Draw dolphin (Flipper mascot)
static void draw_dolphin(Canvas* canvas, int x, int y, uint8_t frame) {
    // Corpo do golfinho
    canvas_draw_circle(canvas, x + 8, y + 10, 8);

    // Cabeça
    canvas_draw_circle(canvas, x + 6, y + 5, 5);

    // Olho (pisca a cada 2 frames)
    if(frame % 4 < 3) {
        canvas_draw_dot(canvas, x + 7, y + 4);
        canvas_draw_dot(canvas, x + 8, y + 4);
    } else {
        canvas_draw_line(canvas, x + 6, y + 4, x + 9, y + 4);
    }

    // Sorriso
    canvas_draw_line(canvas, x + 5, y + 7, x + 9, y + 7);
    canvas_draw_dot(canvas, x + 4, y + 6);
    canvas_draw_dot(canvas, x + 10, y + 6);

    // Barbatana superior
    canvas_draw_line(canvas, x + 10, y + 8, x + 13, y + 6);
    canvas_draw_line(canvas, x + 13, y + 6, x + 12, y + 9);

    // Barbatana frontal (movimento de aceno)
    int wave_offset = (frame % 4) - 2;
    canvas_draw_line(canvas, x + 2, y + 10, x, y + 12 + wave_offset);
    canvas_draw_line(canvas, x, y + 12 + wave_offset, x + 2, y + 14 + wave_offset);

    // Cauda
    canvas_draw_line(canvas, x + 14, y + 12, x + 17, y + 10);
    canvas_draw_line(canvas, x + 14, y + 12, x + 17, y + 14);
}

// Draw chameleon
static void draw_chameleon(Canvas* canvas, int x, int y, uint8_t frame) {
    // Corpo
    canvas_draw_circle(canvas, x + 8, y + 10, 7);

    // Cabeça triangular característica
    canvas_draw_line(canvas, x + 12, y + 8, x + 16, y + 6);
    canvas_draw_line(canvas, x + 12, y + 12, x + 16, y + 14);
    canvas_draw_line(canvas, x + 16, y + 6, x + 18, y + 10);
    canvas_draw_line(canvas, x + 16, y + 14, x + 18, y + 10);

    // Olho grande característico (move)
    int eye_x = x + 16 + (frame % 2);
    canvas_draw_circle(canvas, eye_x, y + 9, 2);
    canvas_draw_dot(canvas, eye_x, y + 9);

    // Crista
    for(int i = 0; i < 5; i++) {
        if(i % 2 == (frame % 2)) {
            canvas_draw_dot(canvas, x + 8 + i, y + 4);
        }
    }

    // Patas
    canvas_draw_line(canvas, x + 5, y + 15, x + 4, y + 18);
    canvas_draw_line(canvas, x + 11, y + 15, x + 12, y + 18);

    // Cauda enrolada (característica do camaleão)
    canvas_draw_circle(canvas, x + 2, y + 12, 3);
    canvas_draw_circle(canvas, x + 1, y + 10, 2);

    // Língua (sai de vez em quando)
    if(frame % 8 > 5) {
        canvas_draw_line(canvas, x + 18, y + 10, x + 22 + (frame % 3), y + 10);
        canvas_draw_circle(canvas, x + 22 + (frame % 3), y + 10, 1);
    }
}

// Draw bar counter
static void draw_bar(Canvas* canvas, uint8_t frame) {
    // Balcão do bar
    canvas_draw_box(canvas, 0, 35, 128, 3);
    canvas_draw_box(canvas, 0, 38, 128, 26);

    // Prateleira de trás com garrafas
    canvas_draw_line(canvas, 10, 25, 118, 25);

    // Garrafas (desenho simples)
    for(int i = 0; i < 6; i++) {
        int bottle_x = 15 + i * 18;
        canvas_draw_box(canvas, bottle_x, 18, 4, 7);
        canvas_draw_box(canvas, bottle_x + 1, 16, 2, 2);
    }

    // Copos na mesa para cada personagem
    // Copo do golfinho (esquerda)
    canvas_draw_box(canvas, 25, 32, 5, 3);
    canvas_draw_line(canvas, 24, 35, 30, 35);

    // Copo do camaleão (direita)
    canvas_draw_box(canvas, 90, 32, 5, 3);
    canvas_draw_line(canvas, 89, 35, 95, 35);

    // Bolhas nas bebidas (animadas)
    if(frame % 2 == 0) {
        canvas_draw_dot(canvas, 26, 33);
        canvas_draw_dot(canvas, 92, 33);
    }

    // Banquinhos
    // Banquinho esquerdo (golfinho)
    canvas_draw_box(canvas, 20, 42, 8, 2);
    canvas_draw_line(canvas, 22, 44, 22, 50);
    canvas_draw_line(canvas, 26, 44, 26, 50);

    // Banquinho direito (camaleão)
    canvas_draw_box(canvas, 85, 42, 8, 2);
    canvas_draw_line(canvas, 87, 44, 87, 50);
    canvas_draw_line(canvas, 91, 44, 91, 50);

    // Placa do bar
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "CHAMELEON BAR");

    // Detalhes decorativos
    // Lustre/lâmpada
    canvas_draw_circle(canvas, 64, 8, 3);
    canvas_draw_line(canvas, 64, 5, 64, 0);

    // Efeito de luz (pisca)
    if(frame % 4 < 2) {
        canvas_draw_circle(canvas, 64, 8, 4);
    }
}

// Draw handshake animation
static void draw_handshake_scene(Canvas* canvas, uint8_t frame) {
    // Ground line
    canvas_draw_line(canvas, 0, 50, 128, 50);

    // Golfinho (left side)
    draw_dolphin(canvas, 20, 30, frame);

    // Camaleão (right side)
    draw_chameleon(canvas, 80, 30, frame);

    // Handshake in the middle
    if(frame > 10 && frame < 25) {
        canvas_draw_line(canvas, 35, 40, 75, 40);
        canvas_draw_circle(canvas, 55, 40, 3);
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, "Conectando...");
    }

    if(frame >= 25) {
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, "Conectado!");
        // Hearts
        canvas_draw_str(canvas, 45, 20, "<3");
        canvas_draw_str(canvas, 75, 20, "<3");
    }
}

// Draw workshop scene (working together)
static void draw_workshop_scene(Canvas* canvas, uint8_t frame) {
    // Workshop table
    canvas_draw_box(canvas, 20, 35, 88, 5);
    canvas_draw_line(canvas, 20, 40, 20, 55);
    canvas_draw_line(canvas, 108, 40, 108, 55);

    // Tools on table
    canvas_draw_line(canvas, 30, 30, 35, 35); // Screwdriver
    canvas_draw_box(canvas, 40, 32, 8, 3); // Wrench
    canvas_draw_circle(canvas, 90, 32, 3); // Chip/component

    // Golfinho working (left)
    draw_dolphin(canvas, 15, 20, frame);

    // Camaleão working (right)
    draw_chameleon(canvas, 75, 20, frame);

    // Sparks/work effect
    if(frame % 4 < 2) {
        canvas_draw_dot(canvas, 50 + (frame % 3), 30);
        canvas_draw_dot(canvas, 70 - (frame % 3), 30);
    }

    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "Trabalhando juntos!");
}

// Draw dance celebration
static void draw_dance_scene(Canvas* canvas, uint8_t frame) {
    // Dance floor
    canvas_draw_box(canvas, 10, 45, 108, 19);

    // Music notes
    if(frame % 2 == 0) {
        canvas_draw_str(canvas, 20, 15, "♪");
        canvas_draw_str(canvas, 90, 15, "♫");
        canvas_draw_str(canvas, 55, 10, "♪");
    } else {
        canvas_draw_str(canvas, 25, 12, "♫");
        canvas_draw_str(canvas, 85, 18, "♪");
        canvas_draw_str(canvas, 60, 15, "♫");
    }

    // Dancing positions (alternating)
    int dolphin_y = 35 + (frame % 4 < 2 ? -2 : 2);
    int chameleon_y = 35 + (frame % 4 >= 2 ? -2 : 2);

    draw_dolphin(canvas, 30, dolphin_y, frame);
    draw_chameleon(canvas, 70, chameleon_y, frame);

    canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, "Sucesso!");
}

// Draw error scene
static void draw_error_scene(Canvas* canvas, uint8_t frame) {
    // Sad expressions
    draw_dolphin(canvas, 30, 35, 0); // Static sad dolphin
    draw_chameleon(canvas, 70, 35, 0); // Static sad chameleon

    // X marks over connection
    canvas_draw_line(canvas, 50, 30, 70, 50);
    canvas_draw_line(canvas, 70, 30, 50, 50);

    // Error message
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Falha na Conexao");

    // Sad effects
    if(frame % 8 < 4) {
        canvas_draw_str(canvas, 35, 25, ":(");
        canvas_draw_str(canvas, 85, 25, ":(");
    }
}

// Draw scanning animation
static void draw_scan_scene(Canvas* canvas, uint8_t frame) {
    // Golfinho scanning
    draw_dolphin(canvas, 20, 35, frame);

    // Radar/scan effect
    int scan_radius = (frame % 16) * 4;
    if(scan_radius > 0) {
        canvas_draw_circle(canvas, 25, 40, scan_radius);
        if(scan_radius > 8) {
            canvas_draw_circle(canvas, 25, 40, scan_radius - 8);
        }
    }

    // Found chameleon appears gradually
    if(frame > 20) {
        draw_chameleon(canvas, 80, 35, frame);
        canvas_draw_str_aligned(canvas, 95, 25, AlignCenter, AlignTop, "Encontrado!");
    }

    canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignTop, "Procurando...");
}

// Draw data transfer animation
static void draw_transfer_scene(Canvas* canvas, uint8_t frame) {
    draw_dolphin(canvas, 15, 35, frame);
    draw_chameleon(canvas, 85, 35, frame);

    // Data packets moving between them
    int packet_pos = 30 + (frame % 20) * 3;
    if(packet_pos < 85) {
        canvas_draw_box(canvas, packet_pos, 42, 4, 2);
        canvas_draw_box(canvas, packet_pos - 10, 44, 3, 2);
        canvas_draw_box(canvas, packet_pos - 20, 40, 2, 2);
    }

    // Progress bar
    int progress = (frame * 128) / 32;
    if(progress > 128) progress = 128;
    canvas_draw_frame(canvas, 10, 55, 108, 6);
    canvas_draw_box(canvas, 11, 56, progress * 106 / 128, 4);

    canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, "Transferindo...");
}

// Draw goodbye scene
static void draw_disconnect_scene(Canvas* canvas, uint8_t frame) {
    // Waving goodbye
    draw_dolphin(canvas, 20, 35, frame);
    draw_chameleon(canvas, 80, 35, frame);

    // Wave lines
    if(frame % 4 < 2) {
        canvas_draw_line(canvas, 10, 35, 15, 30);
        canvas_draw_line(canvas, 90, 35, 95, 30);
    }

    // Moving apart
    int separation = frame * 2;
    if(separation > 20) separation = 20;

    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Ate logo!");

    // Fade effect with dots
    if(frame > 15) {
        for(int i = 0; i < (frame - 15); i++) {
            canvas_draw_dot(canvas, 40 + i * 8, 50);
        }
    }
}

// Draw speech bubbles for bar scene
static void draw_speech(Canvas* canvas, uint8_t frame) {
    if(frame < 8) {
        // Golfinho fala primeiro
        canvas_draw_rframe(canvas, 30, 5, 25, 12, 2);
        canvas_draw_dot(canvas, 32, 16);
        canvas_draw_dot(canvas, 30, 18);
        canvas_draw_str_aligned(canvas, 42, 8, AlignCenter, AlignTop, "Ola!");
    } else if(frame < 16) {
        // Camaleão responde
        canvas_draw_rframe(canvas, 75, 5, 30, 12, 2);
        canvas_draw_dot(canvas, 93, 16);
        canvas_draw_dot(canvas, 95, 18);
        canvas_draw_str_aligned(canvas, 90, 8, AlignCenter, AlignTop, "E ai!");
    } else if(frame < 24) {
        // Fazem um brinde
        canvas_draw_rframe(canvas, 45, 8, 38, 12, 2);
        canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignTop, "Saude!");

        // Mostrar copos levantados
        canvas_draw_str(canvas, 22, 28, "^");
        canvas_draw_str(canvas, 95, 28, "^");
    }
}

static void chameleon_animation_view_draw_callback(Canvas* canvas, void* model) {
    ChameleonAnimationViewModel* m = model;

    canvas_clear(canvas);

    switch(m->type) {
    case ChameleonAnimationBar:
        // Desenha o bar
        draw_bar(canvas, m->frame);
        // Desenha os personagens sentados nos bancos
        draw_dolphin(canvas, 15, 38, m->frame);
        draw_chameleon(canvas, 70, 38, m->frame);
        // Balões de fala
        draw_speech(canvas, m->frame);
        // Mensagem de conexão
        if(m->frame > 20) {
            canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, "Conectado!");
        }
        break;

    case ChameleonAnimationHandshake:
        draw_handshake_scene(canvas, m->frame);
        break;

    case ChameleonAnimationWorkshop:
        draw_workshop_scene(canvas, m->frame);
        break;

    case ChameleonAnimationDance:
        draw_dance_scene(canvas, m->frame);
        break;

    case ChameleonAnimationError:
        draw_error_scene(canvas, m->frame);
        break;

    case ChameleonAnimationScan:
        draw_scan_scene(canvas, m->frame);
        break;

    case ChameleonAnimationTransfer:
        draw_transfer_scene(canvas, m->frame);
        break;

    case ChameleonAnimationDisconnect:
        draw_disconnect_scene(canvas, m->frame);
        break;

    case ChameleonAnimationSuccess:
        draw_dance_scene(canvas, m->frame); // Reuse dance for success
        break;

    default:
        draw_handshake_scene(canvas, m->frame);
        break;
    }
}

static void chameleon_animation_view_timer_callback(void* context) {
    ChameleonAnimationView* animation_view = context;

    with_view_model(
        animation_view->view,
        ChameleonAnimationViewModel * model,
        {
            if(model->running) {
                model->frame++;

                // Para após certo número de frames
                if(model->frame >= 32) {
                    model->running = false;
                    if(animation_view->callback) {
                        animation_view->callback(animation_view->callback_context);
                    }
                }
            }
        },
        true);
}

static bool chameleon_animation_view_input_callback(InputEvent* event, void* context) {
    UNUSED(context);
    // Skip animation on any button press
    if(event->type == InputTypeShort) {
        ChameleonAnimationView* animation_view = context;
        if(animation_view->callback) {
            animation_view->callback(animation_view->callback_context);
        }
        return true;
    }
    return false;
}

ChameleonAnimationView* chameleon_animation_view_alloc() {
    ChameleonAnimationView* animation_view = malloc(sizeof(ChameleonAnimationView));

    animation_view->view = view_alloc();
    view_allocate_model(
        animation_view->view, ViewModelTypeLocking, sizeof(ChameleonAnimationViewModel));
    view_set_draw_callback(animation_view->view, chameleon_animation_view_draw_callback);
    view_set_input_callback(animation_view->view, chameleon_animation_view_input_callback);
    view_set_context(animation_view->view, animation_view);

    animation_view->timer = furi_timer_alloc(
        chameleon_animation_view_timer_callback, FuriTimerTypePeriodic, animation_view);

    animation_view->animation_type = ChameleonAnimationBar; // Default

    with_view_model(
        animation_view->view,
        ChameleonAnimationViewModel * model,
        {
            model->frame = 0;
            model->running = false;
            model->type = ChameleonAnimationBar;
        },
        false);

    return animation_view;
}

void chameleon_animation_view_free(ChameleonAnimationView* animation_view) {
    furi_assert(animation_view);

    furi_timer_free(animation_view->timer);
    view_free(animation_view->view);
    free(animation_view);
}

View* chameleon_animation_view_get_view(ChameleonAnimationView* animation_view) {
    furi_assert(animation_view);
    return animation_view->view;
}

void chameleon_animation_view_set_callback(
    ChameleonAnimationView* animation_view,
    ChameleonAnimationViewCallback callback,
    void* context) {
    furi_assert(animation_view);
    animation_view->callback = callback;
    animation_view->callback_context = context;
}

void chameleon_animation_view_set_type(
    ChameleonAnimationView* animation_view,
    ChameleonAnimationType type) {
    furi_assert(animation_view);
    animation_view->animation_type = type;

    with_view_model(
        animation_view->view, ChameleonAnimationViewModel * model, { model->type = type; }, false);
}

void chameleon_animation_view_start(ChameleonAnimationView* animation_view) {
    furi_assert(animation_view);

    with_view_model(
        animation_view->view,
        ChameleonAnimationViewModel * model,
        {
            model->frame = 0;
            model->running = true;
            model->type = animation_view->animation_type;
        },
        true);

    furi_timer_start(animation_view->timer, 1000 / ANIMATION_FPS);
}

void chameleon_animation_view_stop(ChameleonAnimationView* animation_view) {
    furi_assert(animation_view);

    furi_timer_stop(animation_view->timer);

    with_view_model(
        animation_view->view,
        ChameleonAnimationViewModel * model,
        { model->running = false; },
        false);
}
