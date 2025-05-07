#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "proc.h"
#include "kalloc.h"
#include "mmap.h"
#include <stddef.h>

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

// Page fault handler for mmap regions
static int
handle_mmap_fault(uint64 fault_addr, uint64 scause)
{
  struct proc *p = myproc();
  struct mmap_area *ma = NULL;

  // printf("handle_mmap_fault: addr=0x%lx scause=0x%lx\n", fault_addr, scause);

  // Find the valid mapping record that matches the fault address
  for (int i = 0; i < MAX_MMAP_AREA; i++) {
    if (mmap_areas[i].used && mmap_areas[i].p == p &&
        fault_addr >= mmap_areas[i].addr &&
        fault_addr < mmap_areas[i].addr + mmap_areas[i].length) {
      ma = &mmap_areas[i];
      // printf("handle_mmap_fault: found mapping at 0x%lx length=%d\n", 
      //        ma->addr, ma->length);
      break;
    }
  }
  if (ma == NULL) {
    // printf("handle_mmap_fault: no mapping found\n");
    return -1; // no mapping
  }

  // Write fault but no write permission
  if (scause == 15 && !(ma->prot & PROT_WRITE)) {
    // printf("handle_mmap_fault: write fault but no write permission\n");
    return -1;
  }

  // Page-aligned address
  uint64 va = PGROUNDDOWN(fault_addr);

  // If already mapped, consider handled
  pte_t *pte = walk(p->pagetable, va, 0);
  if (pte && (*pte & PTE_V)) {
    // printf("handle_mmap_fault: page already mapped\n");
    return 1;
  }

  // Allocate new physical page
  char *mem = kalloc();
  if (mem == NULL) {
    // printf("handle_mmap_fault: kalloc failed\n");
    return -1;
  }
  memset(mem, 0, PGSIZE); // fill with 0

  // File mapping: read content from file
  if (!(ma->flags & MAP_ANONYMOUS)) {
    uint64 file_off = ma->offset + (va - ma->addr);
    // printf("handle_mmap_fault: reading file offset=0x%lx\n", file_off);
    if (readi(ma->f->ip, 0, (uint64)mem, file_off, PGSIZE) < 0) {
      // printf("handle_mmap_fault: readi failed\n");
      kfree(mem);
      return -1;
    }
  }

  int perm = PTE_U | PTE_R | ((ma->prot & PROT_WRITE) ? PTE_W : 0); // set PTE flag
  if (mappages(p->pagetable, va, PGSIZE, (uint64)mem, perm) < 0) { // map page table
    // printf("handle_mmap_fault: mappages failed\n");
    kfree(mem);
    return -1;
  }
  sfence_vma(); // TLB invalidation
  // printf("handle_mmap_fault: mapped page at 0x%lx\n", va);
  return 1;
}

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  uint64 scause = r_scause();
  uint64 stval = r_stval(); // fault address
  struct proc *p = myproc();

  if ((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // Redirect traps to kernelvec
  w_stvec((uint64)kernelvec);

  // Save user PC
  p->trapframe->epc = r_sepc();

  if (scause == 8) {
    // syscall
    if (killed(p))
      exit(-1);
    p->trapframe->epc += 4;
    intr_on();
    syscall();
  } else if (devintr() != 0) {
    // device interrupt
  } else if ((scause == 13 || scause == 15) &&
             stval >= MMAPBASE && stval < MMAPBASE + 0x10000000UL) {
    // mmap page fault
    int ret = handle_mmap_fault(stval, scause);
    if (ret == 1) {
      // handled: go to usertrapret for proper return
      usertrapret();
      return;
    }
    // ret == -1: invalid access, kill the process
    setkilled(p);
  } else {
    // unexpected trap
    printf("usertrap(): unexpected scause 0x%lx pid=%d\n", scause, p->pid);
    printf("            sepc=0x%lx stval=0x%lx\n", r_sepc(), stval);
    setkilled(p);
  }

  if (killed(p))
    exit(-1);

  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to userret in trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    // interrupt or trap from an unknown source
    printf("scause=0x%lx sepc=0x%lx stval=0x%lx\n", scause, r_sepc(), r_stval());
    panic("kerneltrap");
  }


  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  struct proc *p = myproc();
  
  // If there is a currently running process, update EEVDF-related fields
  if(p != 0 && p->state == RUNNING) {
    p->runtime++;           // Increase actual runtime
    p->time_slice--;        // Decrease remaining time slice
    p->total_tick++;        // Increase total tick count
    
    // Update vruntime (apply the exact formula for EEVDF)
    uint64 delta_runtime = 1;  
    uint64 scaled_runtime = delta_runtime * 1024 * 1000 / p->weight;  // Use millitick units
    p->vruntime += scaled_runtime;

    // If time_slice is 0, update vdeadline and immediately yield
    if(p->time_slice <= 0) {
      p->vdeadline = p->vruntime + (5 * 1024 * 1000 / p->weight);  // Use millitick units
      p->time_slice = 5;  // Reset time slice
      release(&tickslock);  // Release tickslock before calling yield()
      yield();  // Yield CPU
      return;  // Return after yield()
    }
  }
  
  wakeup(&ticks);
  release(&tickslock);

  // ask for the next timer interrupt. this also clears
  // the interrupt request. 100000 is about a thousandth
  // of a second.
  w_stimecmp(r_time() + 100000);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if(scause == 0x8000000000000009L){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000005L){
    // timer interrupt.
    clockintr();
    return 2;
  } else {
    return 0;
  }
}

