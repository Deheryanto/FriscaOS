#include "heap.h"
#include "pmm.h"

/* Heap node header — sits immediately before every allocated block.
 *
 * Layout of the heap:
 *
 *    ┌──────────────────────┐
 *    │  heap_node_t (header)  │  ← size, is_free, next
 *    ├──────────────────────┤
 *    │  user data (payload)   │  ← pointer returned by kmalloc()
 *    └──────────────────────┘
 *
 * The heap is a singly-linked list of these nodes. Free and used
 * blocks are tracked by the `is_free` flag, not by removing nodes. */
typedef struct heap_node {
    size_t size;                /* Size of the payload (bytes) */
    int is_free;                /* 1 = free, 0 = allocated */
    struct heap_node* next;     /* Next node in the linked list */
} heap_node_t;

/* Head of the heap linked list */
static heap_node_t* heap_start = NULL;

/* Initialize the heap with 4 contiguous physical blocks (16 KB).
 *
 * Calls pmm_alloc_block() four times in a row and assumes the PMM
 * returns *contiguous* blocks — which is not guaranteed in general,
 * but works here because the PMM allocates linearly from low to high.
 *
 * Block #1 becomes the first (and only) heap node.
 * Blocks #2, #3, #4 are allocated but not directly used — they just
 * ensure the region is reserved and contiguous for the heap. */
void heap_init(void) {
    /* Get the first block — this will be the start of the heap */
    void* initial_mem = pmm_alloc_block();

    /* Allocate 3 more blocks right after it to extend the heap region */
    for (int i = 0; i < 3; i++) {
        pmm_alloc_block(); 
    }
    
    /* Place the first heap node at the start of the first block.
     * Its payload covers the remaining bytes of all 4 blocks
     * (4 * 4096 = 16384 bytes total, minus the header size). */
    heap_start = (heap_node_t*)initial_mem;
    heap_start->size = (4 * PMM_BLOCK_SIZE) - sizeof(heap_node_t);
    heap_start->is_free = 1;
    heap_start->next = NULL;
}

/* Allocate `size` bytes from the heap.
 *
 * Uses a first-fit strategy: scan the linked list, pick the first
 * free block large enough. If the block is much larger than needed,
 * split it — the extra space becomes a new free node.
 *
 * Returns a pointer to the payload (after the header), or NULL on failure. */
void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    /* Round up to a 4-byte boundary for alignment.
     *   (size + 3) & ~3  →  e.g. 5 → 8,  8 → 8,  9 → 12 */
    size = (size + 3) & ~3;

    heap_node_t* current = heap_start;
    while (current != NULL) {
        /* Is this node free and big enough? */
        if (current->is_free && current->size >= size) {

            /* Split if the leftover space is worth it.
             * We only split if the remaining space can hold:
             *   a new header + at least 16 bytes of payload.
             * Otherwise, just hand over the whole block (internal fragmentation). */
            if (current->size >= size + sizeof(heap_node_t) + 16) {
                /* Create a new node right after the allocated region */
                heap_node_t* new_node = (heap_node_t*)((uint8_t*)current + sizeof(heap_node_t) + size);
                
                /* The new node owns whatever space is left over */
                new_node->size = current->size - size - sizeof(heap_node_t);
                new_node->is_free = 1;
                new_node->next = current->next;

                /* Shrink the current node to exactly `size` and link to new node */
                current->size = size;
                current->next = new_node;
            }

            /* Mark this node as allocated and return the payload pointer
             * (header address + header size). */
            current->is_free = 0;
            return (void*)((uint8_t*)current + sizeof(heap_node_t));
        }
        current = current->next;
    }
    /* No suitable block found */
    return NULL;
}

/* Free a previously allocated block.
 *
 * Marks the node as free, then walks the list to merge any
 * adjacent free nodes (coalescing) to reduce fragmentation. */
void kfree(void* ptr) {
    if (!ptr) return;

    /* Recover the header — it lives immediately before the payload */
    heap_node_t* node = (heap_node_t*)((uint8_t*)ptr - sizeof(heap_node_t));
    node->is_free = 1;

    /* Coalesce adjacent free blocks.
     *
     * Walk the list; whenever a free node is followed by another free node,
     * merge them by absorbing the next node's header + payload into the
     * current node, then skip over the absorbed node. */
    heap_node_t* current = heap_start;
    while (current && current->next) {
        if (current->is_free && current->next->is_free) {
            /* Merge: current absorbs the next node entirely */
            current->size += sizeof(heap_node_t) + current->next->size;
            current->next = current->next->next;

        /* Do NOT advance — the new `next` might also be free,
         * so we check this position again on the next iteration. */
        } else {
            current = current->next;
        }
    }
}