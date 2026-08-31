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
	if (!git_url)
		return NULL;

	const char *at_symbol = strrchr(git_url, '@');
	if (!at_symbol)
		return NULL;

	const char *repo_start = at_symbol + 1;

	size_t len = strlen(repo_start);

	char *version_number = (char *)arena_alloc(arena, len + 1);
	if (!version_number)
		return NULL;

	strncpy(version_number, repo_start, len);
	version_number[len] = '\0';

	return version_number;
}

char *get_modified_url(Arena *arena, const char *git_url) {
	if (!git_url)
		return NULL;

	const char *at_symbol = strrchr(git_url, '@');
	if (!at_symbol)
		return NULL;
	size_t len = at_symbol - git_url;
	char *url = (char *)arena_alloc(arena, len + 1);
	if (!url)
		return NULL;

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

int copy_file(const char *src_path, const char *dest_path) {
	FILE *src = fopen(src_path, "rb");
	if (src == NULL) {
		perror("Error opening source file");
		return -1;
	}

	FILE *dest = fopen(dest_path, "wb");
	if (dest == NULL) {
		perror("Error opening/creating destination file");
		fclose(src);
		return -1;
	}

	char buffer[BUFFER_SIZE];

	size_t bytes_read;

	while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, src)) > 0) {
		size_t bytes_written = fwrite(buffer, 1, bytes_read, dest);
		if (bytes_written < bytes_read) {
			perror("Error writing to destination file");
			fclose(src);
			fclose(dest);
			return -1;
		}
	}

	fclose(src);
	fclose(dest);
	return 0;
}

bool check_if_dep_path(const char *str) {
	size_t len_prefix = strlen("deps");
	size_t len_str = strlen(str);

	if (len_str < len_prefix) {
		return false;
	}

	return strncmp(str, "deps", len_prefix) == 0;
}
