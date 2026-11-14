#ifndef CMAP_H
#define CMAP_H

#include <arena.h>
#include <cvector.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define LOAD_FACTOR_THRESHOLD 0.75f

typedef struct Entry Entry;
typedef struct Cmap Cmap;

#define Cmap(T) T *

void *map_init_impl(Arena *arena);
#define map_init(arena) map_init_impl((arena))

void map_add_impl(Cmap *cmap, const char *key, const void *val,
                  size_t val_size);
#define map_add(map, key, val)                                                 \
  do {                                                                         \
    VEC_TYPEOF(*(map)) CONCAT(_map_tmp_, __LINE__) = (val);                    \
    map_add_impl((Cmap *)(map), (key), &CONCAT(_map_tmp_, __LINE__),           \
                 sizeof(*(map)));                                              \
  } while (0)

void *map_get_impl(Cmap *cmap, const char *key);
#define map_get(map, key) (*(VEC_TYPEOF(map))map_get_impl((Cmap *)(map), (key)))

#define map_get_ptr(map, key)                                                  \
  ((VEC_TYPEOF(map))map_get_impl((Cmap *)(map), (key)))

bool map_has_impl(Cmap *cmap, const char *key);
#define map_has(map, key) map_has_impl((Cmap *)(map), (key))

bool map_remove_impl(Cmap *cmap, const char *key);
#define map_remove(map, key) map_remove_impl((Cmap *)(map), (key))

Vector(char *) map_keys_impl(Cmap *cmap);
#define map_keys(map) map_keys_impl((Cmap *)(map))

size_t map_len_impl(Cmap *map);
#define map_len(map) map_len_impl((Cmap *)(map))

void map_free_impl(Cmap **cmap);
#define map_free(map_ptr) map_free_impl((Cmap **)(map_ptr))

#endif // CMAP_H
