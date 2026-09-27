<div align="center">
<img src="./assets/flint.png" alt="Flint Logo" width="360">
<h1>Flint</h1>
<p><b>The Cargo experience for C/C++—minimalist, Git-native, and CMake-free.</b></p> 
<a href="https://mainak55512.github.io/flint-cherts/"><strong>CLI Docs</strong></a> | 
<a href="https://mainak55512.github.io/flint-cherts/compositions/"><strong>Chert Compositions</strong></a>
<br><br>
  
</div>
<div align="center">
  
![Platform](https://img.shields.io/badge/platform-POSIX-blue?style=flat-square) ![Status](https://img.shields.io/badge/status-Beta-orange?style=flat-square) ![License](https://img.shields.io/badge/license-MIT-green?style=flat-square)

</div>

---

**Flint** brings the modern developer workflow of Rust’s `cargo` or Go modules to C and C++. 

Instead of writing hundreds of lines of complex `CMakeLists.txt` or Makefile glue code, Flint manages dependencies directly from standard Git repositories and drives GCC/Clang compilers automatically through a single JSON manifest.

---

## Demo

<div align="center">
  <br />
  <img src="assets/demo.gif" alt="Flint Terminal Demo" width="85%" />
  <p><sub><i>Flint initializing a workspace, fetching dependencies, and running a build.</i></sub></p>
  <br />
</div>

---

## Features

- **Cargo-Like Simplicity:** Build, sync dependencies, and execute code in a single command.
- **Git-Native Package Management:** Fetch dependencies directly into standard subdirectories using Git URLs.
- **Version & Commit Pinning:** Lock dependencies by release tag, branch, or exact commit hash.
- **Single Manifest (`composition.json`):** One human-readable file replaces entire build script directory structures.
- **Chert Compositions:** Instant compatibility layer for standard C/C++ repositories without a native `composition.json`.
- **Global `VERSION` Macro:** Automatically inject project version headers into C/C++ source code during compilation.
- **Convention Over Configuration:** Clean standard directory layout (`src/`, `include/`, `deps/`).

---

## Quick Start

### Prerequisites
- **OS:** Linux *(Beta)*
- **Compiler:** `gcc` or `clang`
- **Tool:** `git`

### 1. Installation

Install via official script:

```bash
curl -fsSL -H "Accept: application/vnd.github.v3.raw" https://api.github.com/repos/mainak55512/flint/contents/install.sh | bash
```

Or build from source:

Bash

```
git clone https://github.com/mainak55512/flint.git
cd flint && ./install.sh
```

### 2. Quick Workflow

```bash
# Initialize a new C project workspace
flint init

# Add a Git dependency
flint add https://github.com/user/example_lib@1.0.0

# Compile and execute immediately
flint run

```

## Project Structure

Flint enforces a clean, zero-config directory model:

```
my_project/
├── src/                # Source files (.c, .cpp)
├── include/            # Local header files (.h, .hpp)
├── deps/               # External dependencies (Managed by Flint)
├── static/             # Static library files (.a)
├── shared/             # Dynamic/Shared library files (.so)
└── composition.json    # Project manifest & build configuration

```

## Manifest Specification (`composition.json`)

```json
{
  "project_name": "example_project",
  "project_language": "c",
  "version": "0.1.0",
  "compiler_path": "/usr/bin/gcc",
  "executable": true,
  "flags": ["-Wall", "-O2"],
  "lib_links": [],
  "include_paths": ["include"],
  "src": ["src"],
  "dependencies": {
    "example_lib": {
      "version": "1.0.0",
      "remote": "https://github.com/user/example_lib.git"
    }
  }
}

```

## How Flint Compares

| Feature / Attribute | Flint | CMake + vcpkg | cmkr + FetchContent | Meson + WrapDB |
|---|---|---|---|---|
| Tool Architecture | Integrated Build System & PM | Meta-Build Generator + Standalone PM | Meta-Wrapper Generator (outputs CMakeLists.txt) | Meta-Build Generator + Package Resolver |
| Runtime Dependencies | System git, gcc/clang | C++ runtime, git, build backend (ninja/make) | cmkr, CMake, git, build backend | Python 3, ninja, git |
| Config Format | composition.json (JSON) | CMakeLists.txt + vcpkg.json (Custom DSL + JSON) | cmake.toml (TOML) | meson.build (Declarative DSL) |
| Package Management Method | Git-native clones to deps/ via CLI (flint add) | Manifest/Port-tree repos & binary caching | CMake FetchContent (configure-time download) | Wrap files (.wrap) & WrapDB registry |
| Non-Native Library Handling | Chert Compositions (custom specs for non-Flint repos) | vcpkg Port Overlay recipes | Requires manual CMake target wrapping | Meson Wrap subproject patches |
| Incremental Build Engine | Native state/modification tracking | Delegated to backend (Ninja/Make) | Delegated to CMake backend | Delegated to Ninja | 
| Directory Model | Flexible | Fully explicit & customisable | Explicit & customisable | Explicit & customisable |
| Platform Support | Linux & macOS (Current) | Cross-Platform (Linux, macOS, Windows) | Cross-Platform (Linux, macOS, Windows) | Cross-Platform (Linux, macOS, Windows) |
| Cross-Compilation | Flags passed to system compiler | Toolchain files (-DCMAKE_TOOLCHAIN_FILE) | Toolchain files via CMake | Cross-definition files (--cross-file) |


## Community & Support

-   **CLI Reference:** [Flint Documentation](https://mainak55512.github.io/flint-cherts/)    
       
-   **Chert Packages:** [Flint Cherts Repository](https://mainak55512.github.io/flint-cherts/compositions/)
             
-   **Bug Reports & Requests:** [GitHub Issues](https://github.com/mainak55512/flint/issues)

### Package Discovery & Contributions

When you create a new library project using **`flint`**, it is automatically designed to be modular and can be fetched directly by other projects via `flint add <repo_url>@<version>`.

If you are working with a **legacy or existing third-party C/C++ library** that doesn't natively support `flint`, you can still use it by defining a chert composition (`composition.json`). 

#### How to Contribute Legacy Libraries:
We maintain a community repository of pre-made library compositions called **[flint-cherts](https://github.com/mainak55512/flint-cherts)**. 

If you write a `chert composition` for a popular C/C++ library, contributions to `flint-cherts` are highly appreciated! 

1. Check out the **[flint-cherts README Guide](https://github.com/mainak55512/flint-cherts)** to learn how to structure a `composition.json`.
2. Submit a Pull Request with your library composition.
3. Help the community seamlessly use legacy C/C++ packages without needing manual setup!
    
         
## License

Distributed under the **MIT License**. See `LICENSE` for details.
