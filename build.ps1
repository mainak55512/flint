
<#
.SYNOPSIS
    Automated build script for Flint on Windows (PowerShell)
#>

$ErrorActionPreference = "Stop"
if ($PSVersionTable.PSVersion.Major -ge 7) {
    $PSNativeCommandUseErrorActionPreference =$false
}

$VERSION = "v0.5.5"

# 1. Compiler Detection
$compiler =$null
if (Get-Command x86_64-w64-mingw32-gcc -ErrorAction SilentlyContinue) {
    Write-Host "  -> Found x86_64-w64-mingw32-gcc (MinGW Cross-Compiler)" -ForegroundColor Cyan
    $compiler = "x86_64-w64-mingw32-gcc"
} elseif (Get-Command gcc -ErrorAction SilentlyContinue) {
    Write-Host "  -> Found gcc" -ForegroundColor Cyan
    $compiler = "gcc"
} elseif (Get-Command clang -ErrorAction SilentlyContinue) {
    Write-Host "  -> Found clang" -ForegroundColor Cyan
    $compiler = "clang"
} else {
    Write-Error "! Error: Neither gcc, clang, nor mingw-gcc was found in your PATH."
    exit 1
}

Write-Host "* Compiling Flint (Release) using $compiler..." -ForegroundColor Yellow

# 2. Compilation Arguments, Include Paths & Source Files
$argsList = @(
    "-O3",
    "-DNDEBUG",
    "-Wno-unused-result",
    "-fno-stack-protector",
    "-D_CRT_SECURE_NO_WARNINGS",
    "-DPROJECT_VERSION=`"$VERSION`"",
    "-I./include",
    "-I./deps/arena/include",
    "-I./deps/CString/include",
    "-I./deps/container/include",
    "-I./deps/Cmap/include",
    "-I./deps/yyjson/src",
    "./deps/arena/lib/arena.c",
    "./deps/CString/lib/cstring.c",
    "./deps/container/lib/cvector.c",
    "./deps/Cmap/lib/cmap.c",
    "./deps/yyjson/src/yyjson.c",
    "./src/main.c",
    "./src/cli.c",
    "./src/config_handler.c",
    "./src/file_handler.c",
    "./src/package_manager.c",
    "./src/utils.c",
    "./src/project_handler.c",
    "-o",
    "flint.exe"
)

# 3. Run Compilation (Safely handling stderr warnings/errors so they don't abort prematurely)
Write-Host "`nExecuting compilation:" -ForegroundColor DarkGray
$oldErrorAction = $ErrorActionPreference
$ErrorActionPreference = "Continue"

& $compiler $argsList

$ErrorActionPreference = $oldErrorAction

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n* Build successful! Output saved as flint.exe" -ForegroundColor Green
} else {
    Write-Error "! Compilation failed with exit code $LASTEXITCODE."
    exit $LASTEXITCODE
}

Write-Host "--------------------------------------------------------" -ForegroundColor DarkGray

# 4. Optional PATH Installation Prompt
$choice = Read-Host "? Do you want to copy 'flint.exe' to a local bin folder in your PATH? (y/N)"

if ($choice -match "^[yY](es)?$") {
    $userBin = "$env:USERPROFILE\bin"
    if (!(Test-Path $userBin)) {
        New-Item -ItemType Directory -Force -Path $userBin | Out-Null
    }
    Copy-Item "flint.exe" "$userBin\flint.exe" -Force
    Write-Host "* Done! Copied to $userBin\flint.exe." -ForegroundColor Green
    Write-Host "* Make sure $userBin is added to your user PATH environment variable." -ForegroundColor Yellow
} else {
    Write-Host "* Global installation skipped." -ForegroundColor Cyan
    Write-Host "* Executable saved to current directory as flint.exe" -ForegroundColor Cyan
}
