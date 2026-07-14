#ifndef MENU_H_
#define MENU_H_

typedef const void* MenuData;

class Menu {
public:
    void init();
    void draw();
    void update();
    void goMain();

    const MenuData* currentMenu;
    int8_t currentSelection;
    int8_t debounceInput;
    int8_t selectionDelta;

    int8_t numMenuItems();

    void switchMenu(const MenuData* newMenu);

    static void chooseNewSlot();
    static void chooseDifficulty();
    static void loadGame();
    static void loadSelectedSave();
    static void setDifficulty();
    static void viewScores();
    static void toggleSound();
    static void showHelp();

    void printStat(const char* name, uint8_t num, uint8_t count, int x, int y, uint8_t startTime);
};

extern const void* const Menu_GameOver[] PROGMEM;
extern const void* const Menu_YouWin[] PROGMEM;
extern const void* const Menu_FloorComplete[] PROGMEM;

#endif
