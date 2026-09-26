#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dfc_credential.h"
#include "dfc_der.h"

int main(int argc, char** argv) {
    if(argc != 4 && argc != 5) {
        fprintf(stderr, "usage: %s output.dfcb app-count file-bytes [generation]\n", argv[0]);
        return 2;
    }

    char* end = NULL;
    unsigned long app_count = strtoul(argv[2], &end, 10);
    if(*end || app_count == 0 || app_count > DFC_MAX_APPS) return 2;
    unsigned long file_bytes = strtoul(argv[3], &end, 10);
    if(*end || file_bytes > DFC_FILE_POOL_SIZE || file_bytes > DfcStorage8KByteCount) return 2;
    unsigned long generation = argc == 5 ? strtoul(argv[4], &end, 10) : 1;
    if((argc == 5 && *end) || generation < 1 || generation > 3) return 2;

    DfcCredential* credential = dfc_credential_alloc();
    if(!credential) return 1;
    dfc_credential_init_factory(credential);
    credential->card.generation = (DfcGeneration)generation;
    credential->card.storage = file_bytes > DfcStorage4KByteCount ? DfcStorage8KByteCount :
                                                                    DfcStorage4KByteCount;
    memcpy(credential->uid, (const uint8_t[]){0x04, 0x53, 0x54, 0x52, 0x45, 0x53, 0x53}, 7);

    for(unsigned long i = 0; i < app_count; i++) {
        const uint8_t aid[3] = {(uint8_t)(i + 1), 0, 0};
        if(!dfc_credential_create_application_desfire_order(
               credential, aid, 0x0F, DFC_KEY_TYPE_DES_2K3DES | 1)) {
            fprintf(stderr, "cannot add application %lu\n", i);
            return 1;
        }
    }

    unsigned long remaining = file_bytes;
    for(uint8_t number = 0; remaining; number++) {
        size_t length = remaining > DFC_MAX_FILE_DATA ? DFC_MAX_FILE_DATA : remaining;
        DfcFile* file = dfc_credential_create_file(credential, 0, number);
        if(!file || !dfc_file_resize(credential, file, length)) {
            fprintf(stderr, "cannot add file %u\n", number);
            return 1;
        }
        file->type = 0x00;
        file->comm_settings = DFC_COMM_PLAIN;
        file->access_rights = 0xEEEE;
        file->declared_size = length;
        file->contents_complete = true;
        memset(dfc_file_data(credential, file), (int)number, length);
        remaining -= length;
    }

    size_t size = 0;
    DfcDerStatus status = dfc_der_encoded_size(credential, &size);
    if(status != DfcDerOk) {
        fprintf(stderr, "credential rejected: %s\n", dfc_der_status_name(status));
        return 1;
    }
    uint8_t* data = malloc(size);
    if(!data || dfc_der_encode(credential, data, size, &size) != DfcDerOk) return 1;

    FILE* output = fopen(argv[1], "wb");
    if(!output || fwrite(data, 1, size, output) != size || fclose(output)) return 1;
    printf(
        "%s: %lu apps, %lu file bytes, %zu encoded bytes\n", argv[1], app_count, file_bytes, size);
    free(data);
    dfc_credential_free(credential);
    return 0;
}
