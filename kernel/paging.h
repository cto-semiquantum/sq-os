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

#endif /* PAGING_H */
