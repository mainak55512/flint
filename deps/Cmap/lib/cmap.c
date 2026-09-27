#include <cmap.h>
#include <container.h>

Cmap *map_init() {
	Arena *arena = arena_init(4096);
	Cmap *cmap = (Cmap *)arena_alloc(arena, sizeof(Cmap));

	Vector *buckets = vector_init(Entry *);

	cmap->buckets = buckets;

	cmap->capacity = 5;
	cmap->count = 0;
	cmap->arena = arena;

	for (int i = 0; i < cmap->capacity; i++) {
		append(Entry *, cmap->buckets, NULL);
	}

	return cmap;
}

void map_free(Cmap *cmap) {
	if (cmap->buckets != NULL) {
		vector_free(cmap->buckets);
	}
	Arena *arena_to_free = cmap->arena;
	arena_free(&arena_to_free);
}

unsigned long hash_function(const char *str) {
	unsigned long hash = 5381;
	int c;

	while ((c = *str++)) {
		hash = ((hash << 5) + hash) + c;
	}
	return hash;
}

void map_add(Cmap *cmap, const char *key, void *val) {
	unsigned long hash = hash_function(key);
	int idx = hash % cmap->capacity;

	Entry *current = at(Entry *, cmap->buckets, idx);
	while (current != NULL) {
		if (strcmp(current->key, key) == 0) {
			current->value = val;
			return;
		}
		current = current->next;
	}

	if ((float)(cmap->count + 1) / cmap->capacity >= LOAD_FACTOR_THRESHOLD) {
		map_reset(cmap);
		idx = hash % cmap->capacity;
	}

	Entry *entry = (Entry *)arena_alloc(cmap->arena, sizeof(Entry));
	size_t key_len = strlen(key) + 1;
	char *new_key = (char *)arena_alloc(cmap->arena, key_len);
	memcpy(new_key, key, key_len);
	entry->key = new_key;
	entry->value = val;
	entry->next = at(Entry *, cmap->buckets, idx);
	replace_at(Entry *, cmap->buckets, idx, entry);
}

void *map_get(Cmap *cmap, const char *key) {
	unsigned long hash = hash_function(key);
	int idx = hash % cmap->capacity;

	Entry *current = at(Entry *, cmap->buckets, idx);
	while (current != NULL) {
		if (strcmp(current->key, key) == 0) {
			return current->value;
		}
		current = current->next;
	}
	return NULL;
}

void map_reset(Cmap *cmap) {
	int new_capacity = cmap->capacity * 2;
	Vector *new_buckets = vector_init(Entry *);

	for (int i = 0; i < new_capacity; i++) {
		append(Entry *, new_buckets, NULL);
	}

	for (int i = 0; i < cmap->capacity; i++) {
		Entry *current = at(Entry *, cmap->buckets, i);

		while (current != NULL) {
			Entry *next = current->next;

			unsigned long hash = hash_function(current->key);
			int new_idx = hash % new_capacity;

			current->next = at(Entry *, new_buckets, new_idx);
			replace_at(Entry *, new_buckets, new_idx, current);

			current = next;
		}
	}

	vector_free(cmap->buckets);
	cmap->buckets = new_buckets;
	cmap->capacity = new_capacity;
}

Vector *map_keys(Cmap *cmap) {
	Vector *keys = vector_init(char *);
	for (int i = 0; i < length(cmap->buckets); i++) {
		Entry *current = at(Entry *, cmap->buckets, i);
		while (current != NULL) {
			append(const char *, keys, current->key);
			current = current->next;
		}
	}
	return keys;
}
