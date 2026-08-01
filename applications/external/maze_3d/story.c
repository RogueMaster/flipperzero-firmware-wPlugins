#include "maze3d.h"

// 剧情文本系统 - 英文 ASCII (内置字体不支持中文, 600字中文位图体积过大)
// 中文模式下标题/选项标签用 XBM, 正文用英文(玩家可读)
//
// story_id:
//   0 = STORY_INTRO    开场剧情 (600字, 2选项影响起始状态)
//   1 = STORY_BETWEEN  关卡间过场
//   2 = STORY_ENDING   通关结局

// 每页用 \n 分行, 每行 <= 21 字符 (FontSecondary 8px, 128宽)

static const char* INTRO_PAGES[] = {
    // page 1
    "You wake in a stone\n"
    "labyrinth, no memory\n"
    "of how you came here.\n"
    "Torchlight flickers on\n"
    "damp walls carved with\n"
    "strange runes. A distant\n"
    "echo: you are not alone.",

    // page 2
    "Legends tell of the Maze\n"
    "That Shifts - a prison\n"
    "that feeds on those who\n"
    "lose their way. Each level\n"
    "descends deeper. Few\n"
    "escape. Fewer remember\n"
    "the way out.",

    // page 3
    "Two paths lie before you.\n"
    "The runes whisper of a\n"
    "choice that will shape\n"
    "your fate. Steel your\n"
    "heart. The walls are\n"
    "listening. Choose well:",
};
#define INTRO_NP 3

static const char* BETWEEN_PAGES[] = {
    "The floor gives way.\n"
    "You fall into a deeper\n"
    "section. The walls here\n"
    "feel older, hungrier.\n"
    "Another exit waits -\n"
    "if you can find it.\n"
    "Descend. Survive.",
};
#define BETWEEN_NP 1

static const char* ENDING_PAGES[] = {
    "Light breaks through\n"
    "stone. You stumble out\n"
    "into open air, the\n"
    "labyrinth's whisper\n"
    "fading behind you.\n"
    "You escaped... for now.\n"
    "The Maze never lets go.",
};
#define ENDING_NP 1

int story_pages(int story_id) {
    switch(story_id) {
        case 0: return INTRO_NP;
        case 1: return BETWEEN_NP;
        case 2: return ENDING_NP;
        default: return 0;
    }
}

const char* story_page_text(int story_id, int page) {
    if(page < 0) return "";
    switch(story_id) {
        case 0: return (page < INTRO_NP) ? INTRO_PAGES[page] : "";
        case 1: return (page < BETWEEN_NP) ? BETWEEN_PAGES[page] : "";
        case 2: return (page < ENDING_NP) ? ENDING_PAGES[page] : "";
        default: return "";
    }
}

const char* story_choice_a(int story_id) {
    switch(story_id) {
        case 0: return "A) Warrior: +HP, no items";
        default: return "A) Continue";
    }
}

const char* story_choice_b(int story_id) {
    switch(story_id) {
        case 0: return "B) Seeker: -HP, +1 torch";
        default: return "B) Continue";
    }
}

const char* story_title(int story_id) {
    switch(story_id) {
        case 0: return "PROLOGUE";
        case 1: return "DESCENT";
        case 2: return "ESCAPE";
        default: return "STORY";
    }
}
