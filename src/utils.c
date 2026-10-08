#include "container.h"
#include "yyjson.h"
#include <flint.h>

const char *get_filename_without_path(const char *path) {
	const char *last_slash = strrchr(path, '/');
	const char *last_backslash = strrchr(path, '\\');

	const char *filename = path;
	if (last_slash && last_slash > filename) {
		filename = last_slash + 1;
	}
	if (last_backslash && last_backslash > filename) {
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

Vector *remove_excludes(Vector *collected, yyjson_val *excludes) {
	Vector *vec = vector_init(String *);

	for (int i = 0; i < length(collected); i++) {
		bool present = false;
		size_t idx = 0, max = 0;
		yyjson_val *val;
		yyjson_arr_foreach(excludes, idx, max, val) {
			if (STR_CMP(string(at(String *, collected, i)),
						yyjson_get_str(val)) == 0) {
				present = true;
				break;
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

/*
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
*/

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
	char buffer[128];

	FILE *fp = popen(string(string_concat_cstr(arena, 3, "git -C ", target_dir,
											   " rev-parse HEAD")),
					 "r");
	if (fp == NULL) {
		perror("Failed to run git command");
		return "";
	}

	if (fgets(buffer, sizeof(buffer), fp) != NULL) {
		buffer[strcspn(buffer, "\r\n")] = '\0';
		strncpy(hash, buffer, 40);
	}

	int status = pclose(fp);

	if (status == -1) {
		perror("pclose failed");
		return "";
	} else if (WEXITSTATUS(status) != 0) {
		fprintf(stderr, "Git error: Exit code %d (Not a git repository?)\n",
				WEXITSTATUS(status));
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
	char *sink_path = "2>/dev/null";
	char *cmd = string(string_concat_cstr(arena, 6, "git -C ", target_dir,
										  " describe --tags --exact-match ",
										  ref_hash, " ", sink_path));
	FILE *fp = popen(cmd, "r");
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

/*
bool set_remove(Vector **v, char *elem) {
	if (!v || !*v)
		return false;

	if (!set_contains(*v, elem)) {
		return false;
	}

	Vector *old_v = *v;
	Vector *new_v = vector_init(char *);

	int len = length(old_v);
	for (int i = 0; i < len; i++) {
		char *item = at(char *, old_v, i);
		if (STR_CMP(item, elem) != 0) {
			append(char *, new_v, item);
		}
	}

	*v = new_v;
	vector_free(old_v);
	return true;
}
	*/

bool check_if_dep_path(const char *str) {
	size_t len_prefix = strlen("deps");
	size_t len_str = strlen(str);

	if (len_str < len_prefix) {
		return false;
	}

	return strncmp(str, "deps", len_prefix) == 0;
}
