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
} SaveData;

#define SAVE_MAGIC 0x4D415A34u // "MAZ4" (v4.4 复用 reserved 字段, 结构大小不变)

void storage_load(void) {
    SaveData d = {0};
    Storage* st = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(st);
    settings_defaults();
    bool ok = storage_file_open(f, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    if(ok && storage_file_read(f, &d, sizeof(d)) == sizeof(d) && d.magic == SAVE_MAGIC) {
        g.campaign_cleared = d.campaign_cleared;
        g.endless_floor = d.endless_floor;
        g.sfx_enabled = (d.sfx_enabled != 0);
        g.opening_enabled = (d.opening_enabled != 0);
        g.show_debug = (d.show_debug != 0);
        g.dev_mode = (d.dev_mode != 0);
    } else {
        // 旧版 MAZ3 存档: 只读取前 3 项
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
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

void storage_save(void) {
    SaveData d;
    d.magic = SAVE_MAGIC;
    d.campaign_cleared = g.campaign_cleared;
    d.endless_floor = g.endless_floor;
    d.sfx_enabled = g.sfx_enabled ? 1 : 0;
    d.opening_enabled = g.opening_enabled ? 1 : 0;
    d.show_debug = g.show_debug ? 1 : 0;
    d.dev_mode = g.dev_mode ? 1 : 0;
    memset(d.reserved, 0, sizeof(d.reserved));
    Storage* st = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(st);
    if(storage_file_open(f, SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, &d, sizeof(d));
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}
