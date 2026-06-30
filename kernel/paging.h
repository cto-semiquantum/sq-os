#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

/* paging_init — initialize the page directory and page tables, identity mapping
 * the first 4MB (Kernel) and second 4MB (User), and enable paging. */
void paging_init(void);

/* is_paging_enabled — returns 1 if paging is active, 0 otherwise. */
uint8_t is_paging_enabled(void);

/* get_page_directory_addr — returns the physical address of the page directory. */
uint32_t get_page_directory_addr(void);

/* paging_create_user_directory — allocates a new page directory and links the kernel space mapping (entry 0). */
uint32_t *paging_create_user_directory(void **out_raw_pd);

/* map_user_page — maps a virtual address to a physical address in the specified page directory. */
void map_user_page(uint32_t *pd, uint32_t vaddr, uint32_t paddr, void **alloc_blocks, int *alloc_count);

#endif /* PAGING_H */
