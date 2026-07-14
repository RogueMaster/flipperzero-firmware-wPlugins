#include <furi.h>
#include <gui/gui.h>

#define WIDTH  128
#define HEIGHT 64

static volatile int exit_app;
static uint8_t grid[WIDTH][HEIGHT];
static uint8_t new_grid[WIDTH][HEIGHT];
static uint8_t fullscreen;
static uint8_t speed;
static uint16_t cells;
static uint32_t cycles;

typedef enum {
    ModeTypeRandom,
    ModeTypeBlinker,
    ModeTypeGlider,
    ModeTypeGliderGun,
    ModeTypePentomino,
    ModeTypeDiehard,
    ModeTypeAcorn,
    // ----------------
    ModeTypeMax
} ModeType;
static ModeType mode;

typedef enum {
    StageTypeStartup, // Show startup screen
    StageTypeInit, // Initialize grid
    StageTypeShowInfo, // Show info for current mode
    StageTypeRunning, // Running simulation
    StageTypeEnd // End of simulation has been reached
} StageType;
static StageType stage;

void init_grid(void) {
    memset(grid, 0, sizeof(grid));

    if(mode == ModeTypeRandom) {
        int x, y;
        for(x = 0; x < WIDTH; x++)
            for(y = 0; y < HEIGHT; y++)
                grid[x][y] = (random() & 1);
    } else if(mode == ModeTypeBlinker) {
        grid[1 + (WIDTH / 2)][0 + (HEIGHT / 2)] = 1;
        grid[1 + (WIDTH / 2)][1 + (HEIGHT / 2)] = 1;
        grid[1 + (WIDTH / 2)][2 + (HEIGHT / 2)] = 1;
    } else if(mode == ModeTypeGlider) {
        grid[0 + (WIDTH / 2)][2 + (HEIGHT / 2)] = 1;
        grid[1 + (WIDTH / 2)][0 + (HEIGHT / 2)] = 1;
        grid[1 + (WIDTH / 2)][2 + (HEIGHT / 2)] = 1;
        grid[2 + (WIDTH / 2)][1 + (HEIGHT / 2)] = 1;
        grid[2 + (WIDTH / 2)][2 + (HEIGHT / 2)] = 1;
    } else if(mode == ModeTypeGliderGun) {
        grid[1][5] = 1;
        grid[1][6] = 1;
        grid[2][5] = 1;
        grid[2][6] = 1;
        grid[11][5] = 1;
        grid[11][6] = 1;
        grid[11][7] = 1;
        grid[12][4] = 1;
        grid[12][8] = 1;
        grid[13][3] = 1;
        grid[13][9] = 1;
        grid[14][3] = 1;
        grid[14][9] = 1;
        grid[15][6] = 1;
        grid[16][4] = 1;
        grid[16][8] = 1;
        grid[17][5] = 1;
        grid[17][6] = 1;
        grid[17][7] = 1;
        grid[18][6] = 1;
        grid[21][3] = 1;
        grid[21][4] = 1;
        grid[21][5] = 1;
        grid[22][3] = 1;
        grid[22][4] = 1;
        grid[22][5] = 1;
        grid[23][2] = 1;
        grid[23][6] = 1;
        grid[25][1] = 1;
        grid[25][2] = 1;
        grid[25][6] = 1;
        grid[25][7] = 1;
        grid[35][3] = 1;
        grid[35][4] = 1;
        grid[36][3] = 1;
        grid[36][4] = 1;
    } else if(mode == ModeTypePentomino) {
        grid[0 + (WIDTH / 2)][1 + (HEIGHT / 2)] = 1;
        grid[1 + (WIDTH / 2)][0 + (HEIGHT / 2)] = 1;
        grid[1 + (WIDTH / 2)][1 + (HEIGHT / 2)] = 1;
        grid[1 + (WIDTH / 2)][2 + (HEIGHT / 2)] = 1;
        grid[2 + (WIDTH / 2)][0 + (HEIGHT / 2)] = 1;
    } else if(mode == ModeTypeDiehard) {
        grid[0 + (WIDTH / 2)][4 + (HEIGHT / 2)] = 1;
        grid[1 + (WIDTH / 2)][4 + (HEIGHT / 2)] = 1;
        grid[1 + (WIDTH / 2)][5 + (HEIGHT / 2)] = 1;
        grid[5 + (WIDTH / 2)][5 + (HEIGHT / 2)] = 1;
        grid[6 + (WIDTH / 2)][3 + (HEIGHT / 2)] = 1;
        grid[6 + (WIDTH / 2)][5 + (HEIGHT / 2)] = 1;
        grid[7 + (WIDTH / 2)][5 + (HEIGHT / 2)] = 1;
    } else if(mode == ModeTypeAcorn) {
        grid[0 + (WIDTH / 2)][4 + (HEIGHT / 2)] = 1;
        grid[1 + (WIDTH / 2)][2 + (HEIGHT / 2)] = 1;
        grid[1 + (WIDTH / 2)][4 + (HEIGHT / 2)] = 1;
        grid[3 + (WIDTH / 2)][3 + (HEIGHT / 2)] = 1;
        grid[4 + (WIDTH / 2)][4 + (HEIGHT / 2)] = 1;
        grid[5 + (WIDTH / 2)][4 + (HEIGHT / 2)] = 1;
        grid[6 + (WIDTH / 2)][4 + (HEIGHT / 2)] = 1;
    }

    cycles = 0;
}

static void draw_str_in_rounded_frame(Canvas* canvas, const char* str) {
    int width;
    width = canvas_string_width(canvas, str);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 59 - width / 2, 11, width + 6, 15);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, 60 - width / 2, 12, width + 4, 13, 2);
    canvas_draw_str(canvas, 62 - width / 2, 21, str);
}

static void draw_grid_callback(Canvas* canvas, void* context) {
    int x, y;
    char str[16];
    UNUSED(context);

    // Clear display
    canvas_clear(canvas);
    cells = 0;

    // Draw grid to canvas
    for(x = 0; x < WIDTH; x++) {
        for(y = 0; y < HEIGHT; y++) {
            if(grid[x][y]) {
                canvas_draw_dot(canvas, x, y);
                cells++;
            }
        }
    }

    // Handle mode info
    if(stage == StageTypeShowInfo) {
        if(mode == ModeTypeRandom)
            draw_str_in_rounded_frame(canvas, "Random");
        else if(mode == ModeTypeBlinker)
            draw_str_in_rounded_frame(canvas, "Blinker");
        else if(mode == ModeTypeGlider)
            draw_str_in_rounded_frame(canvas, "Glider");
        else if(mode == ModeTypeGliderGun)
            draw_str_in_rounded_frame(canvas, "Glider gun");
        else if(mode == ModeTypePentomino)
            draw_str_in_rounded_frame(canvas, "Pentomino");
        else if(mode == ModeTypeDiehard)
            draw_str_in_rounded_frame(canvas, "Diehard");
        else if(mode == ModeTypeAcorn)
            draw_str_in_rounded_frame(canvas, "Acorn");
    }

    // Handle status line
    if(!fullscreen) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 0, 54, 128, 10);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_line(canvas, 0, 53, 127, 53);
        snprintf(str, sizeof(str), "Cyc:%li", cycles);
        canvas_draw_str(canvas, 0, 62, str);
        snprintf(str, sizeof(str), "Cell:%i", cells);
        canvas_draw_str(canvas, 56, 62, str);
        snprintf(str, sizeof(str), "Spd:%i", speed);
        canvas_draw_str(canvas, 105, 62, str);
    }
}

void update_grid(void) {
    int x, y;

    // Todo: Clearing new_grid shouldn't be needed here (it's completely set below), but somehow it is?!
    // When switching from random to blinker, sometimes the grid is not updated correctly :-(
    memset(new_grid, 0, sizeof(grid));

    cycles++;
    for(x = 0; x < WIDTH; x++) {
        for(y = 0; y < HEIGHT; y++) {
            int neighbors = 0;
            for(int dx = -1; dx <= 1; dx++) {
                for(int dy = -1; dy <= 1; dy++) {
                    if(dx == 0 && dy == 0) {
                        continue;
                    }
                    int nx = (x + dx) % WIDTH;
                    int ny = (y + dy) % HEIGHT;
                    neighbors += grid[nx][ny];
                }
            }

            if((grid[x][y] == 1) && (neighbors < 2)) // Underpopulation
                new_grid[x][y] = 0;
            else if((grid[x][y] == 1) && (neighbors > 3)) // Overpopulation
                new_grid[x][y] = 0;
            else if((grid[x][y] == 0) && (neighbors == 3)) // Reproduction
                new_grid[x][y] = 1;
            else // Stasis
                new_grid[x][y] = grid[x][y];
        }
    }

    memcpy(grid, new_grid, sizeof(grid));
}

static void input_callback(InputEvent* input_event, void* context) {
    UNUSED(context);
    if(input_event->key == InputKeyBack) // Back key pressed?
    {
        exit_app = 1;
    } else if((input_event->key == InputKeyOk) && (input_event->type == InputTypeRelease)) {
        fullscreen++;
        fullscreen %= 2;
    } else if((input_event->key == InputKeyUp) && (input_event->type == InputTypeRelease)) {
        if(speed < 9) speed++;
    } else if((input_event->key == InputKeyDown) && (input_event->type == InputTypeRelease)) {
        if(speed > 0) speed--;
    } else if((input_event->key == InputKeyRight) && (input_event->type == InputTypeRelease)) {
        mode++;
        mode %= ModeTypeMax;
        stage = StageTypeInit;
    } else if((input_event->key == InputKeyLeft) && (input_event->type == InputTypeRelease)) {
        if(mode == 0)
            mode = ModeTypeMax - 1;
        else
            mode--;
        stage = StageTypeInit;
    }
}

int32_t game_of_life_app(void* p) {
    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* view_port = view_port_alloc();
    void* my_context = NULL;
    UNUSED(p);

    // Prepare
    view_port_draw_callback_set(view_port, draw_grid_callback, my_context);
    view_port_input_callback_set(view_port, input_callback, my_context);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Init
    mode = ModeTypeRandom;
    stage = StageTypeStartup;
    speed = 5;

    // Loop until back key is pressed
    while(exit_app != 1) {
        static uint32_t timer;

        // Handle speed
        furi_delay_ms(50 * (10 - speed));
        timer += (50 * (10 - speed));

        // Handle different modes and update grid
        if(stage == StageTypeStartup) {
            if(timer >= 100) {
                stage = StageTypeInit;
                timer = 0;
            }
        } else if(stage == StageTypeInit) {
            init_grid();
            stage = StageTypeShowInfo;
            timer = 0;
        } else if(stage == StageTypeShowInfo) {
            if(timer >= 2000) {
                stage = StageTypeRunning;
                timer = 0;
            }
        } else if(stage == StageTypeRunning) {
            update_grid();
        } else if(stage == StageTypeEnd) {
            // Do nothing...
        }

        // Update canvas
        view_port_update(view_port);
    }

    // Clean up
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);

    return 0;
}
