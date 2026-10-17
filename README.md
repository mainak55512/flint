<div align="center">
<img src="./assets/flint.png" alt="Flint Logo" width="320">
<h1>Flint</h1>
<p><b>A small, Git-native build tool for C/C++. One manifest file, no CMake.</b></p>

[![Status](https://img.shields.io/badge/status-beta-orange?style=flat-square)](#limitations-and-roadmap)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-blue?style=flat-square)](#install)
[![License](https://img.shields.io/badge/license-MIT-green?style=flat-square)](LICENSE)

[CLI Docs](https://mainak55512.github.io/flint-cherts/) · [Chert Compositions](https://mainak55512.github.io/flint-cherts/compositions/) · [Issues](https://github.com/mainak55512/flint/issues)

</div>

---

Flint builds C and C++ projects and fetches their dependencies straight from Git repositories. You describe the project in a single `composition.json` and Flint drives `gcc` or `clang` for you. There is no generator step and no runtime beyond `git` and a compiler.

Flint is written in C and builds itself with Flint (see its own [`composition.json`](./composition.json)).

<div align="center">
  <img src="assets/demo.gif" alt="Flint Terminal Demo" width="85%" />
</div>

## Try it in 60 seconds

```bash
mkdir hello && cd hello
flint init
flint add [https://github.com/mainak55512/Cmap@v0.1.1](https://github.com/mainak55512/Cmap@v0.1.1)
flint run

```

`flint add` writes the dependency into your manifest, including the exact commit hash it resolved the tag to.

## Why Flint?

* **One manifest.** Name, compiler, flags, and dependencies live in one file.
* **Git is the package source.** Any repository with a tag or commit can be a dependency. No account, no registry to publish to.
* **Commit-pinned dependencies.** Each dependency records a version (tag) and a commit `hash`, so builds do not silently change if a tag moves.
* **Dedicated `sync` section.** Manage external/chert-synced repositories cleanly in their own dedicated `sync` block rather than mixing them into standard `dependencies`.
* **Source discovery by convention.** Flint finds sources, headers, and static/shared libraries in the standard directories. No file lists to maintain.
* **Incremental builds.** Flint checks whether files have been modified since the last build. If unchanged, Flint skips compilation for that file.
* **Editor support out of the box.** Flint generates `compile_commands.json`, so clangd, Neovim, Helix, and VS Code index your code and your dependencies.
* **Small footprint.** Needs only `git` and `gcc` or `clang` (with a MinGW backend on Windows). No Python, no Ninja, no CMake.
* **Version header generation.** `PROJECT_VERSION` is injected into your build, and `.h.in` templates are processed natively.

## Install

**Requirements:**

* `git`
* **Linux / macOS:** `gcc` or `clang`
* **Windows:** `gcc` or `clang` targeting the **MinGW** backend (e.g., `x86_64-w64-mingw32-gcc`, MSYS2 MinGW, or `clang` with MinGW target)

**Pre-built binaries**

From v0.5.20, pre-built binaries for Linux, macOS and windows are available in the [releases](https://github.com/mainak55512/flint/releases) section.


If you want to build flint from source follow the below steps:


**Linux & macOS:**

```bash
git clone https://github.com/mainak55512/flint.git
cd flint && ./build.sh

```

**Windows (PowerShell):**

```powershell
git clone https://github.com/mainak55512/flint.git
cd flint; 
.\build.ps1

```


## Project layout

Flint expects a conventional layout (directory names are configurable, see the CLI docs):

```
my_project/
├── src/                # .c / .cpp sources
├── include/            # your headers
├── deps/               # dependencies (managed by Flint)
├── static/             # prebuilt static libraries (.a / .lib)
├── shared/             # prebuilt shared libraries (.so / .dll)
└── composition.json    # manifest

```

Anything listed in `exclude_dirs` or `excludes` is skipped during source discovery.

## The manifest

This is Flint's own manifest:

```jsonc
{
    // Project configuration
    "project_name": "flint",
    "project_language": "c",
    "version": "v0.5.20",
    "compiler_path": "clang",
    "executable": true,

    // Build flags
    "flags": [
        "-O3",
        "-Wno-unused-command-line-argument",
        "-s",
        "-fno-stack-protector"
    ],

    // Skipped during source discovery
    "excludes": ["src/config_refactor.c"],
    "exclude_dirs": ["build"],

    // Native Flint dependencies
    "dependencies": {
        "arena": {
            "version": "v0.1.1",
            "remote": "[https://github.com/mainak55512/arena](https://github.com/mainak55512/arena)",
            "hash": "b1659723d4d45a7eba5a8b12648a22f0212da17e"
        },
        "CString": {
            "version": "v0.1.1",
            "remote": "[https://github.com/mainak55512/CString](https://github.com/mainak55512/CString)",
            "hash": "a58d598a907596bac876dbbaa7482d0d2a7e91cc"
        },
        "container": {
            "version": "v0.1.1",
            "remote": "[https://github.com/mainak55512/container](https://github.com/mainak55512/container)",
            "hash": "c2a3a5af3f5ebe3f63cf540073021dde24e56e38"
        },
        "Cmap": {
            "version": "v0.1.1",
            "remote": "[https://github.com/mainak55512/Cmap](https://github.com/mainak55512/Cmap)",
            "hash": "5827928228c9d2c44eacbe44b4457b496554be57"
        }
    },

    // Synced repositories (e.g., chert compositions for non-Flint libraries)
    "sync": {
        "yyjson": {
            "version": "0.12.0",
            "remote": "[https://github.com/ibireme/yyjson](https://github.com/ibireme/yyjson)",
            "hash": "8b4a38dc994a110abaec8a400615567bd996105f"
        }
    }
}

```

| Field | Meaning |
| --- | --- |
| `project_name` | Name of the produced artifact |
| `project_language` | `"c"` or `"cpp"` |
| `version` | Project version, exposed as the `PROJECT_VERSION` macro |
| `compiler_path` | Compiler executable to use (`gcc` or `clang`; uses MinGW backend on Windows) |
| `executable` | `true` for a program, `false` for a library |
| `flags` | Flags passed to the compiler |
| `lib_links` | Extra libraries to link |
| `excludes` | Specific source files ignored during source discovery |
| `exclude_dirs` | Directories ignored during source discovery |
| `dependencies` | Map of native Flint dependencies with `version` (tag/branch), `remote` (Git URL), and `hash` (exact commit) |
| `sync` | Dedicated section for synced dependencies (such as third-party libraries configured via chert compositions) |

## Template headers

Flint turns a `*.h.in` template into a header by replacing `@KEY@` placeholders with the values in the manifest's `tmpl` block.

`include/config.h.in:`

```c
#define APP_NAME "@APP_NAME@"
#define HAVE_STRLCPY @HAVE_STRLCPY@
#define SIZEOF_LONG @SIZEOF_LONG@

```

`composition.json:`

```jsonc
{
    "tmpl": {
        "APP_NAME": "myapp",
        "HAVE_STRLCPY": "0",
        "SIZEOF_LONG": "8"
    }
}

```

`Generated include/config.h:`

```c
#define APP_NAME "myapp"
#define HAVE_STRLCPY 0
#define SIZEOF_LONG 8

```

Because values are fixed in the manifest, Flint does not detect them for you. If a library needs different values on different systems, you set them yourself.

## Using libraries that don't have a `composition.json`

Most existing C/C++ libraries were not written for Flint. For those, Flint uses **chert compositions**: a `composition.json` maintained separately in the [flint-cherts](https://github.com/mainak55512/flint-cherts) repository that tells Flint how to build the library from source.

Rather than placing these under `dependencies`, you define them under the dedicated **`sync`** section in your `composition.json` so Flint knows to sync their chert composition when fetching.

Be aware of what this means:

* It works best for libraries that are plain source trees: no code generation, no `configure` step, no platform probing.
* Flint can generate headers from templates: it turns `config.h.in` into `config.h` by substituting `@VARIABLE@` placeholders, the same convention autotools and CMake use. Libraries that only need a templated config header can work. Values are declared as key/value pairs in the `tmpl` block of the manifest. See [Template headers](https://www.google.com/search?q=%23template-headers).
* `flint-cherts` is a community-maintained list of recipes, so in practice it plays the role of a small registry. Dependencies themselves still come from Git.

**Use something else if** you need MSVC-specific toolchains, a large package ecosystem, binary caches, IDE-generated projects, or complex conditional builds. **Flint fits** small to medium projects where the build is "these sources plus these Git dependencies" and you want to stop maintaining build scripts.

## Limitations and roadmap

Honest status of the beta:

* Platforms: Linux, macOS, and Windows (`gcc` and `clang` via MinGW backend; no native MSVC `cl.exe` support for now).
* Cross-compilation: not first-class. You can point `compiler_path` at a cross compiler and pass flags.
* Build profiles: a single `flags` list; no separate debug/release profiles yet.
* Conditionals: no per-platform or per-option settings in the manifest.
* Targets: one artifact per manifest.
* Tests and install targets: not built in.
* Build performance: Builds are incremental.
* Auto-discovered `static/` and `shared/` libraries are linked as found, so keep those directories clean.

Planned:

* [x] Support for Windows (`gcc` & `clang` with MinGW backend)
* [ ] Support for separate platform configurations
* [ ] Support for separate build profiles
* [ ] Support for test executions

Feedback on which of these matters most is the most useful contribution right now.

## Contributing

* Report bugs and request features in [Issues](https://github.com/mainak55512/flint/issues).
* Add a library by writing a chert composition: see the [flint-cherts guide](https://github.com/mainak55512/flint-cherts) and open a pull request.
* Libraries built with Flint can be added by anyone with `flint add <repo_url>@<version>`.

## License

MIT. See [LICENSE](LICENSE).
