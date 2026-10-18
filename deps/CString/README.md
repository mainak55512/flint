# CString: Arena-Backed String Library

A lightweight, memory-efficient string manipulation library for C. This library utilizes an **Arena Allocator** to handle memory, ensuring high performance and eliminating manual `free()` calls for individual strings.

---

## Dependencies

This library requires an Arena Allocator to manage memory. You can find the required dependency here:

* **Arena Allocator**: [https://github.com/mainak55512/arena.git](https://github.com/mainak55512/arena.git)

---

## Features

* **Arena-Based Allocation:** Memory is managed in blocks, preventing fragmentation and simplifying cleanup.
* **Opaque String Type:** Encourages proper API usage through the `String` pointer.
* **Essential Utilities:** Includes trimming, concatenation, and sub-string extraction.
* **C-String Interoperability:** Easy conversion to standard `char*` for use with standard library functions.

---

## Building with CMake

This project is configured to be built as a static library. Note that the `CMakeLists.txt` is configured to use `clang-18` and the `C90` standard.

### Prerequisites
* **CMake**: Version 4.1.2 or higher
* **Compiler**: Clang 18

### Build Steps

You can add the library in your **[flint](https://github.com/mainak55512/flint)** project directly:

```bash
flint add https://github.com/mainak55512/CString
```
Or if you are using CMake, follow the below steps:

1. Clone the repository and navigate to the project folder.
2. Create a build directory:
   ```bash
   mkdir build && cd build

    ```

3. Generate the build files and compile:
```bash
cmake ..
make

```

This will produce a static library named `libcstring.a` from the source file `lib/cstring.c`.

---

## API Reference

### Initialization & Conversion

| Function | Description |
| --- | --- |
| `string_from` | Creates a new `String` from a standard C string literal. |
| `string` | Returns the raw `char*` representation of the `String`. |
| `string_len` | Returns the number of characters in the string. |
| `string_get` | Takes string input from console. |

### Manipulation

| Function | Description |
| --- | --- |
| `string_concat` | Joins n `String` objects into a new one. |
| `string_concat_cstr` | Joins n raw C-strings. |
| `string_sub` | Extracts a substring from `begin` (inclusive) to `end` (exclusive). |
| `string_trim` | Removes leading and trailing whitespace. |

---

## Usage Example

```c
#include <arena.h>
#include <cstring.h>
#include <stdio.h>

int main() {
    // 1. Initialize an Arena (e.g., 200 bytes)
    Arena *str_arena = arena_init(200);

    // 2. Create strings
    String *str = string_from(str_arena, "abcdefghijklmnopqrstuvwxyz");
    String *s1  = string_from(str_arena, "    SomeRandomName  ");

    // 3. Substring extraction (5 to 10)
    String *subStr = string_sub(str_arena, str, 5, 10);
    printf("Sub String: %s (Len: %d)\n", string(subStr), string_len(subStr));

    // 4. Concatenation and Trimming
    String *trimStr = string_trim(str_arena, s1);
    String *combined = string_concat(str_arena, 2, subStr, trimStr);
    
    printf("Result: %s\n", string(combined));

    // 5. Bulk free all memory at once - no individual string frees required!
    arena_free(&str_arena);
    return 0;
}

```

---

## Memory Management Strategy

Traditional C string manipulation often involves frequent calls to `malloc` and `realloc`, leading to memory fragmentation or leaks if a pointer is lost.

By passing an `Arena*` to every function, `CString` ensures that all memory allocated during the lifetime of your strings is contained within a single contiguous pool. This provides:

1. **Performance:** Allocation is as simple as bumping a pointer.
2. **Simplicity:** You don't need to track every individual `String` pointer; a single call to `arena_free()` at the end of your scope cleans up the entire operation.
