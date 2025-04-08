#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "elf.h"
#include "kalloc.h" // Used when printing memory info

struct cpu cpus[NCPU];

// Array of weights according to nice values (nice=0~39)
static int nice_to_weight[40] = {
    88818,  // nice = 0
    71054,  // nice = 1
    56843,  // nice = 2
    45475,  // nice = 3
    36380,  // nice = 4
    29104,  // nice = 5
    23283,  // nice = 6
    18626,  // nice = 7
    14901,  // nice = 8
    11921,  // nice = 9
    9537,   // nice = 10
    7629,   // nice = 11
    6104,   // nice = 12
    4883,   // nice = 13
    3906,   // nice = 14
    3125,   // nice = 15
    2500,   // nice = 16
    2000,   // nice = 17
    1600,   // nice = 18
    1280,   // nice = 19
    1024,   // nice = 20
    819,    // nice = 21
    655,    // nice = 22
    524,    // nice = 23
    419,    // nice = 24
    336,    // nice = 25
    268,    // nice = 26
    215,    // nice = 27
    172,    // nice = 28
    137,    // nice = 29
    110,    // nice = 30
    88,     // nice = 31
    70,     // nice = 32
    56,     // nice = 33
    45,     // nice = 34
    36,     // nice = 35
    29,     // nice = 36
    23,     // nice = 37
    18,     // nice = 38
    15      // nice = 39
};


static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "SLEEPING",
  [RUNNABLE]  "RUNNABLE",
  [RUNNING]   "RUNNING",
  [ZOMBIE]    "ZOMBIE"
}; // Define the states array

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
      p->nice = 20;  // Set default nice value to 20
      p->weight = nice_to_weight[20];  // Set default weight value
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  // EEVDF scheduler related fields initialization
  p->runtime = 0;        // Initialize actual runtime
  p->vruntime = 0;       // Initialize virtual runtime
  p->vdeadline = 0;      // Initialize virtual deadline
  p->time_slice = 5;     // Initialize time slice (5 ticks)
  p->weight = 1024;      // Default weight (when nice=20)
  p->total_tick = 0;     // Initialize total tick count

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  // EEVDF scheduler related fields initialization
  acquire(&np->lock);
  np->nice = p->nice;           // Copy parent's nice value
  np->weight = p->weight;       // Copy parent's weight value
  np->runtime = 0;              // Initialize actual runtime to 0
  np->vruntime = p->vruntime;   // Copy parent's vruntime value
  np->vdeadline = np->vruntime + (5 * 1024 / np->weight); // Copy parent's vdeadline value
  np->time_slice = 5;           // Initialize time slice
  np->total_tick = 0;           // Initialize total tick count to 0
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// EEVDF data collection function
void
collect_eevdf_data(struct eevdf_data *data)
{
  struct proc *p;
  struct proc *current = myproc();
  
  data->min_vruntime = (uint64)-1;  // Initialize to maximum value
  data->sum_weight = 0;
  data->sum_weighted_diff = 0;

  // Iterate through all processes at once to collect data
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == RUNNING || p->state == RUNNABLE || p == current) {
      // Calculate sum_weight and min_vruntime
      data->sum_weight += p->weight;
      if(p->vruntime < data->min_vruntime)
        data->min_vruntime = p->vruntime;
    }
    release(&p->lock);
  }

  // If min_vruntime is still maximum value (i.e., no RUNNING/RUNNABLE processes)
  // Set it to 0 for eligibility calculation
  if(data->min_vruntime == (uint64)-1)
    data->min_vruntime = 0;

  // Second iteration: calculate sum_weighted_diff
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == RUNNING || p->state == RUNNABLE || p == current) {
      data->sum_weighted_diff += (p->weight * (p->vruntime - data->min_vruntime));
    }
    release(&p->lock);
  }
}

// EEVDF eligibility calculation function
int
is_eligible(struct proc *p, struct eevdf_data *data)
{
  if(data->sum_weight == 0)
    return 1;

  uint64 p_diff = (p->vruntime - data->min_vruntime) * data->sum_weight;
  uint64 avg_diff = data->sum_weighted_diff;
  
  return avg_diff >= p_diff;
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c;
  struct proc *best = 0;  // Variable to store the best process
  struct eevdf_data data;

  c = mycpu();
  c->proc = 0;

  for(;;){
    // Enable interrupts on this processor.
    intr_on();

    // Collect EEVDF data
    collect_eevdf_data(&data);

    // Traverse all processes to find the best process
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Calculate eligibility directly using the formula
        int eligible = 1;  // Default to eligible if sum_weight is 0
        if(data.sum_weight > 0) {
          uint64 p_diff = (p->vruntime - data.min_vruntime) * data.sum_weight;
          uint64 avg_diff = data.sum_weighted_diff;
          eligible = (avg_diff >= p_diff);
        }
        
        if(eligible) {
          if(!best || p->vdeadline < best->vdeadline) {
            // If there is already a selected process, release the lock
            if(best)
              release(&best->lock);
            best = p; // Select the best process
          } else {
            release(&p->lock);
          }
        } else {
          release(&p->lock);
        }
      } else {
        release(&p->lock);
      }
    }

    // If the best process is selected
    if(best) {
      p = best;
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&p->lock);
      best = 0;
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  
  // Update vdeadline when time_slice is exhausted
  if(p->time_slice <= 0) {
    p->vdeadline = p->vruntime + (5 * 1024 / p->weight);  // Add the virtual runtime for the next 5 ticks
    p->time_slice = 5;  // Reset time slice
  }
  
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    fsinit(ROOTDEV);

    first = 0;
    // ensure other cores see first=0.
    __sync_synchronize();
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  // Wake up processes in sleep state and update only time_slice and vdeadline
  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
        
        // Reset time_slice and recalculate vdeadline
        // vruntime remains unchanged
        p->time_slice = 5;  // Initialize time slice
        p->vdeadline = p->vruntime + (5 * 1024 / p->weight);  // Recalculate vdeadline
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}

int
getnice(int pid)
{
  struct proc *p;
  int nice = -1; // Initialize nice to -1

  for(p = proc; p < &proc[NPROC]; p++) { // Iterate through all processes
    acquire(&p->lock);
    if(p->pid == pid) { // Check if the process ID matches
      nice = p->nice; // Get the nice value
      release(&p->lock);
      return nice; // Return the nice value
    }
    release(&p->lock);
  }
  return -1; // Return -1 if the process ID is not found
}

int
setnice(int pid, int value)
{
  struct proc *p;
  int old_nice;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->pid == pid) {
      old_nice = p->nice;
      p->nice = value;
      p->weight = nice_to_weight[value];  // Update weight value
      p->vdeadline = p->vruntime + (5 * 1024 / p->weight);  // Recalculate vdeadline
      release(&p->lock);
      return old_nice;
    }
    release(&p->lock);
  }
  return -1;
}

void
meminfo(void)
{
  struct run *r; // pointer that points to the free memory
  uint64 free_memory = 0; // variable to store the free memory size
  
  acquire(&kmem.lock); // acquire the lock
  r = kmem.freelist; // assign the free memory to r
  while(r) { // while r is not null
    free_memory += PGSIZE; // add the page size to the free memory
    r = r->next; // move to the next free memory
  }
  release(&kmem.lock); // release the lock
  
  printf("Available memory: %ld bytes\n", free_memory); // print the free memory
}

int
waitpid(int pid, int *status)
{
  struct proc *np;
  int havekids; // check if the process has children
  struct proc *p = myproc();

  acquire(&wait_lock); // acquire the lock

  for(;;) {
    
    havekids = 0;

    for(np = proc; np < &proc[NPROC]; np++){ // scan through the table
      if(np->state == UNUSED)
        continue; // if the process is unused, skip it
      
      if(np->parent != p)
        continue; // if the process is not the parent, skip it
      
      if(np->pid != pid)
        continue; // if the process id is not the same, skip it
      havekids = 1; // if the process has children, set havekids to 1
      
      if(np->state == ZOMBIE){ // if the process is a zombie
        if(status != 0 && copyout(p->pagetable, (uint64)status, (char *)&np->xstate,
                                sizeof(np->xstate)) < 0) { // copy the exit status to the parent
          release(&wait_lock);
          return -1; // if the copyout fails, return -1
        }
        freeproc(np); // free the process
        release(&wait_lock);
        return 0;  // Return 0 on successful termination
      }
    }

    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;  // Return -1 if process doesn't exist or no permission
    }
    
    sleep(p, &wait_lock);  // wait for the process to exit
  }  
}

void
ps(int pid)
{
  struct proc *p;
  char *state;

  // Print header
  printf("=== TEST START ===\n");
  printf("name\tpid\tstate\t\tpriority\truntime/weight\truntime\t\tvruntime\tvdeadline\tis_eligible\ttick %u\n", ticks * 1000);

  // Collect EEVDF data
  struct eevdf_data data;
  collect_eevdf_data(&data);

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p->state == UNUSED)
      continue;
    
    if(pid != 0 && p->pid != pid)
      continue;

    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    
    uint64 milli_runtime_per_weight = p->weight > 0 ? (p->runtime * 1000) / p->weight : 0;

    // Convert to millitick units (multiply by 1000)
    uint64 milli_runtime = p->runtime * 1000;
    uint64 milli_vruntime = p->vruntime * 1000;
    uint64 milli_vdeadline = p->vdeadline * 1000;

    if(p->state == RUNNING) {
      printf("%s\t%d\t%s\t\t%d\t\t%ld\t\t%ld\t\t%ld\t\t%ld\t\t%s\n",
            p->name, p->pid, state, p->nice,
            milli_runtime_per_weight, milli_runtime, milli_vruntime,
            milli_vdeadline, is_eligible(p, &data) ? "true" : "false");
    }
    else if(p->state == ZOMBIE) {
      printf("%s\t%d\t%s\t\t%d\t\t%ld\t\t%ld\t\t%ld\t\t%ld\t\t%s\n",
            p->name, p->pid, state, p->nice,
            milli_runtime_per_weight, milli_runtime, milli_vruntime,
            milli_vdeadline, is_eligible(p, &data) ? "true" : "false");
    }
    else {
      printf("%s\t%d\t%s\t%d\t\t%ld\t\t%ld\t\t%ld\t\t%ld\t\t%s\n",
            p->name, p->pid, state, p->nice,
            milli_runtime_per_weight, milli_runtime, milli_vruntime,
            milli_vdeadline, is_eligible(p, &data) ? "true" : "false");
    }
  }
}
