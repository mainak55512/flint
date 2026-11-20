#ifndef FLINT_ERR_H
#define FLINT_ERR_H

#include <stdbool.h>
#include <stddef.h>

#define FLINT_ERROR_LIST(X)                                                    \
	X(FLINT_OK, "Success")                                                     \
	X(FLINT_ERR_UNKNOWN, "Unknown error")                                      \
	X(FLINT_ERR_COMPOSITION_READ, "Failed to read composition")                \
	X(FLINT_ERR_COMPOSITION_WRITE, "Failed to write composition")              \
	X(FLINT_ERR_TOOL_UNAVAILABLE, "Required tool is unavailable")              \
	X(FLINT_ERR_ARCHIVER_UNAVAILABLE, "Archiver is unavailable")               \
	X(FLINT_ERR_ALREADY_INIT, "Flint project is already initiated")            \
	X(FLINT_ERR_COMPOSITION_EXISTS, "Flint composition already exists")        \
	X(FLINT_ERR_INVALID_PATH, "Invalid path provided")                         \
	X(FLINT_ERR_COMPILER_NOT_FOUND, "Supported compiler not found")            \
	X(FLINT_ERR_MKDIR, "Directory creation failed")                            \
	X(FLINT_ERR_MKFILE, "File creation/update failed")                         \
	X(FLINT_ERR_CONFIG_INIT, "Config initialization failed")                   \
	X(FLINT_ERR_CMD_EXECUTION, "Unexpected command execution error")           \
	X(FLINT_ERR_TRAVERSAL, "Subdirectory traversal error")                     \
	X(FLINT_ERR_COPY_HEADERS, "Error encountered while copying headers")       \
	X(FLINT_ERR_CONFIG_NOT_FOUND, "Flint composition not found")               \
	X(FLINT_ERR_COMPILE_COMMANDS,                                              \
	  "Error encountered while generating compile_commands")                   \
	X(FLINT_ERR_CONFIG_UNKNOWN,                                                \
	  "Unknown error encountered while handling composition")

typedef enum {
#define X(code, msg) code,
	FLINT_ERROR_LIST(X)
#undef X
		FLINT_ERR_COUNT
} flint_err_t;

#define FLINT_DEFINE_RESULT(T, Name)                                           \
	typedef struct {                                                           \
		flint_err_t err;                                                       \
		T value;                                                               \
	} Name

#define FLINT_OK_VAL(ResType, val) ((ResType){.err = FLINT_OK, .value = (val)})
#define FLINT_ERR_VAL(ResType, code) ((ResType){.err = (code)})

const char *flint_strerror(flint_err_t err);
const char *flint_err_name(flint_err_t err);

#endif // FLINT_ERR_H
