#include "mjs_fal_imports.h"
#include "mjs_fal.h"
#include <loader/firmware_api/firmware_api.h>

/* GNU hashes of the 53 SDK exports forwarded into the engine. Pure value
 * helpers require no FAL. Sorted for binary search; validated against the SDK
 * and the typed forwarding list by test_mjs_fal.py. */
static const uint32_t mjs_fal_import_hashes[] = {
    0x00cba46bu, /* mjs_prepend_errorf */
    0x028d40d4u, /* mjs_sprintf */
    0x02f86a97u, /* mjs_set_exec_flags_poller */
    0x0688626au, /* mjs_call */
    0x0689dc13u, /* mjs_exec */
    0x0689dca8u, /* mjs_exit */
    0x068e7d2du, /* mjs_next */
    0x07eb4327u, /* mjs_get_cstring */
    0x0b2dd07du, /* mjs_array_buf_get_ptr */
    0x19770b31u, /* mjs_get_stack_trace */
    0x1ce68f24u, /* mjs_mk_array */
    0x2d2b6522u, /* mjs_get_offset_by_call_frame_num */
    0x357854afu, /* mjs_struct_to_obj */
    0x3e574980u, /* mjs_mk_array_buf */
    0x5007b55au, /* mjs_mk_foreign_func */
    0x5a8ccb27u, /* mjs_disasm_all */
    0x5b64a712u, /* mjs_get_lineno_by_offset */
    0x665323b8u, /* mjs_destroy */
    0x68fc631eu, /* mjs_dataview_get_buf */
    0x6aac992fu, /* mjs_mk_foreign */
    0x70b6c701u, /* mjs_array_del */
    0x70b6d3ccu, /* mjs_array_get */
    0x70b706d8u, /* mjs_array_set */
    0x76294ed9u, /* mjs_is_truthy */
    0x8796810cu, /* mjs_array_push */
    0x87d816f1u, /* mjs_strerror */
    0x8b7a0625u, /* mjs_to_boolean_v */
    0x8f8e5136u, /* mjs_get_v_proto */
    0xa2f07b5fu, /* mjs_set_ffi_resolver */
    0xbcc216aeu, /* mjs_array_length */
    0xc4480b87u, /* mjs_to_string */
    0xc9e513e8u, /* mjs_arg */
    0xc9e51f03u, /* mjs_del */
    0xc9e52bceu, /* mjs_get */
    0xc9e55022u, /* mjs_own */
    0xc9e55edau, /* mjs_set */
    0xcb00debeu, /* mjs_get_global */
    0xcb5a4f62u, /* mjs_create */
    0xcd1484c2u, /* mjs_disown */
    0xd778c9d4u, /* mjs_apply */
    0xd7df6403u, /* mjs_get_v */
    0xd85bd689u, /* mjs_nargs */
    0xd8b88a0fu, /* mjs_set_v */
    0xd93acffcu, /* mjs_mk_object */
    0xe3d9a0fcu, /* mjs_mk_string */
    0xe6563372u, /* mjs_exec_file */
    0xe72deaa2u, /* mjs_ffi_resolve */
    0xe7914ee4u, /* mjs_get_string */
    0xed7500ceu, /* mjs_return */
    0xf0d83307u, /* mjs_strcmp */
    0xf65738c5u, /* mjs_get_this */
    0xf743cca9u, /* mjs_set_errorf */
    0xfe2b16b2u, /* mjs_get_context */
};

static bool mjs_fal_is_import(uint32_t hash) {
    size_t begin = 0;
    size_t end = sizeof(mjs_fal_import_hashes) / sizeof(mjs_fal_import_hashes[0]);
    while(begin < end) {
        size_t mid = begin + (end - begin) / 2;
        uint32_t found = mjs_fal_import_hashes[mid];
        if(hash == found) return true;
        if(hash < found)
            end = mid;
        else
            begin = mid + 1;
    }
    return false;
}

static bool
    mjs_fal_imports_resolve(const ElfApiInterface* api, uint32_t hash, Elf32_Addr* address) {
    MjsFalImports* imports = (MjsFalImports*)api;
    if(!imports->delegate->resolver_callback(imports->delegate, hash, address)) return false;
    if(!imports->acquired && mjs_fal_is_import(hash)) {
        Elf32_Addr resident_address;
        /* A private resolver may intentionally override an mJS symbol. Only
         * pin our engine when the resolved address is our resident bridge. */
        if(firmware_api_interface->resolver_callback(
               firmware_api_interface, hash, &resident_address) &&
           *address == resident_address) {
            if(imports->failed || !mjs_fal_acquire()) {
                imports->failed = true;
                return false;
            }
            imports->acquired = true;
        }
    }
    return true;
}

void mjs_fal_imports_init(MjsFalImports* imports, const ElfApiInterface* delegate) {
    imports->api = *delegate;
    imports->api.resolver_callback = mjs_fal_imports_resolve;
    imports->delegate = delegate;
    imports->acquired = false;
    imports->failed = false;
}

void mjs_fal_imports_release(MjsFalImports* imports) {
    if(imports->acquired) {
        imports->acquired = false;
        mjs_fal_release();
    }
}
