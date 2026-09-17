// Runs a list of APDUs against a credential through the same entry point the
// firmware's ISO-DEP glue uses, and prints what comes back. A command the engine
// answers with nothing is a command the card is silent on, which on the wire is
// a frame delay timeout rather than a status.
//
// Usage: dfc_probe_apdu <file.dfcb> <hex-apdu> [<hex-apdu> ...]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dfc_credential.h"
#include "dfc_der.h"
#include "dfc_virtual_picc.h"

static size_t from_hex(const char* hex, uint8_t* out, size_t cap) {
    size_t n = 0;
    for(const char* p = hex; *p && n < cap;) {
        if(*p == ' ' || *p == ':') {
            p++;
            continue;
        }
        unsigned byte;
        if(sscanf(p, "%2x", &byte) != 1) return 0;
        out[n++] = (uint8_t)byte;
        p += 2;
    }
    return n;
}

int main(int argc, char** argv) {
    if(argc < 3) {
        fprintf(stderr, "usage: %s <file.dfcb> <hex-apdu> [...]\n", argv[0]);
        return 2;
    }
    FILE* fh = fopen(argv[1], "rb");
    if(!fh) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    static uint8_t raw[1u << 16];
    size_t raw_len = fread(raw, 1, sizeof(raw), fh);
    fclose(fh);

    static DfcCredential credential;
    memset(&credential, 0, sizeof(credential));
    DfcDerStatus st = dfc_der_decode(&credential, raw, raw_len);
    if(st != DfcDerOk) {
        printf("decode: %s\n", dfc_der_status_name(st));
        return 1;
    }
    printf("credential: UID ");
    for(size_t i = 0; i < credential.uid_len; i++) printf("%02X", credential.uid[i]);
    printf(", %u app(s), %u file(s)\n",
           (unsigned)credential.num_apps,
           (unsigned)credential.num_files);

    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&credential);
    DfcVirtualPiccActivation activation;
    if(dfc_virtual_picc_scan_iso14443a(session, &activation) != DfcVirtualPiccStatusOk) {
        printf("scan: failed\n");
        return 1;
    }
    printf("activation: UID ");
    for(size_t i = 0; i < activation.uid_len; i++) printf("%02X", activation.uid[i]);
    printf(", SAK %02X, ATS ", activation.sak);
    for(size_t i = 0; i < activation.ats_len; i++) printf("%02X", activation.ats[i]);
    printf("\n\n");

    for(int a = 2; a < argc; a++) {
        uint8_t cmd[512];
        size_t cmd_len = from_hex(argv[a], cmd, sizeof(cmd));
        if(cmd_len == 0) {
            printf("%-28s bad hex\n", argv[a]);
            continue;
        }
        uint8_t response[512];
        size_t response_len = 0;
        DfcVirtualPiccStatus rc = dfc_virtual_picc_iso_dep_exchange(
            session, cmd, cmd_len, response, sizeof(response), &response_len);
        printf("%-30s -> status %d, %2u octets", argv[a], (int)rc, (unsigned)response_len);
        if(response_len) {
            printf(": ");
            for(size_t i = 0; i < response_len; i++) printf("%02X", response[i]);
        } else {
            printf("  <-- SILENT");
        }
        printf("\n");
    }

    dfc_virtual_picc_session_free(session);
    return 0;
}
