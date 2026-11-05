#include "dnie_photo.h"

#define TAG        "DniePhoto"
#define COPY_CHUNK 512

typedef struct {
    File* file;
    DnieHash* hash;
    DniePhotoProgress progress;
    void* context;
    uint8_t last_percent;
} FileSink;

static bool file_sink(const uint8_t* data, size_t len, void* context) {
    FileSink* s = context;
    if(s->hash) dnie_hash_update(s->hash, data, len);
    return storage_file_write(s->file, data, len) == len;
}

static void file_progress(size_t done, size_t total, void* context) {
    FileSink* s = context;
    uint8_t pct = total ? (uint8_t)(done * 100 / total) : 0;
    // Solo avisamos a la GUI cada 2 %, para no saturar la cola de eventos
    if(s->progress && (pct >= s->last_percent + 2 || pct == 100)) {
        s->last_percent = pct;
        s->progress(pct, s->context);
    }
}

DnieLdsResult dnie_photo_read_dg2(
    DnieTransport* poller,
    DnieSm* sm,
    DniePhoto* photo,
    DniePhotoProgress progress,
    void* context,
    DnieHash* hash) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(storage, DNIE_DATA_DIR);

    File* file = storage_file_alloc(storage);
    DnieLdsResult r;
    if(!storage_file_open(file, DNIE_DG2_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        memset(&photo->lds, 0, sizeof(photo->lds));
        photo->lds.result = r = DnieLdsErrorSink;
    } else {
        FileSink sink = {
            .file = file,
            .hash = hash,
            .progress = progress,
            .context = context,
            .last_percent = 0};
        r = dnie_lds_read_file(
            poller, sm, DNIE_FID_DG2, file_sink, file_progress, &sink, &photo->dg2_len, &photo->lds);
        storage_file_close(file);
    }
    storage_file_free(file);
    if(r != DnieLdsOk) storage_simply_remove(storage, DNIE_DG2_PATH);
    furi_record_close(RECORD_STORAGE);
    return r;
}

bool dnie_photo_extract(DniePhoto* photo) {
    photo->image_found = photo->j2k_ok = photo->saved = false;
    photo->path = NULL;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* in = storage_file_alloc(storage);
    File* out = storage_file_alloc(storage);
    uint8_t* buf = malloc(DNIE_DG2_HEAD_LEN);

    do {
        if(!storage_file_open(in, DNIE_DG2_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) break;
        size_t total = storage_file_size(in);
        size_t head = storage_file_read(in, buf, DNIE_DG2_HEAD_LEN);

        if(!dnie_dg2_find_image(buf, head, total, &photo->dg2)) {
            FURI_LOG_E(TAG, "No encuentro la imagen en DG2");
            break;
        }
        photo->image_found = true;

        // Cabecera J2K: tomamos lo que tengamos en el buffer desde el inicio de la imagen
        if(photo->dg2.image_type == DnieImageJpeg2000) {
            if(!storage_file_seek(in, photo->dg2.image_offset, true)) break;
            size_t n = storage_file_read(in, buf, DNIE_DG2_HEAD_LEN);
            photo->j2k_ok = dnie_j2k_parse_header(buf, n, &photo->j2k);
        }

        // Copiar la imagen a su propio fichero
        photo->path = photo->dg2.image_type == DnieImageJpeg ? DNIE_PHOTO_JPG_PATH :
                                                               DNIE_PHOTO_J2K_PATH;
        if(!storage_file_seek(in, photo->dg2.image_offset, true)) break;
        if(!storage_file_open(out, photo->path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) break;
        size_t left = photo->dg2.image_len;
        bool ok = true;
        while(left > 0 && ok) {
            size_t want = left > COPY_CHUNK ? COPY_CHUNK : left;
            size_t got = storage_file_read(in, buf, want);
            ok = got == want && storage_file_write(out, buf, got) == got;
            left -= got;
        }
        storage_file_close(out);
        photo->saved = ok;
    } while(false);

    storage_file_close(in);
    storage_file_free(in);
    storage_file_free(out);
    free(buf);
    storage_simply_remove(storage, DNIE_DG2_PATH); // ya no lo necesitamos
    furi_record_close(RECORD_STORAGE);
    return photo->saved;
}

void dnie_photo_wipe(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_remove(storage, DNIE_DG2_PATH);
    storage_simply_remove(storage, DNIE_PHOTO_J2K_PATH);
    storage_simply_remove(storage, DNIE_PHOTO_JPG_PATH);
    furi_record_close(RECORD_STORAGE);
}
