#include "types.h"

void kinit(void);
void* kalloc(void);
void kfree(void*);

struct run {
  struct run *next; 
}; // struct run is a linked list of free memory

extern struct {
  struct spinlock lock; // lock for the allocator
  struct run *freelist; // pointer to the free list
} kmem; // kmem is the memory allocator
