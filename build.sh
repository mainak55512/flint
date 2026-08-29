#!/usr/bin/env bash

set -e

REPO_URL="https://github.com/mainak55512/flint.git"

ORIGINAL_DIR="$(pwd)"

echo "* Checking dependencies..."

if ! command -v git >/dev/null 2>&1; then
    echo "! Error: 'git' is not installed or not found in your PATH." >&2
    exit 1
fi

HAS_GCC=0
HAS_CLANG=0

if command -v gcc >/dev/null 2>&1; then
    echo "  -> Found gcc"
    HAS_GCC=1
fi

if command -v clang >/dev/null 2>&1; then
    echo "  -> Found clang"
    HAS_CLANG=1
fi

if [ $HAS_GCC -eq 1 ]; then
    COMPILER="gcc"
elif [ $HAS_CLANG -eq 1 ]; then
    COMPILER="clang"
else
    echo "! Error: Neither gcc nor clang was found in your PATH." >&2
    exit 1
fi

TEMP_DIR=$(mktemp -d)

cleanup() {
    rm -rf "$TEMP_DIR"
}
trap cleanup EXIT

echo "* Cloning Flint repository..."
git clone --depth 1 "$REPO_URL" "$TEMP_DIR"
cd "$TEMP_DIR"

echo "* Compiling Flint (Release) using $COMPILER..."

$COMPILER \
  -O3 \
  -DNDEBUG \
  -fstack-protector-strong \
  -D_FORTIFY_SOURCE=2 \
  -Wno-unused-result \
  -I./include \
  -I./deps/arena/include \
  -I./deps/CString/include \
  -I./deps/container/include \
  -I./deps/yyjson/include \
  ./deps/arena/lib/arena.c \
  ./deps/CString/lib/cstring.c \
  ./deps/container/lib/cvector.c \
  ./deps/yyjson/src/yyjson.c \
  ./src/main.c \
  ./src/cli.c \
  ./src/config_handler.c \
  ./src/file_handler.c \
  ./src/package_manager.c \
  ./src/utils.c \
  ./src/project_handler.c \
  -o flint

echo "* Build successful!"
echo "--------------------------------------------------------"

read -p "? Do you want to install 'flint' globally to /usr/local/bin? (y/N): " choice </dev/tty

case "$choice" in 
    [yY][eE][sS]|[yY])
        echo "* Installing to /usr/local/bin..."
        if sudo cp flint /usr/local/bin/flint && sudo chmod +x /usr/local/bin/flint; then
            echo "* Done! You can now run 'flint' from anywhere in your terminal."
        else
            echo "! Failed to install executable."
        fi
        ;;
    *)
        echo "* Global installation skipped. Executable cleanup completed."
        cp flint "$ORIGINAL_DIR/flint"
        echo "* Executable saved to $ORIGINAL_DIR/flint"
        ;;
esac
