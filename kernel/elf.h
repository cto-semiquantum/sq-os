#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stddef.h>
#include "process.h"

#define ELF_MAGIC 0x464C457F /* "\x7FELF" in little endian */

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} Elf32_Phdr;

#define PT_LOAD 1

/* Load an ELF binary from raw data buffer.
 * Allocates process page directory, maps program segments to user space,
 * tracks raw page buffers, and returns the entry point virtual address.
 * Returns 0 on failure. */
uint32_t elf_load(const uint8_t *elf_data, uint32_t size, Process *p, uint32_t **out_pd);

#endif /* ELF_H */
