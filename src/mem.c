/*************************************************
*         PMW memory handling functions          *
*************************************************/

/* Copyright Philip Hazel 2026 */
/* This file created: December 2020 */
/* This file last modified: January 2026 */

/* This module handles two types of memory management. "Independent" blocks are
used for large items such as font tables. Smaller blocks are doled out from
large chunks as required. The chain of chunks can then easily be freed on exit.
*/

#include "pmw.h"


static void *anchor = NULL;
static void *current = NULL;
static bstr **record = NULL;
static size_t top = MEMORY_CHUNKSIZE;
static size_t independent_total = 0;
static usint chunk_count = 0;



/*************************************************
*                Return info                     *
*************************************************/

usint
mem_get_info(size_t *available, size_t *independent)
{
*independent = independent_total;
*available = MEMORY_CHUNKSIZE - top;
return chunk_count;
}


/*************************************************
*                    Free all                    *
*************************************************/

void
mem_free(void)
{
void *p = anchor;
while (p != NULL)
  {
  void *q = p;
  p = (void *)(*((char **)p));
  free(q);
  }
anchor = current = NULL;  /*Tidiness */
}



/*************************************************
*        Register an independent block           *
*************************************************/

/* The string-reading function manages its own independent blocks for holding
PMW strings. By registering each block here, it ensures freeing happens at the
end. */

void
mem_register(void *block, size_t size)
{
*((void **)block) = anchor;
anchor = block;
independent_total += size;
}



/*************************************************
*           Get a new independent block          *
*************************************************/

/* Each independent block is separate but hung on the chain so it gets freed at
the end. */

void *
mem_get_independent(size_t size)
{
void *new;
DEBUG(D_memorydetail) (void)fprintf(stderr, "Get independent %zd\n", size);
size += sizeof(char *);
new = malloc(size);
if (new == NULL) error(ERR0, "", "mem_get_independent()", size);  /* Hard */
*((void **)new) = anchor;
anchor = new;
independent_total += size;
return (void *)((char *)new + sizeof(char *));
}



/*************************************************
*             Get a new small block              *
*************************************************/

/* Small blocks are carved out of larger chunks. The size is rounded up to a
multiple of the pointer size, which hopefully means that each block is aligned
for any data type. */

void *
mem_get(size_t size)
{
size_t available = MEMORY_CHUNKSIZE - top;
void *yield;

size = (size + sizeof(char *) - 1);
size -= size % sizeof(char *);

DEBUG(D_memorydetail) (void)fprintf(stderr, "Get small %zd (%zd available)\n", size,
  available);

/* We should never be requesting a block that is bigger than the chunk size.
Get a new chunk if the request won't fit in the current one. Since all blocks
are relatively small compared to the chunk size, the wastage is unimportant. */

if (size > MEMORY_MAXBLOCK) error(ERR1, size, MEMORY_MAXBLOCK);  /* Hard */

if (available < size)
  {
  char *newblock = malloc(MEMORY_CHUNKSIZE);
  DEBUG(D_memorydetail) (void)fprintf(stderr, "\nNew small chunk 0x%p - 0x%p\n",
    (void *)newblock, (void *)(newblock + MEMORY_CHUNKSIZE));
  if (newblock == NULL) error(ERR0, "", "mem_get()", MEMORY_CHUNKSIZE); /* Hard */
  chunk_count++;
  *((void **)newblock) = anchor;
  anchor = newblock;
  current = newblock;
  top = sizeof(char *);
  }

yield = (char *)current + top;
top += size;

return yield;
}



/*************************************************
*    Remember where to record the next item      *
*************************************************/

/* This is called when reading a stave when a bar repeat count is read. It
ensures that whatever comes next in the bar is remembered so that it can be
used for the repeated bars. */

void
mem_record_next_item(bstr **p)
{
record = p;
}



/*************************************************
*         Connect an item to the chain           *
*************************************************/

/* Underlay text items are not obtained by mem_get_item() because they are not
immediately added to the chain of bar items. This is because the text has to be
divided up among the following notes. This function adds an existing item to
the end of the chain. */

void
mem_connect_item(bstr *p)
{
p->next = NULL;
p->prev = read_lastitem;
read_lastitem->next = p;
read_lastitem = p;
if (record != NULL)
  {
  *record = p;
  record = NULL;
  }
}



/*************************************************
*    Get a new bar item block at end of chain    *
*************************************************/

/* These blocks are held in a two-way chain. */

void *
mem_get_item(size_t size, usint type)
{
bstr *yield = mem_get(size);
yield->type = type;
mem_connect_item(yield);
return yield;
}



/*************************************************
*   Get a new bar item block inserted in chain   *
*************************************************/

/* These blocks are held in a two-way chain. This function inserts before an
existing block. */

void *
mem_get_insert_item(size_t size, usint type, bstr *next)
{
bstr *yield = mem_get(size);
yield->type = type;
yield->next = next;
yield->prev = next->prev;
yield->prev->next = yield;
next->prev = yield;
if (record != NULL)
  {
  *record = yield;
  record = NULL;
  }
return yield;
}



/*************************************************
*              Duplicate an item                 *
*************************************************/

/* Put a new item on the end of the chain that is a duplicate of an existing
item. This is used for replicating ornaments and notes and when splitting up
underlay/overlay. */

void *
mem_duplicate_item(void *p, size_t size)
{
size_t offset = offsetof(bstr, type);
void *new = mem_get_item(size, 0);
memcpy((char *)new + offset, (char *)p + offset, size - offset);
return new;
}



/*************************************************
*         Copy a C string into a new block       *
*************************************************/

uschar *
mem_copystring(uschar *s)
{
size_t len = Ustrlen(s) + 1;
uschar *yield = mem_get(len);
memcpy(yield, s, len);
return yield;
}



/*************************************************
*         Get a block from a cached list         *
*************************************************/

/* A number of types of small block that are used and re-used are put on free
chains in between. This function gets a block off such a chain, or gets a new
one if the chain is empty. These blocks all have a "next" pointer at their
start. */

typedef struct cached_block {
  struct cached_block *next;
} cached_block;

void *
mem_get_cached(void **anchorptr, size_t size)
{
void *yield = *anchorptr;
if (yield == NULL) yield = mem_get(size);
  else *anchorptr = ((cached_block *)yield)->next;
return yield;
}



/*************************************************
*           Add a block to a cached list         *
*************************************************/

void
mem_free_cached(void **anchorptr, void *p)
{
((cached_block *)p)->next = *anchorptr;
*anchorptr = p;
}


/* End of mem.c */
