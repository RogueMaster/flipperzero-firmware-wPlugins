#pragma once

#include "../bambu_rfid.h"
#include "../bambu_reader.h"

#include <gui/view.h>
#include <gui/view_dispatcher.h>

class BrUiManager {
public:
    explicit BrUiManager(BrApp& app);
    ~BrUiManager();

    bool init();
    void run();

private:
    enum class Screen : uint8_t {
        MainMenu,
        Scan,
        Browse,
        Detail,
        About
    };
    enum CustomEvent : uint32_t {
        ScanProgress = 1,
        ScanDone = 2
    };

    static constexpr uint8_t StackMax = 6;
    static constexpr uint8_t VisibleRows = 4;

    BrApp& app_;
    ViewDispatcher* dispatcher_;
    View* view_;
    Screen stack_[StackMax];
    uint8_t depth_;

    uint8_t menu_index_;
    uint16_t browse_index_;
    uint16_t detail_offset_;
    uint16_t about_offset_;
    BrSavedEntry saved_[BR_MAX_SAVED];
    uint16_t saved_count_;
    BrTagInfo current_info_;
    char current_source_[BR_PATH_MAX];

    FuriThread* scan_thread_;
    volatile bool scan_cancel_;
    BrScanContext scan_context_;
    BrScanResult scan_result_;
    bool scan_running_;

    bool push(Screen screen);
    bool pop();
    Screen screen() const;
    void enterScreen(Screen screen);
    void refresh();

    void startScan();
    void stopScan();
    void finishScan();
    void reloadSaved();
    bool openSaved(uint16_t index);

    void draw(Canvas* canvas);
    bool input(InputEvent* event);
    bool custom(uint32_t event);
    bool back();

    void drawHeader(Canvas* canvas, const char* title) const;
    void drawMainMenu(Canvas* canvas) const;
    void drawScan(Canvas* canvas) const;
    void drawBrowse(Canvas* canvas) const;
    void drawDetail(Canvas* canvas) const;
    void drawAbout(Canvas* canvas) const;

    uint16_t detailCount() const;
    void detailLine(uint16_t index, char* out, size_t out_size) const;

    static void drawCallback(Canvas* canvas, void* model);
    static bool inputCallback(InputEvent* event, void* context);
    static bool customCallback(void* context, uint32_t event);
    static bool navigationCallback(void* context);
};
