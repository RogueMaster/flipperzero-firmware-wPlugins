#include "bambu_rfid.h"
#include "bambu_storage.h"
#include "ui/ui_manager.h"

#include <new>

extern "C" int32_t bambu_rfid_app(void* p) {
    UNUSED(p);

    BrApp app{};
    app.storage = static_cast<Storage*>(furi_record_open(RECORD_STORAGE));
    app.gui = static_cast<Gui*>(furi_record_open(RECORD_GUI));
    br_storage_init(app.storage);

    BrUiManager* ui = new(std::nothrow) BrUiManager(app);
    int32_t result = 0;
    if(!ui || !ui->init()) {
        result = -1;
    } else {
        ui->run();
    }

    delete ui;
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    return result;
}
