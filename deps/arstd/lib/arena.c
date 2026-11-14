#include <arena.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef union {
  char c;
  short s;
  int i;
  long l;
  float f;
  double d;
  void *p;
  void (*fp)(void);
} MaxAlign;

struct AlignMax {
  char c;
  MaxAlign m;
};

#define MAX_ALIGN offsetof(struct AlignMax, m)
#define ALIGN_UP(size) (((size) + (MAX_ALIGN - 1)) & ~(MAX_ALIGN - 1))

struct Arena {
  void *region;
  size_t size;
  size_t capacity;
  struct Arena *next;
};

void *m_align_alloc(size_t capacity) {
  size_t alignment;
  size_t header_size;
  size_t total_size;
  void *origin;
  char *current;
  size_t current_addr;
  size_t mask;
  size_t aligned_addr;
  char *aligned_ptr;
  char **head;

  if (capacity == 0) {
    return NULL;
  }

  alignment = MAX_ALIGN;

  header_size = sizeof(void *);

  /*
   * (alignment - 1) ensures proper padding is done and no byte is wasted
   * for most cases sizeof(void *) is 8 bytes, so for worst case scenario we
   * need maximum 7 bytes for padding.
   * */
  total_size = capacity + (alignment - 1) + header_size;
  origin = malloc(total_size);
  if (origin == NULL) {
    return NULL;
  }

  /*
   * This moves the pointer to starting of memory block of ((alignment - 1) +
   * aligned memory)
   * */
  current = (char *)origin + header_size;

  /*
   * current and (alignment - 1) needs to be casted to an integer type
   * as we need to do bit manipulation later
   * */
  current_addr = (size_t)current;
  mask = (size_t)(alignment - 1);

  aligned_addr = (current_addr + mask) & ~mask;

  /*
   * casting to char* for pointer arithmetic
   * */
  aligned_ptr = (char *)aligned_addr;

  head = (char **)(aligned_ptr - header_size);
  *head = (char *)origin;

  memset(aligned_ptr, 0, capacity);

  return (void *)aligned_ptr;
}

void m_align_free(void *ptr) {
  char **head;
  void *origin;

  if (ptr == NULL) {
    return;
  }

  /*
   * head is a pointer-to-pointer which stores the address of the original
   * address returned by malloc. ptr is the aligned pointer, so if we move
   * back by a size of a pointer we will have the address where the original
   * pointer is stored.
   *
   *     v-head
   * [   |========][============]
   * ^-malloc      ^-aligned memory
   * */
  head = (char **)((char *)ptr - sizeof(void *));
  origin = (void *)*head;

  free(origin);
}

void *m_align_realloc(void *ptr, size_t old_size, size_t new_size) {
  void *new_ptr;
  size_t copy_size;

  /* If ptr is NULL, behave like malloc */
  if (ptr == NULL) {
    return m_align_alloc(new_size);
  }

  if (new_size == 0 || new_size == (size_t)-1) {
    m_align_free(ptr);
    return NULL;
  }

  new_ptr = m_align_alloc(new_size);
  if (new_ptr == NULL) {
    return NULL; /* Keep old ptr valid on failure */
  }

  copy_size = old_size < new_size ? old_size : new_size;
  memcpy(new_ptr, ptr, copy_size);

  m_align_free(ptr);

  return new_ptr;
}

Arena *arena_init(size_t capacity) {
  Arena *arena;
  void *data;
  size_t aligned_capacity;

  if (capacity == 0) {
    capacity = 4096;
  }
  aligned_capacity = ALIGN_UP(capacity);

  arena = (Arena *)m_align_alloc(sizeof(Arena));
  data = m_align_alloc(aligned_capacity);

  if (arena == NULL || data == NULL) {
    fprintf(stderr, "Fatal: Failed to allocate arena");
    abort();
  }

  arena->region = data;
  arena->capacity = aligned_capacity;
  arena->size = 0;
  arena->next = NULL;

  return arena;
}

void *arena_alloc(Arena *arena, size_t size) {
  Arena *current_arena;
  void *data;
  Arena *new_arena;
  size_t aligned_size;

  if (arena == NULL || size == 0) {
    return NULL;
  }

  /*
   * Round size up to a multiple of MAX_ALIGN so every subsequent
   * allocation in the arena starts at a maximally aligned address.
   * */
  aligned_size = ALIGN_UP(size);
  current_arena = arena;
  data = NULL;

  while (current_arena != NULL) {
    /*
     * if the element fits in the arena, then store it.
     * */
    if (current_arena->size + aligned_size <= current_arena->capacity) {
      data = (char *)current_arena->region + current_arena->size;
      current_arena->size += aligned_size;
      return data;
    }

    /*
     * if it doesn't fit, check if it has any other arena attached to it.
     * if no arena is attached, then allocate a new one.
     * */
    if (current_arena->next != NULL) {
      current_arena = current_arena->next;
    } else {
      new_arena = NULL;
      if (current_arena->capacity < aligned_size) {
        new_arena = arena_init(aligned_size);
      } else {
        new_arena = arena_init(arena->capacity);
      }
      current_arena->next = new_arena;
      data = new_arena->region;
      new_arena->size = aligned_size;
      return data;
    }
  }

  return data;
}

static void *(*const volatile secure_memset)(void *, int, size_t) = memset;

void arena_free_secure(Arena **arena) {
  Arena *current_arena;
  Arena *next_arena;

  if (arena == NULL || *arena == NULL) {
    return;
  }

  current_arena = *arena;
  while (current_arena != NULL) {
    next_arena = current_arena->next;

    if (current_arena->region != NULL && current_arena->size > 0) {
      secure_memset(current_arena->region, 0, current_arena->size);
    }
    m_align_free(current_arena->region);
    current_arena->region = NULL;

    secure_memset(current_arena, 0, sizeof(Arena));
    m_align_free(current_arena);

    current_arena = next_arena;
  }

  *arena = NULL;
}

void arena_free(Arena **arena) {
  Arena *current_arena;
  Arena *next_arena;

  if (arena == NULL || *arena == NULL) {
    return;
  }

  current_arena = *arena;
  while (current_arena != NULL) {
    next_arena = current_arena->next;
    m_align_free(current_arena->region);
    current_arena->region = NULL;
    m_align_free(current_arena);
    current_arena = next_arena;
  }

  *arena = NULL;
}

void arena_reset(Arena **arena) {
  Arena *next;

  if (arena == NULL || *arena == NULL) {
    return;
  }

  (*arena)->size = 0;
  next = (*arena)->next;
  arena_free(&next);
  (*arena)->next = NULL;
  memset((*arena)->region, 0, (*arena)->capacity);
}

void arena_rewind(Arena *arena) {
  Arena *current;

  current = arena;
  while (current != NULL) {
    memset(current->region, 0, current->size);
    current->size = 0;
    current = current->next;
  }
}

ArenaTemp arena_temp_begin(Arena *arena) {
  ArenaTemp temp;
  Arena *tail;

  temp.arena = arena;
  temp.tail = NULL;
  temp.saved_size = 0;

  if (arena == NULL) {
    return temp;
  }

  tail = arena;
  while (tail->next != NULL && tail->next->size > 0) {
    tail = tail->next;
  }

  temp.tail = tail;
  temp.saved_size = tail->size;
  return temp;
}

void arena_temp_end(ArenaTemp temp) {
  Arena *curr;

  if (temp.arena == NULL || temp.tail == NULL) {
    return;
  }

  if (temp.tail->size > temp.saved_size) {
    memset((char *)temp.tail->region + temp.saved_size, 0,
           temp.tail->size - temp.saved_size);
  }
  temp.tail->size = temp.saved_size;

  curr = temp.tail->next;
  while (curr != NULL) {
    if (curr->size > 0) {
      memset(curr->region, 0, curr->size);
      curr->size = 0;
    }
    curr = curr->next;
  }
}

size_t arena_used(Arena *arena) {
  size_t total;

  total = 0;
  while (arena != NULL) {
    total += arena->size;
    arena = arena->next;
  }
  return total;
}

size_t arena_capacity(Arena *arena) {
  size_t total;

  total = 0;
  while (arena != NULL) {
    total += arena->capacity;
    arena = arena->next;
  }
  return total;
}
