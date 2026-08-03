#include "maze3d.h"

// 8x8 单色贴图, 每行1字节(LSB=最左像素), 共8字节/贴图
// 1=实心(亮), 0=空(暗)
// 索引: 0=砖墙 1=石墙 2=金属 3=藤蔓
// v5.0: 重新绘制四张贴图, 结构更清晰、辨识度更高
const uint8_t TEXTURES[TEX_COUNT][8] = {
    // 砖墙: 跑步砌法(错半砖), 横向灰浆床 + 错位竖缝
    {0xFF,0x81,0x7B,0x7B,0xFF,0x81,0xDE,0xDE},
    // 石墙: 圆润卵石, 暗缝分隔
    {0x3C,0x7E,0xDB,0xBD,0x66,0xBD,0x7E,0x3C},
    // 金属墙: 嵌板 + 四角铆钉 + 中部螺栓
    {0x81,0x42,0x3C,0xA5,0xA5,0x3C,0x42,0x81},
    // 藤蔓墙: 有机叶须缠绕
    {0x66,0xA5,0x99,0x5A,0xDB,0x42,0xBD,0xDB},
};

// 取贴图像素 (1=亮 0=暗)
uint8_t texture_sample(int tex_id, int tx, int ty) {
    if(tex_id < 0 || tex_id >= TEX_COUNT) return 1;
    tx &= 7;
    ty &= 7;
    return (TEXTURES[tex_id][ty] >> (tx & 7)) & 1;
}
