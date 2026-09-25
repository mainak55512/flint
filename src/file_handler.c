#include "cstring.h"
#include <flint.h>

int create_append_file(char *file_path, char *content) {
	FILE *fp = fopen(file_path, "w+");
	if (fp == NULL) {
		perror("fopen failed");
		return 1;
	}
	fprintf(fp, "%s", content);
	fclose(fp);
	return 0;
}

String *collect_files(Arena *str_arena, String *path, String *type) {
	Arena *arena = arena_init(1024);
	String *src_files = string_from(str_arena, "");
	String *ext_1 = string_from(str_arena, ".c");
	String *ext_2 = string_from(str_arena, ".cpp");
	String *ext_3 = NULL;

	char sep[2] = {'\n', '\0'};

	if (STR_CMP(string(type), "static") == 0) {
		sep[0] = ' ';
		ext_1 = string_from(str_arena, ".a");
		ext_2 = string_from(str_arena, ".lib");
	} else if (STR_CMP(string(type), "dyn") == 0) {
		sep[0] = ' ';
		ext_1 = string_from(str_arena, ".so");
		ext_2 = string_from(str_arena, ".dll");
	} else if (STR_CMP(string(type), "header") == 0) {
		sep[0] = '\n';
		ext_1 = string_from(str_arena, ".h");
		ext_2 = string_from(str_arena, ".hpp");
		ext_3 = string_from(str_arena, ".h.in");
	}

	DIR *dir;
	struct dirent *entry;

	dir = opendir(string(path));

	// if (dir == NULL) {
	// 	perror("Unable to open directory");
	// 	return NULL;
	// }

	if (dir != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			if (STR_CMP(entry->d_name, ".") == 0 ||
				STR_CMP(entry->d_name, "..") == 0) {
				continue;
			}
			size_t name_len = strlen(entry->d_name);
			char *dot = strrchr(entry->d_name, '.');
			bool match = (dot != NULL && (STR_CMP(dot, string(ext_1)) == 0 ||
										  STR_CMP(dot, string(ext_2)) == 0));

			if (!match && ext_3 != NULL) {
				size_t ext3_len = string_len(ext_3);
				if (name_len >= ext3_len &&
					STR_CMP(entry->d_name + name_len - ext3_len,
							string(ext_3)) == 0) {
					match = true;
					size_t target_len = name_len - 3;
					char header_name[256];

					if (target_len < sizeof(header_name)) {
						strncpy(header_name, entry->d_name, target_len);
						header_name[target_len] = '\0';
					}

					char *in_path = string(string_concat_cstr(
						arena, 3, string(path), "/", entry->d_name));
					char *out_path = string(string_concat_cstr(
						arena, 3, string(path), "/", header_name));
					gen_header_from_tmpl(in_path, out_path);

					strncpy(header_name, entry->d_name,
							sizeof(header_name) - 1);
					header_name[sizeof(header_name) - 1] = '\0';
				}
			}
			if (match) {

				if (string_len(src_files) > 0) {
					src_files = string_concat_cstr(str_arena, 2,
												   string(src_files), sep);
				}

				src_files =
					string_concat_cstr(str_arena, 4, string(src_files),
									   string(path), "/", entry->d_name);
			}
		}
		closedir(dir);
	}
	arena_free(&arena);
	return src_files;
}

void get_files_vec(Arena *str_arena, Vector *src_arr, Vector *source_files,
				   yyjson_val *root, /* yyjson_val *deps,*/ String *cwd,
				   String *file_type) {
	String *retrieve_type = file_type;

	if (STR_CMP(string(file_type), "header") == 0) {
		retrieve_type = string_from(str_arena, "include_paths");
	} else if (STR_CMP(string(file_type), "static") == 0) {
		retrieve_type = string_from(str_arena, "static_lib");
	} else if (STR_CMP(string(file_type), "dyn") == 0) {
		retrieve_type = string_from(str_arena, "shared_lib");
	} else {
		retrieve_type = string_clone(str_arena, file_type);
	}

	yyjson_val *excludes = yyjson_obj_get(root, "excludes");

	// yyjson_val *src_arr = yyjson_obj_get(root, string(retrieve_type));
	// if (yyjson_is_arr(src_arr)) {
	// 	yyjson_arr_iter iter;
	// 	yyjson_arr_iter_init(src_arr, &iter);
	// 	yyjson_val *val;
	// while ((val = yyjson_arr_iter_next(&iter))) {
	for (int i = 0; i < length(src_arr); i++) {
		Vector *src_temp_arr;

		/*
		if (STR_CMP(string(retrieve_type), "src") == 0) {
			src_temp_arr = remove_excludes(
				string_split_lines(
					str_arena,
					collect_files(
						str_arena,
						// string_from(str_arena, (char *)yyjson_get_str(val)),
						string_from(str_arena, at(char *, src_arr, i)),
						string_from(str_arena, string(file_type)))),
				excludes);
		} else {
			src_temp_arr = string_split_lines(
				str_arena,
				collect_files(
					str_arena,
					// string_from(str_arena, (char *)yyjson_get_str(val)),
					string_from(str_arena, at(char *, src_arr, i)),
					string_from(str_arena, string(file_type))));
		}
*/
		src_temp_arr = remove_excludes(
			string_split_lines(
				str_arena,
				collect_files(
					str_arena,
					// string_from(str_arena, (char *)yyjson_get_str(val)),
					string_from(str_arena, at(char *, src_arr, i)),
					string_from(str_arena, string(file_type)))),
			excludes);
		for (int i = 0; i < length(src_temp_arr); i++) {
			char *elem = string(at(String *, src_temp_arr, i));
			if (STR_CMP(elem, "") != 0) {
				append(char *, source_files, elem);
			}
		}
		vector_free(src_temp_arr);
		// 	}
		// }

		/*
		if (yyjson_is_obj(deps) && yyjson_obj_size(deps) != 0) {
			yyjson_obj_iter iter;
			yyjson_obj_iter_init(deps, &iter);
			yyjson_val *key, *dep_obj;
			while ((key = yyjson_obj_iter_next(&iter))) {
				dep_obj = yyjson_obj_iter_get_val(key);
				const char *dep_name = yyjson_get_str(key);

				yyjson_val *dep_src =
					yyjson_obj_get(dep_obj, string(retrieve_type));
				if (yyjson_is_arr(dep_src) && yyjson_arr_size(dep_src) != 0) {
					yyjson_arr_iter src_iter;
					yyjson_arr_iter_init(dep_src, &src_iter);
					yyjson_val *src_val;
					while ((src_val = yyjson_arr_iter_next(&src_iter))) {
						String *path =
							string_concat_cstr(str_arena, 5, string(cwd),
											   "/deps/", (char *)dep_name, "/",
											   (char *)yyjson_get_str(src_val));
						Vector *src_temp_arr = string_split_lines(
							str_arena,
							collect_files(
								str_arena, path,
								string_from(str_arena, string(file_type))));
						for (int i = 0; i < length(src_temp_arr); i++) {
							char *elem = string(at(String *, src_temp_arr, i));
							if (STR_CMP(elem, "") != 0) {
								append(char *, source_files, elem);
							}
						}
						vector_free(src_temp_arr);
					}
				}
			}*/
	}
}

void get_src_vec(Arena *str_arena, Vector *src_arr, Vector *source_files,
				 yyjson_val *root, /* yyjson_val *deps,*/ String *cwd) {
	get_files_vec(str_arena, src_arr, source_files, root /*, deps*/, cwd,
				  string_from(str_arena, "src"));
}

void get_header_vec(Arena *str_arena, Vector *src_arr, Vector *source_files,
					yyjson_val *root, /* yyjson_val *deps,*/ String *cwd) {
	get_files_vec(str_arena, src_arr, source_files, root /*, deps*/, cwd,
				  string_from(str_arena, "header"));
}

void get_stat_lib_vec(Arena *str_arena, Vector *src_arr, Vector *source_files,
					  yyjson_val *root, /* yyjson_val *deps,*/ String *cwd) {
	get_files_vec(str_arena, src_arr, source_files, root /*, deps*/, cwd,
				  string_from(str_arena, "static"));
}
void get_shared_lib_vec(Arena *str_arena, Vector *src_arr, Vector *source_files,
						yyjson_val *root, /* yyjson_val *deps,*/ String *cwd) {
	get_files_vec(str_arena, src_arr, source_files, root /*, deps*/, cwd,
				  string_from(str_arena, "dyn"));
}

long long get_file_modified_time(const char *path) {
	struct stat attr;
	if (stat(path, &attr) == 0) {
		return (long long)attr.st_mtime;
	}
	return 0;
}

bool are_headers_newer(const char *d_file_path, long long obj_time) {
	FILE *f = fopen(d_file_path, "r");
	if (!f)
		return false;

	char token[1024];
	bool should_recompile = false;

	while (fscanf(f, "%1023s", token) == 1) {
		size_t len = strlen(token);
		if (len == 0)
			continue;

		if (strcmp(token, "\\") == 0)
			continue;

		if (token[len - 1] == ':') {
			token[len - 1] = '\0';
			len--;

			if (len == 0)
				continue;
		}

		if (strstr(token, ".cache/") != NULL && strstr(token, ".o") != NULL) {
			continue;
		}

		long long dep_file_time = get_file_modified_time(token);
		if (dep_file_time > obj_time) {
			should_recompile = true;
			break;
		}
	}

	fclose(f);
	return should_recompile;
}

bool directory_exists(const char *path) {
#ifdef _WIN32
	DWORD dwAttrib = GetFileAttributesA(path);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
			(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#else
	struct stat stats;
	return (stat(path, &stats) == 0 && S_ISDIR(stats.st_mode));
#endif
}

bool file_exists(const char *file_name) {
	if (access(file_name, F_OK) == 0) {
		return true;
	}
	return false;
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

int remove_directory(Arena *arena, const char *path) {
	DIR *d = opendir(path);
	size_t path_len = strlen(path);
	int r = -1;

	if (d) {
		struct dirent *p;
		r = 0;

		while (!r && (p = readdir(d))) {
			int r2 = -1;
			char *buf;
			size_t len;

			if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
				continue;
			}

			len = path_len + strlen(p->d_name) + 2;
			buf = arena_alloc(arena, len);

			if (buf) {
				struct stat statbuf;
				snprintf(buf, len, "%s/%s", path, p->d_name);

				if (!stat(buf, &statbuf)) {
					if (S_ISDIR(statbuf.st_mode)) {
						r2 = remove_directory(arena, buf);
					} else {
						r2 = unlink(buf);
					}
				}
				// free(buf);
			}
			r = r2;
		}
		closedir(d);
	}

	if (!r) {
		r = rmdir(path);
	}
	return r;
}

char *read_current_version_from_file(Arena *arena) {
	FILE *file = fopen("./build/.cache/VERSION", "r");
	if (!file) {
		return arena_strdup(arena, "unknown");
	}

	char buffer[256];
	if (!fgets(buffer, sizeof(buffer), file)) {
		fclose(file);
		return arena_strdup(arena, "unknown");
	}
	fclose(file);

	buffer[strcspn(buffer, "\r\n")] = '\0';

	// fclose(file);
	return arena_strdup(arena, buffer);
}

void update_version_file(char *version) {
	FILE *file = fopen("./build/.cache/VERSION", "w");
	if (!file) {
		perror("[x] Failed to open/create file\n");
		return;
	}

	fputs(version, file);
	fclose(file);
}

void expand_line(const char *src, FILE *out, yyjson_val *vars) {
	const char *ptr = src;

	while (*ptr != '\0') {
		if (*ptr == '@') {
			const char *end = strchr(ptr + 1, '@');
			if (end) {
				size_t key_len = (size_t)(end - (ptr + 1));
				char key[128];
				if (key_len < sizeof(key)) {
					strncpy(key, ptr + 1, key_len);
					key[key_len] = '\0';

					const char *val = yyjson_get_str(yyjson_obj_get(vars, key));
					if (val) {
						fputs(val, out);
					}
					ptr = end + 1;
					continue;
				}
			}
		}
		fputc(*ptr++, out);
	}
}

int gen_header_from_tmpl(const char *in_path, const char *out_path) {
	FILE *in = fopen(in_path, "r");
	if (!in) {
		printf("file read error\n");
		return -1;
	}

	FILE *out = fopen(out_path, "w");
	if (!out) {
		fclose(in);
		return -1;
	}
	yyjson_read_err err;
	yyjson_doc *doc = yyjson_read_file("./composition.json", 0, NULL, &err);
	if (!doc) {
		fprintf(stderr, "Read error: %s\n", err.msg);
		return -1;
	}
	yyjson_val *root = yyjson_doc_get_root(doc);

	yyjson_val *tmpl_vars = yyjson_obj_get(root, "tmpl");

	char line[1024];
	while (fgets(line, sizeof(line), in)) {
		char *trimmed = line;
		while (isspace((unsigned char)*trimmed))
			trimmed++;

		if (strncmp(trimmed, "#cmakedefine ", 13) == 0) {
			char key[128] = {0};
			char rest[256] = {0};

			int matched = sscanf(trimmed + 13, "%127s %[^\n]", key, rest);
			const char *val = yyjson_get_str(yyjson_obj_get(tmpl_vars, key));

			if (val && strcmp(val, "0") != 0 && strcmp(val, "OFF") != 0) {
				if (matched > 1 && strlen(rest) > 0) {
					fprintf(out, "#define %s ", key);
					expand_line(rest, out, tmpl_vars);
					fputc('\n', out);
				} else {
					fprintf(out, "#define %s\n", key);
				}
			} else {
				fprintf(out, "/* #undef %s */\n", key);
			}
			continue;
		}

		expand_line(line, out, tmpl_vars);
	}

	fclose(in);
	fclose(out);
	yyjson_doc_free(doc);
	return 0;
}

char *format_path(char *path) {
	if (STR_CMP(path, ".") == 0) {
		return "";
	}
	if (path[0] == '.' && path[1] == '/') {
		return path + 2;
	}
	return path;
}

int has_extension(const char *filename, const char *ext) {
	size_t len = strlen(filename);
	size_t ext_len = strlen(ext);
	if (len < ext_len)
		return 0;
	return STR_CMP(filename + len - ext_len, ext) == 0;
}

int is_source_file(const char *filename) {
	return has_extension(filename, ".c") || has_extension(filename, ".cpp") ||
		   has_extension(filename, ".cc") || has_extension(filename, ".cxx");
}

int is_header_file(const char *filename) {
	return has_extension(filename, ".h") || has_extension(filename, ".hpp") ||
		   has_extension(filename, ".hxx") || has_extension(filename, ".h.in");
}
int is_static_lib_file(const char *filename) {
	return has_extension(filename, ".a");
}
int is_shared_lib_file(const char *filename) {
	return has_extension(filename, ".so");
}

int is_path_excluded(Vector *excludes_dirs, char *dir_path) {
	const char *formatted_dir = format_path(dir_path);
	size_t dir_len = strlen(formatted_dir);

	for (int i = 0; i < length(excludes_dirs); i++) {
		const char *excluded = at(const char *, excludes_dirs, i);
		size_t ex_len = strlen(excluded);

		if (ex_len == 0)
			continue;

		if (strncmp(formatted_dir, excluded, ex_len) == 0) {
			if (formatted_dir[ex_len] == '\0' || formatted_dir[ex_len] == '/') {
				return 1;
			}
		}
	}
	return 0;
}

void traverse_dir(Arena *arena, String *dir_path, Vector *src_dirs,
				  Vector *hdr_dirs, Vector *static_libs, Vector *shared_libs,
				  Vector *excludes_dirs) {
	if (is_path_excluded(excludes_dirs, string(dir_path))) {
		return;
	}
	DIR *dir = opendir(string(dir_path));
	if (!dir)
		return;

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 ||
			strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		String *full_path =
			string_concat_cstr(arena, 3, string(dir_path), "/", entry->d_name);

		struct stat st;
		if (stat(string(full_path), &st) == -1)
			continue;

		if (S_ISDIR(st.st_mode)) {
			traverse_dir(arena, full_path, src_dirs, hdr_dirs, static_libs,
						 shared_libs, excludes_dirs);
		} else if (S_ISREG(st.st_mode)) {
			if (is_source_file(entry->d_name)) {
				set_add(src_dirs, format_path(string(dir_path)));
			}
			if (is_header_file(entry->d_name)) {
				set_add(hdr_dirs, format_path(string(dir_path)));
			}
			if (is_static_lib_file(entry->d_name)) {
				set_add(static_libs, format_path(string(dir_path)));
			}
			if (is_shared_lib_file(entry->d_name)) {
				set_add(shared_libs, format_path(string(dir_path)));
			}
		}
	}

	closedir(dir);
}

void clear_cache() {
	Arena *arena = arena_init(2048);
	remove_directory(arena, "build/.cache");
	arena_free(&arena);
}
