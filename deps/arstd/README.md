# `arstd` — Arena Standard Library for C

**`arstd` (Arena Standard Library)** is a lightweight, high-performance foundational toolkit for C that eliminates manual `malloc`/`free` bookkeeping by uniting memory management and core data structures under a shared **Arena Allocator**. 

Through a single umbrella header (`#include <arstd.h>`), `arstd` provides region-based memory allocation (`Arena`), compile-time type-safe segmented dynamic arrays (`Vector(T)`), single-allocation generic hash maps (`Cmap(T)`), and managed strings (`String`). All containers are engineered to allocate contiguously, grow with zero wasted arena space, and reclaim all memory in `O(1)` time with a single `arena_free(&arena)` call—while retaining an aligned heap fallback when `NULL` is passed instead of an arena.

---

## Key Features

* **Unified Arena Lifecycle (`arena.h`):** Allocate memory for vectors, hash maps, and strings from a single `Arena *`. Reset or free the entire object graph in one call without walking linked lists or freeing individual buffers.
* **Compile-Time Type Deduction (`typeof`):** Declare typed containers (`Vector(int)`, `Cmap(String *)`) that deduce element and value types automatically—no `void *` casts or passing `TYPE` to every macro call.
* **Zero-Copy Segmented Vectors (`cvector.h`):** Dynamic arrays backed by exponentially growing segments ($16, 32, 64\dots$). Resizing never calls `realloc` or `memcpy`, leaving zero dead space in the arena and keeping element pointers stable. Indexing via `at()` runs in $O(1)$ branchless time using bitwise `CLZ` math.
* **Single-Allocation Type-Safe Hash Maps (`cmap.h`):** Generic separate-chaining hash map that packs the entry header, aligned value payload, and key string into a single arena allocation (`char data[]`). Features cached 64-bit hashes, bitwise bucket masks, zero-waste resizing, and $O(1)$ `map_keys()` / `map_remove()`.
* **Managed Strings (`cstring.h`):** Safe, arena-backed string creation, slicing, trimming, case conversion, variadic concatenation, and comparison utilities.

---

## Installation & Build Steps

### Using [flint](https://github.com/mainak55512/flint)

Add `arstd` directly to your `flint` project:

```bash
flint add https://github.com/mainak55512/arstd@v0.1.5
```

### Using CMake

1. Clone the repository and navigate to the project directory:
   ```bash
   git clone https://github.com/mainak55512/arstd.git
   cd arstd
   ```
2. Create a build directory and compile:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```

This produces the static library `libarstd.a`. Include the entire toolkit in your code with:

```c
#include <arstd.h>
```
*(Or include individual modules selectively via `<arena.h>`, `<cvector.h>`, `<cmap.h>`, and `<cstring.h>`).*

---

## API Reference

### 1. Arena & Aligned Memory (`<arena.h>`)

| Function / Macro | Description | Signature |
| --- | --- | --- |
| `arena_init` | Initializes a new `Arena` with the specified initial byte capacity (rounded up to `MAX_ALIGN`). | `Arena *arena_init(size_t capacity)` |
| `arena_alloc` | Allocates a zero-initialized, maximally aligned chunk of `size` bytes from the arena (automatically chains a new block if full). | `void *arena_alloc(Arena *arena, size_t size)` |
| `arena_push` | Typed helper macro that allocates `count` zero-initialized elements of type `T` on the arena. | `T *arena_push(Arena *arena, T, size_t count)` |
| `arena_reset` | Wipes all allocations, frees any chained overflow blocks, and restores the arena to its initial base capacity. | `void arena_reset(Arena **arena)` |
| `arena_rewind` | Zeroes out all used bytes and resets offsets to `0` across all chained blocks without freeing them, keeping capacity warm for reuse. | `void arena_rewind(Arena *arena)` |
| `arena_free_secure` | Securely zeroes out and frees the arena and all chained memory blocks, setting `*arena` to `NULL`. | `void arena_free_secure(Arena **arena)` |
| `arena_free` | Frees the arena and all chained memory blocks, setting `*arena` to `NULL`. | `void arena_free(Arena **arena)` |
| `arena_temp_begin` | Takes an $O(1)$ snapshot of the arena's current position for temporary scratchpad allocations. | `ArenaTemp arena_temp_begin(Arena *arena)` |
| `arena_temp_end` | Rewinds and zeroes out all allocations made since `arena_temp_begin`, restoring the arena to the saved checkpoint. | `void arena_temp_end(ArenaTemp temp)` |
| `arena_used` | Returns the total number of bytes currently allocated across all chained blocks in the arena. | `size_t arena_used(Arena *arena)` |
| `arena_capacity` | Returns the total byte capacity allocated across all chained blocks in the arena. | `size_t arena_capacity(Arena *arena)` |
| `m_align_alloc` | Allocates zero-initialized heap memory aligned to the platform's maximum alignment requirement (`MAX_ALIGN`). | `void *m_align_alloc(size_t capacity)` |
| `m_align_realloc` | Reallocates maximally aligned heap memory from `old_size` to `new_size`. | `void *m_align_realloc(void *ptr, size_t old_size, size_t new_size)` |
| `m_align_free` | Frees memory allocated by `m_align_alloc` or `m_align_realloc`. | `void m_align_free(void *ptr)` |

---

### 2. Segmented Dynamic Array (`<cvector.h>`)

| Macro / Function | Description | Signature (as seen by user) |
| :--- | :--- | :--- |
| `Vector(TYPE)` | Declares a typed vector handle for compile-time type checking and deduction. | `Vector(TYPE) vec` |
| `vector_init` | Initializes a new segmented vector backed by `arena` (or `NULL` for heap allocation). | `Vector(TYPE) vector_init(Arena *arena)` |
| `append` | Appends a copy of `value` to the end of `vec`. | `void append(Vector(TYPE) vec, TYPE value)` |
| `at` | Accesses the element at `pos` in $O(1)$ time. Returns a **modifiable lvalue** (`at(vec, 0) = val`). | `TYPE at(Vector(TYPE) vec, int pos)` |
| `pop` | Removes and returns the last element of `vec`. | `TYPE pop(Vector(TYPE) vec)` |
| `length` | Returns the current number of elements in `vec`. | `int length(Vector(TYPE) vec)` |
| `vector_free` | Frees heap memory if initialized with `NULL` (safe pointer-nulling no-op if arena-backed). | `void vector_free(Vector(TYPE) *vec_ptr)` |

---

### 3. Type-Safe Hash Map (`<cmap.h>`)

| Macro / Function | Description | Signature (as seen by user) |
| :--- | :--- | :--- |
| `Cmap(TYPE)` | Declares a typed hash map handle with `const char *` keys and values of `TYPE`. | `Cmap(TYPE) map` |
| `map_init` | Allocates and initializes a new hash map on `arena` (or `NULL` for heap allocation). | `Cmap(TYPE) map_init(Arena *arena)` |
| `map_add` | Inserts or updates `key` by copying `val` directly into the map entry. | `void map_add(Cmap(TYPE) map, const char *key, TYPE val)` |
| `map_get` | Looks up `key` and returns a **modifiable lvalue** of type `TYPE`. | `TYPE map_get(Cmap(TYPE) map, const char *key)` |
| `map_has` | Returns `true` if `key` exists in the map, or `false` otherwise. | `bool map_has(Cmap(TYPE) map, const char *key)` |
| `map_remove` | Removes `key` from the map and updates the key list in $O(1)$ time. Returns `true` if removed. | `bool map_remove(Cmap(TYPE) map, const char *key)` |
| `map_keys` | Returns the internal `Vector(char *)` containing all active keys in $O(1)$ time. | `Vector(char *) map_keys(Cmap(TYPE) map)` |
| `map_len` | Returns the total number of active key-value pairs in $O(1)$ time. | `size_t map_len(Cmap(TYPE) map)` |
| `map_free` | Frees heap memory if initialized with `NULL` (safe pointer-nulling no-op if arena-backed). | `void map_free(Cmap(TYPE) *map_ptr)` |

---

### 4. Managed String Library (`<cstring.h>`)

| Function / Macro | Description | Signature |
| --- | --- | --- |
| `string_from` | Creates a single-allocation `String *` object from a null-terminated C-string (`const char *`). | `String *string_from(Arena *arena, const char *str)` |
| `string_from_len` | Creates a single-allocation `String *` object from a C-string with an explicit `len`. | `String *string_from_len(Arena *arena, size_t len, const char *str)` |
| `string_format` | Creates a formatted `String *` object using `printf`-style format specifiers. | `String *string_format(Arena *arena, const char *fmt, ...)` |
| `string_clone` | Creates a deep copy of an existing `String *` object on `arena`. | `String *string_clone(Arena *arena, String *str)` |
| `string_get` | Reads a line of input from the console (`stdin`) into a new `String *`. | `String *string_get(Arena *arena)` |
| `string` | Returns the underlying null-terminated `char *` buffer from a `String *`. | `char *string(String *st)` |
| `string_len` | Returns the character length of `str` in $O(1)$ time. | `int string_len(String *str)` |
| `string_sub` | Returns a substring from `begin` (inclusive) to `end` (exclusive). | `String *string_sub(Arena *arena, String *str, int begin, int end)` |
| `string_concat` | Concatenates `n` `String *` objects into a new `String *` in a single allocation. | `String *string_concat(Arena *arena, int n, ...)` |
| `string_concat_cstr` | Concatenates `n` raw C-strings (`const char *`) into a new `String *` in a single allocation. | `String *string_concat_cstr(Arena *arena, int n, ...)` |
| `string_trim` | Returns a new `String *` with all leading and trailing whitespace (`' '`, `\t`, `\n`, `\r`) removed. | `String *string_trim(Arena *arena, String *str)` |
| `string_upper` | Returns a new `String *` with all lowercase ASCII characters converted to uppercase. | `String *string_upper(Arena *arena, String *str)` |
| `string_lower` | Returns a new `String *` with all uppercase ASCII characters converted to lowercase. | `String *string_lower(Arena *arena, String *str)` |
| `string_reverse` | Returns a new `String *` with its characters reversed. | `String *string_reverse(Arena *arena, String *str)` |
| `string_repeat` | Repeats `str` `n` times into a new `String *`. | `String *string_repeat(Arena *arena, String *str, int n)` |
| `string_comp` | Case-insensitive comparison of two `String *` objects (`true` if equal). | `bool string_comp(struct String *s1, struct String *s2)` |
| `string_comp_case` | Case-sensitive comparison of two `String *` objects (`true` if equal). | `bool string_comp_case(struct String *s1, struct String *s2)` |
| `string_starts_with` | Returns `true` if `str` starts with `prefix`. | `bool string_starts_with(String *str, const char *prefix)` |
| `string_ends_with` | Returns `true` if `str` ends with `suffix`. | `bool string_ends_with(String *str, const char *suffix)` |
| `string_find` | Returns the first byte index of `substr` in `str`, or `-1` if not found. | `int string_find(String *str, const char *substr)` |
| `string_contains` | Returns `true` if `str` contains `substr`. | `bool string_contains(String *str, const char *substr)` |
| `string_replace` | Replaces all occurrences of `old_sub` with `new_sub` in a new `String *`. | `String *string_replace(Arena *arena, String *str, const char *old_sub, const char *new_sub)` |
| `string_split` | Splits `str` by `delim` into an arena-backed `Vector(String *)` (or into individual characters if `delim` is `""`). | `Vector(String *) string_split(Arena *arena, String *str, const char *delim)` |
| `string_join` | Joins a `Vector(String *)` into a single `String *` separated by `delim`. | `String *string_join(Arena *arena, Vector(String *) parts, const char *delim)` |
| `string_read_file` | Reads an entire file from disk into an arena-allocated `String *`. | `String *string_read_file(Arena *arena, const char *filepath)` |
| `string_write_file` | Writes a `String *`'s contents to a file on disk (`true` on success). | `bool string_write_file(String *str, const char *filepath)` |
| `sv_from` | Creates a zero-allocation `StringView` from a null-terminated C-string (`const char *`). | `StringView sv_from(const char *cstr)` |
| `sv_from_len` | Creates a zero-allocation `StringView` from a pointer and explicit byte length. | `StringView sv_from_len(const char *data, size_t len)` |
| `sv_from_string` | Creates a zero-allocation `StringView` viewing an existing `String *`. | `StringView sv_from_string(String *str)` |
| `sv_to_string` | Promotes a `StringView` slice into an arena-allocated, null-terminated `String *`. | `String *sv_to_string(Arena *arena, StringView sv)` |
| `sv_sub` | Returns a zero-allocation `StringView` slice from `[begin, end)` in $O(1)$ time. | `StringView sv_sub(StringView sv, size_t begin, size_t end)` |
| `sv_trim` | Returns a zero-allocation `StringView` with leading and trailing whitespace stripped. | `StringView sv_trim(StringView sv)` |
| `sv_chop_by_delim` | Splits `*sv` at the first `delim`, returning the left slice and advancing `*sv` past `delim`. | `StringView sv_chop_by_delim(StringView *sv, char delim)` |
| `sv_eq` | Case-sensitive equality check between two `StringView` slices. | `bool sv_eq(StringView a, StringView b)` |
| `sv_eq_cstr` | Case-sensitive equality check between a `StringView` and a C-string (`const char *`). | `bool sv_eq_cstr(StringView sv, const char *cstr)` |
| `sv_starts_with` | Returns `true` if `sv` starts with `prefix`. | `bool sv_starts_with(StringView sv, const char *prefix)` |
| `sv_ends_with` | Returns `true` if `sv` ends with `suffix`. | `bool sv_ends_with(StringView sv, const char *suffix)` |
| `SV_FMT` / `SV_ARG` | `printf` format specifier (`"%.*s"`) and argument expansion macro for printing a `StringView`. | `SV_FMT`, `SV_ARG(sv)` |

---

## Unified Example

The example below demonstrates how `Arena`, `Vector(T)`, `Cmap(T)`, and `String` work together seamlessly without a single manual `free()` call for individual containers or strings:

```c
#include <arstd.h>
#include <stdio.h>

typedef struct {
    int id;
    const char *arch;
} BuildTarget;

int main(void) {
    // 1. Initialize a single shared Arena for the entire program/task
    Arena *arena = arena_init(4096);

    // 2. Create, format, and manipulate managed Strings on the arena
    String *raw_title = string_from(arena, "   arstd: arena standard library   ");
    String *trimmed   = string_trim(arena, raw_title);
    String *banner    = string_upper(arena, trimmed); // Now returns String *
    String *header    = string_format(arena, "=== %s (len: %d) ===",
                                      string(banner), string_len(trimmed));
    printf("%s\n\n", string(header));

    // 3. Split, Replace, and Join Strings with Vector(String *)
    String *csv_langs = string_from(arena, "c89,c99,c11,c23");
    Vector(String *) stds = string_split(arena, csv_langs, ",");
    at(stds, 1) = string_replace(arena, at(stds, 1), "c99", "c99 (strict)");

    String *joined_stds = string_join(arena, stds, " | ");
    printf("Supported Standards (%d): %s\n\n", length(stds), string(joined_stds));

    // 4. Zero-allocation StringView parsing (0 arena bytes consumed!)
    String *config_line = string_from(arena, "  opt_level = release_O3  ");
    StringView sv = sv_trim(sv_from_string(config_line));
    StringView key = sv_trim(sv_chop_by_delim(&sv, '='));
    StringView val = sv_trim(sv);
    printf("Parsed Config via StringView -> Key: '" SV_FMT "', Val: '" SV_FMT "'\n\n",
           SV_ARG(key), SV_ARG(val));

    // 5. Typed allocation via arena_push & Segmented Vector of integers
    BuildTarget *target = arena_push(arena, BuildTarget, 1);
    target->id = 1;
    target->arch = "x86_64";

    Vector(int) primes = vector_init(arena);
    append(primes, 2);
    append(primes, 3);
    append(primes, 5);
    at(primes, 2) = 7; // Direct lvalue modification via at()

    printf("Target [%d: %s] | Primes (%d items): ",
           target->id, target->arch, length(primes));
    for (int i = 0; i < length(primes); i++) {
        printf("%d ", at(primes, i));
    }
    printf("\n\n");

    // 6. Temporary Scratchpad Scope (ArenaTemp)
    //    Any temporary Strings, Vectors, or Maps inside this block are rewound!
    size_t used_before = arena_used(arena);
    {
        ArenaTemp scratch = arena_temp_begin(arena);

        String *temp_path = string_concat_cstr(arena, 3, "/tmp/", target->arch, "/build.log");
        if (string_ends_with(temp_path, ".log") && string_contains(temp_path, "x86_64")) {
            printf("Scratch Check: Validated temporary path '%s'\n", string(temp_path));
        }

        arena_temp_end(scratch); // Rewinds and zeroes temporary memory back to checkpoint
    }
    printf("Arena Memory Introspection: %zu B used / %zu B capacity (Temp reclaimed!)\n\n",
           arena_used(arena), arena_capacity(arena));

    // 7. Create a typed Hash Map mapping usernames (const char *) to String* objects
    Cmap(String *) profiles = map_init(arena);

    map_add(profiles, "alice",   string_concat_cstr(arena, 2, "Role: ", "Architect"));
    map_add(profiles, "bob",     string_concat_cstr(arena, 2, "Role: ", "Systems Engineer"));
    map_add(profiles, "charlie", string_from(arena, "Role: Maintainer"));

    // Mutate an entry in-place and remove another
    map_get(profiles, "bob") = string_from(arena, "Role: Lead Systems Engineer");
    map_remove(profiles, "charlie");

    // 8. Iterate over active map keys in O(1) time
    Vector(char *) keys = map_keys(profiles);
    printf("Active Profiles (%zu):\n", map_len(profiles));
    for (int i = 0; i < length(keys); i++) {
        char *user = at(keys, i);
        String *info = map_get(profiles, user);
        printf("  - %-8s -> %s\n", user, string(info));
    }

    // 9. Securely wipe and reclaim ALL memory in one call!
    //    (Or use arena_rewind(arena) inside loops to reuse warm blocks with 0 mallocs)
    arena_free(&arena);
    return 0;
}
```
