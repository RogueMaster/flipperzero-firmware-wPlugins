#pragma once

#include "lib/ui/UiManager.hpp"
#include "lib/hardware/notification/Notification.hpp"
#include "lib/hardware/subghz/FrequencyManager.hpp"

#include "AppConfig.hpp"
#include "app/screen/MainMenuScreen.hpp"

using namespace std;

class App {
public:
    void Run() {
        UiManager* ui = UiManager::GetInstance();
        ui->InitGui();

        FrequencyManager* frequencyManager = FrequencyManager::GetInstance();
        AppConfig* config = new AppConfig();
        config->Load();

        MainMenuScreen* mainMenuScreen = new MainMenuScreen(config);
        ui->PushView(mainMenuScreen->GetView());
        ui->RunEventLoop();

        delete frequencyManager;
        delete mainMenuScreen;
        delete config;
        delete ui;

        Notification::Dispose();
    }
};
