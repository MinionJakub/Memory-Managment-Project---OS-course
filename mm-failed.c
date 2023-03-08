/*
Memory manager
Ideas:
- the try of making the simplest first-fit
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...)
#endif

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

typedef int32_t word_t; /* Heap is bascially an array of 4-byte words. */

static word_t *heap_start; /* Address of the first block */
static word_t *heap_end;   /* Address past last byte of last block */
static word_t *last;       /* Points at last block */

/*Maybe I will try creating binary buddy system... or not... well back to some
 * simple things*/
typedef enum {
  FREE = 0,       /* Block is free to use */
  USED = 1,       /* Block in used by process */
  LEFT_FREE = 2,  /*If block before it is free then this flag is set */
  RIGHT_FREE = 4, /*If block next to it is free then this flag is set */
} bt_flags;

/*Metadata*/
typedef struct {
  word_t flags; /* Metadata about surrounding and itself */
  // block_t *previous; /* Previous block */
  // block_t *next; /* Next block */
  uint8_t payload[]; /* Zero-length array to access the payload of the block*/
} block_t;

/*--=[flags menaging]=----------------------------------------------*/

static inline word_t get_used(word_t *flags) {
  return
}

/*--=[memory managing]=----------------------------------------------*/

int mm_init(void) {
  if ((long)mem_sbrk(ALIGNMENT - offsetof(block_t, payload)) < 0)
    return -1;
  return 0;
}

void *malloc(size_t size) {
  return NULL;
}

void free(void *ptr) {
}

void *realloc(void *old_ptr, size_t size) {
  if (size == 0) {
    free(old_ptr);
    return NULL;
  }
  if (!old_ptr) {
    return malloc(size);
  }
  return NULL;
}

void *calloc(size_t nmemb, size_t size) {
  size_t bytes = nmemb * size;
  void *new_ptr = malloc(bytes);
  /* If malloc() fails, skip zeroing out the memory. */
  if (new_ptr)
    memset(new_ptr, 0, bytes);
  return new_ptr;
}