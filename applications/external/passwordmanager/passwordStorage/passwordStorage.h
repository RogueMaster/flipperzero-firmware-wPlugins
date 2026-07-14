#include <furi.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/stream/stream.h>
#include <string.h>
#include <stdlib.h>
#include "../main.h"

size_t read_passwords_from_file(const char* filename, Credential* credentials);
bool write_password_to_file(
    const char* filename,
    const char* service,
    const char* username,
    const char* password);
bool delete_line_from_file(const char* path, size_t line_to_delete);
