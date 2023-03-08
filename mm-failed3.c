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

#define SIZEOFHEADER sizeof(word_t)
//#define SIZEWITHFOOTER (SIZEOFHEADER<<1)
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))
#define ADDRESS_SIZE (sizeof(void *))
#define SIZEOFBLOCKINWORDS (ALIGNMENT / SIZEOFHEADER)
#define SIZEINWORDS(size) (size / SIZEOFHEADER)

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
  return (void *)bt + bt_size(bt) - sizeof(word_t);
}

/* Given payload pointer returns an address of boundary tag. */
static inline word_t *bt_fromptr(void *ptr) {
  return (word_t *)ptr - 1;
}

/* Creates boundary tag(s) for given block. */
static inline void bt_make(word_t *bt, size_t size, bt_flags flags) {
  *bt = size | flags; // ustawia rozmiar blocku na rozmiar size
  printf("Address: %lx, Value: %d Size: %ld, Flags: %d\n", (long int)bt, *bt,
         size, flags);
  if (bt_free(bt)) {
    word_t *pointer = bt + SIZEINWORDS(size);
    printf("Address of footer: %lx\n", (long int)(pointer - 1));
    *(pointer - 1) = size | flags; // ustawia footer
  }
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
  return NULL;
}

/* Returns address of previous block or NULL. */
static inline word_t *bt_prev(word_t *bt) {
  return NULL;
}

/* --=[ miscellanous procedures ]=------------------------------------------ */

/* Calculates block size incl. header, footer & payload,
 * and aligns it to block boundary (ALIGNMENT). */
static inline size_t blksz(size_t size) {
  return ALIGN(size + SIZEOFHEADER);
}

static void *morecore(size_t size) {
  void *ptr = mem_sbrk(size);
  if (ptr == (void *)-1)
    return NULL;
  return ptr;
}

/* --=[ mm_init ]=---------------------------------------------------------- */

int mm_init(void) {
  void *ptr = morecore(ALIGNMENT - sizeof(word_t));
  if (!ptr)
    return -1;
  heap_start = ptr;
  heap_end = ptr;
  last = ptr;
  return 0;
}

/* --=[ malloc ]=----------------------------------------------------------- */

static inline size_t get_size(word_t *bt) {
  return (*(bt) & (~15));
}

// s

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
  while (search <= last) {

    size_t block_size = get_size((word_t *)search);
    if (block_size >= reqsz && bt_free(search) && block_size < min_size) {
      min_size = block_size;
      minimal = search;
    }
    // printf("ID: %d, Placement: %lx, Size: %ld\n",block_number,(long
    // int)search,get_size(search));
    block_number++;
    search += SIZEINWORDS(block_size) + 0 * SIZEOFBLOCKINWORDS;

    // counter++;
  }

  return minimal;
}
#endif

// static int count = 0;

void *malloc(size_t size) {

  if (!size)
    return NULL; /* Gdy size jest zero mamy zrwócić null-a */

  word_t *placement;

  size = blksz(size); /* Rozmiar po dodaniu header-a oraz przy spełnieniu że
                         rozmiar bloków jest podzielny przez 16*/

  /* Przypadek początkowy */
  if (heap_start == heap_end) {
    placement = heap_end;
    morecore(size);
    heap_start = placement;
    heap_end += SIZEINWORDS(size);
    bt_make(placement, size, USED);
    return bt_payload(placement);
  }

  placement = find_fit(size); /* Znajdujemy najlepsze miejsce w heap-ie */

  /* Jeśli nie będzie miejsca na heapie to trzeba go zwiekszyc i ustawic
   * odpowiednio last*/
  if (placement == NULL) {
    placement = heap_end;
    morecore(size);
    heap_end += SIZEINWORDS(size);
    last = placement;
    bt_make(placement, size, USED);
    return bt_payload(placement);
  }

  size_t placement_size = get_size(placement);

  /* Jesli znaleziony blok jest za duzy */
  if (placement_size > size) {
    printf("Placement size: %lx, size: %ld, diffrence: %ld\n", placement_size,
           size, placement_size - size);
    // split(placement,size,placement_size-size);

    size = placement_size;
  }
  printf("Placement: %lx, HeapEnd: %lx\n", (long int)placement,
         (long int)heap_end);
  bt_make(placement, size, USED);
  return bt_payload(placement);
}

/* --=[ free ]=------------------------------------------------------------- */

void free(void *ptr) {
  word_t *header = ptr;
  header -= 4;
  size_t size = get_size(header);
  word_t *next_header = header + SIZEINWORDS(size);
  if (next_header <= last) {
    while (bt_free(next_header) && next_header <= last) {
      size += get_size(next_header);
      if (next_header == last) {
        last = header;
        break;
      }
      next_header = header + SIZEINWORDS(size);
    }
    if (next_header <= last)
      bt_set_prevfree(next_header);
  }
  if (bt_get_prevfree(header)) {
    // printf("HEJ\n");
    size_t p_size = SIZEINWORDS(get_size(header - 1));
    if (header == last)
      last = header - p_size;
    header -= p_size;
    size += get_size(header);
  }
  printf("Header: %lx, Size: %ld\n", (long int)header, size);
  bt_make(header, size, FREE);
}

/* --=[ realloc ]=---------------------------------------------------------- */

void *realloc(void *old_ptr, size_t size) {
  /* według specyfikacji zadania */
  if (size == 0) {
    free(old_ptr);
    return NULL;
  }

  /* według specyfikacji */
  if (old_ptr == NULL)
    return malloc(size);

  free(old_ptr);
  void *ptr = malloc(size);

  if (ptr == old_ptr)
    return ptr;

  size_t pom_size = SIZEINWORDS(get_size(old_ptr));
  for (size_t i = 0; i < pom_size; i += SIZEOFHEADER) {
    ((word_t *)ptr)[SIZEOFHEADER + i] = ((word_t *)old_ptr)[SIZEOFHEADER + i];
  }
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
}
