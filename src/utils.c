#include <flint.h>

bool is_platform_element(char *entry, char **out_flag) {
	char *colon = strchr(entry, ':');

	if (colon == NULL) {
		*out_flag = entry;
		return true;
	}

	// if (colon == NULL || entry[0] == '-') {
	// 	*out_flag = entry;
	// 	return true;
	// }

	size_t os_len = (size_t)(colon - entry);

	if (entry[0] == '-' || os_len <= 1) {
		*out_flag = entry;
		return true;
	}

	bool match = (strlen(CURRENT_OS) == os_len &&
				  strncmp(entry, CURRENT_OS, os_len) == 0) ||
				 (IS_UNIX && os_len == 4 && strncmp(entry, "unix", 4) == 0);

	if (match) {
		*out_flag = colon + 1;
		return true;
	}

	return false;
}

String *join_vec_to_str(Arena *arena, Vector *v, String *sep) {
	String *output = NULL;

	for (int i = 0; i < length(v); i++) {
		String *item = at(String *, v, i);
		char *raw_flag = NULL;

		if (!is_platform_element(string(item), &raw_flag)) {
			continue;
		}

		String *flag_str = string_from(arena, raw_flag);

		if (output == NULL) {
			output = flag_str;
		} else {
			output = string_concat(arena, 3, output, sep, flag_str);
		}
	}

	if (output == NULL) {
		return string_from(arena, "");
	}

	return output;
}

const char *get_filename_without_path(const char *path) {
	if (!path)
		return "";

	const char *last_slash = strrchr(path, '/');
	const char *last_backslash = strrchr(path, '\\');

	const char *filename = path;
	if (last_slash && last_slash >= filename) {
		filename = last_slash + 1;
	}
	if (last_backslash && last_backslash >= filename) {
		filename = last_backslash + 1;
	}

	return filename;
}

Vector *string_split(Arena *arena, String *str, char sep) {
	Vector *lines = vector_init(String *);
	char *cstr = string(str);
	int len = string_len(str);
	int start = 0;

	for (int i = 0; i < len; i++) {
		if (cstr[i] == sep) {
			String *line = string_sub(arena, str, start, i);
			append(String *, lines, line);
			start = i + 1;
		}
	}

	if (start <= len) {
		String *line = string_sub(arena, str, start, len);
		append(String *, lines, line);
	}

	return lines;
}

Vector *remove_excludes(Vector *collected, Vector *excludes) {

	if (!excludes) {
		return collected;
	}

	Vector *vec = vector_init(String *);
	for (int i = 0; i < length(collected); i++) {
		char *item_str = string(at(String *, collected, i));
		bool present = false;
		for (int j = 0; j < length(excludes); j++) {
			char *raw_entry = string(at(String *, excludes, j));
			char *excluded = NULL;
			if (!is_platform_element(raw_entry, &excluded)) {
				continue;
			}

			size_t ex_len = strlen(excluded);

			if (ex_len == 0) {
				continue;
			}

			if (strncmp(item_str, excluded, ex_len) == 0) {
				if (item_str[ex_len] == '\0' || item_str[ex_len] == '/'
#if defined(_WIN32)
					|| item_str[ex_len] == '\\'
#endif
				) {
					present = true;
					break;
				}
			}
		}

		if (!present) {
			append(String *, vec, at(String *, collected, i));
		}
	}

	vector_free(&collected);
	return vec;
}

Vector *string_split_lines(Arena *arena, String *str) {
	return string_split(arena, str, '\n');
}

int check_project_lang(char *lang) {
	if (STR_CMP(lang, "c++") == 0 || STR_CMP(lang, "cpp") == 0) {
		return 0;
	}
	return 1;
}

String *get_current_working_dir(Arena *arena) {
	char cwd[1024];
	GET_WD(cwd, sizeof(cwd));
	return string_from(arena, cwd);
}

char *get_repo_name(Arena *arena, const char *git_url) {
	if (!git_url)
		return NULL;

	const char *last_slash = strrchr(git_url, '/');
	if (!last_slash)
		return NULL;

	const char *repo_start = last_slash + 1;

	const char *git_suffix = strstr(repo_start, ".git");

	size_t len;
	if (git_suffix) {
		len = git_suffix - repo_start;
	} else {
		len = strlen(repo_start);
	}

	char *repo_name = (char *)arena_alloc(arena, len + 1);
	if (!repo_name)
		return NULL;

	strncpy(repo_name, repo_start, len);
	repo_name[len] = '\0';

	return repo_name;
}

char *get_version_number(Arena *arena, const char *git_url) {
	if (!git_url) {
		return NULL;
	}

	const char *at_symbol = strrchr(git_url, '@');
	if (!at_symbol || *(at_symbol + 1) == '\0') {
		return NULL;
	}

	if (strchr(at_symbol, '/') != NULL || strchr(at_symbol, ':') != NULL) {
		return NULL;
	}

	const char *repo_start = at_symbol + 1;
	size_t len = strlen(repo_start);

	char *version_number = (char *)arena_alloc(arena, len + 1);
	if (!version_number) {
		return NULL;
	}

	strncpy(version_number, repo_start, len);
	version_number[len] = '\0';

	return version_number;
}

char *get_modified_url(Arena *arena, const char *git_url) {
	if (!git_url)
		return NULL;

	const char *at_symbol = strrchr(git_url, '@');
	if (!at_symbol || *(at_symbol + 1) == '\0') {
		return NULL;
	}
	size_t len = at_symbol - git_url;
	char *url = (char *)arena_alloc(arena, len + 1);
	if (!url) {
		return NULL;
	}

	strncpy(url, git_url, len);
	url[len] = '\0';

	return url;
}

char *get_lib_hash(Arena *arena, char *target_dir) {
	char *hash = (char *)arena_alloc(arena, 41 * sizeof(char));
	if (!hash)
		return "";

	char buffer[128];

	String *cmd =
		string_concat_cstr(arena, 3, "git -C ", target_dir, " rev-parse HEAD");
	FILE *fp = popen(string(cmd), "r");
	if (fp == NULL) {
		perror("Failed to run git command");
		return "";
	}

	if (fgets(buffer, sizeof(buffer), fp) != NULL) {
		buffer[strcspn(buffer, "\r\n")] = '\0';
		strncpy(hash, buffer, 40);
		hash[40] = '\0';
	} else {
		hash[0] = '\0';
	}

	int status = pclose(fp);

	if (status == -1) {
		perror("pclose failed");
		return "";
	}

#if defined(_WIN32) && !defined(__MINGW32__) && !defined(__MINGW64__)
	int exit_code = status;
#elif defined(_WIN32)
	int exit_code = status;
#else
	int exit_code = WEXITSTATUS(status);
#endif

	if (exit_code != 0) {
		fprintf(stderr, "Git error: Exit code %d (Not a git repository?)\n",
				exit_code);
		return "";
	}

	return hash;
}

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

char *get_tag_from_hash(Arena *arena, const char *target_dir,
						const char *ref_hash) {
	char buffer[128];

	String *cmd_str =
		string_concat_cstr(arena, 5, "git -C \"", target_dir,
						   "\" describe --tags --exact-match ", ref_hash, " ");
	cmd_str = string_concat_cstr(arena, 2, string(cmd_str), NULL_DEVICE);

	FILE *fp = popen(string(cmd_str), "r");
	if (fp == NULL) {
		perror("Failed to run git command");
		return arena_strdup(arena, "unknown");
	}

	if (fgets(buffer, sizeof(buffer), fp) != NULL) {
		buffer[strcspn(buffer, "\r\n")] = '\0';
		char *git_tag = arena_strdup(arena, buffer);
		pclose(fp);
		return git_tag;
	}

	pclose(fp);
	return arena_strdup(arena, "unknown");
}

bool set_contains(Vector *v, char *elem) {

	for (int i = 0; i < length(v); i++) {
		if (STR_CMP(at(char *, v, i), elem) == 0) {
			return true;
		}
	}
	return false;
}

void set_add(Vector *v, char *elem) {
	if (!set_contains(v, elem)) {
		append(char *, v, elem);
	}
}

bool check_if_dep_path(const char *str) {
	size_t len_prefix = strlen("deps");
	size_t len_str = strlen(str);

	if (len_str < len_prefix) {
		return false;
	}

	return strncmp(str, "deps", len_prefix) == 0;
}

bool starts_with(char *str, char *prefix) {
	return strncmp(str, prefix, strlen(prefix)) == 0;
}

bool set_contains_str(Vector *v, String *elem) {
	for (int i = 0; i < length(v); i++) {
		if (STR_CMP(string(at(String *, v, i)), string(elem)) == 0) {
			return true;
		}
	}
	return false;
}

void set_add_str(Vector *v, String *elem) {
	if (!set_contains_str(v, elem)) {
		append(String *, v, elem);
	}
}

char *safe_str(String *s) { return s ? string(s) : ""; }

String *safe_get_string(Arena *arena, yyjson_val *obj, const char *key) {
	yyjson_val *val = yyjson_obj_get(obj, key);
	const char *str = yyjson_get_str(val);
	return str ? string_from(arena, (char *)str) : NULL;
}

Vector *parse_string_array(Arena *arena, yyjson_val *obj, char *key) {
	yyjson_val *arr = yyjson_obj_get(obj, key);
	Vector *target_vec = vector_init(String *);
	size_t idx, max;
	yyjson_val *val;
	if (yyjson_is_arr(arr)) {
		yyjson_arr_foreach(arr, idx, max, val) {
			const char *s = yyjson_get_str(val);
			if (s)
				append(String *, target_vec, string_from(arena, (char *)s));
		}
	}
	return target_vec;
}
