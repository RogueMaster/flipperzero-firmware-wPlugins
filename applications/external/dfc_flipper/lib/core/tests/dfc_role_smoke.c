// Links one build role. Every object of the core goes into the link, so an
// object that calls into a part the role omits fails here rather than on a
// device. The references below keep the role's own entry points in the image.
#include "dfc_build_config.h"
#include "dfc_credential.h"
#include "dfc_der.h"
#include "dfc_text.h"
#if DFC_ENABLE_EMULATOR
#include "dfc_virtual_picc.h"
#endif
#if DFC_ENABLE_READER
#include "dfc_reader.h"
#endif

typedef void (*DfcRoleEntry)(void);

static const DfcRoleEntry entries[] = {
    (DfcRoleEntry)dfc_credential_alloc,
#if DFC_ENABLE_DER_DECODER
    (DfcRoleEntry)dfc_der_decode,
#endif
#if DFC_ENABLE_DER_ENCODER
    (DfcRoleEntry)dfc_der_encode,
#endif
#if DFC_ENABLE_TEXT_CODEC
    (DfcRoleEntry)dfc_text_parse,
    (DfcRoleEntry)dfc_text_write,
#endif
#if DFC_ENABLE_EMULATOR
    (DfcRoleEntry)dfc_virtual_picc_session_alloc,
    (DfcRoleEntry)dfc_virtual_picc_iso_dep_exchange,
#endif
#if DFC_ENABLE_READER
    (DfcRoleEntry)dfc_command_encode_raw,
    (DfcRoleEntry)dfc_reader_step,
#endif
};

int main(void) {
    int present = 0;
    for(size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
        present += entries[i] != 0;
    }
    return present == (int)(sizeof(entries) / sizeof(entries[0])) ? 0 : 1;
}
