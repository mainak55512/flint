#include <flint_log.h>

#include <stdarg.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#define ISATTY(fd) _isatty(fd)
#define FILENO(fp) _fileno(fp)
#else
#include <unistd.h>
#define ISATTY(fd) isatty(fd)
#define FILENO(fp) fileno(fp)
#endif

#define ANSI_RED "\x1b[1;31m"
#define ANSI_GREEN "\x1b[1;32m"
#define ANSI_YELLOW "\x1b[1;33m"
#define ANSI_BLUE "\x1b[1;34m"
#define ANSI_CYAN "\x1b[1;36m"

bool flint_log_use_color(FILE *stream) {
	if (getenv("NO_COLOR") != NULL) {
		return false;
	}

#if defined(_WIN32)
	HANDLE h = (HANDLE)_get_osfhandle(FILENO(stream));
	if (h != INVALID_HANDLE_VALUE) {
		DWORD mode = 0;
		if (GetConsoleMode(h, &mode)) {
			SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
		}
	}
#endif

	return ISATTY(FILENO(stream)) != 0;
}

static void flint_log_prefix(FILE *out, FlintLogLevel level, bool color) {
	const char *tag = "";
	const char *ansi = "";

	switch (level) {
	case FLINT_LOG_INFO:
		tag = "INFO";
		ansi = ANSI_BLUE;
		break;
	case FLINT_LOG_SUCCESS:
		tag = "SUCCESS";
		ansi = ANSI_GREEN;
		break;
	case FLINT_LOG_WARN:
		tag = "WARN";
		ansi = ANSI_YELLOW;
		break;
	case FLINT_LOG_ERROR:
		tag = "ERROR";
		ansi = ANSI_RED;
		break;
	case FLINT_LOG_PROMPT:
		tag = "PROMPT";
		ansi = ANSI_CYAN;
		break;
	}

	char bracketed[16];
	snprintf(bracketed, sizeof(bracketed), "[%s]", tag);

	if (color) {
		fprintf(out, "%s%-9s%s ", ansi, bracketed, FLINT_ANSI_RESET);
	} else {
		fprintf(out, "%-9s ", bracketed);
	}
}

void flint_log_msg(FlintLogLevel level, const char *fmt, ...) {
	FILE *out =
		(level == FLINT_LOG_ERROR || level == FLINT_LOG_WARN) ? stderr : stdout;
	bool color = flint_log_use_color(out);

	flint_log_prefix(out, level, color);

	va_list args;
	va_start(args, fmt);
	vfprintf(out, fmt, args);
	va_end(args);

	if (level == FLINT_LOG_PROMPT) {
		fflush(out);
	} else {
		fputc('\n', out);
	}
}

void flint_log_error(const char *context, flint_err_t err) {
	bool color = flint_log_use_color(stderr);
	flint_log_prefix(stderr, FLINT_LOG_ERROR, color);

	if (color) {
		fprintf(stderr, "%s%s:%s %s %s(%d)%s: %s\n", FLINT_ANSI_BOLD, context,
				FLINT_ANSI_RESET, flint_err_name(err), FLINT_ANSI_DIM, err,
				FLINT_ANSI_RESET, flint_strerror(err));
	} else {
		fprintf(stderr, "%s: %s (%d): %s\n", context, flint_err_name(err), err,
				flint_strerror(err));
	}
}
