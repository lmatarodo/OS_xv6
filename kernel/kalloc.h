#include "types.h"

// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

void kinit(void);
void* kalloc(void);
void kfree(void*);

// Physical memory allocator. The allocator maintains a
// linked list of free physical memory pages, each of which
// is 4096 bytes long.  See kalloc.c for details.
struct run {
  struct run *next;
};

extern struct {
  struct spinlock lock;
  struct run *freelist;
} kmem; 