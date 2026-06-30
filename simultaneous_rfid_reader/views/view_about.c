#include "view_about.h"

/**
 * @brief      Callback for returning to submenu.
 * @details    This function is called when user press back button.
 * @param      context  The context - unused
 * @return     next view id
*/
uint32_t uhf_reader_navigation_about_submenu_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewSubmenu;
}

/**
 * @brief      Allocates the about screen
 * @details    Allocates the contents of the about screen.
 * @param      app  The UHFReaderApp used to allocate variables 
*/
void view_about_alloc(UHFReaderApp* App) {
    
    //Creating the about widget 
    App->WidgetAbout = widget_alloc();
    FuriString* TmpString = furi_string_alloc();
    widget_add_text_box_element(
        App->WidgetAbout, 0, 0, 128, 14, AlignCenter, AlignBottom, UHF_RFID_BLANK_INV, false);
    widget_add_text_box_element(
        App->WidgetAbout, 0, 0, 128, 14, AlignCenter, AlignBottom, UHF_RFID_NAME, false);
    
    //Adding version and developer information
    furi_string_printf(TmpString, "\e#%s\n", "Information:");
    furi_string_cat_printf(TmpString, "Version: %s\n", UHF_RFID_VERSION_APP);
    furi_string_cat_printf(TmpString, "Developed by: %s\n", UHF_RFID_MEM_DEVELOPER);
    furi_string_cat_printf(TmpString, "Github: %s\n\n", UHF_RFID_GITHUB);
    furi_string_cat_printf(TmpString, "\e#%s\n", "Description:");
    
    //Section with high level overview of app functions 
    furi_string_cat_printf(
        TmpString,
        "UHF RFID Reader\n"
        "Made for use with a M6E, M7E, or YRM100 compatible reader.\n"
        "Can read up to 150 tags simultaneously using M6E or M7E reader!\n"
        "Can read/write, save, lock, kill, and dump data from read tags.\n\n");
   
    //Hardware requirements
    furi_string_cat_printf(TmpString, "\e#%s\n", "Hardware Requirements:");
    furi_string_cat_printf(
        TmpString,
        "Any of these options work, however, changes to the wiring and configuration may be necessary.\n"
        "A M6E, M7E, or YRM100 UHF RFID Reader is required!\n"
        "If using a M6E or M7E reader, the following are required:\n"
        "- ThingMagic Nano Embedded RFID Reader Module (M6E or M7E variant)\n"
        "- SparkFun Simultaneous RFID Reader(M6E or M7E variant)\n"
        "- Raspberry Pi Zero (To connect to M6E/M7E board)\n"
        "- Custom UHF RFID Flipper Zero Board (Coming Soon)\n\n");

    //The configuration screen
    furi_string_cat_printf(TmpString, "\e#%s\n", "Configuration:");
    furi_string_cat_printf(
        TmpString,
        "To operate, select the reader module first, then connect.\n"
        "Next, you may toggle other reader settings.\n"
        " The configuration menu has the following options:\n"
        "- Select reader module\n"
        "- Set the power of the reader(M6E/M7E power range 0-2700, YRM100X 1-27)\n"
        "- Change the Baud Rate\n"
        "- Change the Region\n"
        "- Set default access password for reading/writing\n"
        "- Set write save mode (Turn on to save and update stored fields of tag written)\n"
        "- Save configuration settings (Future)\n"
        "- Set/Detect UHF RFID Tag Type (Future)\n"
        "- Toggle the antenna selection (Future for M6E and M7E Only)\n\n");
        
    
    //Read screen information
    furi_string_cat_printf(TmpString, "\e#%s\n", "Read:");
    furi_string_cat_printf(
        TmpString,
        " The read menu has the following options:\n"
        "- Press Ok to start/stop reading\n"
        "- Press Up to save the selected EPC\n"
        "- Hold Up to pause scrolling the selected EPC\n"
        "- Press Down to see TID, EPC, User, and Reserved Memory\n"
        "- Press Left/Right to cycle through tags read (M6E & M7E Only)\n\n");
    
    //Write screen information
    furi_string_cat_printf(TmpString, "\e#%s\n", "Write:");
    furi_string_cat_printf(
        TmpString,
        " The write menu has the following options:\n"
        "- Press Ok to start/stop writing\n"
        "- Press Left to modify the EPC value\n"
        "- Press Right to modify Reserved Memory (First 4 bytes = kill password, last 4 bytes = access password)\n"
        "- Press Up to modify the User Memory Bank\n"
        "- Press Down to modify the TID (Supported but usually locked by manufacturer)\n\n");
    
    furi_string_cat_printf(TmpString, "\e#%s\n", "Lock:");
    furi_string_cat_printf(
        TmpString,
        " The Lock menu has the following options:\n"
        "- Set access password (Can alternatively be set through the write menu)\n"
        "- Pick the Memory Bank or Password to Lock/Unlock, or Permanently Lock/Unlock (In the Open or Secured State)\n"
        "- Select the Lock Mode\n"
        "- Press Execute to perform the lock action\n"
        "- To perform a lock action, first ensure the AP has been set in the config menu, and then select a lock action\n"
        "- If the password is wrong, an error sequence will beep\n\n");
    furi_string_cat_printf(TmpString, "\e#%s\n", "Kill:");
    furi_string_cat_printf(
        TmpString,
        " The Kill menu has the following options:\n"
        "- Set kill password (Can alternatively be set through the write menu)\n"
        "- Press Kill Tag and confirm the kill password to permanently inactivate the tag!\n"
        "- If the password is wrong, an error sequence will beep\n\n");
    
    //Adding the widget to the view dispatcher 
    widget_add_text_scroll_element(
        App->WidgetAbout, 0, 16, 128, 50, furi_string_get_cstr(TmpString));
    furi_string_free(TmpString);
    view_set_previous_callback(
        widget_get_view(App->WidgetAbout), uhf_reader_navigation_about_submenu_callback);
    view_dispatcher_add_view(
        App->ViewDispatcher, UHFReaderViewAbout, widget_get_view(App->WidgetAbout));
}

/**
 * @brief      Frees the about view
 * @details    Frees all variables associated with the about widget.
 * @param      app  The UHFReaderApp - used to free the widget.
*/
void view_about_free(UHFReaderApp* App) {
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewAbout);
    widget_free(App->WidgetAbout);
}
