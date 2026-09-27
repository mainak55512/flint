#include <container.h>
#ifndef ARENA_H
#include <arena.h>
#endif

#ifndef CMAP_H
#define CMAP_H
#include <stdio.h>
#include <string.h>

#define LOAD_FACTOR_THRESHOLD 0.75

typedef struct Entry {
	const char *key;
	void *value;
	struct Entry *next;
} Entry;

typedef struct cmap {
	int capacity;
	int count;
	Vector *buckets;
	Arena *arena;
} Cmap;

Cmap *map_init();
void map_free(Cmap *cmap);

void map_add(Cmap *cmap, const char *key, void *val);
void *map_get(Cmap *cmap, const char *key);
void map_reset(Cmap *cmap);
Vector *map_keys(Cmap *cmap);
#endif
