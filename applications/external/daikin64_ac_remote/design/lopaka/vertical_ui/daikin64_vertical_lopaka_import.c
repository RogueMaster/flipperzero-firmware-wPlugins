// Lopaka import source for Daikin64 vertical UI mockups.
// In Lopaka: create/import a 64x128 Flipper-style screen, then paste/import this file.
// The app firmware would later rotate these 64x128 portrait coordinates onto Flipper's 128x64 display.

void draw_vertical_main(Canvas* canvas) {
    canvas_draw_frame(canvas, 0, 0, 64, 128);
    canvas_draw_str(canvas, 18, 10, "DAIKIN");
    canvas_draw_str(canvas, 5, 20, "IR <");
    canvas_draw_str(canvas, 45, 20, "1/4");
    canvas_draw_line(canvas, 0, 24, 63, 24);
    canvas_draw_str(canvas, 25, 37, "COOL");
    canvas_draw_str(canvas, 6, 49, "AUTO");
    canvas_draw_str(canvas, 13, 77, "27");
    canvas_draw_str(canvas, 54, 77, "C");
    canvas_draw_frame(canvas, 3, 91, 28, 13);
    canvas_draw_str(canvas, 8, 101, "PWR");
    canvas_draw_box(canvas, 33, 91, 28, 13);
    canvas_draw_str(canvas, 35, 101, "SEND");
    canvas_draw_frame(canvas, 3, 109, 28, 13);
    canvas_draw_str(canvas, 5, 119, "MODE");
    canvas_draw_frame(canvas, 33, 109, 28, 13);
    canvas_draw_str(canvas, 40, 119, "FAN");
}

void draw_vertical_functions(Canvas* canvas) {
    canvas_draw_frame(canvas, 0, 0, 64, 128);
    canvas_draw_str(canvas, 22, 10, "FUNCS");
    canvas_draw_str(canvas, 5, 20, "IR <");
    canvas_draw_str(canvas, 45, 20, "2/4");
    canvas_draw_line(canvas, 0, 24, 63, 24);
    canvas_draw_box(canvas, 6, 32, 52, 14);
    canvas_draw_str(canvas, 20, 42, "TURBO");
    canvas_draw_frame(canvas, 6, 51, 52, 14);
    canvas_draw_str(canvas, 20, 61, "QUIET");
    canvas_draw_frame(canvas, 6, 70, 52, 14);
    canvas_draw_str(canvas, 20, 80, "SLEEP");
    canvas_draw_frame(canvas, 6, 89, 52, 14);
    canvas_draw_str(canvas, 20, 99, "SWING");
    canvas_draw_frame(canvas, 6, 108, 24, 13);
    canvas_draw_str(canvas, 13, 118, "LED");
    canvas_draw_frame(canvas, 34, 108, 24, 13);
    canvas_draw_str(canvas, 41, 118, "CLK");
}

void draw_vertical_timer(Canvas* canvas) {
    canvas_draw_frame(canvas, 0, 0, 64, 128);
    canvas_draw_str(canvas, 22, 10, "TIMER");
    canvas_draw_str(canvas, 5, 20, "IR <");
    canvas_draw_str(canvas, 45, 20, "3/4");
    canvas_draw_line(canvas, 0, 24, 63, 24);
    canvas_draw_str(canvas, 6, 39, "ON");
    canvas_draw_box(canvas, 24, 30, 35, 14);
    canvas_draw_str(canvas, 29, 40, "22:30");
    canvas_draw_frame(canvas, 6, 50, 25, 13);
    canvas_draw_str(canvas, 13, 60, "SET");
    canvas_draw_frame(canvas, 35, 50, 24, 13);
    canvas_draw_str(canvas, 42, 60, "CLR");
    canvas_draw_str(canvas, 6, 79, "OFF");
    canvas_draw_frame(canvas, 24, 70, 35, 14);
    canvas_draw_str(canvas, 29, 80, "--:--");
    canvas_draw_frame(canvas, 6, 91, 25, 13);
    canvas_draw_str(canvas, 13, 101, "SET");
    canvas_draw_frame(canvas, 35, 91, 24, 13);
    canvas_draw_str(canvas, 42, 101, "CLR");
    canvas_draw_str(canvas, 18, 120, "OK EDIT");
}

void draw_vertical_favourites(Canvas* canvas) {
    canvas_draw_frame(canvas, 0, 0, 64, 128);
    canvas_draw_str(canvas, 25, 10, "FAVS");
    canvas_draw_str(canvas, 5, 20, "IR <");
    canvas_draw_str(canvas, 45, 20, "4/4");
    canvas_draw_line(canvas, 0, 24, 63, 24);
    canvas_draw_box(canvas, 4, 29, 56, 11);
    canvas_draw_str(canvas, 7, 37, "1 COOL 27");
    canvas_draw_str(canvas, 7, 52, "2 HEAT 25");
    canvas_draw_str(canvas, 7, 67, "3 DRY 26");
    canvas_draw_str(canvas, 7, 82, "4 FAN HI");
    canvas_draw_frame(canvas, 6, 98, 52, 13);
    canvas_draw_str(canvas, 22, 108, "SAVE");
    canvas_draw_str(canvas, 17, 122, "OK SEND");
}
