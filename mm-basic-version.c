#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"
/*Ogolnie to jest taka basic wersja z lista sudo-dwukiernkowa, iteruje sie przez
kazdy blok nie wazne czy wolny czy zajety, wolny blok ma header i footer gdy
zajety ma tylko header, relokacja dziala w najprostszy mozliwy sposob*/

/* If you want debugging output, use the following macro.
 * When you hand in, remove the #define DEBUG line. */
// #define DEBUG
#ifdef DEBUG
#define debug(fmt, ...) printf("%s: " fmt "\n", __func__, __VA_ARGS__)
#define msg(...) printf(__VA_ARGS__)
#else
#define debug(fmt, ...)
#define msg(...)
#endif

#define __unused __attribute__((unused))

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* !DRIVER */

typedef int32_t word_t; /* Heap is bascially an array of 4-byte words. */

typedef enum {
  FREE = 0,     /* Block is free */
  USED = 1,     /* Block is used */
  PREVFREE = 2, /* Previous block is free (optimized boundary tags) */
} bt_flags;

static word_t *heap_start; /* Address of the first block */
static word_t *heap_end;   /* Address past last byte of last block */
static word_t *last;       /* Points at last block */

#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))
#define SIZEOFWORD (sizeof(word_t))
#define BLOCKINWORDS (ALIGNMENT / SIZEOFWORD)
#define SIZEINWORDS(size) (size / SIZEOFWORD)

/* --=[ boundary tag handling ]=-------------------------------------------- */

static inline word_t bt_size(word_t *bt) {
  return *bt & ~(USED | PREVFREE);
}

static inline int bt_used(word_t *bt) {
  return *bt & USED;
}

static inline int bt_free(word_t *bt) {
  return !(*bt & USED);
}

/* Given boundary tag address calculate it's buddy address. */
static inline word_t *bt_footer(word_t *bt) {
  return bt + SIZEINWORDS(bt_size(bt)) - 1;
}

/* Given payload pointer returns an address of boundary tag. */
static inline word_t *bt_fromptr(void *ptr) {
  return (word_t *)ptr - 4;
}

/* Creates boundary tag(s) for given block. */
static inline void bt_make(word_t *bt, size_t size, bt_flags flags) {
  // printf("HEADER: Address: %lx, Value: %d Size: %lx, Flags: %d\n",(long
  // int)bt,*bt,size,flags);
  *bt = size | flags; /* nadanie headerowi rozmiaru i flag */
  // printf("FOOTER: Address: %lx, Value: %d Size: %lx, Flags: %d\n",(long
  // int)(bt_footer(bt)),*bt,size,flags);
  if (!(USED & flags))
    *(bt_footer(bt)) =
      size | flags; /* nadanie footerowi jesli jest block freee */
}

/* Previous block free flag handling for optimized boundary tags. */
static inline bt_flags bt_get_prevfree(word_t *bt) {
  return *bt & PREVFREE;
}

static inline void bt_clr_prevfree(word_t *bt) {
  if (bt)
    *bt &= ~PREVFREE;
}

static inline void bt_set_prevfree(word_t *bt) {
  *bt |= PREVFREE;
}

/* Returns address of payload. */
static inline void *bt_payload(word_t *bt) {
  return bt + 4;
}

/* Returns address of next block or NULL. */
static inline word_t *bt_next(word_t *bt) {
  return bt + SIZEINWORDS(bt_size(bt));
}

// /* Returns address of previous block or NULL. */
// static inline word_t *bt_prev(word_t *bt) {
// }

/* --=[ miscellanous procedures ]=------------------------------------------ */

/* Calculates block size incl. header, footer & payload,
 * and aligns it to block boundary (ALIGNMENT). */
static inline size_t blksz(size_t size) {
  return ALIGN(size + 16);
}

static void *morecore(size_t size) {
  void *ptr = mem_sbrk(size);
  if (ptr == (void *)-1)
    return NULL;
  return ptr;
}

static void split(word_t *placement, size_t size, size_t size_of_placement) {
  bt_make(placement + SIZEINWORDS(size), size_of_placement - size, FREE);
}

/* --=[ mm_init ]=---------------------------------------------------------- */

int mm_init(void) {
  //   void *ptr = morecore(ALIGNMENT - sizeof(word_t));
  //   if (!ptr)
  //     return -1;
  heap_start = morecore(0);
  heap_end = morecore(0);
  last = morecore(0);
  return 0;
}

/* --=[ malloc ]=----------------------------------------------------------- */

#if 0
/* First fit startegy. */
static word_t *find_fit(size_t reqsz) {
}
#else
/* Best fit startegy. */
static word_t *find_fit(size_t reqsz) {
  int block_number = 1;
  word_t *minimal = NULL;
  size_t min_size = -1;
  word_t *search = heap_start;
  while (search < heap_end) {
    // printf("ID: %d, Placement: %lx, Size: %x, Flags: %x\n",block_number,(long
    // int)search,bt_size(search),(*search) & 0xf);
    size_t size = bt_size(search);
    if (bt_free(search) && size >= reqsz && size < min_size) {
      minimal = search;
      min_size = size;
    }
    search = bt_next(search);
    block_number++;
  }
  return minimal;
}
#endif

void *malloc(size_t size) {
  // printf("MALLOC: allocuje pamiec o rozmiarze %lx\n",size);
  if (size == 0)
    return NULL;

  size = blksz(size);

  /*Poczatkowe warunki - pierwsze wykonanie malloca*/
  if (heap_end == heap_start) {
    word_t *placement = morecore(size);
    heap_end += SIZEINWORDS(size);
    bt_make(placement, size, USED);
    return bt_payload(placement);
  }

  word_t *placement = find_fit(size);

  /* Nie ma takiego bloku pamieci by wstawic requestowana pamiec */
  if (!placement) {
    // printf("Size: %lx \tPointer heap-a before: %lx \t",size,(long
    // int)morecore(0));
    placement = morecore(size);
    // printf("Pointer heap-a after: %lx \n",(long int)morecore(0));
    heap_end += SIZEINWORDS(size);
    // printf("Size before bt_make: %lx\n",size);
    bt_make(placement, size, USED);
    last = placement;
    // printf("Address of payload; %lx\n",(long int)bt_payload(placement));
    return bt_payload(placement);
  }

  size_t size_of_placement = bt_size(placement);
  if (size_of_placement > size) {
    split(placement, size, size_of_placement);
  }
  if (size_of_placement == size && placement + SIZEINWORDS(size) <= last) {
    bt_clr_prevfree(placement + SIZEINWORDS(size));
  }
  bt_make(placement, size, USED);
  return bt_payload(placement);
}

/* --=[ free ]=------------------------------------------------------------- */

void free(void *ptr) {
  if (ptr == NULL)
    return;
  word_t *header = bt_fromptr(ptr);
  size_t size = bt_size(header);
  char is_last = header == last ? 1 : 0;
  // printf("FREE: dealokuje pamiec o Adresie: %lx \t Rozmiar: %lx\n",(long
  // int)header,size);
  if (bt_get_prevfree(header)) {
    // printf("I am adding this much to size: %x, from address
    // %lx\n",bt_size(header-1),(long int)header-1);
    size += bt_size(header - 1);
    header -= SIZEINWORDS(bt_size(header - 1));
  }
  word_t *next_header = header + SIZEINWORDS(size);
  if (next_header <= last) {
    if (bt_free(next_header)) {
      if (next_header == last)
        is_last = 1;
      // printf("I am adding this much to size: %x\n",bt_size(next_header));
      size += bt_size(next_header);
    } else {
      bt_set_prevfree(next_header);
    }
  }
  if (is_last)
    last = header;
  // printf("Header address: %lx, size: %lx\n",(long int)header,size);
  bt_make(header, size, FREE);
}

/* --=[ realloc ]=---------------------------------------------------------- */

void *realloc(void *old_ptr, size_t size) {
  /* If size == 0 then this is just free, and we return NULL. */
  // printf("REALLOC: allocuje pamiec o rozmiarze %lx\n",size);
  if (size == 0) {
    free(old_ptr);
    return NULL;
  }

  /* If old_ptr is NULL, then this is just malloc. */
  if (!old_ptr)
    return malloc(size);

  if (size < bt_size(bt_fromptr(old_ptr)) - 16)
    return old_ptr;
  void *ptr = malloc(size);
  size_t pom_size = bt_size(bt_fromptr(old_ptr)) - ALIGNMENT;
  for (size_t i = 0; i < pom_size; i++) {
    ((char *)ptr)[i] = ((char *)old_ptr)[i];
  }
  // memcpy(ptr,old_ptr,pom_size-ALIGNMENT);
  free(old_ptr);
  return ptr;
}

/* --=[ calloc ]=----------------------------------------------------------- */

void *calloc(size_t nmemb, size_t size) {
  size_t bytes = nmemb * size;
  void *new_ptr = malloc(bytes);
  if (new_ptr)
    memset(new_ptr, 0, bytes);
  return new_ptr;
}

/* --=[ mm_checkheap ]=----------------------------------------------------- */

void mm_checkheap(int verbose) {
  printf("\nSTAN HEAP-A\n");
  int block_number = 0;
  word_t *search = heap_start;
  while (search < heap_end) {
    printf("ID: %d, Placement: %lx, Size: %x, Flags: %x\n", block_number,
           (long int)search, bt_size(search), (*search) & 0xf);
    search = bt_next(search);
    block_number++;
  }
  printf("HEAP Pointer Address: %lx\n", (long int)morecore(0));
  printf("\n");
}
