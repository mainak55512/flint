# CMap: Arena-Backed Hash Map Library

A lightweight, memory-efficient hash map implementation for C using separate chaining for collision resolution. This library utilizes an **Arena Allocator** to manage memory for its nodes and internal structures, ensuring high performance, low fragmentation, and effortless bulk memory cleanup.

---

## Dependencies

This library requires an Arena Allocator and Container utilities to manage internal storage and return key collections. You can find the required dependencies here:

* **Arena Allocator**: [https://github.com/mainak55512/arena.git](https://github.com/mainak55512/arena.git)
* **Container Utilities**: [https://github.com/mainak55512/container.git](https://github.com/mainak55512/container.git)

---

## Features

* **Arena-Based Allocation:** Map metadata, buckets, and entry nodes are stored within an arena, eliminating individual node allocations.
* **Collision Resolution:** Implements linked-list chaining (`Entry *next`) to handle hash collisions safely.
* **Dynamic Resizing:** Automatically grows bucket capacity when the key count exceeds the load factor threshold (`0.75`).
* **Key Introspection:** Allows retrieval of all active keys as a `Vector*`.
* **Generic Values:** Stores `void*` payloads, allowing any data type or structure pointer to be associated with string keys.

---

## Building with CMake

This project is configured to be built as a static library. Note that the `CMakeLists.txt` is configured to use `clang-18` and the `C90` standard.

### Prerequisites

* **CMake**: Version 4.1.2 or higher
* **Compiler**: Clang 18

### Build Steps

You can add the library in your **[flint](https://github.com/mainak55512/flint)** project directly:

```bash
flint add https://github.com/mainak55512/Cmap

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

This will produce a static library named `libcmap.a` from the source file `lib/cmap.c`.

---

## API Reference

### Initialization & Lifecycle

| Function | Description |
| --- | --- |
| `map_init` | Allocates and initializes a new `Cmap` structure. |
| `map_free` | Releases all resources allocated by the map via its backing arena. |
| `map_reset` | Clears all entry data and resets key/value pairs without deallocating the map handle. |

### Access & Mutation

| Function | Description |
| --- | --- |
| `map_add` | Inserts or updates a key-value pair (`const char*`, `void*`) into the map. |
| `map_get` | Looks up a string key and returns the associated `void*` pointer (or `NULL` if missing). |
| `map_keys` | Collects all non-null keys across buckets and returns them inside a `Vector*`. |

---

## Usage Example

```c
#include <cmap.h>
#include <stdio.h>

int main() {
    // 1. Initialize the map (allocates internal arena and buckets)
    Cmap *map = map_init();

    // 2. Insert key-value pairs
    int age = 25;
    char *city = "Tokyo";

    map_add(map, "user_age", &age);
    map_add(map, "user_city", city);

    // 3. Lookup values
    int *retrieved_age = (int *)map_get(map, "user_age");
    char *retrieved_city = (char *)map_get(map, "user_city");

    if (retrieved_age && retrieved_city) {
        printf("User Age: %d\n", *retrieved_age);
        printf("User City: %s\n", retrieved_city);
    }

    // 4. Retrieve all keys
    Vector *keys = map_keys(map);
    printf("Total Keys in Map: %d\n", map->count);

    // 5. Bulk free all memory (map handle, buckets, and nodes) at once
    map_free(map);
    return 0;
}

```

---

## Memory Management Strategy

Standard hash map implementations in C typically perform separate `malloc` calls for every `Entry` node, bucket array allocation, and dynamic resize operation. This often leads to scattered heap allocations and tedious traversal loops during cleanup.

By encapsulating an `Arena*` directly inside the `Cmap` structure, `CMap` handles memory bound to map operations within a unified allocation strategy. This provides:

1. **Performance:** Node allocations and bucket resizes perform pointer bumps on the internal arena instead of expensive system calls.
2. **Simplicity:** A single call to `map_free()` instantly reclaims the entire map—including buckets, chains, and auxiliary vectors—without manually traversing chains to `free()` individual nodes.
