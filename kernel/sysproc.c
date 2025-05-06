#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "kalloc.h"
#include "file.h"
#include "fs.h"
#include "mmap.h"
#include <stddef.h>

extern struct proc proc[NPROC];
extern int freemem_count;

// Global mmap areas array
struct mmap_area mmap_areas[MAX_MMAP_AREA];

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_getnice(void)
{
  int pid;
  argint(0, &pid);
  return getnice(pid);
}

uint64
sys_setnice(void)
{
  int pid, value;
  argint(0, &pid);
  argint(1, &value);
  return setnice(pid, value);
}

uint64
sys_ps(void)
{
  int pid;
  argint(0, &pid);
  ps(pid);
  return 0;
}

uint64
sys_meminfo(void)
{
  meminfo();
  return 0;
}

uint64
sys_waitpid(void)
{
  int pid;
  uint64 status;

  argint(0, &pid);
  argaddr(1, &status);

  return waitpid(pid, (int*)status);
}

uint64
sys_mmap(void)
{
  uint64 addr;
  int length, prot, flags, fd, offset;
  struct file *f = NULL;
  struct proc *p = myproc();
  uint64 vstart;
  int i;

  // fetch args
  argaddr(0, &addr);
  argint(1, &length);
  argint(2, &prot);
  argint(3, &flags);
  argint(4, &fd);
  argint(5, &offset);

  printf("mmap: addr=0x%lx length=%d prot=0x%x flags=0x%x fd=%d offset=%d\n",
         addr, length, prot, flags, fd, offset);

  // validate page alignment
  if(addr % PGSIZE != 0 || length <= 0 || length % PGSIZE != 0) {
    printf("mmap: invalid alignment addr=0x%lx length=%d\n", addr, length);
    return 0;
  }
  vstart = MMAPBASE + addr;

  // ① 겹침 검사
  for(int j = 0; j < MAX_MMAP_AREA; j++){
    if(!mmap_areas[j].used || mmap_areas[j].p != p)
      continue;
    uint64 s = mmap_areas[j].addr;
    uint64 e = s + mmap_areas[j].length;
    if(!(vstart + length <= s || vstart >= e)){
      printf("mmap: overlap with existing region\n");
      return 0;            // 명세대로 실패
    }
  }

  // validate prot
  if(prot != PROT_READ && prot != (PROT_READ | PROT_WRITE)) {
    printf("mmap: invalid prot=0x%x\n", prot);
    return 0;
  }

  // validate flags & fd
  if(!(flags & MAP_ANONYMOUS)) {
    if(fd < 0 || fd >= NOFILE) {
      printf("mmap: invalid fd=%d\n", fd);
      return 0;
    }
    f = p->ofile[fd];
    if(f == NULL) {
    printf("mmap: no file for fd=%d\n", fd);
      return 0;
    }
    // check file permission vs prot
    if((prot & PROT_WRITE) && !f->writable) {
      printf("mmap: file not writable\n");
      return 0;
    }
    if((prot & PROT_READ) && !f->readable) {
      printf("mmap: file not readable\n");
      return 0;
    }
  } else {
    if(fd != -1 || offset != 0) {  // anon: no fd, offset=0
      printf("mmap: invalid anon params fd=%d offset=%d\n", fd, offset);
      return 0;
    }
  }

  // find free slot in mmap_areas
  for(i = 0; i < MAX_MMAP_AREA; i++) {
    if(!mmap_areas[i].used) break;
  }
  if(i == MAX_MMAP_AREA) {
    printf("mmap: no free slots\n");
    return 0;
  }

  // initialize slot
  mmap_areas[i].used   = 1;
  mmap_areas[i].p      = p;
  mmap_areas[i].f      = f;
  mmap_areas[i].addr   = vstart;
  mmap_areas[i].length = length;
  mmap_areas[i].offset = offset;
  mmap_areas[i].prot   = prot;
  mmap_areas[i].flags  = flags;

  printf("mmap: created mapping at 0x%lx length=%d\n", vstart, length);

  // if MAP_POPULATE: allocate & map all pages now
  if(flags & MAP_POPULATE) {
    for(uint64 off = 0; off < (uint64)length; off += PGSIZE) {
      char *mem = kalloc();
      if(mem == NULL) goto error;
      memset(mem, 0, PGSIZE);
      if(!(flags & MAP_ANONYMOUS)) {
        // read file
        if(readi(f->ip, 0, (uint64)mem, offset + off, PGSIZE) < 0)
          goto error;
      }
      // map pte flags
      int perm = PTE_U | PTE_R | ((prot & PROT_WRITE) ? PTE_W : 0);
      if(mappages(p->pagetable, vstart + off, PGSIZE, (uint64)mem, perm) < 0)
        goto error;
      // TLB invalidation after page table update
      sfence_vma();
    }
  }

  return vstart;

error:
  // cleanup partially populated pages
  for(uint64 off = 0; off < (uint64)length; off += PGSIZE) {
    pte_t *pte = walk(p->pagetable, vstart + off, 0);
    if(pte && (*pte & PTE_V)) {
      uint64 pa = PTE2PA(*pte);
      *pte = 0;
      memset((void*)pa, 1, PGSIZE);
      kfree((void*)pa);
      // TLB invalidation after page table update
      sfence_vma();
    }
  }
  mmap_areas[i].used = 0;
  return 0;
}

uint64
sys_munmap(void)
{
  uint64 addr;
  int length;

  // 인자 fetch
  argaddr(0, &addr);
  argint(1, &length);

  printf("munmap: addr=0x%lx length=%d\n", addr, length);
  
  if(length <= 0 || length % PGSIZE != 0) {
    printf("munmap: invalid length=%d\n", length);
    return -1;
  }
  if(addr % PGSIZE != 0) {
    printf("munmap: invalid alignment addr=0x%lx\n", addr);
    return -1;
  }
  if(addr < MMAPBASE || addr + length > MMAPBASE + 0x10000000UL) {
    printf("munmap: invalid address range 0x%lx-0x%lx\n", addr, addr + length);
    return -1;
  }

  return sys_munmap_addrlen(addr, length);
}

// Internal helper for munmap that doesn't validate arguments
int
sys_munmap_addrlen(uint64 addr, int length)
{
  struct proc *p = myproc();
  struct mmap_area *ma = 0;
  
  for(int i = 0; i < MAX_MMAP_AREA; i++) {
    if(mmap_areas[i].used && mmap_areas[i].p == p && mmap_areas[i].addr == addr) {
      ma = &mmap_areas[i];
      break;
    }
  }

  if(!ma || length > ma->length) {
    return -1;
  }

  // 실제 unmap (leaf PTE + 페이지 해제 + pt0/pt1도 내부적으로 처리)
  int npages = length / PGSIZE;
  uvmunmap(p->pagetable, addr, npages, 1);
  sfence_vma();            // TLB invalidation

  // mmap_area 조정 또는 제거
  if(length == ma->length) {
    memset(ma, 0, sizeof(*ma)); // 전체 해제
  } else {
    ma->addr   += length;
    ma->offset += length;
    ma->length -= length;
  }

  return 1;
}

uint64
sys_freemem(void)
{
  return freemem_count;
}
