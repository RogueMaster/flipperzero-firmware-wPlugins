#include "sender_view.h"

#define BYTES_PER_ROW 4
#define VISIBLE_ROWS  2
#define VISIBLE_BYTES (BYTES_PER_ROW * VISIBLE_ROWS)

void draw_sender_view(Canvas* canvas, i2cSender* i2c_sender) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, 0, 0, 128, 64, 3);

    if(!i2c_sender->scanner->scanned) {
        scan_i2c_bus(i2c_sender->scanner);
    }

    canvas_set_font(canvas, FontSecondary);
    if(i2c_sender->scanner->nb_found <= 0) {
        canvas_draw_str_aligned(canvas, 20, 5, AlignLeft, AlignTop, "No peripherals found");
        return;
    }

    // Send button (label depends on mode)
    canvas_draw_rbox(canvas, 45, 48, 45, 13, 3);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_icon(canvas, 50, 50, &I_Ok_btn_9x9);
    canvas_draw_str_aligned(
        canvas,
        62,
        51,
        AlignLeft,
        AlignTop,
        i2c_sender->mode == SENDER_MODE_READ ? "Read" : "Write");

    char buf[12];
    canvas_set_color(canvas, ColorBlack);

    // Row 1: Addr (original NaejEL coords)
    canvas_draw_str_aligned(canvas, 3, 5, AlignLeft, AlignTop, "Addr: ");
    canvas_draw_icon(canvas, 33, 5, &I_ButtonLeft_4x7);
    canvas_draw_icon(canvas, 68, 5, &I_ButtonRight_4x7);
    snprintf(
        buf, sizeof(buf), "0x%02X", (int)i2c_sender->scanner->addresses[i2c_sender->address_idx]);
    canvas_draw_str_aligned(canvas, 43, 5, AlignLeft, AlignTop, buf);

    // Row 2: Reg (named "Value" in the original, edited with Up/Down)
    canvas_draw_str_aligned(canvas, 3, 15, AlignLeft, AlignTop, "Reg: ");
    canvas_draw_icon(canvas, 33, 17, &I_ButtonUp_7x4);
    canvas_draw_icon(canvas, 68, 17, &I_ButtonDown_7x4);
    snprintf(buf, sizeof(buf), "0x%02X", (int)i2c_sender->value);
    canvas_draw_str_aligned(canvas, 43, 15, AlignLeft, AlignTop, buf);

    // Row 3: Len + display (READ) or Data (WRITE)
    if(i2c_sender->mode == SENDER_MODE_READ) {
        canvas_draw_str_aligned(canvas, 3, 25, AlignLeft, AlignTop, "Len: ");
        snprintf(buf, sizeof(buf), "%u", (unsigned)i2c_sender->read_len);
        canvas_draw_str_aligned(canvas, 43, 25, AlignLeft, AlignTop, buf);
        canvas_draw_str_aligned(
            canvas,
            70,
            25,
            AlignLeft,
            AlignTop,
            i2c_sender->display == DISPLAY_HEX ? "HEX" : "ASC");
    } else {
        canvas_draw_str_aligned(canvas, 3, 25, AlignLeft, AlignTop, "Data: ");
        snprintf(buf, sizeof(buf), "0x%02X", (int)i2c_sender->write_data);
        canvas_draw_str_aligned(canvas, 43, 25, AlignLeft, AlignTop, buf);
        canvas_draw_str_aligned(canvas, 70, 25, AlignLeft, AlignTop, "WR");
    }

    if(i2c_sender->must_send) {
        i2c_send(i2c_sender);
    }

    if(!i2c_sender->sended) return;

    // Result rendering
    if(i2c_sender->error) {
        canvas_draw_str_aligned(canvas, 3, 35, AlignLeft, AlignTop, "Error: no ACK");
        return;
    }

    if(i2c_sender->mode == SENDER_MODE_WRITE) {
        canvas_draw_str_aligned(canvas, 3, 35, AlignLeft, AlignTop, "Written OK");
        return;
    }

    // READ + OK: show recv buffer (hex or ascii), paginated via recv_scroll
    uint8_t start = i2c_sender->recv_scroll;
    if(start >= i2c_sender->read_len) start = 0;
    uint8_t end = start + VISIBLE_BYTES;
    if(end > i2c_sender->read_len) end = i2c_sender->read_len;

    for(uint8_t i = start; i < end; i++) {
        uint8_t pos = i - start;
        uint8_t row = pos / BYTES_PER_ROW;
        uint8_t col = pos % BYTES_PER_ROW;
        uint8_t x = 3 + col * 27;
        uint8_t y = 33 + row * 8;
        if(i2c_sender->display == DISPLAY_HEX) {
            snprintf(buf, sizeof(buf), "0x%02X", (int)i2c_sender->recv[i]);
        } else {
            uint8_t b = i2c_sender->recv[i];
            char c = (b >= 0x20 && b <= 0x7E) ? (char)b : '.';
            snprintf(buf, sizeof(buf), "'%c'", c);
        }
        canvas_draw_str_aligned(canvas, x, y, AlignLeft, AlignTop, buf);
    }

    // Scroll indicators on the right edge
    if(start > 0) {
        canvas_draw_icon(canvas, 118, 33, &I_ButtonUp_7x4);
    }
    if(end < i2c_sender->read_len) {
        canvas_draw_icon(canvas, 118, 42, &I_ButtonDown_7x4);
    }
    // Position indicator: "Sx/N" at y=25 trailing area
    snprintf(buf, sizeof(buf), "%u/%u", (unsigned)end, (unsigned)i2c_sender->read_len);
    canvas_draw_str_aligned(canvas, 100, 25, AlignLeft, AlignTop, buf);
}
