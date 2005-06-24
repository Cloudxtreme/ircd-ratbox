/* 
 * memdebug.c
 * 
 * Used to track memory usage in an application.
 * Every allocation/reallocation/deallocation should be
 * reported using DbgMem* functions. 
 * Whenever a check is required, enumeration is initialized,
 * then all known allocated blocks should be reported using DbgMemEnum
 * and finally enumeration is finished. All unreported or double 
 * reported pointers are reported as errors.
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include "s_log.h"

#ifdef DEBUGMEM

#ifndef DEBUGMEM_TABLE_BLOCKSIZE
#define DEBUGMEM_TABLE_BLOCKSIZE 1024
#endif

#ifndef DEBUGMEM_FILENAME_LENGTH
#define DEBUGMEM_FILENAME_LENGTH 14
#endif

#define DBGMEM_REPORT_ERROR(x) log(L_DEBUG, "%s", x)
#define DBGMEM_ENUM 0x80000000

/* Quick'n simple hash of a pointer, using bits 3-11 */
#define TABLE_INDEX(x) (((int) x >> 3) & 0xFF)

/* MemAllocEntry used to record information on every allocation */
struct MemAllocEntry {
  char file [DEBUGMEM_FILENAME_LENGTH];
  int line, flags;
  size_t size;
  void * ptr;
};

/* A block of MemAllocEntries */
struct MemAllocTable {
  struct MemAllocTable * next;
  struct MemAllocEntry entries[DEBUGMEM_TABLE_BLOCKSIZE];
  int UsedEntries;
};


/* Toplevel MemAllocTable * array, 256 pointers */
struct MemAllocTable * MemTable[256] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


/* Find a free MemAllocEntry for ptr */
void NeedFreeEntry(void * ptr, struct MemAllocTable ** table, int * index) {
  int TableNdx = TABLE_INDEX(ptr);
  if (!((*table) = MemTable[TableNdx])) {
    MemTable[TableNdx] = (struct MemAllocTable *) malloc(sizeof(struct MemAllocTable));
    memset(MemTable[TableNdx], 0, sizeof(struct MemAllocTable));
    (*table) = MemTable[TableNdx];
  }
  while ( (*table)->UsedEntries == DEBUGMEM_TABLE_BLOCKSIZE) {
    if ( (*table)->next ) 
      (*table) = (*table) -> next;
    else {
      (*table)->next = (struct MemAllocTable *) malloc(sizeof(struct MemAllocTable));
      (*table) = (*table)->next;
      memset( (*table), 0, sizeof(struct MemAllocTable));
    }
  }
  (*table)->UsedEntries++;
  for ((*index)=0; ((*index) < DEBUGMEM_TABLE_BLOCKSIZE) && ((*table)->entries[*index].ptr); (*index)++) ;
  if ((*index) >= DEBUGMEM_TABLE_BLOCKSIZE)
    exit(1);
};


/* Find the MemAllocEntry referencing ptr. Return value indicates whether found */
int FindEntry(void * ptr, struct MemAllocTable ** table, int * index) {
  int TableNdx = TABLE_INDEX(ptr), i;
  struct MemAllocTable * tbl;

  if (!(tbl=MemTable[TableNdx])) 
    return 0;
  
  while (tbl) {
    for (i=0;i<DEBUGMEM_TABLE_BLOCKSIZE;i++) 
      if (tbl->entries[i].ptr == ptr) {
	*table = tbl;
	*index = i;
	return 1;
      }
    tbl=tbl->next;
  };
  return 0;
};

/* Add an allocation to MemTable */
void DbgMemAlloc(char * file, int line, int size, int flags, void * ptr) {
  struct MemAllocTable * table;
  int index;

  NeedFreeEntry(ptr, &table, &index);
  
  strncpy(table->entries[index].file, file, DEBUGMEM_FILENAME_LENGTH - 1);
  table->entries[index].file[DEBUGMEM_FILENAME_LENGTH - 1] = 0;
  table->entries[index].line = line;
  table->entries[index].flags = flags;
  table->entries[index].ptr = ptr;
  table->entries[index].size = size;
};

/* Remove an allocation from MemTable */
void DbgMemFree(char * file, int line, int flags, void * ptr) {
  struct MemAllocTable * table;
  int index;

  if (!FindEntry(ptr, &table, &index)) {
    char tmp[256];
    sprintf(tmp, "%s:%i frees unlisted pointer 0x%08x", file, line, ptr); 
    DBGMEM_REPORT_ERROR(tmp);
  } else {
    memset(&table->entries[index], 0, sizeof(struct MemAllocEntry));
    table->UsedEntries--;
  }
};


/* Remove and then readd an allocation */
void DbgMemRealloc(char * file, int line, int flags, void * ptr, int newsize, void * newptr) {
  struct MemAllocTable * table;
  int index;

  if (!FindEntry(ptr, &table, &index)) {
    char tmp[256];
    sprintf(tmp, "%s:%i reallocates unlisted pointer 0x%08x", file, line, ptr); 
    DBGMEM_REPORT_ERROR(tmp);  
  } else {
    memset(&table->entries[index], 0, sizeof(struct MemAllocEntry));
    table->UsedEntries--;
    DbgMemAlloc(file, line, newsize, flags, newptr);
  }
};


/* remove DBGMEM_ENUM flag from all MemTable entries */
void DbgMemInitEnum() {  
  int TableNdx, index;
  for (TableNdx=0;TableNdx<256;TableNdx++)
    if (MemTable[TableNdx])
      for (index=0;index<DEBUGMEM_TABLE_BLOCKSIZE;index++)
	MemTable[TableNdx]->entries[index].flags &= ~(DBGMEM_ENUM);
};


/* report error for all MemTable entries without DBGMEM_ENUM flag */
void DbgMemEndEnum() {
  int TableNdx, index;
  for (TableNdx=0;TableNdx<256;TableNdx++)
    if (MemTable[TableNdx])
      for (index=0;index<DEBUGMEM_TABLE_BLOCKSIZE;index++)
	if (MemTable[TableNdx]->entries[index].ptr && !(MemTable[TableNdx]->entries[index].flags & DBGMEM_ENUM)) {
	  char tmp[256];
	  sprintf(tmp, "Unreported allocation: %s:%i allocated %i bytes at 0x%08x", 
		  MemTable[TableNdx]->entries[index].file,
		  MemTable[TableNdx]->entries[index].line,
		  MemTable[TableNdx]->entries[index].size,
		  MemTable[TableNdx]->entries[index].ptr);
	  DBGMEM_REPORT_ERROR(tmp);
	}
};

/* set DBGMEM_ENUM flag on MemTable entry referencing ptr. Report
   error if flag already set */
void _DbgMemEnum(void * ptr, char * file, int line) {
  struct MemAllocTable * table;
  int index;
  if (!FindEntry(ptr, &table, &index)) {
    char tmp[256];
    sprintf(tmp, "%s:%i reported unlisted pointer 0x%08x", file, line, ptr);
    DBGMEM_REPORT_ERROR(tmp);
  } else {
    if (table->entries[index].flags & DBGMEM_ENUM) {
      char tmp[256];
      sprintf(tmp, "%s:%i reported already enumerated pointer 0x%08x, allocated at %s:%i", 
	       file, line, ptr, table->entries[index].file, 
	       table->entries[index].line);
      DBGMEM_REPORT_ERROR(tmp);
    } else
      table->entries[index].flags |= DBGMEM_ENUM;
  };
};


void DoMemoryReport() {
  log(L_NOTICE, "Memory report starting");
  DbgMemInitEnum();
  /*
    DbgMemEnum all known alloced pointers here.
  */
  DbgMemEndEnum();
  log(L_NOTICE, "Memory report ended");
  

};

#endif




