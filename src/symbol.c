#include "private.h"
#include "tinyhook.h"

#include <mach-o/dyld.h>   // _dyld_*
#include <mach-o/loader.h> // mach_header_64, load_command...
#include <mach-o/nlist.h>  // nlist_64
#include <stdint.h>
#include <string.h> // strcmp()

struct imageinfo {
    void *image_slide;
    void *image_header;
    void *linkedit_base;

    bool export;
    uint32_t export_off;

    bool symtab;
    uint32_t string_off;
    uint32_t symbol_off;

    bool dysymtab;
    uint32_t ilocalsym;       /* index to local symbols */
    uint32_t nlocalsym;       /* number of local symbols */
    uint32_t iextdefsym;      /* index to externally defined symbols */
    uint32_t nextdefsym;      /* number of externally defined symbols */
    uint32_t indirectsym_off; /* file offset to the indirect symbol table */

    uint64_t asymstub; /* addr of the stub section */
    uint32_t isymstub; /* starting index in indirect symbol table */
    uint32_t nsymstub; /* number of indirect symbol entries */
    uint32_t wsymstub; /* byte size of the stubs */
};

static int parse_macho(uint32_t image_index, struct imageinfo *info) {
    const struct mach_header_64 *header = (struct mach_header_64 *)_dyld_get_image_header(image_index);
    if (header == NULL) {
        LOG_ERROR("parse_macho: image_index %d out of range!", image_index);
        return -1;
    }
    info->image_slide = (void *)_dyld_get_image_vmaddr_slide(image_index);
    info->image_header = (void *)header;

    const struct load_command *cmd = (void *)header + sizeof(struct mach_header_64);
    for (int i = 0; i < header->ncmds; i++, cmd = (void *)cmd + cmd->cmdsize) {
        switch (cmd->cmd) {
        case LC_SEGMENT_64: {
            const struct segment_command_64 *seg = (struct segment_command_64 *)cmd;
            if (strcmp(seg->segname, SEG_LINKEDIT) == 0) {
                info->linkedit_base = info->image_slide + seg->vmaddr - seg->fileoff;
                break;
            }
            const struct section_64 *sects = (void *)seg + sizeof(const struct segment_command_64);
            if (strcmp(seg->segname, SEG_TEXT) == 0) {
                for (int j = 0; j < seg->nsects; j++) {
                    if ((sects[j].flags & SECTION_TYPE) == S_SYMBOL_STUBS && sects[j].reserved2) {
                        info->asymstub = sects[j].addr;
                        info->isymstub = sects[j].reserved1;
                        info->wsymstub = sects[j].reserved2;
                        info->nsymstub = sects[j].size / info->wsymstub;
                        break;
                    }
                }
            }
            break;
        }
        case LC_DYLD_INFO:
        case LC_DYLD_INFO_ONLY: {
            const struct dyld_info_command *dyld_info = (struct dyld_info_command *)cmd;
            info->export = true;
            info->export_off = dyld_info->export_off;
            break;
        }
        case LC_DYLD_EXPORTS_TRIE: {
            const struct linkedit_data_command *linkedit_data = (struct linkedit_data_command *)cmd;
            info->export = true;
            info->export_off = linkedit_data->dataoff;
            break;
        }
        case LC_SYMTAB: {
            const struct symtab_command *symtab = (struct symtab_command *)cmd;
            info->symtab = true;
            info->string_off = symtab->stroff;
            info->symbol_off = symtab->symoff;
            break;
        }
        case LC_DYSYMTAB: {
            const struct dysymtab_command *dysymtab = (struct dysymtab_command *)cmd;
            info->dysymtab = true;
            info->ilocalsym = dysymtab->ilocalsym;
            info->nlocalsym = dysymtab->nlocalsym;
            info->iextdefsym = dysymtab->iextdefsym;
            info->nextdefsym = dysymtab->nextdefsym;
            info->indirectsym_off = dysymtab->indirectsymoff;
            break;
        }
        default:
            break;
        }
    }
    return 0;
}

static uint64_t read_uleb128(const uint8_t **p) {
    int bit = 0;
    uint64_t result = 0;
    do {
        uint64_t slice = **p & 0x7f;
        result |= (slice << bit);
        bit += 7;
    } while (*(*p)++ & 0x80);
    return result;
}

static size_t read_cstring(const uint8_t **p) {
    size_t len = 0;
    while (*(*p)++) len++;
    return len;
}

static uint64_t trie_query(const uint8_t *export, const char *name) {
    // documents in <mach-o/loader.h>
    uint64_t value = 0;
    const char *namep = name;
    const uint8_t *pos = export;
    while (namep[0] != '\0') {
        pos += read_uleb128(&pos); // skip exported symbol info
        bool matched = false;
        uint8_t nchild = *pos++;
        for (int i = 0; i < nchild; i++) {
            char *str = (char *)pos;
            size_t len = read_cstring(&pos);
            uint64_t offset = read_uleb128(&pos);
            if (strncmp(namep, str, len) == 0) {
                namep += len;
                pos = export + offset;
                matched = true;
                break;
            }
        }
        if (!matched) break;
    }
    if (namep[0] == '\0') {
        uint64_t info_len = read_uleb128(&pos);
        if (info_len != 0) {
            uint64_t flag = read_uleb128(&pos);
            if (flag == EXPORT_SYMBOL_FLAGS_KIND_REGULAR) {
                value = read_uleb128(&pos);
            }
        }
    }
    return value;
}

static void *resolve_export(struct imageinfo *info, const char *symbol_name) {
    uint64_t value = 0;
    void *export_trie = info->linkedit_base + info->export_off;

    value = trie_query(export_trie, symbol_name);

    if (value == 0) return NULL;
    return info->image_header + value;
}

static void *resolve_symtab(struct imageinfo *info, const char *symbol_name) {
    uint64_t value = 0;
    const char *strtab = info->linkedit_base + info->string_off;
    const struct nlist_64 *symtab = info->linkedit_base + info->symbol_off;

    const struct nlist_64 *extdeftab = symtab + info->iextdefsym;
    for (int i = 0; i < info->nextdefsym; i++) {
        if (strcmp(symbol_name, strtab + extdeftab[i].n_un.n_strx) == 0) {
            value = extdeftab[i].n_value;
            break;
        }
    }
    if (value != 0) return info->image_slide + value;

    const struct nlist_64 *localtab = symtab + info->ilocalsym;
    for (int i = 0; i < info->nlocalsym; i++) {
        if (strcmp(symbol_name, strtab + localtab[i].n_un.n_strx) == 0) {
            value = localtab[i].n_value;
            break;
        }
    }
    if (value == 0) return NULL;
    return info->image_slide + value;
}

static void *resolve_stub(struct imageinfo *info, const char *symbol_name) {
    uint64_t value = 0;
    const char *strtab = info->linkedit_base + info->string_off;
    const struct nlist_64 *symtab = info->linkedit_base + info->symbol_off;
    const uint32_t *indirsymtab = info->linkedit_base + info->indirectsym_off;

    indirsymtab += info->isymstub;
    for (int i = 0; i < info->nsymstub; i++) {
        uint32_t idx = indirsymtab[i];
        if (idx & INDIRECT_SYMBOL_LOCAL) continue;
        if (strcmp(symbol_name, strtab + symtab[idx].n_un.n_strx) == 0) {
            value = info->asymstub + (uint64_t)i * info->wsymstub;
            break;
        }
    }

    if (value == 0) return NULL;
    return info->image_slide + value;
}

void *symbol_resolve(uint32_t image_index, const char *symbol_name, resolve_type_t type) {
    ARG_CHECK(symbol_name != NULL);
    struct imageinfo info;
    memset(&info, 0, sizeof(struct imageinfo));

    if (parse_macho(image_index, &info) != 0) return NULL;
    if (!info.linkedit_base) {
        LOG_ERROR("symbol_resolve: __LINKEDIT segment not found in image with index %d!", image_index);
        return NULL;
    }

    void *symbol_address = NULL;
    resolve_type_t resolve_type = type;
    if (resolve_type == RESOLVE_ALL) resolve_type = RESOLVE_EXPORT | RESOLVE_SYMTAB | RESOLVE_STUBS;
    if ((resolve_type & RESOLVE_EXPORT) && info.export) {
        symbol_address = resolve_export(&info, symbol_name);
        if (symbol_address) return symbol_address;
    }
    if ((resolve_type & RESOLVE_SYMTAB) && info.symtab && info.dysymtab) {
        symbol_address = resolve_symtab(&info, symbol_name);
        if (symbol_address) return symbol_address;
    }
    if ((resolve_type & RESOLVE_STUBS) && info.symtab && info.dysymtab && info.asymstub) {
        symbol_address = resolve_stub(&info, symbol_name);
        if (symbol_address) return symbol_address;
    }

    if (!symbol_address) {
        LOG_ERROR("symbol_resove: symbol '%s' not found in image_index %d!", symbol_name, image_index);
    }

    return symbol_address;
}
