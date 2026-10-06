#include "application_metadata.h"
#include "elf/elf.h"

#include <furi.h>
#include <string.h>

#define METADATA_SECTION_BATCH 16U
#define METADATA_NAME_CACHE    1024U

typedef struct {
    Elf32_Shdr sections[METADATA_SECTION_BATCH];
    uint8_t names[METADATA_NAME_CACHE];
    uint32_t names_offset;
    size_t names_size;
} MetadataBuffer;

static bool metadata_extent_valid(uint64_t file_size, uint32_t offset, uint64_t size) {
    return offset <= file_size && size <= file_size - offset;
}

static bool metadata_read_at(File* file, uint32_t offset, void* data, size_t size) {
    return storage_file_seek(file, offset, true) && storage_file_read(file, data, size) == size;
}

static bool metadata_section_matches(
    File* file,
    const Elf32_Shdr* strings,
    uint32_t name_offset,
    MetadataBuffer* buffer,
    bool* matches) {
    static const char name[] = ".fapmeta";
    *matches = false;
    if(name_offset >= strings->sh_size) return false;
    if(strings->sh_size - name_offset < sizeof(name)) return true;

    if(buffer->names_size < sizeof(name) || name_offset < buffer->names_offset ||
       name_offset - buffer->names_offset > buffer->names_size - sizeof(name)) {
        buffer->names_offset = name_offset - name_offset % METADATA_NAME_CACHE;
        if(name_offset - buffer->names_offset > METADATA_NAME_CACHE - sizeof(name)) {
            buffer->names_offset = name_offset;
        }
        buffer->names_size = MIN(strings->sh_size - buffer->names_offset, METADATA_NAME_CACHE);
        if(!metadata_read_at(
               file,
               strings->sh_offset + buffer->names_offset,
               buffer->names,
               buffer->names_size)) {
            return false;
        }
    }

    // Include the terminator: a prefix such as .fapmeta.extra is not metadata.
    *matches = memcmp(
                   buffer->names + (name_offset - buffer->names_offset), name, sizeof(name)) == 0;
    return true;
}

bool flipper_application_metadata_load(
    Storage* storage,
    const char* path,
    FlipperApplicationManifest* manifest) {
    furi_check(storage);
    furi_check(path);
    furi_check(manifest);

    bool success = false;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    MetadataBuffer* buffer = NULL;
    do {
        if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) break;
        const uint64_t file_size = storage_file_size(file);
        // Storage seeks and ELF32 section offsets are both 32-bit.
        if(file_size < sizeof(Elf32_Ehdr) || file_size > UINT32_MAX) break;

        Elf32_Ehdr header;
        if(!metadata_read_at(file, 0, &header, sizeof(header))) break;
        if(memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 ||
           header.e_ident[EI_CLASS] != ELFCLASS32 || header.e_ident[EI_DATA] != ELFDATA2LSB ||
           header.e_ident[EI_VERSION] != EV_CURRENT || header.e_version != EV_CURRENT ||
           header.e_machine != EM_ARM || (header.e_type != ET_REL && header.e_type != ET_EXEC) ||
           header.e_ehsize != sizeof(Elf32_Ehdr) || header.e_shentsize != sizeof(Elf32_Shdr) ||
           header.e_shnum < 2 || header.e_shstrndx == SHN_UNDEF ||
           header.e_shstrndx >= header.e_shnum ||
           !metadata_extent_valid(
               file_size, header.e_shoff, (uint64_t)header.e_shnum * sizeof(Elf32_Shdr))) {
            break;
        }

        Elf32_Shdr strings;
        if(!metadata_read_at(
               file,
               header.e_shoff + (uint32_t)header.e_shstrndx * sizeof(Elf32_Shdr),
               &strings,
               sizeof(strings)) ||
           strings.sh_type != SHT_STRTAB || strings.sh_size == 0 ||
           !metadata_extent_valid(file_size, strings.sh_offset, strings.sh_size)) {
            break;
        }

        // Heap scratch stays below 2 KiB; the loader menu has a 2 KiB thread stack.
        // Batching headers and caching names avoids seek/read/tell/restore per section.
        buffer = malloc(sizeof(MetadataBuffer));
        if(!buffer) break;
        buffer->names_offset = 0;
        buffer->names_size = 0;
        for(uint32_t first = 1; first < header.e_shnum;) {
            const size_t count = MIN((uint32_t)header.e_shnum - first, METADATA_SECTION_BATCH);
            if(!metadata_read_at(
                   file,
                   header.e_shoff + first * sizeof(Elf32_Shdr),
                   buffer->sections,
                   count * sizeof(Elf32_Shdr))) {
                break;
            }

            bool stop = false;
            for(size_t i = 0; i < count; i++) {
                const Elf32_Shdr* section = &buffer->sections[i];
                bool matches;
                if(!metadata_section_matches(file, &strings, section->sh_name, buffer, &matches)) {
                    stop = true;
                    break;
                }
                if(!matches) continue;
                stop = true;
                if(section->sh_type != SHT_PROGBITS ||
                   section->sh_size < sizeof(FlipperApplicationManifestOfw) ||
                   section->sh_size > sizeof(FlipperApplicationManifestEx) ||
                   !metadata_extent_valid(file_size, section->sh_offset, section->sh_size)) {
                    break;
                }

                memset(manifest, 0, sizeof(*manifest));
                if(!metadata_read_at(file, section->sh_offset, manifest, section->sh_size)) break;
                if(section->sh_size < sizeof(FlipperApplicationManifestEx)) {
                    manifest->flags = FlipperApplicationFlagDefault;
                }
                success = flipper_application_manifest_is_valid(manifest) &&
                          flipper_application_manifest_is_target_compatible(manifest) &&
                          manifest->name[0] &&
                          memchr(manifest->name, '\0', sizeof(manifest->name)) != NULL;
                break;
            }
            if(stop) break;
            first += count;
        }
    } while(false);

    free(buffer);
    storage_file_free(file);
    return success;
}
