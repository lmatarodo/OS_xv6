#ifndef _MMAP_H_
#define _MMAP_H_

#include "types.h"
#include "param.h"

// Forward declaration of proc structure
struct proc;
struct file;

// mmap_area structure definition
struct mmap_area {
  struct file *f;    // file for file-mapping, NULL if anonymous
  uint64 addr;       // start virtual address (MMAPBASE + offset)
  int length;        // mapping length in bytes (multiple of PGSIZE)
  int offset;        // file offset (page-aligned)
  int prot;          // PROT_READ, PROT_WRITE
  int flags;         // MAP_ANONYMOUS, MAP_POPULATE
  struct proc *p;    // owning process
  int used;          // slot in use
};

// Global mmap areas array
extern struct mmap_area mmap_areas[MAX_MMAP_AREA];

#endif // _MMAP_H_ 