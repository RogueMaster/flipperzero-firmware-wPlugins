#pragma once

#include "app/AppFileSystem.hpp"
#include "lib/ui/UiManager.hpp"
#include "lib/ui/view/SubMenuUiView.hpp"
#include "lib/ui/view/TextInputUiView.hpp"

#define MIN_CAT_NAME_LENGTH 2
#define MAX_CAT_NAME_LENGTH MAX_FILENAME_LENGTH

class SelectCategoryScreen {
private:
    SubMenuUiView* menu;
    CategoryType categoryType;
    forward_list<char*> categories;
    function<void(CategoryType, const char*)> categorySelectedHandler;
    TextInputUiView* nameInput;
    char* categoryAddedName;

public:
    SelectCategoryScreen(
        bool canCreateNew,
        CategoryType categoryType,
        function<void(CategoryType, const char*)> categorySelectedHandler
    ) {
        this->categoryType = categoryType;
        this->categorySelectedHandler = categorySelectedHandler;

        menu = new SubMenuUiView("Select category");
        menu->SetOnDestroyHandler(HANDLER(&SelectCategoryScreen::destory));

        if(canCreateNew) {
            menu->AddItem("+ Create NEW", HANDLER_1ARG(&SelectCategoryScreen::createNew));
        }

        if(categoryType == User) {
            menu->AddItem("<Default/Uncategorized>", [categoryType, categorySelectedHandler](uint32_t) {
                return categorySelectedHandler(categoryType, NULL);
            });
        }

        AppFileSysytem().GetCategories(&categories, categoryType);
        for(char* category : categories) {
            addCategory(category);
        }
    }

    UiView* GetView() {
        return menu;
    }

private:
    void createNew(uint32_t) {
        if(categoryAddedName != NULL) {
            categorySelectedHandler(categoryType, categoryAddedName);
            return;
        }
        if(nameInput == NULL) {
            nameInput = new TextInputUiView("Enter category name", MIN_CAT_NAME_LENGTH, MAX_CAT_NAME_LENGTH);
            nameInput->SetOnDestroyHandler([this]() { this->nameInput = NULL; });
            nameInput->SetResultHandler(HANDLER_1ARG(&SelectCategoryScreen::addAndSelectCategory));
        }
        UiManager::GetInstance()->PushView(nameInput);
    }

    void addCategory(char* name) {
        menu->AddItem(name, [this, name](uint32_t) { return this->categorySelectedHandler(this->categoryType, name); });
    }

    void addAndSelectCategory(char* name) {
        categoryAddedName = name;
        menu->SetItemLabel(0, name);
        UiManager::GetInstance()->PopView(true);
    }

    void destory() {
        while(!categories.empty()) {
            delete[] categories.front();
            categories.pop_front();
        }

        if(nameInput != NULL) {
            delete nameInput;
        }

        delete this;
    }
};
