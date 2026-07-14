#include "py/reader.h"
#include "py/qstr.h"

#include "mp_flipper_file_reader.h"

static mp_uint_t mp_flipper_file_reader_read_internal(void* data) {
    uint32_t character = mp_flipper_file_reader_read(data);

    return character == MP_FLIPPER_FILE_READER_EOF ? MP_READER_EOF : character;
}

void mp_reader_new_file(mp_reader_t* reader, qstr filename) {
    reader->data = mp_flipper_file_reader_context_alloc(qstr_str(filename));
    reader->readbyte = mp_flipper_file_reader_read_internal;
    reader->close = mp_flipper_file_reader_close;
}
