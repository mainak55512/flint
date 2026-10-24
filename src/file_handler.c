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
	// Arena *arena = arena_init(1024);
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
		ext_3 = string_from(str_arena, ".dylib");
	} else if (STR_CMP(string(type), "header") == 0) {
		sep[0] = '\n';
		ext_1 = string_from(str_arena, ".h");
		ext_2 = string_from(str_arena, ".hpp");
		ext_3 = string_from(str_arena, ".h.in");
	}

#if defined(_WIN32)
	WIN32_FIND_DATAA find_data;
	String *search_pattern =
		string_concat_cstr(str_arena, 2, string(path), "/*");
	HANDLE hFind = FindFirstFileA(string(search_pattern), &find_data);

	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			const char *d_name = find_data.cFileName;
#else
	DIR *dir = opendir(string(path));
	struct dirent *entry;

	if (dir != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			const char *d_name = entry->d_name;
#endif
			if (STR_CMP(d_name, ".") == 0 || STR_CMP(d_name, "..") == 0) {
				continue;
			}
			size_t name_len = strlen(d_name);
			const char *dot = strrchr(d_name, '.');
			const char *matched_name = d_name;
			char header_name[256];

			bool match = (dot != NULL && (STR_CMP(dot, string(ext_1)) == 0 ||
										  STR_CMP(dot, string(ext_2)) == 0));

			if (!match && ext_3 != NULL) {
				if (STR_CMP(string(type), "header") == 0) {
					size_t ext3_len = string_len(ext_3);
					if (name_len >= ext3_len &&
						STR_CMP(d_name + name_len - ext3_len, string(ext_3)) ==
							0) {
						match = true;
						size_t target_len = name_len - 3;

						if (target_len < sizeof(header_name)) {
							strncpy(header_name, d_name, target_len);
							header_name[target_len] = '\0';
						} else {
							strncpy(header_name, d_name,
									sizeof(header_name) - 1);
							header_name[sizeof(header_name) - 1] = '\0';
						}

						char *in_path = string(string_concat_cstr(
							str_arena, 3, string(path), "/", d_name));
						char *out_path = string(string_concat_cstr(
							str_arena, 3, string(path), "/", header_name));
						gen_header_from_tmpl(in_path, out_path);
						matched_name = header_name;
					}
				} else if (dot != NULL && STR_CMP(dot, string(ext_3)) == 0) {
					match = true;
				}
			}

			if (match) {
				if (string_len(src_files) > 0) {
					src_files = string_concat_cstr(str_arena, 2,
												   string(src_files), sep);
				}

				src_files = string_concat_cstr(str_arena, 4, string(src_files),
											   string(path), "/", matched_name);
			}
#if defined(_WIN32)
		} while (FindNextFileA(hFind, &find_data) != 0);
		FindClose(hFind);
	}
#else
		}
		closedir(dir);
	}
#endif
	// arena_free(&arena);
	return src_files;
}

void get_files_vec(Config *conf, Vector *src_arr, Vector *source_files,
				   String *cwd, String *file_type) {

	for (int i = 0; i < length(src_arr); i++) {
		Vector *src_temp_arr;

		src_temp_arr = remove_excludes(
			string_split_lines(
				conf->arena,
				collect_files(conf->arena,
							  string_from(conf->arena, at(char *, src_arr, i)),
							  string_from(conf->arena, string(file_type)))),
			conf->excludes);
		for (int i = 0; i < length(src_temp_arr); i++) {
			char *elem = string(at(String *, src_temp_arr, i));
			if (STR_CMP(elem, "") != 0) {
				append(char *, source_files, elem);
			}
		}
		vector_free(&src_temp_arr);
	}
}

void get_src_vec(Config *conf, Vector *src_arr, Vector *source_files,
				 String *cwd) {
	get_files_vec(conf, src_arr, source_files, cwd,
				  string_from(conf->arena, "src"));
}

void get_header_vec(Config *conf, Vector *src_arr, Vector *source_files,
					String *cwd) {
	get_files_vec(conf, src_arr, source_files, cwd,
				  string_from(conf->arena, "header"));
}

void get_stat_lib_vec(Config *conf, Vector *src_arr, Vector *source_files,
					  String *cwd) {
	get_files_vec(conf, src_arr, source_files, cwd,
				  string_from(conf->arena, "static"));
}
void get_shared_lib_vec(Config *conf, Vector *src_arr, Vector *source_files,
						String *cwd) {
	get_files_vec(conf, src_arr, source_files, cwd,
				  string_from(conf->arena, "dyn"));
}

long long get_file_modified_time(const char *path) {
	struct STAT attr;
	if (STAT(path, &attr) == 0) {
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

		if ((strstr(token, ".cache/") != NULL ||
			 strstr(token, ".cache\\") != NULL) &&
			strstr(token, ".o") != NULL) {
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
#if defined(_WIN32)
	return _access(file_name, 0) == 0;
#else
	return access(file_name, F_OK) == 0;
#endif
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
	size_t path_len = strlen(path);
	int r = -1;

#if defined(_WIN32)
	// WIN32_FIND_DATAA find_data;
	// size_t search_len = path_len + 3;
	// char *search_path = arena_alloc(arena, search_len);
	// if (!search_path)
	// 	return -1;
	// snprintf(search_path, search_len, "%s/*", path);

	// HANDLE hFind = FindFirstFileA(search_path, &find_data);
	// if (hFind != INVALID_HANDLE_VALUE) {
	// 	r = 0;
	// 	do {
	// 		const char *d_name = find_data.cFileName;
	// 		if (!strcmp(d_name, ".") || !strcmp(d_name, "..")) {
	// 			continue;
	// 		}

	// 		int r2 = -1;
	// 		size_t len = path_len + strlen(d_name) + 2;
	// 		char *buf = arena_alloc(arena, len);

	// 		if (buf) {
	// 			snprintf(buf, len, "%s/%s", path, d_name);
	// 			if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
	// 				r2 = remove_directory(arena, buf);
	// 			} else {
	// 				if (find_data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
	// 					SetFileAttributesA(buf, find_data.dwFileAttributes &
	// 												~FILE_ATTRIBUTE_READONLY);
	// 				}
	// 				r2 = _unlink(buf);
	// 			}
	// 		}
	// 		r = r2;
	// 	} while (!r && FindNextFileA(hFind, &find_data) != 0);
	// 	FindClose(hFind);
	// }

	// if (!r) {
	// 	DWORD attr = GetFileAttributesA(path);
	// 	if (attr != INVALID_FILE_ATTRIBUTES &&
	// 		(attr & FILE_ATTRIBUTE_READONLY)) {
	// 		SetFileAttributesA(path, attr & ~FILE_ATTRIBUTE_READONLY);
	// 	}
	// 	r = _rmdir(path);
	// }

	int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
	if (wlen <= 0)
		return -1;

	wchar_t *wpath = arena_alloc(arena, (wlen + 1) * sizeof(wchar_t));

	if (!wpath)
		return -1;

	MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wlen);
	wpath[wlen] = L'\0';

	size_t search_len = wcslen(wpath) + 3;
	wchar_t *wsearch_path = arena_alloc(arena, search_len * sizeof(wchar_t));
	if (!wsearch_path)
		return -1;
	swprintf(wsearch_path, search_len, L"%ls/*", wpath);

	WIN32_FIND_DATAW find_data;
	HANDLE hFind = FindFirstFileW(wsearch_path, &find_data);
	r = 0;

	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			wchar_t *d_name = find_data.cFileName;
			if (!wcscmp(d_name, L".") || !wcscmp(d_name, L"..")) {
				continue;
			}

			size_t item_wlen = wcslen(wpath) + wcslen(d_name) + 2;
			wchar_t *wbuf = arena_alloc(arena, item_wlen * sizeof(wchar_t));
			if (!wbuf) {
				r = -1;
				break;
			}
			swprintf(wbuf, item_wlen, L"%ls/%ls", wpath, d_name);

			if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				int u8_len = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, NULL, 0,
												 NULL, NULL);
				char *u8_buf = arena_alloc(arena, u8_len);
				if (!u8_buf) {
					r = -1;
					break;
				}
				WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, u8_buf, u8_len, NULL,
									NULL);

				if (remove_directory(arena, u8_buf) < 0) {
					r = -1;
				}
			} else {
				if (find_data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
					SetFileAttributesW(wbuf, find_data.dwFileAttributes &
												 ~FILE_ATTRIBUTE_READONLY);
				}
				if (_wunlink(wbuf) < 0) {
					r = -1;
				}
			}
		} while (FindNextFileW(hFind, &find_data) != 0);
		FindClose(hFind);
	} else {
		r = -1;
	}

	if (r == 0) {
		DWORD attr = GetFileAttributesW(wpath);
		if (attr != INVALID_FILE_ATTRIBUTES &&
			(attr & FILE_ATTRIBUTE_READONLY)) {
			SetFileAttributesW(wpath, attr & ~FILE_ATTRIBUTE_READONLY);
		}

		int retries = 5;
		while (retries > 0) {
			if (_wrmdir(wpath) == 0) {
				r = 0;
				break;
			}
			r = -1;
			Sleep(50);
			retries--;
		}
	}

#else
	DIR *d = opendir(path);

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

				if (!lstat(buf, &statbuf)) {
					if (S_ISDIR(statbuf.st_mode)) {
						r2 = remove_directory(arena, buf);
					} else {
						r2 = unlink(buf);
					}
				}
			}
			r = r2;
		}
		closedir(d);
	}

	if (!r) {
		r = rmdir(path);
	}
#endif
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
	if (!path)
		return "";
	if (STR_CMP(path, ".") == 0) {
		return "";
	}
	if (path[0] == '.' && (path[1] == '/' || path[1] == '\\')) {
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
	if (!excludes_dirs) {
		return 0;
	}
	const char *formatted_dir = format_path(dir_path);

	for (int i = 0; i < length(excludes_dirs); i++) {
		char *raw_entry = string(at(String *, excludes_dirs, i));
		char *excluded = NULL;

		if (!is_platform_element(raw_entry, &excluded)) {
			continue;
		}

		size_t ex_len = strlen(excluded);
		if (ex_len == 0)
			continue;

		if (strncmp(formatted_dir, excluded, ex_len) == 0) {
			if (formatted_dir[ex_len] == '\0' || formatted_dir[ex_len] == '/'
#if defined(_WIN32)
				|| formatted_dir[ex_len] == '\\'
#endif
			) {
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

#if defined(_WIN32)
	WIN32_FIND_DATAA find_data;
	String *search_pattern =
		string_concat_cstr(arena, 2, string(dir_path), "/*");
	HANDLE hFind = FindFirstFileA(string(search_pattern), &find_data);
	if (hFind == INVALID_HANDLE_VALUE)
		return;

	do {
		const char *d_name = find_data.cFileName;
		if (strcmp(d_name, ".") == 0 || strcmp(d_name, "..") == 0) {
			continue;
		}

		String *full_path =
			string_concat_cstr(arena, 3, string(dir_path), "/", d_name);

		if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			traverse_dir(arena, full_path, src_dirs, hdr_dirs, static_libs,
						 shared_libs, excludes_dirs);
		} else {
			if (is_source_file(d_name)) {
				set_add(src_dirs, format_path(string(dir_path)));
			}
			if (is_header_file(d_name)) {
				set_add(hdr_dirs, format_path(string(dir_path)));
			}
			if (is_static_lib_file(d_name)) {
				set_add(static_libs, format_path(string(dir_path)));
			}
			if (is_shared_lib_file(d_name)) {
				set_add(shared_libs, format_path(string(dir_path)));
			}
		}
	} while (FindNextFileA(hFind, &find_data) != 0);
	FindClose(hFind);
#else
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
#endif
}

void clear_cache() {
	Arena *arena = arena_init(2048);
	remove_directory(arena, "build/.cache");
	printf("[✓] Cache cleared");
	arena_free(&arena);
}
