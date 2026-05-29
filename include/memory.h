#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

/* ================================================================
 * SQ-OS Kernel Heap
 * ================================================================
 *
 * Physical memory map (safe zone for heap):
 *
 *   0x00000 – 0x004FF   Real-mode IVT + BDA
 *   0x07E00 – ~0x10000  Kernel .text / .rodata / .data / .bss
 *   0x02000 – 0x02800   IDT (256 × 8-byte entries)
 *   0x50000 – 0x5F9FF   VGA backbuffer  (64 000 bytes)
 *   0x90000             Stack base (grows down)
 *   0xA0000 – 0xAF9FF   VGA framebuffer (64 000 bytes)
 *   0x100000 →          *** KERNEL HEAP (this module) ***
 *
 * Design:
 *   • Bump-pointer allocator with block headers.
 *   • Each allocation is prefixed by a HeapBlock header (8 bytes).
 *   • kfree() marks blocks as free but does NOT coalesce (v1).
 *   • heap_stats() returns used / free / total / block count.
 *   • 8-byte aligned allocations (safe for any primitive type).
 * ================================================================ */

#define HEAP_START   0x100000U        /* 1 MB physical               */
#define HEAP_SIZE    0x100000U        /* 1 MB pool                   */
#define HEAP_END    (HEAP_START + HEAP_SIZE)

#define HEAP_MAGIC   0xDEADBEEFU     /* Sanity sentinel in header   */
#define HEAP_ALIGN   8U              /* All allocations 8-byte aligned */

/* ----------------------------------------------------------------
 * Block header — sits immediately before every allocation.
 * Total header size: 12 bytes, padded to 16 for alignment.
 * ---------------------------------------------------------------- */
typedef struct HeapBlock {
    uint32_t magic;      /* HEAP_MAGIC — detects corruption          */
    uint32_t size;       /* Usable bytes (NOT including header)      */
    uint8_t  free;       /* 1 = freed by kfree(), 0 = in use        */
    uint8_t  _pad[3];    /* Padding to keep 8-byte alignment         */
} HeapBlock;

#define HEAP_HEADER_SIZE  sizeof(HeapBlock)   /* 12 bytes → 16 w/ padding */

/* ----------------------------------------------------------------
 * Heap statistics structure — returned by heap_stats()
 * ---------------------------------------------------------------- */
typedef struct HeapStats {
    uint32_t total_bytes;     /* Total heap size in bytes             */
    uint32_t used_bytes;      /* Bytes currently allocated (payload)  */
    uint32_t free_bytes;      /* Bytes not yet allocated              */
    uint32_t alloc_count;     /* Total kmalloc() calls so far         */
    uint32_t free_count;      /* Total kfree() calls so far           */
    uint32_t block_count;     /* Number of live (non-freed) blocks    */
    uint32_t overhead_bytes;  /* Bytes consumed by headers            */
} HeapStats;

/* ================================================================
 * Public API
 * ================================================================ */

/* heap_init — initialise the heap. Call once before any kmalloc(). */
void heap_init(void);

/* kmalloc — allocate `size` bytes (8-byte aligned).
 * Returns NULL if heap is full or size == 0. */
void *kmalloc(size_t size);

/* kfree — release a previously allocated block.
 * Marks the block header as free. Does NOT coalesce adjacent blocks. */
void kfree(void *ptr);

/* heap_stats — fill a HeapStats struct with current heap metrics. */
void heap_stats(HeapStats *out);

/* heap_used — convenience: returns used payload bytes (no struct needed). */
size_t heap_used(void);

#endif /* MEMORY_H */
