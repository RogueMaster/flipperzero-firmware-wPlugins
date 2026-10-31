#pragma once

#include <flipper_application/elf/elf_api_interface.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Host symbols FlipPass plugins may call directly, resolved by name hash at plugin load. */
extern const ElfApiInterface* const flippass_app_api_interface;

#ifdef __cplusplus
}
#endif
