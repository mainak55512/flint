#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <stdbool.h>
#elif !defined(__cplusplus) && !defined(__bool_true_and_false_are_defined)
typedef int bool;
#define true 1
#define false 0
#define __bool_true_and_false_are_defined 1
#endif

typedef struct Arena Arena;

typedef struct ArenaTemp {
  Arena *arena;
  Arena *tail;
  size_t saved_size;
} ArenaTemp;

void *m_align_alloc(size_t capacity);

void m_align_free(void *ptr);

void *m_align_realloc(void *ptr, size_t old_size, size_t new_size);

Arena *arena_init(size_t capacity);

void *arena_alloc(Arena *arena, size_t size);

void arena_free(Arena **arena);

void arena_free_secure(Arena **arena);

void arena_reset(Arena **arena);

void arena_rewind(Arena *arena);

ArenaTemp arena_temp_begin(Arena *arena);

void arena_temp_end(ArenaTemp temp);

size_t arena_used(Arena *arena);

size_t arena_capacity(Arena *arena);

#define arena_push(arena, T, count)                                            \
  ((T *)arena_alloc((arena), sizeof(T) * (size_t)(count)))

#endif /* ARENA_H */
