#ifndef CSTRING_H
#define CSTRING_H

#include <arena.h>
#include <cvector.h>
#include <stdarg.h>
#include <stddef.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <stdbool.h>
#elif !defined(__cplusplus) && !defined(__bool_true_and_false_are_defined)
typedef int bool;
#define true 1
#define false 0
#define __bool_true_and_false_are_defined 1
#endif

typedef struct String String;

/* Creates a String object from c-string (const char *) */
String *string_from(Arena *arena, const char *str);

/* Creates a String object from c-string (const char *) with specified length */
String *string_from_len(Arena *arena, size_t len, const char *str);

/* Creates a formatted String object using printf-style format specifiers */
String *string_format(Arena *arena, const char *fmt, ...);

/* Creates a deep copy of an existing String object */
String *string_clone(Arena *arena, String *str);

/* Concatinates n string objects */
String *string_concat(Arena *arena, int n, ...);

/* Concatinates n c-strings (const char *) */
String *string_concat_cstr(Arena *arena, int n, ...);

/* Returns the length of the string */
int string_len(String *str);

/* Returns a substring from a String where `begin` is inclusive but `end` is
 * exclusive */
String *string_sub(Arena *arena, String *str, int begin, int end);

/* Returns the c-string (char *) from the String object */
char *string(String *st);

/* Trims all leading and trailing white-spaces from the String */
String *string_trim(Arena *arena, String *str);

/* Takes String input from console */
String *string_get(Arena *arena);

/* Converts all lower-case characters in a string to upper-case */
String *string_upper(Arena *arena, String *str);

/* Converts all upper-case characters in a string to lower-case */
String *string_lower(Arena *arena, String *str);

/* Returns a new String with its characters reversed */
String *string_reverse(Arena *arena, String *str);

/* Repeats `str` n times into a new String */
String *string_repeat(Arena *arena, String *str, int n);

/* Compares each corresponding character of two null-terminated strings without
 * case sensitivity */
bool string_comp(struct String *s1, struct String *s2);

/* Compares each corresponding character of two null-terminated strings with
 * case sensitivity */
bool string_comp_case(struct String *s1, struct String *s2);

/* Returns true if `str` starts with `prefix` */
bool string_starts_with(String *str, const char *prefix);

/* Returns true if `str` ends with `suffix` */
bool string_ends_with(String *str, const char *suffix);

/* Returns the first byte index of `substr` in `str`, or -1 if not found */
int string_find(String *str, const char *substr);

/* Returns true if `str` contains `substr` */
bool string_contains(String *str, const char *substr);

/* Replaces all occurrences of `old_sub` with `new_sub` */
String *string_replace(Arena *arena, String *str, const char *old_sub,
                       const char *new_sub);

/* Splits `str` by `delim` into a Vector(String *) allocated on the arena */
Vector(String *) string_split(Arena *arena, String *str, const char *delim);

/* Joins a Vector(String *) into a single String separated by `delim` */
String *string_join(Arena *arena, Vector(String *) parts, const char *delim);

/* Reads an entire file from disk into an arena-allocated String */
String *string_read_file(Arena *arena, const char *filepath);

/* Writes a String's contents to a file on disk */
bool string_write_file(String *str, const char *filepath);

// -------------------- StringView Section --------------------------

// Non-owning, zero-allocation window into an existing String or char*
typedef struct {
  const char *data;
  size_t length;
} StringView;

// Printf format macros: printf("View: " SV_FMT "\n", SV_ARG(sv));
#define SV_FMT "%.*s"
#define SV_ARG(sv) (int)(sv).length, (sv).data

StringView sv_from(const char *cstr);
StringView sv_from_len(const char *data, size_t len);
StringView sv_from_string(String *str);
String *sv_to_string(Arena *arena, StringView sv);
StringView sv_sub(StringView sv, size_t begin, size_t end);
StringView sv_trim(StringView sv);
StringView sv_chop_by_delim(StringView *sv, char delim);
bool sv_eq(StringView a, StringView b);
bool sv_eq_cstr(StringView sv, const char *cstr);
bool sv_starts_with(StringView sv, const char *prefix);
bool sv_ends_with(StringView sv, const char *suffix);

#endif // CSTRING_H
