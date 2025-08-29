# Lab pgtbl
这一部分我基本没有听懂...我鸽了半个月再回来复习, 结果还是没懂...没准是我真没有基础吧, 我直接跑去看[OSETP](https://pages.cs.wisc.edu/~remzi/OSTEP/)去了, 硬啃了两天速通到虚拟化部分, 回来才真正知道该怎么做。

以下摘抄于源实验提示:

```md
Some hints:
- You can perform the mapping in proc_pagetable() in kernel/proc.c.
- Choose permission bits that allow userspace to only read the page.
- You may find that mappages() is a useful utility.
- Don't forget to allocate and initialize the page in allocproc().
- Make sure to free the page in freeproc().
```

## `ugetpid`: Speed up system calls

先瞄一眼调用的API长啥样:

```ulib.c
int
ugetpid(void)
{
  struct usyscall *u = (struct usyscall *)USYSCALL;
  return u->pid;
}
```
```
```

这里USYSCALL的定义是某个固定的user va值, 结合提示, 思路是在每个进程创建时在内核多使用一个page, 并把这个page映射到user pagetable, 以达到避免陷入内核而加速syscall的目的.

(每个进程都多使用一个page是不是有点浪费? 就这一点我考虑到各进程都share同一个usyscall的page, 结果是会影响上下文切换的速度和提高实现的复杂度. 我认为上下文的切换速度应该更重要一些, 所以这个想法不行.)

因为是这个进程管理的page, 所以应该将这个page的地址存储在struct proc中:

```c proc.h
struct proc {
  struct spinlock lock;

  enum procstate state;
  void *chan;
  int killed;
  int xstate;
  int pid;

  struct proc *parent;

  uint64 kstack;
  uint64 sz;
  uint64 usyscall;             // Lab pgtbl, Speed up system calls: va of USYSCALL
  pagetable_t pagetable;
  struct trapframe *trapframe;
  struct context context;
  struct file *ofile[NOFILE];
  struct inode *cwd;
  char name[16];
};
```

这里因为众所周知的头文件引用原因, 我把数据类型声明成了`uint64`而不是在`memlayout.h`给的`struct usyscall`(我linter都瘸了, 我也懒得去检查).

接下来就是按照提示去做, 在`proc_pagetable()`添加map:

```proc.c
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // Lab pgtbl, Speed up system calls: map the USYSCALL page.
  if(mappages(pagetable, USYSCALL, PGSIZE,
              p->usyscall, PTE_R | PTE_U) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}
```

在`allocproc()`分配空间并初始化:

```proc.c
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

  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Lab pgtbl, Speed up system calls: Allocate a USYSCALL page
  if((p->usyscall = (uint64)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  ((struct usyscall*)(p->usyscall))->pid = p->pid;

  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}
```

在`freeproc()`释放:

```proc.c
static void
freeproc(struct proc *p)
{
  // Lab pgtbl, Speed up system calls: Free USYSCALL page.
  if(p->usyscall)
    kfree((void*)p->usyscall);
  p->usyscall = 0;
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
```

这下就完成了. 使用`./grade-lab-pgtbl ugetpid`测试.

结果报`panic: freewalk: leaf`, 提示居然缺斤少两, 没有提醒我map完之后释放. 需要在`proc.c`的`proc_freepagetable()`补上.

```proc.c
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, USYSCALL, 1, 0);  // Lab pgtbl, Speed up system call: unmap USYSCALL
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}
```

## `vmprint`: Print a page table

第一步直接照做, 在`exec.c:exec()`中插入这一行`if(p->pid==1) vmprint(p->pagetable);`.

接下来是`vmprint()`的声明和定义.

```c defs.h
// vm.c
void            kvminit(void);
void            kvminithart(void);
void            kvmmap(pagetable_t, uint64, uint64, uint64, int);
int             mappages(pagetable_t, uint64, uint64, uint64, int);
pagetable_t     uvmcreate(void);
void            uvminit(pagetable_t, uchar *, uint);
uint64          uvmalloc(pagetable_t, uint64, uint64);
uint64          uvmdealloc(pagetable_t, uint64, uint64);
int             uvmcopy(pagetable_t, pagetable_t, uint64);
void            uvmfree(pagetable_t, uint64);
void            uvmunmap(pagetable_t, uint64, uint64, int);
void            uvmclear(pagetable_t, uint64);
void            vmprint(pagetable_t);            // Lab pgtbl, Print a page table.
uint64          walkaddr(pagetable_t, uint64);
int             copyout(pagetable_t, uint64, char *, uint64);
int             copyin(pagetable_t, char *, uint64, uint64);
int             copyinstr(pagetable_t, char *, uint64, uint64);
```

这个函数理所当然应该定义在`vm.c`中. 没看懂这里`defs.h`的摆放顺序, 好像是先按照字典序, 然后这几个copy又没有按照字典序.

定义我决定放在两个有关`walk`的函数之后:

```vm.c
// Lab pgtbl, Print a page table: vmprint definition.
void vmprint_helper(pagetable_t pagetable, int level)
{
  if (level < 0)
    return;

  static char* prefix[]={".. .. ..", ".. ..", ".."};

  // page entry number 512(1 << 9)
  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];
    if (pte & PTE_V) {
      printf("%s%d: pte %p pa %p\n", prefix[level], i, pte, PTE2PA(pte));
      vmprint_helper((pagetable_t)PTE2PA(pte), level-1);
    }
  }
}

void vmprint(pagetable_t pagetable)
{
  int level = 2; // three level page directories

  printf("page table %p\n", pagetable);
  vmprint_helper(pagetable, level);
}
```

测试`./grade-lab-pgtbl print`.

## `pgaccess`: Detecting which pages have been accessed

实验已经帮我们定义好了函数原型`int pgaccess(void *base, int len, void *mask)`和一些麻烦的注册, 只需要专注于实现就可以了(从`user/pgtbltest.c`和`./grade-lab-pgtbl pgaccess`可以直接运行及相关内容可以看出).

[ ] TODO: not complemented
