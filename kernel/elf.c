#include "elf.h"
#include "paging.h"
#include "memory.h"
#include "process.h"
#include <stddef.h>

uint32_t elf_load(const uint8_t *elf_data, uint32_t size, Process *p, uint32_t **out_pd) {
    if (size < sizeof(Elf32_Ehdr)) return 0;

    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)elf_data;

    /* Verify ELF magic and 32-bit little-endian x86 structure */
    if (ehdr->e_ident[0] != 0x7F ||
        ehdr->e_ident[1] != 'E'  ||
        ehdr->e_ident[2] != 'L'  ||
        ehdr->e_ident[3] != 'F') {
        return 0;
    }

    if (ehdr->e_ident[4] != 1 || ehdr->e_machine != 3) {
        return 0;
    }

    /* Allocate new process page directory */
    void *raw_pd = NULL;
    uint32_t *pd = paging_create_user_directory(&raw_pd);
    if (!pd) return 0;

    /* Save the raw pointer so it gets cleaned up on process exit */
    p->alloced_blocks[p->alloced_count++] = raw_pd;
    p->cr3 = (uint32_t)pd;

    /* Find program headers */
    Elf32_Phdr *phdr = (Elf32_Phdr *)(elf_data + ehdr->e_phoff);
    uint32_t max_user_vaddr = 0x00500000;

    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) continue;

        uint32_t cur_vaddr = phdr[i].p_vaddr;
        uint32_t end_vaddr = phdr[i].p_vaddr + phdr[i].p_memsz;
        uint32_t file_offset = phdr[i].p_offset;

        if (end_vaddr > max_user_vaddr) {
            max_user_vaddr = end_vaddr;
        }

        while (cur_vaddr < end_vaddr) {
            uint32_t page_vaddr = cur_vaddr & ~4095;

            /* Check if this page is already mapped in our page table */
            uint32_t pd_idx = page_vaddr >> 22;
            uint32_t pt_idx = (page_vaddr >> 12) & 0x3FF;
            uint32_t *pt = NULL;
            uint32_t phys_page = 0;

            if (pd[pd_idx] & 1) {
                pt = (uint32_t *)(pd[pd_idx] & ~4095);
                if (pt[pt_idx] & 1) {
                    phys_page = pt[pt_idx] & ~4095;
                }
            }

            if (!phys_page) {
                /* Allocate a physical page and align it to 4KB */
                uint8_t *raw_page = kmalloc(4096 * 2);
                if (!raw_page) {
                    return 0;
                }
                p->alloced_blocks[p->alloced_count++] = raw_page;
                phys_page = ((uint32_t)raw_page + 4095) & ~4095;

                /* Zero-initialize the page (protects BSS) */
                for (int k = 0; k < 1024; k++) {
                    ((uint32_t *)phys_page)[k] = 0;
                }

                /* Map the user page */
                map_user_page(pd, page_vaddr, phys_page, p->alloced_blocks, &p->alloced_count);
            }

            /* Copy chunk of file data into physical page using kernel's identity-mapped access */
            uint32_t page_offset = cur_vaddr & 4095;
            uint32_t bytes_to_copy = 4096 - page_offset;
            if (bytes_to_copy > (end_vaddr - cur_vaddr)) {
                bytes_to_copy = end_vaddr - cur_vaddr;
            }

            uint32_t offset_in_segment = cur_vaddr - phdr[i].p_vaddr;
            if (offset_in_segment < phdr[i].p_filesz) {
                uint32_t file_chunk = phdr[i].p_filesz - offset_in_segment;
                if (file_chunk > bytes_to_copy) file_chunk = bytes_to_copy;

                uint8_t *dst = (uint8_t *)(phys_page + page_offset);
                const uint8_t *src = elf_data + file_offset + offset_in_segment;
                for (uint32_t b = 0; b < file_chunk; b++) {
                    dst[b] = src[b];
                }
            }

            cur_vaddr += bytes_to_copy;
        }
    }

    /* Set user heap start pointer at the 4KB boundary following the segments */
    p->user_heap_ptr = (max_user_vaddr + 4095) & ~4095;

    *out_pd = pd;
    return ehdr->e_entry;
}
