#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "py/obj.h"
#include "py/stream.h"
#include "py/runtime.h"
#include "py/mperrno.h"

#include "mp_flipper_fileio.h"

extern const mp_obj_type_t mp_flipper_binary_fileio_type;
extern const mp_obj_type_t mp_flipper_text_fileio_type;

typedef struct _mp_flipper_fileio_file_descriptor_t {
    mp_obj_base_t base;
    void* handle;
    mp_obj_t name;
    uint8_t access_mode;
    uint8_t open_mode;
} mp_flipper_fileio_file_descriptor_t;

void* mp_flipper_file_new_file_descriptor(void* handle, const char* name, uint8_t access_mode, uint8_t open_mode, bool is_text) {
    mp_flipper_fileio_file_descriptor_t* fd = mp_obj_malloc_with_finaliser(
        mp_flipper_fileio_file_descriptor_t, is_text ? &mp_flipper_text_fileio_type : &mp_flipper_binary_fileio_type);

    fd->handle = handle;
    fd->name = mp_obj_new_str(name, strlen(name));
    fd->access_mode = access_mode;
    fd->open_mode = open_mode;

    return fd;
}

static mp_uint_t mp_flipper_fileio_read(mp_obj_t self, void* buf, mp_uint_t size, int* errcode) {
    mp_flipper_fileio_file_descriptor_t* fd = MP_OBJ_TO_PTR(self);

    if(fd->handle == NULL) {
        *errcode = MP_EIO;

        return MP_STREAM_ERROR;
    }

    return mp_flipper_file_read(fd->handle, buf, size, errcode);
}

static mp_uint_t mp_flipper_fileio_write(mp_obj_t self, const void* buf, mp_uint_t size, int* errcode) {
    mp_flipper_fileio_file_descriptor_t* fd = MP_OBJ_TO_PTR(self);

    if(fd->handle == NULL) {
        *errcode = MP_EIO;

        return MP_STREAM_ERROR;
    }

    return mp_flipper_file_write(fd->handle, buf, size, errcode);
}

static mp_uint_t mp_flipper_fileio_ioctl(mp_obj_t self, mp_uint_t request, uintptr_t arg, int* errcode) {
    mp_flipper_fileio_file_descriptor_t* fd = MP_OBJ_TO_PTR(self);

    if(fd->handle == NULL) {
        return 0;
    }

    if(request == MP_STREAM_SEEK) {
        struct mp_stream_seek_t* seek = (struct mp_stream_seek_t*)(uintptr_t)arg;
        size_t position;
        bool success;

        switch(seek->whence) {
        case MP_SEEK_SET:
            mp_flipper_file_seek(fd->handle, seek->offset);

            break;

        case MP_SEEK_CUR:
            position = mp_flipper_file_tell(fd->handle);

            mp_flipper_file_seek(fd->handle, position + seek->offset);

            break;

        case MP_SEEK_END:
            position = mp_flipper_file_size(fd->handle);

            mp_flipper_file_seek(fd->handle, position + seek->offset);

            break;
        }

        seek->offset = mp_flipper_file_tell(fd->handle);

        return 0;
    }

    if(request == MP_STREAM_FLUSH) {
        if(!mp_flipper_file_sync(fd->handle)) {
            *errcode = MP_EIO;

            return MP_STREAM_ERROR;
        }

        return 0;
    }

    if(request == MP_STREAM_CLOSE) {
        if(!mp_flipper_file_close(fd->handle)) {
            *errcode = MP_EIO;

            fd->handle = NULL;

            return MP_STREAM_ERROR;
        }

        fd->handle = NULL;

        return 0;
    }

    *errcode = MP_EINVAL;

    return MP_STREAM_ERROR;
}

static void fileio_attr(mp_obj_t self_in, qstr attr, mp_obj_t* dest) {
    mp_flipper_fileio_file_descriptor_t* fd = MP_OBJ_TO_PTR(self_in);

    if(dest[0] == MP_OBJ_NULL) {
        if(attr == MP_QSTR_name) {
            dest[0] = fd->name;

            return;
        }

        if(attr == MP_QSTR_readable) {
            dest[0] = (fd->access_mode & MP_FLIPPER_FILE_ACCESS_MODE_READ) ? mp_const_true : mp_const_false;

            return;
        }

        if(attr == MP_QSTR_writable) {
            dest[0] = (fd->access_mode & MP_FLIPPER_FILE_ACCESS_MODE_WRITE) ? mp_const_true : mp_const_false;

            return;
        }

        dest[1] = MP_OBJ_SENTINEL;
    } else {
        return;
    }
}

static const mp_map_elem_t mp_flipper_file_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_stream_read_obj)},
    {MP_ROM_QSTR(MP_QSTR_readline), MP_ROM_PTR(&mp_stream_unbuffered_readline_obj)},
    {MP_ROM_QSTR(MP_QSTR_readlines), MP_ROM_PTR(&mp_stream_unbuffered_readlines_obj)},
    {MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj)},
    {MP_ROM_QSTR(MP_QSTR_flush), MP_ROM_PTR(&mp_stream_flush_obj)},
    {MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&mp_stream_close_obj)},
    {MP_ROM_QSTR(MP_QSTR_seek), MP_ROM_PTR(&mp_stream_seek_obj)},
    {MP_ROM_QSTR(MP_QSTR_tell), MP_ROM_PTR(&mp_stream_tell_obj)},
    {MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&mp_stream_close_obj)},
    {MP_ROM_QSTR(MP_QSTR___enter__), MP_ROM_PTR(&mp_identity_obj)},
    {MP_ROM_QSTR(MP_QSTR___exit__), MP_ROM_PTR(&mp_stream___exit___obj)},
};
static MP_DEFINE_CONST_DICT(mp_flipper_file_locals_dict, mp_flipper_file_locals_dict_table);

static const mp_stream_p_t mp_flipper_binary_fileio_stream_p = {
    .read = mp_flipper_fileio_read,
    .write = mp_flipper_fileio_write,
    .ioctl = mp_flipper_fileio_ioctl,
    .is_text = false,
};

MP_DEFINE_CONST_OBJ_TYPE(
    mp_flipper_binary_fileio_type,
    MP_QSTR_BinaryFileIO,
    MP_TYPE_FLAG_ITER_IS_STREAM,
    protocol,
    &mp_flipper_binary_fileio_stream_p,
    attr,
    fileio_attr,
    locals_dict,
    &mp_flipper_file_locals_dict);

static const mp_stream_p_t mp_flipper_text_fileio_stream_p = {
    .read = mp_flipper_fileio_read,
    .write = mp_flipper_fileio_write,
    .ioctl = mp_flipper_fileio_ioctl,
    .is_text = true,
};

MP_DEFINE_CONST_OBJ_TYPE(
    mp_flipper_text_fileio_type,
    MP_QSTR_TextFileIO,
    MP_TYPE_FLAG_ITER_IS_STREAM,
    protocol,
    &mp_flipper_text_fileio_stream_p,
    attr,
    fileio_attr,
    locals_dict,
    &mp_flipper_file_locals_dict);

mp_obj_t mp_flipper_builtin_open(size_t n_args, const mp_obj_t* args, mp_map_t* kwargs) {
    const char* file_name = mp_obj_str_get_str(args[0]);

    uint8_t access_mode = MP_FLIPPER_FILE_ACCESS_MODE_READ;
    uint8_t open_mode = MP_FLIPPER_FILE_OPEN_MODE_OPEN_EXIST;
    bool is_text = true;

    if(n_args > 1) {
        size_t len;

        const char* mode = mp_obj_str_get_data(args[1], &len);

        for(size_t i = 0; i < len; i++) {
            if(i == 0 && mode[i] == 'r') {
                access_mode = MP_FLIPPER_FILE_ACCESS_MODE_READ;
                open_mode = MP_FLIPPER_FILE_OPEN_MODE_OPEN_EXIST;

                continue;
            }

            if(i == 0 && mode[i] == 'w') {
                access_mode = MP_FLIPPER_FILE_ACCESS_MODE_WRITE;
                open_mode = MP_FLIPPER_FILE_OPEN_MODE_CREATE_ALWAYS;

                continue;
            }

            if(i == 1 && mode[i] == 'b') {
                is_text = false;

                continue;
            }

            if(i == 1 && mode[i] == 't') {
                is_text = true;

                continue;
            }

            if(i >= 1 && mode[i] == '+') {
                access_mode = MP_FLIPPER_FILE_ACCESS_MODE_READ | MP_FLIPPER_FILE_ACCESS_MODE_WRITE;
                open_mode = MP_FLIPPER_FILE_OPEN_MODE_OPEN_APPEND;

                continue;
            }

            mp_raise_OSError(MP_EINVAL);
        }
    }

    void* handle = mp_flipper_file_open(file_name, access_mode, open_mode);
    void* fd = mp_flipper_file_new_file_descriptor(handle, file_name, access_mode, open_mode, is_text);

    if(handle == NULL) {
        mp_raise_OSError(MP_ENOENT);
    }

    return MP_OBJ_FROM_PTR(fd);
}
MP_DEFINE_CONST_FUN_OBJ_KW(mp_flipper_builtin_open_obj, 1, mp_flipper_builtin_open);

static const mp_rom_map_elem_t mp_module_io_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_io)},
    {MP_ROM_QSTR(MP_QSTR_open), MP_ROM_PTR(&mp_flipper_builtin_open_obj)},
    {MP_ROM_QSTR(MP_QSTR_BinaryFileIO), MP_ROM_PTR(&mp_flipper_binary_fileio_type)},
    {MP_ROM_QSTR(MP_QSTR_TextFileIO), MP_ROM_PTR(&mp_flipper_text_fileio_type)},
    {MP_ROM_QSTR(MP_QSTR_SEEK_SET), MP_ROM_INT(MP_SEEK_SET)},
    {MP_ROM_QSTR(MP_QSTR_SEEK_CUR), MP_ROM_INT(MP_SEEK_CUR)},
    {MP_ROM_QSTR(MP_QSTR_SEEK_END), MP_ROM_INT(MP_SEEK_END)},
};

static MP_DEFINE_CONST_DICT(mp_module_io_globals, mp_module_io_globals_table);

const mp_obj_module_t mp_module_io = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t*)&mp_module_io_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_io, mp_module_io);