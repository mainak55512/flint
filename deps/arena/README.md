# Arena Allocator Library

A lightweight arena-style memory allocator for C projects.

## Features
- Simple initialization with a fixed capacity
- Automatically allocates memory if capacity is full
- Fast linear allocations using a pre-allocated buffer
- One-shot memory release via `arena_free()`
- C89 compatible

## Building & Installing

### Requirements
- CMake (>= 3.10 recommended)
- Clang or GCC

### Build Steps
You can add the library in your **[flint](https://github.com/mainak55512/flint)** project directly:

```bash
flint add https://github.com/mainak55512/arena
```
Or if you are using CMake, follow the below steps:

1. Clone the repository in your project root.
    ```sh
    git clone https://github.com/mainak55512/arena.git
    ```
2. In your `CMakeLists.txt` add the following lines:
    ```cmake
    target_include_directories(${PROJECT_NAME} PRIVATE arena/include)
    add_library(arena STATIC arena/lib/arena.c)
    target_include_directories(arena PRIVATE arena/include)
    target_link_libraries(${PROJECT_NAME} PRIVATE arena)
    ```
3. ```sh
    mkdir build
    cd build
    cmake ..
    make
    ./your-executable
    ```

## Sample CMakeLists.txt

```cmake
    cmake_minimum_required(VERSION 3.10)
    project(arena_test)

    add_executable(${PROJECT_NAME} main.c)

    target_include_directories(${PROJECT_NAME} PRIVATE arena/include)
    add_library(arena STATIC arena/lib/arena.c)
    target_include_directories(arena PRIVATE arena/include)
    target_link_libraries(${PROJECT_NAME} PRIVATE arena)
```

## Example

```c
#include <arena.h>
#include <stdio.h>

int main() {
	// Initialize the arena
	Arena *arena = arena_init(20);

	// Allocate memory on arena
	double *num = (double *)arena_alloc(arena, sizeof(double));
	double *num1 = (double *)arena_alloc(arena, sizeof(double));
	double *num2 = (double *)arena_alloc(arena, sizeof(double));
	double *num3 = (double *)arena_alloc(arena, sizeof(double));
	double *num4 = (double *)arena_alloc(arena, sizeof(double));

	printf("Num: %f\n", *num); // 0.000000

	*num = 11.22;
	*num1 = 2.345;

	printf("Num: %f\n", *num); // 11.220000
	printf("Num1: %f\n", *num1); // 2.345000

	// Reset the arena
	arena_reset(&arena);

	double *num_new = (double *)arena_alloc(arena, sizeof(double));

	printf("Num_new: %f\n", *num_new); // 0.000000

	*num_new = 4.55;

	printf("Num_new: %f\n", *num_new); // 4.550000
	printf("Num1: %f\n", *num1); // 0.000000

	// Free the arena
	arena_free(&arena);
}
```
