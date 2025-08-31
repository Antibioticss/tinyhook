#include "../include/tinyhook.h"
#include "private.h"

#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <stdint.h>
#include <string.h>

static int update_pointer(struct section_64 *sym_sec, intptr_t slide, char *str_tbl, struct nlist_64 *nl_tbl,
                          uint32_t *indirect_sym_tbl, const char *symbol_name, void *replacement) {
    if (!sym_sec) return 1;
    void **sym_ptrs = (void *)sym_sec->addr + slide;
    uint32_t *indirect_sym_entry = indirect_sym_tbl + sym_sec->reserved1; // reserved1 is an index!
    int nptrs = sym_sec->size / sizeof(void *);
    for (int i = 0; i < nptrs; i++) {
        uint32_t symtab_index = indirect_sym_entry[i];
        if (symtab_index == INDIRECT_SYMBOL_LOCAL || symtab_index == (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS)) {
            continue;
        }
        if (strcmp(symbol_name, str_tbl + nl_tbl[symtab_index].n_un.n_strx) == 0) {
            int kr = mach_vm_protect(mach_task_self(), (mach_vm_address_t)sym_ptrs, sym_sec->size, FALSE,
                                     VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
            if (kr != 0) return 1;
            sym_ptrs[i] = replacement;
            return 0;
        }
    }
    return 1;
}

int tiny_interpose(uint32_t image_index, const char *symbol_name, void *replacement) {
    intptr_t image_slide = _dyld_get_image_vmaddr_slide(image_index);
    struct mach_header_64 *mh_header = (struct mach_header_64 *)_dyld_get_image_header(image_index);
    struct load_command *ld_command = (void *)mh_header + sizeof(struct mach_header_64);
    struct section_64 *nl_sym_sect = NULL;
    struct section_64 *la_sym_sect = NULL;
    struct symtab_command *symtab_cmd = NULL;
    struct dysymtab_command *dysymtab_cmd = NULL;
    struct segment_command_64 *linkedit_cmd = NULL;
    for (int i = 0; i < mh_header->ncmds; i++) {
        if (ld_command->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *segment = (struct segment_command_64 *)ld_command;
            if (strcmp(segment->segname, "__DATA_CONST") == 0) {
                if (nl_sym_sect) continue;
                struct section_64 *data_const_sect = (void *)(segment + 1);
                for (int i = 0; i < segment->nsects; i++) {
                    if ((data_const_sect[i].flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS) {
                        nl_sym_sect = data_const_sect + i;
                    }
                }
            }
            else if (strcmp(segment->segname, "__DATA") == 0) {
                if (la_sym_sect) continue;
                struct section_64 *data_sect = (void *)(segment + 1);
                for (int i = 0; i < segment->nsects; i++) {
                    if ((data_sect[i].flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS) {
                        la_sym_sect = data_sect + i;
                    }
                }
            }
            else if (strcmp(segment->segname, "__LINKEDIT") == 0) {
                linkedit_cmd = (struct segment_command_64 *)ld_command;
            }
        }
        else if (ld_command->cmd == LC_SYMTAB) {
            symtab_cmd = (struct symtab_command *)ld_command;
        }
        else if (ld_command->cmd == LC_DYSYMTAB) {
            dysymtab_cmd = (struct dysymtab_command *)ld_command;
        }
        ld_command = (void *)ld_command + ld_command->cmdsize;
    }
    intptr_t linkedit_base = image_slide + linkedit_cmd->vmaddr - linkedit_cmd->fileoff;
    char *str_tbl = (void *)linkedit_base + symtab_cmd->stroff;
    struct nlist_64 *nl_tbl = (void *)linkedit_base + symtab_cmd->symoff;
    uint32_t *indirect_sym_tbl = (void *)linkedit_base + dysymtab_cmd->indirectsymoff;

    if ((update_pointer(nl_sym_sect, image_slide, str_tbl, nl_tbl, indirect_sym_tbl, symbol_name, replacement) != 0) &&
        (update_pointer(la_sym_sect, image_slide, str_tbl, nl_tbl, indirect_sym_tbl, symbol_name, replacement) != 0)) {
        LOG_ERROR("tiny_interpose: no matching indirect symbol found!");
        return 1;
    }
    return 0;
}
