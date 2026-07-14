#pragma once
#define TOOL_COUNTER    0
#define TOOL_LIGHTER    1
#define TOOL_WIRETESTER 2
#define TOOL_VOLTMETER  3

#define CAPTION_POSX 125
#define CAPTION_POSY 47

typedef struct {
    int selectedTool;
    int mode; //0-tool, 1-tool select
    bool ringing;
    int lighter_mode;
    bool alarming;
    bool running;
    int counter;
    bool ExtLedOn;
    float pc3Voltage;
    FuriHalAdcHandle* adc_handle;
} App_Global_Data;

int AppInit();
int AppDeinit();
int AppWork();
void Draw(Canvas* c, void* ctx);
int KeyProc(int type, int key);
int ToolDefaultKey(int type, int key);
int ToolLighterKey(int type, int key);
void WiretesterSound(bool state);
void ExtLedToggle(int action);
