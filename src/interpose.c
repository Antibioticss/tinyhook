#include "../include/tinyhook.h"
#include "private.h"

#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <stdint.h>
#include <string.h>

int tiny_interpose(uint32_t image_index, const char *symbol_name, void *replacement) {
    intptr_t image_slide = _dyld_get_image_vmaddr_slide(image_index);
    struct mach_header_64 *mh_header = (struct mach_header_64 *)_dyld_get_image_header(image_index);
    struct load_command *ld_command = (void *)mh_header + sizeof(struct mach_header_64);
    struct section_64 *sym_sects[2] = {NULL, NULL};
    struct symtab_command *symtab_cmd = NULL;
    struct dysymtab_command *dysymtab_cmd = NULL;
    struct segment_command_64 *linkedit_cmd = NULL;
    for (int i = 0; i < mh_header->ncmds; i++) {
        if (ld_command->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *segment = (struct segment_command_64 *)ld_command;
            if (strcmp(segment->segname, "__DATA_CONST") == 0) {
                if (sym_sects[0]) continue;
                struct section_64 *data_const_sect = (void *)(segment + 1);
                for (int j = 0; j < segment->nsects; j++) {
                    if ((data_const_sect[j].flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS) {
                        sym_sects[0] = data_const_sect + j; // __nl_symbol_ptr
                        break;
                    }
                }
            }
            else if (strcmp(segment->segname, "__DATA") == 0) {
                if (sym_sects[1]) continue;
                struct section_64 *data_sect = (void *)(segment + 1);
                for (int j = 0; j < segment->nsects; j++) {
                    if ((data_sect[j].flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS) {
                        sym_sects[1] = data_sect + j; // __la_symbol_ptr
                        break;
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
    if (linkedit_cmd == NULL || symtab_cmd == NULL || dysymtab_cmd == NULL) {
        LOG_ERROR("tiny_interpose: bad mach-o structure!");
        return 1;
    }
    void *linkedit_base = (void *)image_slide + linkedit_cmd->vmaddr - linkedit_cmd->fileoff;
    char *str_tbl = linkedit_base + symtab_cmd->stroff;
    struct nlist_64 *nl_tbl = linkedit_base + symtab_cmd->symoff;
    uint32_t *indirect_sym_tbl = linkedit_base + dysymtab_cmd->indirectsymoff;

    int err = 0;
    bool found = 0;
    for (int i = 0; i < 2; i++) {
        struct section_64 *sym_sec = sym_sects[i];
        if (!sym_sec) continue;
        void **sym_ptrs = (void *)sym_sec->addr + image_slide;
        uint32_t *indirect_sym_entry = indirect_sym_tbl + sym_sec->reserved1; // reserved1 is an index!
        size_t nptrs = sym_sec->size / sizeof(void *);
        for (int j = 0; j < nptrs; j++) {
            uint32_t symtab_index = indirect_sym_entry[j];
            if (symtab_index == INDIRECT_SYMBOL_LOCAL ||
                symtab_index == (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS)) {
                continue;
            }
            if (strcmp(symbol_name, str_tbl + nl_tbl[symtab_index].n_un.n_strx) == 0) {
                found = 1;
                if (i == 0) { // __nl_symbol_ptr in __DATA_CONST
                    err = mach_vm_protect(mach_task_self(), (mach_vm_address_t)sym_ptrs, sym_sec->size, FALSE,
                                          VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
                    if (err != 0) goto exit;
                }
                sym_ptrs[j] = replacement;
                goto exit;
            }
        }
    }

exit:
    if (!found) {
        err = -1;
        LOG_ERROR("tiny_interpose: no matching indirect symbol found!");
    }
    return err;
}
