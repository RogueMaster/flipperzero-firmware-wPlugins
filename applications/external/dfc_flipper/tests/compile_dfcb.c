#include <stdio.h>
#include <stdlib.h>

#include "dfc_credential.h"
#include "dfc_der.h"
#include "dfc_text.h"

int main(int argc, char** argv) {
    if(argc != 3) {
        fprintf(stderr, "usage: %s input.dfc output.dfcb\n", argv[0]);
        return 2;
    }

    FILE* input = fopen(argv[1], "rb");
    if(!input) return 1;
    if(fseek(input, 0, SEEK_END) || ftell(input) < 0) return 1;
    size_t input_len = (size_t)ftell(input);
    rewind(input);
    char* text = malloc(input_len + 1);
    if(!text || fread(text, 1, input_len, input) != input_len || fclose(input)) return 1;
    text[input_len] = '\0';

    DfcCredential* credential = dfc_credential_alloc();
    if(!credential) return 1;
    DfcTextError detail = {0};
    DfcTextStatus status = dfc_text_parse(credential, text, input_len, &detail);
    if(status != DfcTextOk) {
        fprintf(stderr, "line %u: %s\n", (unsigned)detail.line, detail.message);
        return 1;
    }

    size_t output_len = 0;
    if(dfc_der_encoded_size(credential, &output_len) != DfcDerOk) return 1;
    uint8_t* binary = malloc(output_len);
    if(!binary || dfc_der_encode(credential, binary, output_len, &output_len) != DfcDerOk)
        return 1;
    FILE* output = fopen(argv[2], "wb");
    if(!output || fwrite(binary, 1, output_len, output) != output_len || fclose(output)) return 1;
    printf(
        "%s: %zu applications, %zu files, %zu bytes\n",
        argv[2],
        credential->num_apps,
        credential->num_files,
        output_len);
    free(binary);
    dfc_credential_free(credential);
    free(text);
    return 0;
}
