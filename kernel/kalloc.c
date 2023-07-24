// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run
{
  struct run *next;
};

struct
{
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void kinit()
{
  /*
  initlock(&kmem.lock, "kmem");
  freerange(end, (void *)PHYSTOP);
  */
  initlock(&kmem[0].lock, "kmem0");
  initlock(&kmem[1].lock, "kmem1");
  initlock(&kmem[2].lock, "kmem2");
  initlock(&kmem[3].lock, "kmem3");
  initlock(&kmem[4].lock, "kmem4");
  initlock(&kmem[5].lock, "kmem5");
  initlock(&kmem[6].lock, "kmem6");
  initlock(&kmem[7].lock, "kmem7");

  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
  {
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  int cpu_id = cpuid();

  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmem[cpu_id].lock);
  r->next = kmem[cpu_id].freelist;
  kmem[cpu_id].freelist = r;
  release(&kmem[cpu_id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  int cpu_id = cpuid();

  acquire(&kmem[cpu_id].lock);
  r = kmem[cpu_id].freelist;
  if (r)
    kmem[cpu_id].freelist = r->next;
  else
  {
    for (int i = 0; i < NCPU; i++)
    {
      if (i == cpu_id)
        continue;
      if (kmem[i].freelist == 0)
      {
        continue;
      }
      acquire(&kmem[i].lock);

      r = kmem[i].freelist;
      if (r)
      {
        // 可分配
        kmem[i].freelist = r->next;
        release(&kmem[i].lock);
        break;
      }
      else
      {
        release(&kmem[i].lock);
        continue;
      }
    }
  }
  release(&kmem[cpu_id].lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}
