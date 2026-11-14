#include <cvector.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define VEC_BASE_SHIFT 4
#define VEC_BASE_CAP (1U << VEC_BASE_SHIFT)
#define VEC_MAX_SEGS 28

struct Vector {
  Arena *arena;
  size_t element_size;
  int capacity;
  int length;
  int allocated_segs;
  char *segments[VEC_MAX_SEGS];
};

static uint32_t vec_clz32(uint32_t x) {
#if defined(_MSC_VER)
  unsigned long idx;
  _BitScanReverse(&idx, x);
  return 31 - idx;
#elif defined(__GNUC__) || defined(__clang__)
  return (uint32_t)__builtin_clz(x);
#else
  uint32_t n = 0;
  if (x <= 0x0000FFFF) {
    n += 16;
    x <<= 16;
  }
  if (x <= 0x00FFFFFF) {
    n += 8;
    x <<= 8;
  }
  if (x <= 0x0FFFFFFF) {
    n += 4;
    x <<= 4;
  }
  if (x <= 0x3FFFFFFF) {
    n += 2;
    x <<= 2;
  }
  if (x <= 0x7FFFFFFF) {
    n += 1;
  }
  return n;
#endif
}

static void *vec_alloc(Arena *arena, size_t size) {
  if (arena != NULL) {
    return arena_alloc(arena, size);
  }
  return m_align_alloc(size);
}

static char *vec_slot_ptr(Vector *vector, int pos) {
  uint32_t shifted = (uint32_t)pos + VEC_BASE_CAP;
  uint32_t bit = 31 - vec_clz32(shifted);
  uint32_t seg_idx = bit - VEC_BASE_SHIFT;
  uint32_t offset = shifted & ~(1U << bit);

  return vector->segments[seg_idx] + ((size_t)offset * vector->element_size);
}

void *vector_init_impl(Arena *arena, size_t element_size) {
  Vector *vector = (Vector *)vec_alloc(arena, sizeof(Vector));
  if (vector == NULL) {
    return NULL;
  }

  vector->arena = arena;
  vector->element_size = element_size;
  vector->capacity = 0;
  vector->length = 0;
  vector->allocated_segs = 0;
  return vector;
}

void append_impl(Vector *vector, const void *element, size_t element_size) {
  if (vector->element_size == 0) {
    vector->element_size = element_size;
  }
  uint32_t pos = (uint32_t)vector->length;
  uint32_t shifted = pos + VEC_BASE_CAP;
  uint32_t bit = 31 - vec_clz32(shifted);
  uint32_t seg_idx = bit - VEC_BASE_SHIFT;
  uint32_t offset = shifted & ~(1U << bit);

  if ((int)seg_idx >= vector->allocated_segs) {
    size_t seg_cap = (size_t)1U << bit;
    vector->segments[seg_idx] =
        (char *)vec_alloc(vector->arena, seg_cap * vector->element_size);
    vector->allocated_segs++;
    vector->capacity += (int)seg_cap;
  }

  memcpy(vector->segments[seg_idx] + ((size_t)offset * vector->element_size),
         element, vector->element_size);
  vector->length++;
}

void *pop_impl(Vector *vector) {
  if (vector->length > 0) {
    vector->length--;
    return vec_slot_ptr(vector, vector->length);
  }
  return NULL;
}

void *at_impl(Vector *vector, int pos) {
  if (pos >= 0 && pos < vector->length) {
    return vec_slot_ptr(vector, pos);
  }
  return NULL;
}

int length_impl(Vector *vector) { return vector->length; }

void vector_free_impl(Vector **vector) {
  int i;
  if (vector != NULL && *vector != NULL) {
    if ((*vector)->arena == NULL) {
      for (i = 0; i < (*vector)->allocated_segs; i++) {
        m_align_free((*vector)->segments[i]);
      }
      m_align_free(*vector);
    }
    *vector = NULL;
  }
}
