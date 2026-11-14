#include <cmap.h>
#include <stdint.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#include <stddef.h>
#define MAP_ALIGN alignas(max_align_t)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#include <stdalign.h>
#include <stddef.h>
#define MAP_ALIGN _Alignas(max_align_t)
#elif defined(__GNUC__) || defined(__clang__)
#define MAP_ALIGN __attribute__((aligned(__BIGGEST_ALIGNMENT__)))
#else
#define MAP_ALIGN
#endif

struct Entry {
	unsigned long hash;
	struct Entry *next;
	int key_idx;
	MAP_ALIGN char data[];
};

struct Cmap {
	int capacity;
	int count;
	size_t val_size;
	Vector(Entry *) buckets;
	Vector(char *) keys;
	Arena *arena;
};

static void *map_alloc(Arena *arena, size_t size) {
	if (arena != NULL) {
		return arena_alloc(arena, size);
	}
	return m_align_alloc(size);
}

static unsigned long hash_function(const char *str) {
	unsigned long hash = 5381;
	int c;
	while ((c = (unsigned char)*str++)) {
		hash = ((hash << 5) + hash) + c;
	}
	return hash;
}

static inline char *entry_key(Entry *entry, size_t val_size) {
	return entry->data + val_size;
}

static Entry *find_entry(Cmap *cmap, const char *key, unsigned long hash,
						 int idx) {
	Entry *current = at(cmap->buckets, idx);
	while (current != NULL) {
		if (current->hash == hash &&
			strcmp(entry_key(current, cmap->val_size), key) == 0) {
			return current;
		}
		current = current->next;
	}
	return NULL;
}

static void map_grow(Cmap *cmap) {
	int old_capacity = cmap->capacity;
	int new_capacity = old_capacity * 2;
	unsigned long mask = (unsigned long)(new_capacity - 1);

	Entry *all_entries = NULL;
	for (int i = 0; i < old_capacity; i++) {
		Entry *current = at(cmap->buckets, i);
		while (current != NULL) {
			Entry *next = current->next;
			current->next = all_entries;
			all_entries = current;
			current = next;
		}
		at(cmap->buckets, i) = NULL;
	}

	for (int i = old_capacity; i < new_capacity; i++) {
		append(cmap->buckets, NULL);
	}
	cmap->capacity = new_capacity;

	Entry *current = all_entries;
	while (current != NULL) {
		Entry *next = current->next;
		int new_idx = (int)(current->hash & mask);

		current->next = at(cmap->buckets, new_idx);
		at(cmap->buckets, new_idx) = current;

		current = next;
	}
}

void *map_init_impl(Arena *arena) {
	Cmap *cmap = (Cmap *)map_alloc(arena, sizeof(Cmap));
	if (cmap == NULL) {
		return NULL;
	}

	cmap->capacity = 16;
	cmap->count = 0;
	cmap->val_size = 0;
	cmap->arena = arena;
	cmap->buckets = vector_init(arena);
	cmap->keys = vector_init(arena);

	for (int i = 0; i < cmap->capacity; i++) {
		append(cmap->buckets, NULL);
	}

	return cmap;
}

void map_add_impl(Cmap *cmap, const char *key, const void *val,
				  size_t val_size) {
	if (cmap->val_size == 0) {
		cmap->val_size = val_size;
	}

	unsigned long hash = hash_function(key);
	int idx = (int)(hash & (unsigned long)(cmap->capacity - 1));

	Entry *existing = find_entry(cmap, key, hash, idx);
	if (existing != NULL) {
		memcpy(existing->data, val, val_size);
		return;
	}

	if ((float)(cmap->count + 1) / (float)cmap->capacity >=
		LOAD_FACTOR_THRESHOLD) {
		map_grow(cmap);
		idx = (int)(hash & (unsigned long)(cmap->capacity - 1));
	}

	size_t key_len = strlen(key) + 1;
	Entry *entry =
		(Entry *)map_alloc(cmap->arena, sizeof(Entry) + val_size + key_len);

	entry->hash = hash;
	entry->key_idx = length(cmap->keys);
	memcpy(entry->data, val, val_size);

	char *stored_key = entry_key(entry, val_size);
	memcpy(stored_key, key, key_len);

	entry->next = at(cmap->buckets, idx);
	at(cmap->buckets, idx) = entry;
	cmap->count++;

	append(cmap->keys, stored_key);
}

void *map_get_impl(Cmap *cmap, const char *key) {
	unsigned long hash = hash_function(key);
	int idx = (int)(hash & (unsigned long)(cmap->capacity - 1));

	Entry *entry = find_entry(cmap, key, hash, idx);
	return entry != NULL ? (void *)entry->data : NULL;
}

bool map_has_impl(Cmap *cmap, const char *key) {
	return map_get_impl(cmap, key) != NULL;
}

bool map_remove_impl(Cmap *cmap, const char *key) {
	unsigned long hash = hash_function(key);
	int idx = (int)(hash & (unsigned long)(cmap->capacity - 1));

	Entry *current = at(cmap->buckets, idx);
	Entry *prev = NULL;

	while (current != NULL) {
		if (current->hash == hash &&
			strcmp(entry_key(current, cmap->val_size), key) == 0) {
			if (prev == NULL) {
				at(cmap->buckets, idx) = current->next;
			} else {
				prev->next = current->next;
			}

			int removed_idx = current->key_idx;
			char *last_key = pop(cmap->keys);
			if (removed_idx < length(cmap->keys)) {
				at(cmap->keys, removed_idx) = last_key;
				unsigned long last_hash = hash_function(last_key);
				int last_bucket =
					(int)(last_hash & (unsigned long)(cmap->capacity - 1));
				Entry *swapped =
					find_entry(cmap, last_key, last_hash, last_bucket);
				if (swapped != NULL) {
					swapped->key_idx = removed_idx;
				}
			}

			if (cmap->arena == NULL) {
				m_align_free(current);
			}
			cmap->count--;
			return true;
		}
		prev = current;
		current = current->next;
	}
	return false;
}

Vector(char *) map_keys_impl(Cmap *cmap) { return cmap->keys; }

size_t map_len_impl(Cmap *map) { return map != NULL ? (size_t)map->count : 0; }

void map_free_impl(Cmap **cmap) {
	if (cmap != NULL && *cmap != NULL) {
		if ((*cmap)->arena == NULL) {
			for (int i = 0; i < (*cmap)->capacity; i++) {
				Entry *current = at((*cmap)->buckets, i);
				while (current != NULL) {
					Entry *next = current->next;
					m_align_free(current);
					current = next;
				}
			}
			vector_free(&(*cmap)->buckets);
			vector_free(&(*cmap)->keys);
			m_align_free(*cmap);
		}
		*cmap = NULL;
	}
}
