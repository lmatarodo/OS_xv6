#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#include "types.h"

// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
};

void initlock(struct spinlock*, char*);
void acquire(struct spinlock*);
void release(struct spinlock*);
void push_off(void);
void pop_off(void);

#endif // _SPINLOCK_H_

