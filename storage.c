#include "maze3d.h"
#include <storage/storage.h>
#include <stdlib.h>

#define SAVE_PATH APP_DATA_PATH("maze3d.sav")

typedef struct {
    uint32_t magic;
    int campaign_cleared;
    int endless_floor;
    // v4.3+: 设置
    uint8_t sfx_enabled;      // 1 on, 0 off
    uint8_t opening_enabled;  // 1 on, 0 off
    // v4.4+: 调试信息 / 开发模式 (复用原 reserved, 旧 MAZ4 存档此处为 0 即关闭, 兼容)
    uint8_t show_debug;       // 1 on, 0 off
    uint8_t dev_mode;         // 1 on, 0 off
    uint8_t reserved[4];
} SaveDataV4;

// v6.0: 扩展存档, 含成就统计
typedef struct {
    uint32_t magic;
    int campaign_cleared;
    int endless_floor;
    uint8_t sfx_enabled;
    uint8_t opening_enabled;
    uint8_t show_debug;
    uint8_t dev_mode;
    // v6.0 成就统计
    uint32_t ach_total_kills;
    uint32_t ach_total_clears;
    uint32_t ach_total_mined;
    uint32_t ach_flags;
    uint8_t reserved[8];
} SaveData;

#define SAVE_MAGIC_V5 0x4D415A35u // "MAZ5" (v6.0 成就系统)
#define SAVE_MAGIC_V4 0x4D415A34u // "MAZ4" (旧版, 向后兼容)

void storage_load(void) {
    SaveData d = {0};
    Storage* st = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(st);
    settings_defaults();
    // 先尝试 v6.0 (MAZ5) 格式
    bool ok = storage_file_open(f, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    if(ok && storage_file_read(f, &d, sizeof(d)) == sizeof(d) && d.magic == SAVE_MAGIC_V5) {
        g.campaign_cleared = d.campaign_cleared;
        g.endless_floor = d.endless_floor;
        g.sfx_enabled = (d.sfx_enabled != 0);
        g.opening_enabled = (d.opening_enabled != 0);
        g.show_debug = (d.show_debug != 0);
        g.dev_mode = (d.dev_mode != 0);
        g.ach_total_kills = d.ach_total_kills;
        g.ach_total_clears = d.ach_total_clears;
        g.ach_total_mined = d.ach_total_mined;
        g.ach_flags = d.ach_flags;
    } else {
        // 回退: 读旧 MAZ4 (无成就字段, 默认 0)
        SaveDataV4 d4 = {0};
        storage_file_close(f);
        if(storage_file_open(f, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING) &&
           storage_file_read(f, &d4, sizeof(d4)) == sizeof(d4) && d4.magic == SAVE_MAGIC_V4) {
            g.campaign_cleared = d4.campaign_cleared;
            g.endless_floor = d4.endless_floor;
            g.sfx_enabled = (d4.sfx_enabled != 0);
            g.opening_enabled = (d4.opening_enabled != 0);
            g.show_debug = (d4.show_debug != 0);
            g.dev_mode = (d4.dev_mode != 0);
            g.ach_total_kills = 0;
            g.ach_total_clears = 0;
            g.ach_total_mined = 0;
            g.ach_flags = 0;
        } else {
            // 更旧 MAZ3
            uint32_t magic_old = 0;
            storage_file_close(f);
            if(storage_file_open(f, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING) &&
               storage_file_read(f, &magic_old, 4) == 4 && magic_old == 0x4D415A33u) {
                int cc = 0, ef = 1;
                storage_file_read(f, &cc, 4);
                storage_file_read(f, &ef, 4);
                g.campaign_cleared = cc;
                g.endless_floor = ef;
            } else {
                g.campaign_cleared = 0;
                g.endless_floor = 1;
            }
        }
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

void storage_save(void) {
    SaveData d;
    d.magic = SAVE_MAGIC_V5;
    d.campaign_cleared = g.campaign_cleared;
    d.endless_floor = g.endless_floor;
    d.sfx_enabled = g.sfx_enabled ? 1 : 0;
    d.opening_enabled = g.opening_enabled ? 1 : 0;
    d.show_debug = g.show_debug ? 1 : 0;
    d.dev_mode = g.dev_mode ? 1 : 0;
    d.ach_total_kills = g.ach_total_kills;
    d.ach_total_clears = g.ach_total_clears;
    d.ach_total_mined = g.ach_total_mined;
    d.ach_flags = g.ach_flags;
    memset(d.reserved, 0, sizeof(d.reserved));
    Storage* st = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(st);
    if(storage_file_open(f, SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, &d, sizeof(d));
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}
