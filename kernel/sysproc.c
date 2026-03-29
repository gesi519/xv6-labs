#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

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
  acquire(&tickslock);
  ticks0 = ticks;
#ifdef LAB_TRAPS
  backtrace(); // 打印调用栈
#endif
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


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  return 0;
}
#endif

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

// lab trace 原型
int
sys_trace() {
  int mask;
  argint(0, &mask);
  myproc()->trace_mask = mask; // 保存掩码到 proc 结构
  return 0;
}

extern uint64 freemem_bytes(void); // 我们会在 kalloc.c 实现

uint64
sys_sysinfo(void)
{
    struct sysinfo info;
    struct proc *p = myproc();
    uint64 addr;

    argaddr(0, &addr);

    info.freemem = freemem_bytes();

    int n = 0;
    struct proc *cur;
    for(cur = proc; cur < &proc[NPROC]; cur++){
        acquire(&cur->lock);
        if(cur->state != UNUSED)
            n++;
        release(&cur->lock);
    }
    info.nproc = n;

    if(copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0)
        return -1;

    return 0;
}

uint64
sys_sigreturn(void) {
  struct proc *p = myproc();
  // 恢复用户态寄存器
  memmove(p->trapframe, &p->alarm_trapframe, sizeof(struct trapframe));
  p->alarm_ticks = 0; // 重置 alarm_ticks
  p->alarm_on = 0; // 关闭 alarm

  return p->trapframe->a0;
}

uint64
sys_sigalarm(void) {
  int ticks;
  uint64 handler;

  argint(0, &ticks);
  argaddr(1, &handler);

  struct proc *p = myproc();

  p->alarm_interval = ticks;
  p->alarm_handler = handler;
  p->alarm_ticks = 0;

  return 0;
}