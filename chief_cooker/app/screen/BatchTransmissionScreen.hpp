#pragma once

#include "lib/String.hpp"
#include "lib/ui/view/UiView.hpp"
#include "lib/ui/view/ProgressbarPopupUiView.hpp"

class BatchTransmissionScreen {
private:
    ProgressbarPopupUiView* popup;
    String statusStr;

public:
    BatchTransmissionScreen(int pagersTotal) {
        popup = new ProgressbarPopupUiView("Transmitting...");
        SetProgress(0, pagersTotal);
        popup->SetOnDestroyHandler(HANDLER(&BatchTransmissionScreen::destroy));
    }

    void SetProgress(int pagerNum, int pagersTotal) {
        float progressValue = (float)pagerNum / pagersTotal;
        popup->SetProgress(statusStr.format("Pager %d / %d", pagerNum, pagersTotal), progressValue);
    }

private:
    void destroy() {
        delete this;
    }

public:
    UiView* GetView() {
        return popup;
    }
};
