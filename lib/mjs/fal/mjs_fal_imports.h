#pragma once

#include <flipper_application/elf/elf_api_interface.h>

/* Per-image resolver proxy. Public ElfApiInterface layout is unchanged. */
typedef struct {
    ElfApiInterface api;
    const ElfApiInterface* delegate;
    bool acquired;
    bool failed;
} MjsFalImports;

void mjs_fal_imports_init(MjsFalImports* imports, const ElfApiInterface* delegate);
void mjs_fal_imports_release(MjsFalImports* imports);
