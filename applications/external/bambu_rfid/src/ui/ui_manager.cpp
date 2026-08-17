#include "ui_manager.h"
#include "../bambu_storage.h"
#include "../bambu_tag.h"

#include <gui/canvas.h>
#include <gui/elements.h>
#include <input/input.h>

#include <stdio.h>
#include <string.h>

namespace {
struct BrViewModel {
    BrUiManager* ui;
};

const char* MainMenuItems[] = {"Scan tag", "Browse scans", "About"};
const char* AboutItems[] = {
    "Bambu RFID " BR_APP_VERSION,
    "Reads Bambu Lab filament RFID tags using UID-derived A and B sector keys.",
    "Each scan saves BIN, JSON, key BIN, NFC, and README files in a UID folder.",
    "The browser also accepts legacy flat .nfc and 1024-byte .bin dumps.",
    "Use Up/Down to scroll and Back to return.",
};

constexpr uint8_t MenuFirstBaseline = 21;
constexpr uint8_t TextFirstBaseline = 22;
constexpr uint8_t TextLineHeight = 10;
constexpr uint16_t ContentWidth = 116;
constexpr size_t WrappedLineBuffer = 64;

void draw_menu_row(Canvas* canvas, uint8_t row, const char* text, bool selected) {
    const uint8_t y = MenuFirstBaseline + row * 11;
    if(selected) {
        canvas_draw_box(canvas, 2, y - 8, 124, 10);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_str(canvas, 6, y, text);
    if(selected) canvas_set_color(canvas, ColorBlack);
}

void draw_scrollbar(Canvas* canvas, uint16_t index, uint16_t total, uint8_t y, uint8_t height) {
    if(total <= 1) return;
    const uint8_t thumb = height > 8 ? 8 : height;
    const uint8_t travel = height - thumb;
    const uint8_t pos = (uint8_t)((uint32_t)travel * index / (total - 1));
    canvas_draw_frame(canvas, 124, y, 3, height);
    canvas_draw_box(canvas, 124, y + pos, 3, thumb);
}

bool next_wrapped_line(
    Canvas* canvas,
    const char* text,
    size_t* offset,
    uint16_t max_width,
    char* out,
    size_t out_size) {
    if(!text || !offset || !out || out_size < 2) return false;

    size_t begin = *offset;
    while(text[begin] == ' ')
        ++begin;
    if(text[begin] == '\0') {
        *offset = begin;
        return false;
    }
    if(text[begin] == '\n') {
        out[0] = '\0';
        *offset = begin + 1;
        return true;
    }

    size_t cursor = begin;
    size_t end = begin;
    size_t last_space = begin;
    bool width_exceeded = false;

    while(text[cursor] != '\0' && text[cursor] != '\n' && cursor - begin < out_size - 1) {
        if(text[cursor] == ' ') last_space = cursor;

        const size_t candidate_len = cursor - begin + 1;
        memcpy(out, text + begin, candidate_len);
        out[candidate_len] = '\0';

        if(canvas_string_width(canvas, out) > max_width) {
            width_exceeded = true;
            if(last_space > begin) {
                end = last_space;
            } else if(cursor > begin) {
                end = cursor;
            } else {
                end = cursor + 1;
            }
            break;
        }

        ++cursor;
        end = cursor;
    }

    if(!width_exceeded && end == begin && text[begin] != '\0') end = begin + 1;

    while(end > begin && text[end - 1] == ' ')
        --end;
    const size_t line_len = end - begin;
    memcpy(out, text + begin, line_len);
    out[line_len] = '\0';

    size_t next = width_exceeded ? (last_space > begin ? last_space + 1 : end) : cursor;
    while(text[next] == ' ')
        ++next;
    if(text[next] == '\n') ++next;
    if(next <= begin) next = begin + 1;
    *offset = next;
    return true;
}

uint8_t draw_wrapped_text(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    uint16_t max_width,
    uint8_t line_height,
    const char* text,
    uint8_t max_lines) {
    size_t offset = 0;
    uint8_t lines = 0;
    char line[WrappedLineBuffer];
    while(lines < max_lines &&
          next_wrapped_line(canvas, text, &offset, max_width, line, sizeof(line))) {
        canvas_draw_str(canvas, x, y + lines * line_height, line);
        ++lines;
    }
    return lines;
}

void format_fixed_2(char* out, size_t out_size, float value, const char* unit) {
    uint32_t scaled = (uint32_t)(value * 100.0f + 0.5f);
    snprintf(
        out,
        out_size,
        "%lu.%02lu %s",
        (unsigned long)(scaled / 100U),
        (unsigned long)(scaled % 100U),
        unit);
}

void format_fixed_1(char* out, size_t out_size, float value, const char* unit) {
    uint32_t scaled = (uint32_t)(value * 10.0f + 0.5f);
    snprintf(
        out,
        out_size,
        "%lu.%01lu %s",
        (unsigned long)(scaled / 10U),
        (unsigned long)(scaled % 10U),
        unit);
}
} // namespace

BrUiManager::BrUiManager(BrApp& app)
    : app_(app)
    , dispatcher_(nullptr)
    , view_(nullptr)
    , stack_{}
    , depth_(0)
    , menu_index_(0)
    , browse_index_(0)
    , detail_offset_(0)
    , about_offset_(0)
    , saved_{}
    , saved_count_(0)
    , current_info_{}
    , current_source_{}
    , scan_thread_(nullptr)
    , scan_cancel_(false)
    , scan_context_{}
    , scan_result_{}
    , scan_running_(false) {
}

BrUiManager::~BrUiManager() {
    stopScan();
    if(dispatcher_ && view_) view_dispatcher_remove_view(dispatcher_, 0);
    if(view_) view_free(view_);
    if(dispatcher_) view_dispatcher_free(dispatcher_);
}

bool BrUiManager::init() {
    dispatcher_ = view_dispatcher_alloc();
    view_ = view_alloc();
    if(!dispatcher_ || !view_) return false;

    view_dispatcher_set_event_callback_context(dispatcher_, this);
    view_dispatcher_set_custom_event_callback(dispatcher_, customCallback);
    view_dispatcher_set_navigation_event_callback(dispatcher_, navigationCallback);

    view_allocate_model(view_, ViewModelTypeLocking, sizeof(BrViewModel));
    view_set_context(view_, this);
    view_set_draw_callback(view_, drawCallback);
    view_set_input_callback(view_, inputCallback);
    auto* model = static_cast<BrViewModel*>(view_get_model(view_));
    model->ui = this;
    view_commit_model(view_, true);

    view_dispatcher_add_view(dispatcher_, 0, view_);
    view_dispatcher_attach_to_gui(dispatcher_, app_.gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(dispatcher_, 0);

    // Treat the main menu exactly like every other screen: it is the first
    // screen pushed onto the UI stack. Popping it leaves the stack empty and
    // terminates the dispatcher/app.
    return push(Screen::MainMenu);
}

void BrUiManager::run() {
    view_dispatcher_run(dispatcher_);
}

BrUiManager::Screen BrUiManager::screen() const {
    return depth_ ? stack_[depth_ - 1] : Screen::MainMenu;
}

bool BrUiManager::push(Screen next) {
    if(depth_ >= StackMax) return false;
    stack_[depth_++] = next;
    enterScreen(next);
    refresh();
    return true;
}

bool BrUiManager::pop() {
    if(depth_ == 0) return false;
    if(screen() == Screen::Scan) stopScan();

    --depth_;
    if(depth_ == 0) {
        view_dispatcher_stop(dispatcher_);
        return true;
    }

    refresh();
    return true;
}

void BrUiManager::enterScreen(Screen next) {
    if(next == Screen::Scan) {
        startScan();
    } else if(next == Screen::Browse) {
        reloadSaved();
    } else if(next == Screen::Detail) {
        detail_offset_ = 0;
    } else if(next == Screen::About) {
        about_offset_ = 0;
    }
}

void BrUiManager::refresh() {
    if(!view_) return;
    auto* model = static_cast<BrViewModel*>(view_get_model(view_));
    model->ui = this;
    view_commit_model(view_, true);
}

void BrUiManager::startScan() {
    stopScan();
    memset(&scan_result_, 0, sizeof(scan_result_));
    scan_cancel_ = false;
    scan_context_.storage = app_.storage;
    scan_context_.dispatcher = dispatcher_;
    scan_context_.cancel = &scan_cancel_;
    scan_context_.done_event = ScanDone;
    scan_context_.progress_event = ScanProgress;
    scan_context_.result = &scan_result_;

    scan_thread_ = furi_thread_alloc_ex("BambuScan", 4096, br_scan_worker, &scan_context_);
    if(!scan_thread_) {
        snprintf(
            scan_result_.message, sizeof(scan_result_.message), "Could not start scan thread");
        return;
    }
    scan_running_ = true;
    furi_thread_start(scan_thread_);
}

void BrUiManager::stopScan() {
    if(!scan_thread_) return;
    scan_cancel_ = true;
    furi_thread_join(scan_thread_);
    furi_thread_free(scan_thread_);
    scan_thread_ = nullptr;
    scan_running_ = false;
}

void BrUiManager::finishScan() {
    if(scan_thread_) {
        furi_thread_join(scan_thread_);
        furi_thread_free(scan_thread_);
        scan_thread_ = nullptr;
    }
    scan_running_ = false;
    if(scan_result_.ok) {
        current_info_ = scan_result_.info;
        snprintf(
            current_source_,
            sizeof(current_source_),
            "%s/hf-mf-%s.nfc",
            current_info_.uid_hex,
            current_info_.uid_hex);
    }
}

void BrUiManager::reloadSaved() {
    saved_count_ = br_saved_scan(app_.storage, saved_, BR_MAX_SAVED);
    if(saved_count_ == 0)
        browse_index_ = 0;
    else if(browse_index_ >= saved_count_)
        browse_index_ = saved_count_ - 1;
}

bool BrUiManager::openSaved(uint16_t index) {
    if(index >= saved_count_) return false;
    current_info_ = saved_[index].info;
    snprintf(current_source_, sizeof(current_source_), "%s", saved_[index].filename);
    return push(Screen::Detail);
}

void BrUiManager::drawHeader(Canvas* canvas, const char* title) const {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 9, title);
    canvas_draw_line(canvas, 0, 11, 127, 11);
    canvas_set_font(canvas, FontSecondary);
}

void BrUiManager::drawMainMenu(Canvas* canvas) const {
    drawHeader(canvas, BR_APP_NAME);
    for(size_t i = 0; i < COUNT_OF(MainMenuItems); ++i) {
        draw_menu_row(
            canvas,
            static_cast<uint8_t>(i),
            MainMenuItems[i],
            i == static_cast<size_t>(menu_index_));
    }
}

void BrUiManager::drawScan(Canvas* canvas) const {
    drawHeader(canvas, "Scan Bambu tag");

    if(scan_running_) {
        canvas_draw_circle(canvas, 18, 34, 11);
        canvas_draw_circle(canvas, 18, 34, 5);
        canvas_draw_line(canvas, 18, 23, 18, 16);
        canvas_draw_line(canvas, 13, 18, 18, 16);
        canvas_draw_line(canvas, 23, 18, 18, 16);

        char status[128];
        snprintf(
            status,
            sizeof(status),
            "%s\nKeep the tag steady",
            scan_result_.message[0] ? scan_result_.message : "Starting scan...");
        draw_wrapped_text(canvas, 39, 23, 85, TextLineHeight, status, 3);
        return;
    }

    if(scan_result_.ok) {
        char status[192];
        snprintf(
            status,
            sizeof(status),
            "%s\n%s\n%s  %s",
            scan_result_.message[0] ? scan_result_.message : "Tag read successfully",
            scan_result_.info.detailed_filament_type,
            scan_result_.info.color_hex,
            scan_result_.info.variant_id);
        draw_wrapped_text(canvas, 4, TextFirstBaseline, 120, TextLineHeight, status, 3);
        elements_button_center(canvas, "Details");
    } else {
        draw_wrapped_text(
            canvas,
            4,
            TextFirstBaseline,
            120,
            TextLineHeight,
            scan_result_.message[0] ? scan_result_.message : "Scan stopped",
            3);
        elements_button_center(canvas, "Retry");
    }
}

void BrUiManager::drawBrowse(Canvas* canvas) const {
    drawHeader(canvas, "Saved Bambu tags");
    if(saved_count_ == 0) {
        canvas_draw_str(canvas, 8, 32, "No saved scans yet.");
        canvas_draw_str(canvas, 8, 45, "Scan a spool tag first.");
        return;
    }

    uint16_t first = 0;
    if(browse_index_ >= VisibleRows) first = browse_index_ - VisibleRows + 1;
    if(first + VisibleRows > saved_count_)
        first = saved_count_ > VisibleRows ? saved_count_ - VisibleRows : 0;
    for(uint8_t row = 0; row < VisibleRows && first + row < saved_count_; ++row) {
        draw_menu_row(canvas, row, saved_[first + row].display_name, first + row == browse_index_);
    }
    draw_scrollbar(canvas, browse_index_, saved_count_, 17, 43);
}

uint16_t BrUiManager::detailCount() const {
    return BR_DETAIL_ROWS;
}

void BrUiManager::detailLine(uint16_t index, char* out, size_t out_size) const {
    const BrTagInfo& t = current_info_;
    switch(index) {
    case 0:
        snprintf(out, out_size, "UID: %s", t.uid_hex);
        break;
    case 1:
        snprintf(out, out_size, "Type: %s", t.filament_type);
        break;
    case 2:
        snprintf(out, out_size, "Detail: %s", t.detailed_filament_type);
        break;
    case 3:
        snprintf(out, out_size, "Material: %s", t.material_id);
        break;
    case 4:
        snprintf(out, out_size, "Variant: %s", t.variant_id);
        break;
    case 5:
        snprintf(
            out,
            out_size,
            "Color: %s%s%s",
            t.color_hex,
            t.color_count == 2 ? " / " : "",
            t.color_count == 2 ? t.second_color_hex : "");
        break;
    case 6:
        snprintf(out, out_size, "Weight: %u g", t.spool_weight_g);
        break;
    case 7:
        snprintf(out, out_size, "Diameter: ");
        format_fixed_2(out + strlen(out), out_size - strlen(out), t.filament_diameter_mm, "mm");
        break;
    case 8:
        snprintf(out, out_size, "Length: %u m", t.filament_length_m);
        break;
    case 9:
        snprintf(out, out_size, "Spool width: ");
        format_fixed_2(out + strlen(out), out_size - strlen(out), t.spool_width_mm, "mm");
        break;
    case 10:
        snprintf(out, out_size, "Hotend: %u-%u C", t.hotend_min_c, t.hotend_max_c);
        break;
    case 11:
        snprintf(out, out_size, "Bed: %u C (type %u)", t.bed_temp_c, t.bed_temp_type);
        break;
    case 12:
        snprintf(out, out_size, "Dry: %u C / %u h", t.drying_temp_c, t.drying_time_h);
        break;
    case 13:
        snprintf(out, out_size, "Nozzle: ");
        format_fixed_1(out + strlen(out), out_size - strlen(out), t.nozzle_diameter_mm, "mm");
        break;
    case 14:
        snprintf(out, out_size, "Made: %s", t.production_date);
        break;
    case 15:
        snprintf(out, out_size, "Short date: %s", t.short_date);
        break;
    case 16:
        snprintf(out, out_size, "Tray UID: %s", t.tray_uid_hex);
        break;
    case 17:
        snprintf(out, out_size, "XCam: %s", t.xcam_hex);
        break;
    case 18:
        snprintf(
            out, out_size, "Block17: %s  %s", t.block17_hex, t.complete ? "complete" : "partial");
        break;
    default:
        out[0] = '\0';
        break;
    }
}

void BrUiManager::drawDetail(Canvas* canvas) const {
    drawHeader(canvas, "Tag details");

    char text[96];
    uint16_t item = detail_offset_;
    uint8_t y = TextFirstBaseline;
    while(item < detailCount() && y <= 62) {
        detailLine(item, text, sizeof(text));
        const uint8_t available_lines = static_cast<uint8_t>((62 - y) / TextLineHeight + 1);
        const uint8_t used =
            draw_wrapped_text(canvas, 4, y, ContentWidth, TextLineHeight, text, available_lines);
        if(used == 0) break;
        y = static_cast<uint8_t>(y + used * TextLineHeight);
        ++item;
    }

    draw_scrollbar(canvas, detail_offset_, detailCount(), 14, 47);
}

void BrUiManager::drawAbout(Canvas* canvas) const {
    drawHeader(canvas, "About");

    uint16_t item = about_offset_;
    uint8_t y = TextFirstBaseline;
    while(item < COUNT_OF(AboutItems) && y <= 62) {
        const uint8_t available_lines = static_cast<uint8_t>((62 - y) / TextLineHeight + 1);
        const uint8_t used = draw_wrapped_text(
            canvas, 4, y, ContentWidth, TextLineHeight, AboutItems[item], available_lines);
        if(used == 0) break;
        y = static_cast<uint8_t>(y + used * TextLineHeight);
        ++item;
    }

    draw_scrollbar(canvas, about_offset_, COUNT_OF(AboutItems), 14, 47);
}

void BrUiManager::draw(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    switch(screen()) {
    case Screen::MainMenu:
        drawMainMenu(canvas);
        break;
    case Screen::Scan:
        drawScan(canvas);
        break;
    case Screen::Browse:
        drawBrowse(canvas);
        break;
    case Screen::Detail:
        drawDetail(canvas);
        break;
    case Screen::About:
        drawAbout(canvas);
        break;
    }
}

bool BrUiManager::input(InputEvent* event) {
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(event->key == InputKeyBack && event->type == InputTypeShort) return back();

    switch(screen()) {
    case Screen::MainMenu:
        if(event->key == InputKeyUp && menu_index_ > 0)
            --menu_index_;
        else if(
            event->key == InputKeyDown &&
            static_cast<size_t>(menu_index_) + 1U < COUNT_OF(MainMenuItems))
            ++menu_index_;
        else if(event->key == InputKeyOk && event->type == InputTypeShort) {
            if(menu_index_ == 0)
                push(Screen::Scan);
            else if(menu_index_ == 1)
                push(Screen::Browse);
            else
                push(Screen::About);
        }
        break;
    case Screen::Scan:
        if(event->key == InputKeyOk && event->type == InputTypeShort && !scan_running_) {
            if(scan_result_.ok)
                push(Screen::Detail);
            else
                startScan();
        }
        break;
    case Screen::Browse:
        if(saved_count_ && event->key == InputKeyUp && browse_index_ > 0)
            --browse_index_;
        else if(saved_count_ && event->key == InputKeyDown && browse_index_ + 1 < saved_count_)
            ++browse_index_;
        else if(saved_count_ && event->key == InputKeyOk && event->type == InputTypeShort)
            openSaved(browse_index_);
        break;
    case Screen::Detail:
        if(event->key == InputKeyUp && detail_offset_ > 0)
            --detail_offset_;
        else if(event->key == InputKeyDown && detail_offset_ + 1 < detailCount())
            ++detail_offset_;
        break;
    case Screen::About:
        if(event->key == InputKeyUp && about_offset_ > 0)
            --about_offset_;
        else if(
            event->key == InputKeyDown &&
            static_cast<size_t>(about_offset_) + 1U < COUNT_OF(AboutItems))
            ++about_offset_;
        break;
    }
    refresh();
    return true;
}

bool BrUiManager::custom(uint32_t event) {
    if(event == ScanProgress) {
        refresh();
        return true;
    }
    if(event == ScanDone) {
        finishScan();
        refresh();
        return true;
    }
    return false;
}

bool BrUiManager::back() {
    return pop();
}

void BrUiManager::drawCallback(Canvas* canvas, void* model) {
    BrViewModel* m = static_cast<BrViewModel*>(model);
    if(m && m->ui) m->ui->draw(canvas);
}

bool BrUiManager::inputCallback(InputEvent* event, void* context) {
    BrUiManager* ui = static_cast<BrUiManager*>(context);
    return ui ? ui->input(event) : false;
}

bool BrUiManager::customCallback(void* context, uint32_t event) {
    BrUiManager* ui = static_cast<BrUiManager*>(context);
    return ui ? ui->custom(event) : false;
}

bool BrUiManager::navigationCallback(void* context) {
    BrUiManager* ui = static_cast<BrUiManager*>(context);
    return ui ? ui->back() : false;
}
