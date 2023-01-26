#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64 sys_exit(void) {
  int n;
  if (argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0; // not reached
}

uint64 sys_getpid(void) { return myproc()->pid; }

uint64 sys_fork(void) { return fork(); }

uint64 sys_wait(void) {
  uint64 p;
  if (argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64 sys_sbrk(void) {
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;

  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64 sys_sleep(void) {
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (myproc()->killed) {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

#ifdef LAB_PGTBL
int sys_pgaccess(void) {
  uint64 address;
  if (argaddr(0, &address) < 0)
    return -1;

  int page_num;
  if (argint(1, &page_num) < 0)
    return -1;

  page_num = page_num > 32 ? 32 : page_num;

  uint64 buffer;
  if (argaddr(2, &buffer) < 0)
    return -1;

  pagetable_t pgtbl = myproc()->pagetable;
  pte_t *pte = walk(pgtbl, (uint64)address, 0);
  int mask = 0;
  for (int i = 0; i < page_num; ++i) {
    if (pte[i] & PTE_A) {
      mask |= 1 << i;
      pte[i] &= ~PTE_A;
    }
  }
  return copyout(pgtbl, buffer, (char *)&mask, sizeof(int));
}
#endif

uint64 sys_kill(void) {
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64 sys_uptime(void) {
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
