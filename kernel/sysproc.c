#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "kalloc.h"

extern struct proc proc[NPROC];

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
  struct proc *p;

  argint(0, &pid);
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->pid == pid) {
      int nice = p->nice;
      release(&p->lock);
      return nice;
    }
    release(&p->lock);
  }
  return -1;
}

uint64
sys_setnice(void)
{
  int pid, value;
  struct proc *p;

  argint(0, &pid);
  argint(1, &value);

  // nice 값의 유효 범위 검사 (0~39)
  if (value < 0 || value > 39)
    return -1;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->pid == pid) {
      p->nice = value;
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

uint64
sys_ps(void)
{
  int pid;
  struct proc *p;

  argint(0, &pid);

  static char *states[] = {
    [UNUSED]    "unused",
    [USED]      "used",
    [SLEEPING]  "SLEEPING",
    [RUNNABLE]  "RUNNABLE",
    [RUNNING]   "RUNNING",
    [ZOMBIE]    "ZOMBIE"
  };

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p->state == UNUSED)
      continue;
    
    // pid가 0이면 모든 프로세스 출력, 아니면 특정 프로세스만 출력
    if(pid != 0 && p->pid != pid)
      continue;

    if(p == proc) {  // 첫 번째 프로세스일 때만 헤더 출력
      printf("name      pid     state     priority\n");
    }

    acquire(&p->lock);
    printf("%s", p->name);
    for(int i = strlen(p->name); i < 10; i++) printf(" ");
    printf("%d", p->pid);
    for(int i = 1; i < 8; i++) printf(" ");
    printf("%s", states[p->state]);
    for(int i = strlen(states[p->state]); i < 10; i++) printf(" ");
    printf("%d\n", p->nice);
    release(&p->lock);
  }

  return 0;
}

uint64
sys_meminfo(void)
{
  acquire(&kmem.lock);
  struct run *r = kmem.freelist;
  uint64 free_memory = 0;
  while(r) {
    free_memory += PGSIZE;
    r = r->next;
  }
  release(&kmem.lock);
  printf("Available memory: %ld bytes\n", free_memory);
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
