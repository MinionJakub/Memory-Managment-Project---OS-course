/*Jakub Chomiczewski 329713*/

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>

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

typedef struct {
  int32_t header;
  uint32_t ptr_prev;
  uint32_t ptr_next;
  int32_t footer;
} block_t;

typedef enum {
  FREE = 0,     /* Block is free */
  USED = 1,     /* Block is used */
  PREVFREE = 2, /* Previous block is free (optimized boundary tags) */
} bt_flags;

// static word_t *heap_start; /* Address of the first block */
// static word_t *heap_end;   /* Address past last byte of last block */
// static word_t *last;       /* Points at last block */

static const size_t footer_size = 4;
static const size_t tags_size = 8;
static size_t chunksize = 0;
static size_t mem_heap_high = 0;
static void *heap_listp = NULL;

#define round_up(size) ((size + ALIGNMENT - 1) & -ALIGNMENT)
// static inline size_t round_up(size_t size) {
//   return (size + ALIGNMENT - 1) & -ALIGNMENT;
// }

/* --=[ boundary tag handling ]=-------------------------------------------- */

#define bt_size(bt) (bt->header & ~(USED | PREVFREE))
// static inline size_t bt_size(block_t *bt) {
//   return bt->header & ~(USED | PREVFREE);
// }
#define get_header(bt) (bt->header)
// static inline int32_t get_header(block_t *bt) {
//   return bt->header;
// }

static inline int bt_used(block_t *bt) {
  if (bt == NULL)
    return true;
  return get_header(bt) & USED;
}

static inline int bt_free(word_t *bt) {
  return !(*bt & USED);
}

/* Given boundary tag address calculate it's buddy address. */
#define bt_footer(bt) ((void *)bt + bt_size(bt) - sizeof(word_t))
// static inline word_t *bt_footer(block_t *bt) {
//   return (void *)bt + bt_size(bt) - sizeof(word_t);
// }

/* Given payload pointer returns an address of boundary tag. */
static inline word_t *bt_fromptr(void *ptr) {
  return (word_t *)ptr - 1;
}

/* Creates boundary tag(s) for given block. */
static inline void bt_make(block_t *bt, size_t size, bt_flags flags) {
  uint32_t value = size | flags;
  bt->header = value;
  size_t footer = (size_t)bt + size - footer_size;
  *(int32_t *)(footer) = value;
}

static inline uint32_t get_ptr_prev(block_t *bt) {
  return bt->ptr_prev;
}

static inline uint32_t get_ptr_next(block_t *bt) {
  return bt->ptr_next;
}

static inline void set_ptr_prev(block_t *bt, uint32_t value) {
  bt->ptr_prev = value;
}

static inline void set_ptr_next(block_t *bt, uint32_t value) {
  bt->ptr_next = value;
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
  return bt + 1;
}

/* Returns address of next block or NULL. */
static inline void *bt_next(block_t *bt) {
  size_t size = bt_size(bt);
  if ((long)(bt) + size + ALIGNMENT <= mem_heap_high) {
    return (void *)((long)(bt) + size);
  }
  return NULL;
}

/* Returns address of previous block or NULL. */
static inline void *bt_prev(block_t *bt) {
  if ((long)(bt)-ALIGNMENT > (long)(heap_listp)) {
    block_t *prev_block_footer = (void *)bt - footer_size;
    size_t size = bt_size(prev_block_footer);
    return (void *)((long)(bt)-size);
  }
  return NULL;
}

static inline block_t *get_next_free(block_t *block) {
  return heap_listp + get_ptr_next(block);
}

static inline block_t *get_prev_free(block_t *block) {
  return heap_listp + get_ptr_prev(block);
}

/* --=[ miscellanous procedures ]=------------------------------------------ */

/* Calculates block size incl. header, footer & payload,
 * and aligns it to block boundary (ALIGNMENT). */
// static inline size_t blksz(size_t size) {
// }

// static void *morecore(size_t size) {
//   void *ptr = mem_sbrk(size);
//   if (ptr == (void *)-1)
//     return NULL;
//   return ptr;
// }

/* --=[ mm_init ]=---------------------------------------------------------- */
// static size_t search; do heaury ale nie daje poprawy

int mm_init(void) {
  //   void *ptr = morecore(ALIGNMENT - sizeof(word_t));
  //   if (!ptr)
  //     return -1;
  //   heap_start = NULL;
  //   heap_end = NULL;
  //   last = NULL;
  //   return 0;
  if ((long)mem_sbrk(ALIGNMENT - footer_size) < 0)
    return -1;
  size_t size = round_up(2);
  // search = -1;
  heap_listp = mem_sbrk(size);
  mem_heap_high = (long)heap_listp + size;
  bt_make(heap_listp, size, false);
  set_ptr_prev(heap_listp, 0);
  set_ptr_next(heap_listp, 0);
  chunksize = (1 << 7);
  return 0;
}

static inline void remove_block(void *ptr) {
  set_ptr_next(get_prev_free(ptr), get_ptr_next(ptr));
  set_ptr_prev(get_next_free(ptr), get_ptr_prev(ptr));
}

static inline void add_to_end(void *ptr) {
  uint32_t ptr_cmp = ptr - heap_listp;
  uint32_t last_block_ptr = get_ptr_prev(heap_listp);
  block_t *last_block = get_prev_free(heap_listp);

  set_ptr_next(last_block, ptr_cmp);
  set_ptr_next(ptr, 0);
  set_ptr_prev(heap_listp, ptr_cmp);
  set_ptr_prev(ptr, last_block_ptr);
}

static inline void set_block_free(void *bt, size_t size) {
  bt_make(bt, size, false);
  add_to_end(bt);
}

/* --=[ malloc ]=----------------------------------------------------------- */

#if 0
/* First fit startegy. */
static word_t *find_fit(size_t reqsz) {
}
#else
/* Best fit startegy. */
static uint32_t too_long = (1 << 9); //(1<<8) + (1<<7) + (1<<6);

static block_t *find_fit(size_t size) {
  block_t *fit_block = NULL;
  block_t *work_block = heap_listp;
  size_t fit_size = 0;
  size_t work_size;
  uint32_t count = 0;

  while ((work_block = get_next_free(work_block)) != heap_listp) {
    work_size = bt_size(work_block);
    if (work_size >= size && (fit_block == NULL || work_size < fit_size)) {
      fit_block = work_block;
      fit_size = work_size;
    }
    if (count == too_long)
      break;
    count++;
  }

  if (fit_block != NULL) {
    size_t diff = fit_size - size;

    if (diff >= 16) {
      block_t *new_free = (block_t *)((long)fit_block + size);
      set_block_free(new_free, diff);
      remove_block(fit_block);
      bt_make(fit_block, size, true);
    } else {
      remove_block(fit_block);
      bt_make(fit_block, fit_size, true);
    }
  }

  return fit_block;
}

static inline void *increase(size_t size) {
  chunksize = size > chunksize          ? size
              : size > (chunksize >> 7) ? chunksize
                                        : size;

  void *ptr;
  size_t diff = chunksize - size;

  if (diff >= 64) {
    ptr = mem_sbrk(chunksize);

    if ((long)ptr > 0) {
      mem_heap_high += chunksize;
      set_block_free(ptr + size, diff);

      return ptr;
    }
  }

  ptr = mem_sbrk(size);
  if ((long)ptr > 0) {
    mem_heap_high += size;

    return ptr;
  }

  return (void *)(-1);
}

#endif

void *malloc(size_t size) {

  size = round_up(tags_size + size);

  block_t *block;

  if ((block = find_fit(size)) != NULL) {
    return &(block->ptr_prev);
  }

  // if(size < search)search = size;
  block = increase(size);
  if ((long)block < 0)
    return NULL;

  bt_make(block, size, true);

  return &(block->ptr_prev);
}

/* --=[ free ]=------------------------------------------------------------- */

static inline void *coalesce(block_t *block) {
  block_t *prev_block = bt_prev(block);
  block_t *next_block = bt_next(block);
  bool prev_alloc = bt_used(prev_block);
  bool next_alloc = bt_used(next_block);
  size_t size = bt_size(block);

  uint16_t val = (prev_alloc << 1) + next_alloc;
  switch (val) {
    case 3:
      add_to_end(block);
      return block;
    case 2:
      remove_block(next_block);
      size += bt_size(next_block);
      bt_make(block, size, false);
      break;
    case 1:
      remove_block(prev_block);
      size += bt_size(prev_block);
      block = prev_block;
      bt_make(block, size, false);
      break;
    default:
      remove_block(next_block);
      remove_block(prev_block);
      size += bt_size(next_block) + bt_size(prev_block);
      block = prev_block;
      bt_make(block, size, false);
      break;
  }

  add_to_end(block);

  return block;
}

void free(void *ptr) {
  if (ptr != NULL) {
    // search = -1; heura ale nie daje poprawy
    block_t *block = ptr - footer_size;
    size_t size = bt_size(block);

    bt_make(block, size, false);

    block = coalesce(block);
  }
}

/* --=[ realloc ]=---------------------------------------------------------- */

static inline void *try_expand(block_t *block, size_t size) {
  size_t csize = bt_size(block);

  // Sprawdza czy już zaalokowana pamięć razem z paddingiem nie jest
  // wystarczająca
  if (csize - tags_size >= size) {
    return block;
  }

  block_t *next_block = bt_next(block);
  if (next_block == NULL) {
    size = round_up(size + tags_size);
    increase(size - csize);
    bt_make(block, size, true);

    return block;
  }

  bool next_alloc = bt_used(next_block);
  size_t new_size = next_alloc ? 0 : csize + bt_size(next_block);
  size = round_up(size + tags_size);
  if (!next_alloc && new_size >= size) {
    remove_block(next_block);
    bt_make(block, new_size, true);

    return block;
  }

  return NULL;
}

void *realloc(void *old_ptr, size_t size) {
  /* If size == 0 then this is just free, and we return NULL. */
  if (size == 0) {
    free(old_ptr);
    return NULL;
  }

  /* If old_ptr is NULL, then this is just malloc. */
  if (!old_ptr)
    return malloc(size);

  // Próba rozszerzenia już zaalokowanej pamięci
  void *new_ptr = try_expand(old_ptr - footer_size, size);
  if (new_ptr != NULL) {
    return new_ptr + footer_size;
  }

  new_ptr = malloc(size);

  /* If malloc() fails, the original block is left untouched. */
  if (!new_ptr)
    return NULL;

  /* Copy the old data. */
  block_t *block = old_ptr - footer_size;
  size_t old_size = bt_size(block);
  if (size < old_size)
    old_size = size;
  memcpy(new_ptr, old_ptr, old_size);
  // old_size >>= 2;
  // for(uint32_t i = 0; i < old_size; i++){
  //   ((word_t*)new_ptr)[i] = ((word_t*)old_ptr)[i];
  // }
  /* Free the old block. */
  free(old_ptr);

  return new_ptr;
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
