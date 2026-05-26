#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <cstring>

#include "lib/ui/view/UiView.hpp"
#include "lib/ui/UiManager.hpp"
#include "lib/hardware/subghz/SubGhzModule.hpp"
#include "lib/hardware/subghz/data/SubGhzReceivedData.hpp"

using namespace std;

#undef LOG_TAG
#define LOG_TAG "FREQ_SCANNER"

static const uint32_t SCAN_FREQUENCIES[] = {
    315000000,
    318000000,
    390000000,
    410000000,
    418000000,
    433075000,
    433420000,
    433920000,
    434420000,
    868350000,
};
static const uint8_t SCAN_FREQUENCIES_COUNT = sizeof(SCAN_FREQUENCIES) / sizeof(SCAN_FREQUENCIES[0]);

static const int8_t RSSI_SENTINEL = -120;
static const int8_t RSSI_BAR_MIN_DBM = -100;
static const int8_t RSSI_BAR_MAX_DBM = -30;
static const uint8_t SCANNER_VISIBLE_ROWS = 5;
static const uint8_t VERIFY_TOP_COUNT = 5;
// 8 ticks @ ~62ms = ~500ms dwell per channel during decoder verify
static const uint8_t VERIFY_DWELL_TICKS = 8;

class FrequencyScannerUiView : public UiView {
private:
    View* view = NULL;
    int8_t rssiDbm[SCAN_FREQUENCIES_COUNT];
    bool pagerSeen[SCAN_FREQUENCIES_COUNT];
    uint8_t scrollOffset = 0;
    bool isExternal = false;
    const char* statusLabel = "Sweep";

public:
    FrequencyScannerUiView() {
        for(uint8_t i = 0; i < SCAN_FREQUENCIES_COUNT; i++) {
            rssiDbm[i] = RSSI_SENTINEL;
            pagerSeen[i] = false;
        }

        view = view_alloc();
        view_set_context(view, this);
        view_set_draw_callback(view, drawCallback);
        view_set_input_callback(view, inputCallback);

        view_allocate_model(view, ViewModelTypeLockFree, sizeof(UiVIewPointerViewModel*));
        with_view_model_cpp(view, UiVIewPointerViewModel*, model, model->uiVIew = this;, false);
    }

    View* GetNativeView() {
        return view;
    }

    void SetIsExternal(bool value) {
        isExternal = value;
    }

    void SetChannelReading(uint8_t channel, int8_t value) {
        if(channel >= SCAN_FREQUENCIES_COUNT) {
            return;
        }
        rssiDbm[channel] = value;
    }

    int8_t GetChannelReading(uint8_t channel) {
        if(channel >= SCAN_FREQUENCIES_COUNT) {
            return RSSI_SENTINEL;
        }
        return rssiDbm[channel];
    }

    void SetPagerSeen(uint8_t channel) {
        if(channel >= SCAN_FREQUENCIES_COUNT) {
            return;
        }
        pagerSeen[channel] = true;
    }

    void SetStatusLabel(const char* label) {
        statusLabel = label;
    }

    void Refresh() {
        view_commit_model(view, true);
    }

    ~FrequencyScannerUiView() {
        if(view != NULL) {
            OnDestory();
            view_free_model(view);
            view_free(view);
            view = NULL;
        }
    }

private:
    void draw(Canvas* canvas) {
        if(!IsOnTop()) {
            return;
        }

        canvas_clear(canvas);
        canvas_set_color(canvas, ColorBlack);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 9, statusLabel);
        canvas_draw_str_aligned(canvas, 127, 9, AlignRight, AlignBottom, isExternal ? "EXT" : "INT");
        canvas_draw_line(canvas, 0, 11, 127, 11);

        uint8_t order[SCAN_FREQUENCIES_COUNT];
        for(uint8_t i = 0; i < SCAN_FREQUENCIES_COUNT; i++) {
            order[i] = i;
        }
        // Stable insertion sort, strongest RSSI first; ties keep ascending frequency order
        for(uint8_t i = 1; i < SCAN_FREQUENCIES_COUNT; i++) {
            uint8_t key = order[i];
            int j = i - 1;
            while(j >= 0 && rssiDbm[order[j]] < rssiDbm[key]) {
                order[j + 1] = order[j];
                j--;
            }
            order[j + 1] = key;
        }

        const int rowHeight = 10;
        const int rowsTop = 14;
        const bool needScrollbar = SCAN_FREQUENCIES_COUNT > SCANNER_VISIBLE_ROWS;
        const int rssiRightEdge = needScrollbar ? 121 : 127;
        char buf[16];

        canvas_set_font(canvas, FontBatteryPercent);
        for(uint8_t r = 0; r < SCANNER_VISIBLE_ROWS && (r + scrollOffset) < SCAN_FREQUENCIES_COUNT; r++) {
            uint8_t ch = order[r + scrollOffset];
            int y = rowsTop + r * rowHeight;
            int textBaseline = y + 7;

            if(pagerSeen[ch]) {
                canvas_draw_str(canvas, 0, textBaseline, "*");
            }

            uint32_t mhzWhole = SCAN_FREQUENCIES[ch] / 1000000;
            uint32_t mhzFrac = (SCAN_FREQUENCIES[ch] / 10000) % 100;
            snprintf(buf, sizeof(buf), "%lu.%02lu", (unsigned long)mhzWhole, (unsigned long)mhzFrac);
            canvas_draw_str(canvas, 5, textBaseline, buf);

            const int barX = 33;
            const int barY = y + 1;
            const int barW = 62;
            const int barH = 6;

            if(rssiDbm[ch] == RSSI_SENTINEL) {
                canvas_draw_str(canvas, barX, textBaseline, "--");
            } else {
                int8_t v = rssiDbm[ch];
                if(v < RSSI_BAR_MIN_DBM) v = RSSI_BAR_MIN_DBM;
                if(v > RSSI_BAR_MAX_DBM) v = RSSI_BAR_MAX_DBM;
                int span = RSSI_BAR_MAX_DBM - RSSI_BAR_MIN_DBM;
                int fill = ((int)(v - RSSI_BAR_MIN_DBM) * barW) / span;

                canvas_draw_frame(canvas, barX, barY, barW, barH);
                if(fill > 0) {
                    canvas_draw_box(canvas, barX, barY, fill, barH);
                }

                snprintf(buf, sizeof(buf), "%d", rssiDbm[ch]);
                canvas_draw_str_aligned(canvas, rssiRightEdge, textBaseline, AlignRight, AlignBottom, buf);
            }
        }

        if(needScrollbar) {
            elements_scrollbar_pos(
                canvas,
                128,
                rowsTop,
                SCANNER_VISIBLE_ROWS * rowHeight,
                scrollOffset,
                SCAN_FREQUENCIES_COUNT - SCANNER_VISIBLE_ROWS + 1
            );
        }
    }

    bool input(InputEvent* event) {
        if(event->type != InputTypePress && event->type != InputTypeRepeat) {
            return false;
        }
        if(event->key == InputKeyUp) {
            if(scrollOffset > 0) {
                scrollOffset--;
                return true;
            }
        } else if(event->key == InputKeyDown) {
            if(scrollOffset + SCANNER_VISIBLE_ROWS < SCAN_FREQUENCIES_COUNT) {
                scrollOffset++;
                return true;
            }
        }
        return false;
    }

    static void drawCallback(Canvas* canvas, void* model) {
        FrequencyScannerUiView* uiView = (FrequencyScannerUiView*)((UiVIewPointerViewModel*)model)->uiVIew;
        uiView->draw(canvas);
    }

    static bool inputCallback(InputEvent* event, void* context) {
        FrequencyScannerUiView* uiView = (FrequencyScannerUiView*)context;
        if(uiView->input(event)) {
            uiView->Refresh();
            return true;
        }
        return false;
    }
};

enum ScannerPhase { PHASE_SWEEP, PHASE_VERIFY };

class FrequencyScannerScreen {
private:
    FrequencyScannerUiView* scannerView;
    SubGhzModule* subghz;
    FuriTimer* timer;
    ScannerPhase phase = PHASE_SWEEP;
    uint8_t sweepChannel = 0;
    uint8_t verifyTop[VERIFY_TOP_COUNT];
    uint8_t verifyIndex = 0;
    uint8_t verifyTicksLeft = 0;

public:
    FrequencyScannerScreen() {
        subghz = new SubGhzModule(SCAN_FREQUENCIES[0]);
        subghz->SetReceiveHandler(HANDLER_1ARG(&FrequencyScannerScreen::onReceive));
        subghz->BeginRssiScan();

        scannerView = new FrequencyScannerUiView();
        scannerView->SetIsExternal(subghz->IsExternal());
        scannerView->SetStatusLabel("Sweep");
        scannerView->SetOnDestroyHandler(HANDLER(&FrequencyScannerScreen::destroy));

        timer = furi_timer_alloc(tickCallback, FuriTimerTypePeriodic, this);
        furi_timer_start(timer, furi_kernel_get_tick_frequency() / 16);
    }

    UiView* GetView() {
        return scannerView;
    }

private:
    static void tickCallback(void* context) {
        FrequencyScannerScreen* self = (FrequencyScannerScreen*)context;
        self->tick();
    }

    void tick() {
        if(phase == PHASE_SWEEP) {
            sweepTick();
        } else {
            verifyTick();
        }

        if(scannerView->IsOnTop()) {
            scannerView->Refresh();
        }
    }

    void sweepTick() {
        float rssi = subghz->ReadRssiAt(SCAN_FREQUENCIES[sweepChannel]);
        scannerView->SetChannelReading(sweepChannel, clampToInt8((int)rssi));

        sweepChannel = (sweepChannel + 1) % SCAN_FREQUENCIES_COUNT;
        if(sweepChannel == 0) {
            enterVerifyPhase();
        }
    }

    void verifyTick() {
        if(verifyTicksLeft > 0) {
            verifyTicksLeft--;
            return;
        }

        verifyIndex++;
        if(verifyIndex >= VERIFY_TOP_COUNT) {
            enterSweepPhase();
        } else {
            tuneToVerifyChannel();
        }
    }

    void enterVerifyPhase() {
        pickTopChannels();
        subghz->EndRssiScan();
        phase = PHASE_VERIFY;
        verifyIndex = 0;
        tuneToVerifyChannel();
        if(verifyIndex == 0) {
            subghz->ReceiveAsync();
        }
    }

    void tuneToVerifyChannel() {
        uint8_t ch = verifyTop[verifyIndex];
        char buf[24];
        uint32_t mhzWhole = SCAN_FREQUENCIES[ch] / 1000000;
        uint32_t mhzFrac = (SCAN_FREQUENCIES[ch] / 10000) % 100;
        snprintf(buf, sizeof(buf), "Verify %lu.%02lu", (unsigned long)mhzWhole, (unsigned long)mhzFrac);
        scannerView->SetStatusLabel(buf);

        subghz->SetReceiveFrequency(SCAN_FREQUENCIES[ch]);
        verifyTicksLeft = VERIFY_DWELL_TICKS;
    }

    void enterSweepPhase() {
        subghz->StopReceive();
        phase = PHASE_SWEEP;
        sweepChannel = 0;
        scannerView->SetStatusLabel("Sweep");
        subghz->BeginRssiScan();
    }

    void pickTopChannels() {
        bool used[SCAN_FREQUENCIES_COUNT];
        for(uint8_t i = 0; i < SCAN_FREQUENCIES_COUNT; i++) {
            used[i] = false;
        }
        for(uint8_t k = 0; k < VERIFY_TOP_COUNT; k++) {
            int8_t bestVal = -127;
            uint8_t bestIdx = 0;
            bool found = false;
            for(uint8_t i = 0; i < SCAN_FREQUENCIES_COUNT; i++) {
                if(used[i]) continue;
                int8_t v = scannerView->GetChannelReading(i);
                if(!found || v > bestVal) {
                    bestVal = v;
                    bestIdx = i;
                    found = true;
                }
            }
            verifyTop[k] = bestIdx;
            used[bestIdx] = true;
        }
    }

    void onReceive(SubGhzReceivedData* data) {
        if(data == NULL) return;
        const char* protocol = data->GetProtocolName();
        if(protocol != NULL &&
           (strcmp(protocol, "Princeton") == 0 || strcmp(protocol, "SMC5326") == 0)) {
            uint32_t freq = data->GetFrequency();
            for(uint8_t i = 0; i < SCAN_FREQUENCIES_COUNT; i++) {
                if(SCAN_FREQUENCIES[i] == freq) {
                    scannerView->SetPagerSeen(i);
                    if(scannerView->IsOnTop()) {
                        scannerView->Refresh();
                    }
                    break;
                }
            }
        }
        delete data;
    }

    static int8_t clampToInt8(int v) {
        if(v < -128) return -128;
        if(v > 127) return 127;
        return (int8_t)v;
    }

    void destroy() {
        if(timer != NULL) {
            furi_timer_stop(timer);
            furi_timer_free(timer);
            timer = NULL;
        }
        if(phase == PHASE_SWEEP) {
            subghz->EndRssiScan();
        }
        delete subghz;
        delete this;
    }
};
