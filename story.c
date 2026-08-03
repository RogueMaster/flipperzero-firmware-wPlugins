#include "maze3d.h"

// 剧情文本系统 - 英文 ASCII (内置字体不支持中文, 600字中文位图体积过大)
// 中文模式下标题/选项标签用 XBM, 正文用英文(玩家可读)
//
// story_id:
//   0 = PROLOGUE    开场剧情 (3页, 2选项影响起始状态)
//   1 = DESCENT     下陷 (2页, 地面塌陷/前人回响)
//   2 = THE KEY     钥匙的秘密 (2页, 神秘钥匙/自由的低语)
//   3 = THE HUNT    猎杀 (2页, 敌人出现/求生本能)
//   4 = CROSSROADS  十字路口 (2页, 深入 vs 宝藏)
//   5 = ESCAPE      逃脱 (2页, 光线裂缝/迷宫召回)

// 每页用 \n 分行, 每行 <= 21 字符 (FontSecondary 8px, 128宽)
// 渲染器每页最多绘制 5 行 (y=14..46, y>=50 截止), 故每页 <= 5 行

static const char* PROLOGUE_PAGES[] = {
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
#define PROLOGUE_NP 3

static const char* DESCENT_PAGES[] = {
    // page 1: 地面塌陷, 坠入更深
    "The floor gives way.\n"
    "You fall into shadow.\n"
    "Down to older stone.\n"
    "The walls hunger now.\n"
    "Descend. Survive.",

    // page 2: 前人回响
    "Marks on cold stone:\n"
    "Names of the lost.\n"
    "Those who came before\n"
    "None found a way out.\n"
    "Their whispers stay.",
};
#define DESCENT_NP 2

static const char* THEKEY_PAGES[] = {
    // page 1: 发现神秘钥匙
    "A glint in the dust.\n"
    "A key, cold and old.\n"
    "It hums in your hand.\n"
    "Old iron, old magic.\n"
    "What does it unlock?",

    // page 2: 关于自由的低语
    "Runes glow soft, low.\n"
    "They speak of a door.\n"
    "A way out of dark.\n"
    "Freedom, they say.\n"
    "But the Maze lies.",
};
#define THEKEY_NP 2

static const char* THEHUNT_PAGES[] = {
    // page 1: 敌人出现
    "Eyes in the darkness.\n"
    "Something stirs near.\n"
    "It knows you are here\n"
    "Stand firm. Breathe.\n"
    "The hunt has begun.",

    // page 2: 战斗开始, 求生本能
    "Your blade is ready.\n"
    "Your torch burns hot.\n"
    "Strike first or fall.\n"
    "Survive. Move. Endure\n"
    "Only one leaves this.",
};
#define THEHUNT_NP 2

static const char* CROSSROADS_PAGES[] = {
    // page 1: 分岔路口
    "Two paths split here.\n"
    "One goes deeper down.\n"
    "One shines with gold.\n"
    "Gold tempts the bold.\n"
    "Which will you take?",

    // page 2: 一条深入, 一条通向宝藏
    "The left path drops.\n"
    "Down into colder dark\n"
    "Right shines bright.\n"
    "Treasure, or a trap?\n"
    "Both may be your end.",
};
#define CROSSROADS_NP 2

static const char* ESCAPE_PAGES[] = {
    // page 1: 裂缝透光
    "A crack in the stone.\n"
    "Pale light leaks in.\n"
    "The way out is near.\n"
    "Push. Crawl. Bleed.\n"
    "Crawl toward the sky.",

    // page 2: 逃脱, 但迷宫召回
    "You reach the surface\n"
    "Cold air fills you.\n"
    "Free... for now.\n"
    "But the Maze recalls.\n"
    "It whispers your name",
};
#define ESCAPE_NP 2

int story_pages(int story_id) {
    switch(story_id) {
        case 0: return PROLOGUE_NP;
        case 1: return DESCENT_NP;
        case 2: return THEKEY_NP;
        case 3: return THEHUNT_NP;
        case 4: return CROSSROADS_NP;
        case 5: return ESCAPE_NP;
        default: return 0;
    }
}

const char* story_page_text(int story_id, int page) {
    if(page < 0) return "";
    switch(story_id) {
        case 0: return (page < PROLOGUE_NP) ? PROLOGUE_PAGES[page] : "";
        case 1: return (page < DESCENT_NP) ? DESCENT_PAGES[page] : "";
        case 2: return (page < THEKEY_NP) ? THEKEY_PAGES[page] : "";
        case 3: return (page < THEHUNT_NP) ? THEHUNT_PAGES[page] : "";
        case 4: return (page < CROSSROADS_NP) ? CROSSROADS_PAGES[page] : "";
        case 5: return (page < ESCAPE_NP) ? ESCAPE_PAGES[page] : "";
        default: return "";
    }
}

const char* story_choice_a(int story_id) {
    switch(story_id) {
        case 0: return "A) Warrior: +HP, no items";
        case 4: return "A) Descend deeper";
        default: return "A) Continue";
    }
}

const char* story_choice_b(int story_id) {
    switch(story_id) {
        case 0: return "B) Seeker: -HP, +1 torch";
        case 4: return "B) Seek the gold";
        default: return "B) Continue";
    }
}

const char* story_title(int story_id) {
    switch(story_id) {
        case 0: return "PROLOGUE";
        case 1: return "DESCENT";
        case 2: return "THE KEY";
        case 3: return "THE HUNT";
        case 4: return "CROSSROADS";
        case 5: return "ESCAPE";
        default: return "STORY";
    }
}
