#ifndef FLINT_LOG_H
#define FLINT_LOG_H

#include <stdbool.h>
#include <stdio.h>

#include <flint_err.h>

#define FLINT_ANSI_RESET "\x1b[0m"
#define FLINT_ANSI_BOLD "\x1b[1m"
#define FLINT_ANSI_DIM "\x1b[2m"

typedef enum {
	FLINT_LOG_INFO,
	FLINT_LOG_SUCCESS,
	FLINT_LOG_WARN,
	FLINT_LOG_ERROR,
	FLINT_LOG_PROMPT
} FlintLogLevel;

bool flint_log_use_color(FILE *stream);

void flint_log_msg(FlintLogLevel level, const char *fmt, ...);

void flint_log_error(const char *context, flint_err_t err);

#define flint_info(...) flint_log_msg(FLINT_LOG_INFO, __VA_ARGS__)
#define flint_success(...) flint_log_msg(FLINT_LOG_SUCCESS, __VA_ARGS__)
#define flint_warn(...) flint_log_msg(FLINT_LOG_WARN, __VA_ARGS__)
#define flint_prompt(...) flint_log_msg(FLINT_LOG_PROMPT, __VA_ARGS__)
#define flint_error(ctx, err) flint_log_error((ctx), (err))

#endif /* FLINT_LOG_H */
