#include <flint.h>

int main(int argc, char *argv[]) {
	Arena *global_str_arena = arena_init(1024);

	flint_err_t err = cli(argc, argv, global_str_arena);

	arena_free(&global_str_arena);
	if (err != FLINT_OK) {
		return 1;
	}
	return 0;
}
