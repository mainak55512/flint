#ifndef CVECTOR_H
#define CVECTOR_H

#include <arena.h>
#include <stddef.h>

typedef struct Vector Vector;

#define CONCAT2(a, b) a##b
#define CONCAT(a, b) CONCAT2(a, b)

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define VEC_TYPEOF(x) typeof(x)
#else
#define VEC_TYPEOF(x) __typeof__(x)
#endif

#define Vector(T) T *

void *vector_init_impl(Arena *arena, size_t element_size);
#define vector_init(arena) vector_init_impl((arena), 0)

void append_impl(Vector *v, const void *element, size_t element_size);
#define append(vec, value)                                                     \
  do {                                                                         \
    VEC_TYPEOF(*(vec)) CONCAT(_tmp_, __LINE__) = (value);                      \
    append_impl((Vector *)(vec), &CONCAT(_tmp_, __LINE__), sizeof(*(vec)));    \
  } while (0)

void *at_impl(Vector *vector, int pos);
#define at(vec, pos) (*(VEC_TYPEOF(vec))at_impl((Vector *)(vec), (pos)))

void *pop_impl(Vector *vector);
#define pop(vec) (*(VEC_TYPEOF(vec))pop_impl((Vector *)(vec)))

int length_impl(Vector *vector);
#define length(vec) length_impl((Vector *)(vec))

void vector_free_impl(Vector **vector);
#define vector_free(vec_ptr) vector_free_impl((Vector **)(vec_ptr))

#endif // CVECTOR_H
