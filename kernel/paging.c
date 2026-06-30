#include "paging.h"
#include "memory.h"
#include <stddef.h>

/* 4KB aligned page directory and page tables */
uint32_t page_directory[1024] __attribute__((aligned(4096)));
uint32_t page_table_kernel[1024] __attribute__((aligned(4096)));
uint32_t page_table_user[1024] __attribute__((aligned(4096)));

static uint8_t paging_enabled = 0;

void paging_init(void) {
    // 1. Clear page directory
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0;
    }

    // 2. Identity map first 4MB (Kernel Space, 0x00000000 to 0x003FFFFF)
    // Attributes: Present (1), Read/Write (2), Supervisor (0) -> 3
    for (int i = 0; i < 1024; i++) {
        page_table_kernel[i] = (i * 4096) | 3;
    }

    // 3. Identity map second 4MB (User Space, 0x00400000 to 0x007FFFFF)
    // Attributes: Present (1), Read/Write (2), User (4) -> 7
    for (int i = 0; i < 1024; i++) {
        page_table_user[i] = (0x00400000 + i * 4096) | 7;
    }

    // 4. Link page tables in directory
    // Entry 0 (0MB to 4MB) = Kernel Space (Supervisor, U/S=0 -> 3)
    page_directory[0] = ((uint32_t)page_table_kernel) | 3;
    
    // Entry 1 (4MB to 8MB) = User Space (User, U/S=1 -> 7)
    page_directory[1] = ((uint32_t)page_table_user) | 7;

    // 5. Load CR3 with page directory physical address
    __asm__ volatile("mov %0, %%cr3" : : "r"(page_directory));

    // 6. Enable paging (PG bit in CR0)
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));

    paging_enabled = 1;
}

uint8_t is_paging_enabled(void) {
    return paging_enabled;
}

uint32_t get_page_directory_addr(void) {
    return (uint32_t)page_directory;
}

uint32_t *paging_create_user_directory(void **out_raw_pd) {
    /* Allocate 2 pages (8KB) to guarantee a 4KB aligned start pointer */
    uint8_t *raw_pd = (uint8_t *)kmalloc(4096 * 2);
    if (!raw_pd) return (void *)0;

    if (out_raw_pd) {
        *out_raw_pd = raw_pd;
    }

    uint32_t *pd = (uint32_t *)(((uint32_t)raw_pd + 4095) & ~4095);

    /* Zero the page directory */
    for (int i = 0; i < 1024; i++) {
        pd[i] = 0;
    }

    /* Copy Kernel mapping (first 4MB, entry 0) from default directory */
    uint32_t *global_pd = (uint32_t *)get_page_directory_addr();
    pd[0] = global_pd[0];

    return pd;
}

void map_user_page(uint32_t *pd, uint32_t vaddr, uint32_t paddr, void **alloc_blocks, int *alloc_count) {
    uint32_t pd_idx = vaddr >> 22;
    uint32_t pt_idx = (vaddr >> 12) & 0x3FF;

    uint32_t *pt;
    if (!(pd[pd_idx] & 1)) {
        /* Allocate 2 pages to guarantee a 4KB aligned page table start */
        uint8_t *raw_pt = (uint8_t *)kmalloc(4096 * 2);
        if (!raw_pt) return;
        
        if (alloc_blocks && alloc_count && *alloc_count < 30) {
            alloc_blocks[(*alloc_count)++] = raw_pt;
        }

        uint32_t aligned_pt = ((uint32_t)raw_pt + 4095) & ~4095;

        /* Zero the page table */
        for (int i = 0; i < 1024; i++) {
            ((uint32_t *)aligned_pt)[i] = 0;
        }

        /* Link page table in directory with attributes: User, R/W, Present (7) */
        pd[pd_idx] = aligned_pt | 7;
    }

    pt = (uint32_t *)(pd[pd_idx] & ~4095);
    /* Map page to physical address with attributes: User, R/W, Present (7) */
    pt[pt_idx] = paddr | 7;
}
