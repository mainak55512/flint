#ifndef FLINT_H
#define FLINT_H

#include <arena.h>
#include <cmap.h>
#include <container.h>
#include <cstring.h>
#include <ctype.h>
#include <errno.h>
#include <mongoose.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <yyjson.h>

#if defined(_WIN32)
#define CURRENT_OS "windows"
#define IS_UNIX 0
#elif defined(__APPLE__)
#define CURRENT_OS "macos"
#define IS_UNIX 1
#elif defined(__linux__)
#define CURRENT_OS "linux"
#define IS_UNIX 1
#else
#define CURRENT_OS "unknown"
#define IS_UNIX 0
#endif

#ifdef _WIN32
// #define _CRT_SECURE_NO_WARNINGS
#include <direct.h>
#include <io.h>
#include <string.h>
#include <sys/stat.h>
#include <windows.h>

#if defined(__MINGW32__) || defined(__MINGW64__)
#include <dirent.h>
#endif

#define strcasecmp _stricmp
#define strncasecmp _strnicmp

#define MAKE_DIR(dir) _mkdir(dir)
#define STAT _stat64
#define STR_CMP _stricmp
#define GET_WD _getcwd

#define NULL_DEVICE "2>NUL"

#if !defined(__MINGW32__) && !defined(__MINGW64__)
#define popen _popen
#define pclose _pclose
#endif

#else
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAKE_DIR(dir) mkdir(dir, 0777)
#define STAT stat
#define STR_CMP strcasecmp
#define GET_WD getcwd
#define NULL_DEVICE "2>/dev/null"
#endif

#define BUFFER_SIZE 4096

typedef struct {
	char *repo_name;
	String *version;
	String *remote;
	String *hash;
} Dependency;

typedef struct {
	String *version;
	String *hash;
	String *remote;
	Vector *flags;
	Vector *lib_links;
	Vector *excludes;
	Vector *exclude_dirs;
	Vector *exclude_exception;
	Cmap *tmpl;
} Sync_config;

typedef struct {
	String *project_name;
	String *project_language;
	String *compiler_path;
	String *version;
	bool executable;
	Vector *flags;
	Vector *lib_links;
	Vector *excludes;
	Vector *exclude_dirs;
	Cmap *tmpl;
	Cmap *dependencies;
	Vector *exclude_exception;
	Cmap *sync;
	Arena *arena;
} Config;

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#ifndef PROJECT_VERSION
#define PROJECT_VERSION unknown
#endif

int create_append_file(char *file_path, char *content);
void create_my_build_config(char *config_file_path, char *project_name,
							char *project_lang, char *compiler_path,
							bool isExec);
int check_project_lang(char *lang);
String *build_project(Arena *global_str_arena);
bool starts_with(char *str, char *prefix);
bool set_contains_str(Vector *v, String *elem);
void set_add_str(Vector *v, String *elem);
char *safe_str(String *s);
Config *config_init();
Sync_config *read_sync_config(Arena *arena, yyjson_val *val);
void sync_config_free(Sync_config *conf);
void config_free(Config *conf);
void read_flint_composition(char *composition_path, Config *conf);
void write_flint_composition(char *composition_path, Config *conf);
String *safe_get_string(Arena *arena, yyjson_val *obj, const char *key);
Vector *parse_string_array(Arena *arena, yyjson_val *obj, char *key);
Cmap *read_lock_file();
bool lock_contains(Cmap *lock, char *libURL);
void write_lock_file(Cmap *lock);

bool set_contains(Vector *v, char *elem);
void set_add(Vector *v, char *elem);
Dependency *clone_lib(Arena *arena, char *libURL, const char *hash);
bool is_mybuild_config_present(char *filename);
int init_project();
String *collect_src_files(Arena *str_arena, String *path);
String *collect_files(Arena *str_arena, String *path, String *type);
Vector *string_split(Arena *arena, String *str, char sep);
Vector *string_split_lines(Arena *arena, String *str);
String *get_current_working_dir(Arena *arena);
int generate_compile_commands(Config *conf);
char *get_repo_name(Arena *arena, const char *git_url);
char *get_version_number(Arena *arena, const char *git_url);
char *get_modified_url(Arena *arena, const char *git_url);
char *get_lib_hash(Arena *arena, char *target_dir);
char *arena_strdup(Arena *arena, const char *str);
char *get_tag_from_hash(Arena *arena, const char *target_dir,
						const char *ref_hash);
Dependency *clone_lib_hashed(Arena *arena, char *libURL, const char *ref_hash);
void add_lib(char *libURL);
void remove_library_partial(char *libURL);
void remove_library(char *repo_name);
void run_project(Arena *global_str_arena);
void sync_lib(char *elem);
const char *get_filename_without_path(const char *path);
int cli(int argc, char *argv[], Arena *global_str_arena);
long long get_file_modified_time(const char *path);
bool are_headers_newer(const char *d_file_path, long long obj_time);
bool directory_exists(const char *path);
String *get_flags(Arena *str_arena, yyjson_val *root, String *type);
int copy_file(const char *src_path, const char *dest_path);
bool file_exists(const char *file_name);
void add_local_lib(int lib_count, char **lib_link);
void add_flag(int lib_count, char **lib_link);
// bool check_if_dep_path(const char *str);
int remove_directory(Arena *arena, const char *path);
char *read_current_version_from_file(Arena *arena);
void update_version_file(char *version);
Vector *remove_excludes(Vector *collected, Vector *excludes);
void list_deps();
void expand_line(const char *src, FILE *out, yyjson_val *vars);
int gen_header_from_tmpl(const char *in_path, const char *out_path);

void traverse_dir(Arena *arena, String *dir_path, Vector *src_dirs,
				  Vector *hdr_dirs, Vector *static_libs, Vector *shared_libs,
				  Vector *excludes_dirs);

void get_src_vec(Config *conf, Vector *src_arr, Vector *source_files,
				 String *cwd);
void get_header_vec(Config *conf, Vector *header_arr, Vector *header_vec,
					String *cwd);
void get_stat_lib_vec(Config *conf, Vector *src_arr, Vector *source_files,
					  String *cwd);
void get_shared_lib_vec(Config *conf, Vector *src_arr, Vector *source_files,
						String *cwd);
void clear_cache();

void fetch_library(Config *current_config, char *libURL, char *hash, Cmap *lock,
				   bool sync);
String *join_vec_to_str(Arena *arena, Vector *v, String *sep);
bool is_platform_element(char *entry, char **out_flag);
Sync_config *get_remote_composition(Arena *arena, char *fragment);

#endif // FLINT_H
