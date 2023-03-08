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
#define SIZEWITHFOOTER (SIZEOFHEADER << 1)
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))
#define ADDRESS_SIZE (sizeof(void *))

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
  return bt + 1;
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
static int counter_req = 0;

static inline size_t get_size(word_t *bt) {
  return (*(bt) & (~15));
}

static inline void make_hedader_and_footer(word_t *bt, size_t size,
                                           bt_flags flags) {

  *(bt) = size + flags; // ustawia rozmiar blocku na rozmiar size

  if (bt_free(bt))
    *(bt + size - SIZEOFHEADER) = size + flags; // ustawia footer
}

static inline size_t allign_size_plus_header(size_t size) {
  return ALIGN(size + SIZEOFHEADER);
}

static void split(word_t *bt, size_t reqsz) {
  // printf("HELLO\n");
  size_t less_size = get_size(bt) - reqsz;
  // char is_last = bt == last ? 1 : 0;
  if (less_size >= ALIGNMENT) {
    word_t *new_header = bt;
    *new_header += reqsz;
    new_header += 1;
    make_hedader_and_footer(new_header, less_size, FREE);
    last = bt == last ? new_header : last;
  }
  return;
}

#if 0
/* First fit startegy. */
static word_t *find_fit(size_t reqsz) {
}
#else
/* Best fit startegy. */
static word_t *find_fit(size_t reqsz) {
  // printf("HELLO THERE!\n");
  word_t *min_size_pointer = NULL;
  size_t min_size = 0;
  word_t *search = heap_start;
  // int counter = 0;
  while (search < last) {
    // printf("sth \t");
    // printf("%ld\n",(long int)search);
    size_t size = get_size(search);
    // printf("%ld\t",size);
    if (reqsz <= size && bt_free(search) && size < min_size) {
      // printf("lol\n");
      min_size_pointer = search;
      min_size = size;
    }
    search += size / SIZEOFHEADER;
    search += 1;
    // printf()
    // sleep(1);
    //  counter ++;
  }
  printf("counter = %d\n", counter_req);
  if (min_size > reqsz) {
    split(search, reqsz);
  }
  return min_size_pointer;
}
#endif

static char is_first = 1;

void *malloc(size_t size) {
  counter_req += 1;
  // printf("%ld\n",size);

  /* Jesli glupio programista poprosil o 0 bajtow to zwracamy NULL-a */
  if (size == 0)
    return NULL;

  /* Jesli size jest rozny od 0 to dodaje mojego int-a trzymajacego rozmiar i
   * alajnuje rozmiar */
  size = allign_size_plus_header(size);

  /* Szukanie miejsca na heap-ie gdzie zaalokowac proszona pamiec */
  word_t *placement = find_fit(size);

  /* Jesli szukanie nie znalazlo takiego miejsca to przesuwamy wskaznik stosu */
  if (placement == NULL) {
    printf("Size %ld\n", size);
    placement = morecore(size);
    heap_end += size;

    /* tworzymy header w zadanej pamieci */
    if (bt_get_prevfree(last))
      make_hedader_and_footer(placement, size, PREVFREE | USED);
    else
      make_hedader_and_footer(placement, size, USED);

    last = placement;
    if (is_first == 1) {
      *heap_start = *placement;
      is_first = 0;
    }
    return bt_payload(placement);
  }
  /* tworzymy header w zadanej pamieci */
  make_hedader_and_footer(placement, size, USED);
  return bt_payload(placement);
}

/* --=[ free ]=------------------------------------------------------------- */

void free(void *ptr) {
  counter_req += 1;
  word_t *header = ((word_t *)ptr) - 1;
  size_t size_of_block = get_size(header);
  char is_last = header == last ? 1 : 0;
  if (bt_get_prevfree(header)) {
    *header -= get_size((header - 1));
    size_of_block += get_size(header);
    // make_hedader_and_footer(previous_block_header,get_size(previous_block_header)+size_of_block,FREE);
    if (is_last) {
      *last = *header;
      return;
    }
  }
  word_t *next_header = header;
  *next_header += size_of_block;
  next_header += 1;
  if (bt_free(next_header)) {
    if (next_header == last)
      *last = *header;
    size_of_block += get_size(next_header);
    *next_header += get_size(next_header);
    next_header += 1;
  }
  make_hedader_and_footer(header, size_of_block, FREE);
  if (next_header < heap_end) {
    bt_set_prevfree(next_header);
  }
  return;
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

  /* header aktualnego wskaznika */
  // word_t *header = old_ptr - SIZEOFHEADER;

  /* rozmiar aktualnego bloku */
  // size_t size_of_block = get_size(header);

  free(old_ptr);
  void *ptr = malloc(size);

  if (ptr == old_ptr)
    return ptr;

  size_t pom_size = get_size(old_ptr) * (ALIGNMENT / SIZEOFHEADER);
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
  word_t *search = heap_start;
  while (search < last) {
    printf("\tBlok o indeksie: %d \t rozmiar: %ld \n", *search,
           get_size(search));
    if (get_size(search) == 0)
      exit(0);
    *search += get_size(search);
    // sleep(1);
  }
}
