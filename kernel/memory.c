#include "memory.h"

/* ================================================================
 * Internal allocator state
 * ================================================================ */
static uint32_t bump_ptr     = 0;   /* Next free byte in heap        */
static uint32_t alloc_count  = 0;   /* Total kmalloc() calls         */
static uint32_t free_count   = 0;   /* Total kfree() calls           */
static uint32_t used_payload = 0;   /* Bytes in live allocations     */
static uint32_t overhead     = 0;   /* Bytes used by block headers   */

/* ================================================================
 * heap_init
 * ================================================================ */
void heap_init(void) {
    bump_ptr    = HEAP_START;
    alloc_count = 0;
    free_count  = 0;
    used_payload= 0;
    overhead    = 0;
}

/* ================================================================
 * kmalloc — bump-pointer with block header
 *
 * Memory layout of each allocation:
 *
 *   [ HeapBlock header (16 bytes) ][ user payload (size bytes) ]
 *    ^                              ^
 *    bump_ptr before alloc          returned pointer
 *
 * 8-byte alignment applied to the payload start address.
 * ================================================================ */
void *kmalloc(size_t size) {
    if (size == 0) return (void *)0;

    /* Reserve space for header, then align payload to HEAP_ALIGN */
    uint32_t hdr_addr = bump_ptr;

    /* Align header start to HEAP_ALIGN */
    hdr_addr = (hdr_addr + HEAP_ALIGN - 1) & ~(HEAP_ALIGN - 1);

    uint32_t payload_addr = hdr_addr + (uint32_t)HEAP_HEADER_SIZE;

    /* Align payload address */
    payload_addr = (payload_addr + HEAP_ALIGN - 1) & ~(HEAP_ALIGN - 1);

    /* Actual end of this block */
    uint32_t block_end = payload_addr + (uint32_t)size;

    /* Check heap exhaustion */
    if (block_end > HEAP_END) {
        return (void *)0; /* Out of memory */
    }

    /* Write block header just before the payload */
    uint32_t actual_hdr = payload_addr - HEAP_HEADER_SIZE;
    HeapBlock *blk = (HeapBlock *)actual_hdr;
    blk->magic = HEAP_MAGIC;
    blk->size  = (uint32_t)size;
    blk->free  = 0;
    blk->_pad[0] = 0;
    blk->_pad[1] = 0;
    blk->_pad[2] = 0;

    /* Advance bump pointer */
    bump_ptr = block_end;

    /* Update counters */
    alloc_count++;
    used_payload += (uint32_t)size;
    overhead     += (uint32_t)(payload_addr - hdr_addr);

    return (void *)payload_addr;
}

/* ================================================================
 * kfree — mark block as freed
 *
 * Walks back HEAP_HEADER_SIZE bytes from the user pointer to find
 * the HeapBlock header. Validates the magic sentinel before marking.
 *
 * Note: does NOT reclaim memory (no coalescing). A future milestone
 * will add a free-list and coalescing pass.
 * ================================================================ */
void kfree(void *ptr) {
    if (!ptr) return;

    uint32_t payload_addr = (uint32_t)ptr;

    /* Sanity: must be within heap bounds */
    if (payload_addr < HEAP_START || payload_addr >= HEAP_END) return;

    /* Recover header */
    HeapBlock *blk = (HeapBlock *)(payload_addr - HEAP_HEADER_SIZE);

    /* Validate magic — if corrupt, silently ignore (no kernel panic yet) */
    if (blk->magic != HEAP_MAGIC) return;

    /* Already freed? */
    if (blk->free) return;

    blk->free = 1;
    free_count++;

    /* Subtract from live payload count */
    if (used_payload >= blk->size) {
        used_payload -= blk->size;
    }
}

/* ================================================================
 * heap_stats — populate a HeapStats struct
 * ================================================================ */
void heap_stats(HeapStats *out) {
    if (!out) return;

    uint32_t allocated_span = (bump_ptr > HEAP_START)
                              ? (bump_ptr - HEAP_START)
                              : 0;

    out->total_bytes   = HEAP_SIZE;
    out->used_bytes    = used_payload;
    out->free_bytes    = HEAP_SIZE - allocated_span;
    out->alloc_count   = alloc_count;
    out->free_count    = free_count;
    out->block_count   = alloc_count - free_count;
    out->overhead_bytes= overhead;
}

/* ================================================================
 * heap_used — convenience wrapper
 * ================================================================ */
size_t heap_used(void) {
    return (size_t)used_payload;
}
