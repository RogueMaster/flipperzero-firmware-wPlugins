#pragma once

#include "app/AppConfig.hpp"

#include "lib/ui/view/UiView.hpp"
#include "lib/ui/view/SubMenuUiView.hpp"
#include "lib/ui/UiManager.hpp"

#include "app/AppNotifications.hpp"

#include "SelectCategoryScreen.hpp"
#include "ScanStationsScreen.hpp"

class MainMenuScreen {
private:
    AppConfig* config;
    SubMenuUiView* menuView;

public:
    MainMenuScreen(AppConfig* config) {
        this->config = config;

        menuView = new SubMenuUiView("Chief Cooker");
        menuView->AddItem("Scan for station signals", HANDLER_1ARG(&MainMenuScreen::scanStationsMenuPressed));
        menuView->AddItem("Saved stations database", HANDLER_1ARG(&MainMenuScreen::stationDatabasePressed));
        menuView->AddItem("About / Manual", HANDLER_1ARG(&MainMenuScreen::aboutPressed));
        menuView->SetOnDestroyHandler(HANDLER(&MainMenuScreen::destroy));
    }

    UiView* GetView() {
        return menuView;
    }

private:
    void scanStationsMenuPressed(uint32_t) {
        UiManager::GetInstance()->ShowLoading();
        UiManager::GetInstance()->PushView((new ScanStationsScreen(config))->GetView());
    }

    void stationDatabasePressed(uint32_t) {
        SubMenuUiView* savedMenuView = new SubMenuUiView("Select database");
        savedMenuView->AddItem("Saved by you", HANDLER_1ARG(&MainMenuScreen::savedStationsPressed));
        savedMenuView->AddItem("Autosaved", HANDLER_1ARG(&MainMenuScreen::autosavedStationsPressed));
        UiManager::GetInstance()->PushView(savedMenuView);
    }

    void savedStationsPressed(uint32_t) {
        UiManager::GetInstance()->ShowLoading();
        UiManager::GetInstance()->PushView(
            (new SelectCategoryScreen(false, User, HANDLER_2ARG(&MainMenuScreen::categorySelected)))->GetView()
        );
    }

    void autosavedStationsPressed(uint32_t) {
        UiManager::GetInstance()->ShowLoading();
        UiManager::GetInstance()->PushView(
            (new SelectCategoryScreen(false, Autosaved, HANDLER_2ARG(&MainMenuScreen::categorySelected)))->GetView()
        );
    }

    void categorySelected(CategoryType categoryType, const char* category) {
        UiManager::GetInstance()->ShowLoading();
        UiManager::GetInstance()->PushView((new ScanStationsScreen(config, categoryType, category))->GetView());
    }

    void aboutPressed(uint32_t index) {
        UNUSED(index);

        Notification::Play(&NOTIFICATION_PAGER_RECEIVE);
        menuView->SetItemLabel(index, "Developed by Denr01!");
    }

    void destroy() {
        delete this;
    }
};
