#include <flint_err.h>
#include <stdio.h>

static const char *const FLINT_ERR_MESSAGES[] = {
#define X(code, msg) [code] = msg,
	FLINT_ERROR_LIST(X)
#undef X
};

static const char *const FLINT_ERR_NAMES[] = {
#define X(code, msg) [code] = #code,
	FLINT_ERROR_LIST(X)
#undef X
};

const char *flint_strerror(flint_err_t err) {
	if (err < 0 || err >= FLINT_ERR_COUNT) {
		return "Unknown Flint error";
	}
	return FLINT_ERR_MESSAGES[err];
}

const char *flint_err_name(flint_err_t err) {
	if (err < 0 || err >= FLINT_ERR_COUNT) {
		return "FLINT_ERR_UNKNOWN";
	}
	return FLINT_ERR_NAMES[err];
}
