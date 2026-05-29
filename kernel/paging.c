#include "paging.h"

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
