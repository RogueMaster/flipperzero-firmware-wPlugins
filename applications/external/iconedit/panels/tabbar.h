#pragma once

// The currently displayed tab
typedef enum {
    Tab_START,
    TabFile = Tab_START,
    TabTools,
    TabSettings,
    TabHelp,
    TabAbout,
    Tab_COUNT,
    Tab_NONE
} Tab;

Tab tabbar_get_selected_tab();
void tabbar_set_selected_tab(Tab tab);
