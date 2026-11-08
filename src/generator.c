#include <flint.h>

typedef enum { INPUT_NONE = 0, INPUT_STRING, INPUT_ARRAY } InputType;

typedef struct {
	InputType input_type;
	union {
		char *str;
		Vector *arr;
	} source_path;
	char *output_path;
	Cmap *config;
} Generator;

char *arena_strdup(Arena *arena, const char *str) {
	if (!str)
		return NULL;
	size_t len = strlen(str);
	char *copy = (char *)arena_alloc(arena, len + 1);
	if (copy) {
		memcpy(copy, str, len);
		copy[len] = '\0';
	}
	return copy;
}

Generator *init_generator(Arena *arena) {
	Generator *gen = (Generator *)arena_alloc(arena, sizeof(Generator));
	gen->config = NULL;
	gen->input_type = INPUT_NONE;
	gen->output_path = NULL;
	gen->source_path.str = NULL;
	gen->source_path.arr = NULL;
	return gen;
}

void free_generator(Generator *gen) {
	if (gen->config) {
		map_free(gen->config);
	}
	if (gen->input_type == INPUT_ARRAY) {
		vector_free(&gen->source_path.arr);
	}
}

void parse_generators(Arena *arena, Generator *gen, yyjson_val *val) {
	yyjson_val *output = yyjson_obj_get(val, "output");
	if (!output)
		return;

	char *output_path_json = (char *)yyjson_get_str(output);

	gen->output_path = arena_strdup(arena, output_path_json);

	yyjson_val *source = yyjson_obj_get(val, "source");

	if (source) {
		if (yyjson_is_str(source)) {
			gen->input_type = INPUT_STRING;
			gen->source_path.str = arena_strdup(arena, yyjson_get_str(source));
		} else if (yyjson_is_arr(source)) {
			gen->input_type = INPUT_ARRAY;
			gen->source_path.arr = vector_init(char *);

			size_t idx, max;
			yyjson_val *element;
			yyjson_arr_foreach(source, idx, max, element) {
				if (yyjson_is_str(element)) {
					append(char *, gen->source_path.arr,
						   arena_strdup(arena, yyjson_get_str(element)));
				}
			}
		}
	}

	yyjson_val *config = yyjson_obj_get(val, "config");
	if (config) {
		Cmap *config_map = map_init();
		size_t idx, max;
		yyjson_val *element, *key;
		yyjson_obj_foreach(config, idx, max, key, element) {
			map_add(config_map, (char *)yyjson_get_str(key),
					(char *)yyjson_get_str(element));
		}
		gen->config = config_map;
	}
}

void update_generator_paths_for_deps(Arena *arena, char *repo_name,
									 Generator *gen) {
	char *current_output_path = gen->output_path;
	gen->output_path = string(string_concat_cstr(arena, 4, "deps/", repo_name,
												 "/", current_output_path));

	if (gen->input_type == INPUT_STRING) {
		char *current_source_path = gen->source_path.str;
		gen->source_path.str = string(string_concat_cstr(
			arena, 4, "deps/", repo_name, "/", current_source_path));
	} else if (gen->input_type == INPUT_ARRAY) {
		Vector *old_sources = gen->source_path.arr;
		Vector *updated_sources = vector_init(char *);

		for (int i = 0; i < length(old_sources); i++) {
			char *current_source_path = at(char *, old_sources, i);
			char *updated_source_path = string(string_concat_cstr(
				arena, 4, "deps/", repo_name, "/", current_source_path));
			append(char *, updated_sources, updated_source_path);
		}
		vector_free(&old_sources);
		gen->source_path.arr = updated_sources;
	}
}

int main() {
	yyjson_doc *doc = yyjson_read_file("src/test.json", 0, NULL, NULL);

	if (!doc) {
		fprintf(stderr, "Failed to read or parse JSON file.\n");
		return 1;
	}

	yyjson_val *root = yyjson_doc_get_root(doc);

	Arena *arena = arena_init(1024 * 1024);

	size_t idx, max;
	yyjson_val *element;
	yyjson_arr_foreach(root, idx, max, element) {
		printf("count: %lu\n", idx);
		if (element) {

			Generator *gen = init_generator(arena);
			parse_generators(arena, gen, element);

			update_generator_paths_for_deps(arena, "test_repo", gen);

			if (gen->output_path) {
				printf("Output path: %s\n", gen->output_path);
			}
			if (gen->input_type == INPUT_STRING) {
				printf("Source path: %s\n", gen->source_path.str);
			} else if (gen->input_type == INPUT_ARRAY) {
				printf("Source path array: \n");
				for (int i = 0; i < length(gen->source_path.arr); i++) {
					printf("%s\n", at(char *, gen->source_path.arr, i));
				}
			}
			if (gen->config) {
				printf("Config: \n");
				Vector *keys = map_keys(gen->config);
				for (int i = 0; i < length(keys); i++) {
					char *key = at(char *, keys, i);
					printf("%s: %s\n", key, (char *)map_get(gen->config, key));
				}
				vector_free(&keys);
			}
			free_generator(gen);
		}
		printf("\n");
	}
	yyjson_doc_free(doc);
	arena_free(&arena);
}
