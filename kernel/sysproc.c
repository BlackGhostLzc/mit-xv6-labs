#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  // 获取第 a0 这个参数，写入变量n中
  if (argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0; // not reached
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
  if (argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
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

  if (argint(0, &pid) < 0)
    return -1;
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
sys_trace(void)
{
  // 这是在内核态了,如何获取参数呢？
  // 需要的参数在寄存器中(获取mask),也就是寄存器a0
  int mask;
  // int argint(int n, int *ip)
  if (argint(0, &mask) < 0)
  {
    return -1;
  }
  myproc()->mask = mask;
  return 0;
}

uint64
sys_sysinfo(void)
{
  // 有一个参数，表示 sysinfo 结构体的地址
  uint64 addr;
  if (argaddr(0, &addr) < 0)
  {
    return -1;
  }
  // freemem    还剩下多少个字节(物理内存)
  // nproc      number of processes whose state is not UNUSED
  uint64 np;
  uint64 fm;
  np = cal_proc_unused();
  fm = cal_freemem();
  struct proc *p = myproc();
  struct sysinfo tmp;
  tmp.freemem = fm;
  tmp.nproc = np;
  if (copyout(p->pagetable, addr, (char *)(&tmp), sizeof(tmp)) < 0)
  {
    return -1;
  }
  return 0;
}
