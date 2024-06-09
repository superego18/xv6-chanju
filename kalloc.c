// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

#include "stddef.h"
#include "stdint.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;


// struct phys_page_refc {
//   struct spinlock lock;
//   char *p_array[4];// kinit1_st, kinit1_fi, kinit2_st, kinit2_fi
//   int p_array_idx;
// } ppr;

// void
// pprinit() {
//   initlock(&ppr.lock, "ppr");
//   ppr.p_array_idx = 0;
// }


// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}


// void
// freerange(void *vstart, void *vend)
// {
//   char *p;
//   p = (char*)PGROUNDUP((uint)vstart);
//   int first = 0;
//   for(; p + PGSIZE <= (char*)vend; p += PGSIZE) {
//     if (first == 0) {
//       first = 1;
//       ppr.p_array[ppr.p_array_idx] = p;
//       ppr.p_array_idx += 1;
//     }
//     kfree(p);
//     ppr.p_array[ppr.p_array_idx] = p;
//     ppr.p_array_idx += 1;
//   }
// }


void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE) {
    kfree(p);
  }
}
//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

// ############################################################## //
// for project04

#define PAGE_NUM 58000
struct run *page_array[PAGE_NUM];
int ref_array[PAGE_NUM];

void save_freelist() {
    struct run *current = kmem.freelist;
    int idx = 0;
    while (current != NULL) {
      page_array[idx] = current;
      ref_array[idx] = 0;
      current = current->next;
      idx += 1;
    }
}

// for check
void print_saved_freelist() {
    cprintf("page_array[0]: %p\n", page_array[0]);
    cprintf("ref_array[0]: %d\n", ref_array[0]); // %d for int
    cprintf("page_array[56887]: %p\n", page_array[56887]);
    cprintf("ref_array[56887]: %d\n", ref_array[56887]); // %d for int
    cprintf("page_array[56888]: %p\n", page_array[56888]);
    cprintf("ref_array[56888]: %d\n", ref_array[56888]); // %d for int
}

void print_freelist() {
    if (kmem.freelist == NULL) {
        cprintf("Freelist is empty.\n");
        return;
    }
    struct run *current = kmem.freelist;
    struct run *prev_current = kmem.freelist;
    cprintf("Freelist elements:\n");
    int cnt= 0;
    while (current != NULL) {
      if (cnt == 0) {
        cprintf("%p\n", (void *)current);
      }
      prev_current = current;
      current = current->next;
      cnt += 1;
    }
    cprintf("%p\n", (void *)prev_current);
    cprintf("%d\n", cnt);
}


// void print_ppr_array() {
//     int i;
//     for (i = 0; i < 4; i++) {
//         cprintf("p_array[%d]: %p\n", i, ppr.p_array[i]);
//     }
// }


int binary_search(struct run *target) {
    int st_idx = 0;
    int fi_idx = PAGE_NUM - 1;
    while (st_idx <= fi_idx) {
        int search_idx = (st_idx + fi_idx) / 2;
        if (page_array[search_idx] == target) {
            return search_idx;
        } else if (page_array[search_idx] < target) {
            st_idx = search_idx + 1;
        } else {
            fi_idx = search_idx - 1;
        }
    }
    return -1;
}

void incr_refc(uint physical_address) {
  struct run *pointer = (struct run *)(uintptr_t)physical_address;
  int idx = binary_search(pointer);
  if (idx == -1) {
    return -1;
  };
  ref_array[idx] += 1;
}

void decr_refc(uint physical_address) {
  struct run *pointer = (struct run *)(uintptr_t)physical_address;
  int idx = binary_search(pointer);
  if (idx == -1) {
    return -1;
  };
  ref_array[idx] -= 1;
}

int get_refc(uint physical_address) {
  struct run *pointer = (struct run *)(uintptr_t)physical_address;
  int idx = binary_search(pointer);
  if (idx == -1) {
    return -1;
  };
  return ref_array[idx];
}

