# Simple C Vector Container

This library provides a simple, generic dynamic array implementation (similar to `std::vector` in C++) for C, utilizing a single header file and **C preprocessor macros** to offer a type-safe interface. It allows you to create resizable arrays of any data type (`int`, `char*`, `structs`, etc.) without requiring explicit manual memory management for the underlying data block.

---

## Usage and Features

The main structure for the dynamic array is opaque (`Vector*`). All core functionalities are exposed via **macros** to handle size calculations and type casting automatically, providing an interface close to a built-in container.


### Build Steps

You can add the library in your **[flint](https://github.com/mainak55512/flint)** project directly:

```bash
flint add https://github.com/mainak55512/container@v0.1.1
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

This will produce a static library named `libccontainer.a` from the source file `lib/cvector.c`.

### Key Functions and Macros

| Function/Macro | Description | C Signature (as seen by user) |
| :--- | :--- | :--- |
| `vector_init` | Initializes a new `Vector`. Must be provided the **type** of element it will store. | `Vector* vector_init(TYPE)` |
| `append` | Appends a **copy** of the provided `value` to the end of the `Vector`. | `append(type, vec, value)` |
| `at` | Accesses the element at the specified `pos`. Returns a **dereferenceable value** of the expected type. | `TYPE at(TYPE, vec, pos)` |
| `pop` | Removes and returns the **last element** of the `Vector`. | `TYPE pop(TYPE, vec)` |
| `replace_at` | Replaces the element at the specified `pos` with a **copy** of the new `value`. | `replace_at(type, vec, pos, value)` |
| `length` | Returns the current number of elements in the `Vector`. | `int length(Vector* vector)` |
| `vector_free` | Releases the memory associated with the `Vector` and its contents. | `void vector_free(Vector* vector)` |

---

## 💡 Example

```c
#include <stdio.h>
#include <container.h>

int main() {
    // 1. Initialize a vector to hold integers
    Vector* int_vec = vector_init(int);
    
    // 2. Append values
    append(int, int_vec, 10);
    append(int, int_vec, 20);
    append(int, int_vec, 30);
    
    printf("Current Length: %d\n", length(int_vec));         // Output: Current Length: 3

    // 3. Access and replace element at index 0
    printf("Value at index 0: %d\n", at(int, int_vec, 0));   // Output: 10
    replace_at(int, int_vec, 0, 99);
    printf("Value after replace: %d\n", at(int, int_vec, 0)); // Output: 99

    // 4. Pop the last element
    int last_element = pop(int, int_vec);
    printf("Popped element: %d\n", last_element);           // Output: 30
    printf("New Length: %d\n", length(int_vec));            // Output: 2

    // 5. Free the vector memory
    vector_free(int_vec);
    
    return 0;
}
