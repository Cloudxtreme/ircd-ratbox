/*
 * balloc.c - Based roughly on Wohali's old block allocator.
 *
 * Mangled by Aaron Sethman <androsyn@ratbox.org>
 * Below is the original header found on this file
 *
 * File:   blalloc.c
 * Owner:  Wohali (Joan Touzet)
 *
 *
 * $Id$
 */

#define WE_ARE_MEMORY_C
#include "setup.h"

#ifndef NOBALLOC
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include "ircd_defs.h"          /* DEBUG_BLOCK_ALLOCATOR */
#include "ircd.h"
#include "memory.h"
#include "balloc.h"
#include "irc_string.h"
#include "tools.h"
#include "s_log.h"
#include "client.h"
#include "fdlist.h"

#include <string.h>
#include <stdlib.h>
static int newblock(BlockHeap * bh);

#ifdef HAVE_MMAP
#include <sys/mman.h>

static inline void free_block(void *ptr, size_t size)
{
  munmap(ptr, size);
}

#ifndef MAP_ANON
static int zero_fd = -1;
void initBlockHeap(void)
{
  zero_fd = open("/dev/zero", O_RDWR);

  if (zero_fd < 0)
    outofmemory();
  fd_open(zero_fd, FD_FILE, "Anonymous mmap()");
}

static inline void *get_block(size_t size)
{
  return (mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, zero_fd, 0));
}
#else /* MAP_ANON */

void initBlockHeap(void)
{
  return;
}
static inline void *get_block(size_t size)
{
  return (mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0));
}

#endif /* MAP_ANON */

#else  /* HAVE_MMAP */
/* Poor bastards don't even have mmap() */
static inline void *get_block(size_t size)
{
  return(malloc(size));
}

static inline void free_block(void *ptr, size_t unused)
{
  free(ptr);
}

void initBlockHeap()
{
  return;
}
#endif /* HAVE_MMAP */



/* ************************************************************************ */
/* FUNCTION DOCUMENTATION:                                                  */
/*    newblock                                                              */
/* Description:                                                             */
/*    mallocs a new block for addition to a blockheap                       */
/* Parameters:                                                              */
/*    bh (IN): Pointer to parent blockheap.                                 */
/* Returns:                                                                 */
/*    0 if successful, 1 if not                                             */
/* ************************************************************************ */

static int newblock(BlockHeap * bh)
{
    MemBlock *newblk;
    Block *b;
    int i;
    void *offset;

    /* Setup the initial data structure. */
    b = (Block *) MyMalloc(sizeof(Block));
    if (b == NULL)
        return 1;
    b->freeElems = bh->elemsPerBlock;
    b->free_list.head = b->free_list.tail = NULL;
    b->used_list.head = b->used_list.tail = NULL;
    b->next = bh->base;

    b->alloc_size = (bh->elemsPerBlock + 1) * (bh->elemSize + sizeof(MemBlock));

    b->elems = get_block(b->alloc_size);
    if (b->elems == NULL)
      {
        return 1;
      }
    offset = b->elems;
    /* Setup our blocks now */
    for (i = 0; i < bh->elemsPerBlock; i++)
      {
        void *data;
        newblk = (void *)offset;
        newblk->block = b;
        data = offset + sizeof(MemBlock);
        newblk->block = b;
        newblk->data = data;
        dlinkAdd(data, &newblk->self, &b->free_list);
        offset = (unsigned char *)((unsigned char *)offset + bh->elemSize + sizeof(MemBlock));
      }

    ++bh->blocksAllocated;
    bh->freeElems += bh->elemsPerBlock;
    bh->base = b;

    return 0;
}


/* ************************************************************************ */
/* FUNCTION DOCUMENTATION:                                                  */
/*    BlockHeapCreate                                                       */
/* Description:                                                             */
/*   Creates a new blockheap from which smaller blocks can be allocated.    */
/*   Intended to be used instead of multiple calls to malloc() when         */
/*   performance is an issue.                                               */
/* Parameters:                                                              */
/*   elemsize (IN):  Size of the basic element to be stored                 */
/*   elemsperblock (IN):  Number of elements to be stored in a single block */
/*         of memory.  When the blockheap runs out of free memory, it will  */
/*         allocate elemsize * elemsperblock more.                          */
/* Returns:                                                                 */
/*   Pointer to new BlockHeap, or NULL if unsuccessful                      */
/* ************************************************************************ */
BlockHeap *BlockHeapCreate(size_t elemsize, int elemsperblock)
{
    BlockHeap *bh;
    assert(elemsize > 0 && elemsperblock > 0);

    /* Catch idiotic requests up front */
    if ((elemsize <= 0) || (elemsperblock <= 0))
      {
        outofmemory();          /* die.. out of memory */
      }

    /* Allocate our new BlockHeap */
    bh = (BlockHeap *) MyMalloc(sizeof(BlockHeap));
    if (bh == NULL)
      {
        outofmemory();          /* die.. out of memory */
      }

#ifdef MEMDEBUG
    elemsize += sizeof(MemoryEntry);
#endif

    bh->elemSize = elemsize;
    bh->elemsPerBlock = elemsperblock;
    bh->blocksAllocated = 0;
    bh->freeElems = 0;
    bh->base = NULL;

    /* Be sure our malloc was successful */
    if (newblock(bh))
      {
        MyFree(bh);
        outofmemory();          /* die.. out of memory */
      }

    if (bh == NULL)
      {
        outofmemory();          /* die.. out of memory */
      }

    return bh;
}

/* ************************************************************************ */
/* FUNCTION DOCUMENTATION:                                                  */
/*    BlockHeapAlloc                                                        */
/* Description:                                                             */
/*    Returns a pointer to a struct within our BlockHeap that's free for    */
/*    the taking.                                                           */
/* Parameters:                                                              */
/*    bh (IN):  Pointer to the Blockheap.                                   */
/* Returns:                                                                 */
/*    Pointer to a structure (void *), or NULL if unsuccessful.             */
/* ************************************************************************ */

void *_BlockHeapAlloc(BlockHeap * bh)
{
    Block *walker;
    dlink_node *new_node;

    assert(bh != NULL);
    if (bh == NULL)
        return ((void *) NULL);

    if (bh->freeElems == 0)
      {   /* Allocate new block and assign */
        /* newblock returns 1 if unsuccessful, 0 if not */

        if (newblock(bh))
	  {
            return ((void *) NULL);
	  }
        walker = bh->base;
        walker->freeElems--;
        bh->freeElems--;
        new_node = walker->free_list.head;
        dlinkDelete(new_node, &walker->free_list);
        dlinkAdd(new_node->data, new_node, &walker->used_list);
        assert(new_node->data != NULL);
        return (new_node->data);
      }

    for (walker = bh->base; walker != NULL; walker = walker->next)
      {
        if (walker->freeElems > 0)
	  {
            bh->freeElems--;
            walker->freeElems--;
            new_node = walker->free_list.head;
            dlinkDelete(new_node, &walker->free_list);
            dlinkAdd(new_node->data, new_node, &walker->used_list);
            assert(new_node->data != NULL);
            return (new_node->data);
	  }
      }
    assert(0 == 1);
    return ((void *) NULL);     /* If you get here, something bad happened ! */
}


/* ************************************************************************ */
/* FUNCTION DOCUMENTATION:                                                  */
/*    BlockHeapFree                                                         */
/* Description:                                                             */
/*    Returns an element to the free pool, does not free()                  */
/* Parameters:                                                              */
/*    bh (IN): Pointer to BlockHeap containing element                      */
/*    ptr (in):  Pointer to element to be "freed"                           */
/* Returns:                                                                 */
/*    0 if successful, 1 if element not contained within BlockHeap.         */
/* ************************************************************************ */
int _BlockHeapFree(BlockHeap * bh, void *ptr)
{
    Block *block;
    struct MemBlock *memblock;
    
    assert(bh != NULL);
    assert(ptr != NULL);

    if (bh == NULL)
      {

        ilog(L_NOTICE, "balloc.c:BlockHeapFree() bh == NULL");
        return 1;
      }

    if (ptr == NULL)
      {
        ilog(L_NOTICE, "balloc.BlockHeapFree() ptr == NULL");
        return 1;
      }

    memblock = ptr - sizeof(MemBlock);
    assert(memblock->block != NULL);
    /* XXX: Should check that the block is really our block */
    block = memblock->block;
    bh->freeElems++;
    block->freeElems++;
    dlinkDelete(&memblock->self, &block->used_list);
    dlinkAdd(memblock->data, &memblock->self, &block->free_list);
    return 0;
}

/* ************************************************************************ */
/* FUNCTION DOCUMENTATION:                                                  */
/*    BlockHeapGarbageCollect                                               */
/* Description:                                                             */
/*    Performs garbage collection on the block heap.  Any blocks that are   */
/*    completely unallocated are removed from the heap.  Garbage collection */
/*    will never remove the root node of the heap.                          */
/* Parameters:                                                              */
/*    bh (IN):  Pointer to the BlockHeap to be cleaned up                   */
/* Returns:                                                                 */
/*   0 if successful, 1 if bh == NULL                                       */
/* ************************************************************************ */
int BlockHeapGarbageCollect(BlockHeap * bh)
{
    Block *walker, *last;

    if (bh == NULL)
        return 1;

    if (bh->freeElems < bh->elemsPerBlock || bh->blocksAllocated == 1)
      {
        /* There couldn't possibly be an entire free block.  Return. */
        return 0;
      }

    last = NULL;
    walker = bh->base;

    while (walker)
      {
        if (walker->freeElems == bh->elemsPerBlock)
	  {
            free_block(walker->elems, walker->alloc_size);
            if (last)
	      {
                last->next = walker->next;
                MyFree(walker);
                walker = last->next;
	      }
	    else
	      {
                bh->base = walker->next;
                MyFree(walker);
                walker = bh->base;
	      }
            bh->blocksAllocated--;
            bh->freeElems -= bh->elemsPerBlock;
	  }
	else
	  {
            last = walker;
            walker = walker->next;
	  }
      }
    return 0;
}

/* ************************************************************************ */
/* FUNCTION DOCUMENTATION:                                                  */
/*    BlockHeapDestroy                                                      */
/* Description:                                                             */
/*    Completely free()s a BlockHeap.  Use for cleanup.                     */
/* Parameters:                                                              */
/*    bh (IN):  Pointer to the BlockHeap to be destroyed.                   */
/* Returns:                                                                 */
/*   0 if successful, 1 if bh == NULL                                       */
/* ************************************************************************ */
int BlockHeapDestroy(BlockHeap * bh)
{
    Block *walker, *next;

    if (bh == NULL)
        return 1;

    for (walker = bh->base; walker != NULL; walker = next)
      {
        next = walker->next;
        free_block(walker->elems, walker->alloc_size);
        MyFree(walker);
      }
    MyFree(bh);
    return 0;
}
#endif
