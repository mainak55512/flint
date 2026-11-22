#ifndef FLINT_H
#define FLINT_H

#include <arstd.h>
#include <ctype.h>
#include <errno.h>
#include <flint_err.h>
#include <flint_log.h>
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
	Vector(String *) flags;
	Vector(String *) lib_links;
	Vector(String *) excludes;
	Vector(String *) exclude_dirs;
	Vector(String *) exclude_exception;
	Cmap(String *) tmpl;
} Sync_config;

typedef struct {
	String *project_name;
	String *project_language;
	String *compiler_path;
	String *version;
	bool executable;
	Vector(String *) flags;
	Vector(String *) lib_links;
	Vector(String *) excludes;
	Vector(String *) exclude_dirs;
	Cmap(String *) tmpl;
	Cmap(Dependency *) dependencies;
	Vector(String *) exclude_exception;
	Cmap(Sync_config *) sync;
	Arena *arena;
} Config;

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#ifndef PROJECT_VERSION
#define PROJECT_VERSION unknown
#endif

FLINT_DEFINE_RESULT(int, flint_res_int_t);
FLINT_DEFINE_RESULT(size_t, flint_res_size_t);
FLINT_DEFINE_RESULT(bool, flint_res_bool_t);
FLINT_DEFINE_RESULT(char *, flint_res_str_t);
FLINT_DEFINE_RESULT(const char *, flint_res_cstr_t);
FLINT_DEFINE_RESULT(void *, flint_res_ptr_t);
FLINT_DEFINE_RESULT(String *, flint_res_string_t);
FLINT_DEFINE_RESULT(Config *, flint_res_config_t);
FLINT_DEFINE_RESULT(Dependency *, flint_res_dep_t);
FLINT_DEFINE_RESULT(Vector(String *), flint_res_string_vec_t);
FLINT_DEFINE_RESULT(Cmap(String *), flint_res_string_map_t);
FLINT_DEFINE_RESULT(Cmap(Dependency *), flint_res_dep_map_t);
FLINT_DEFINE_RESULT(Sync_config *, flint_res_sync_config_t);

int create_append_file(char *file_path, char *content);
void create_my_build_config(char *config_file_path, char *project_name,
							char *project_lang, char *compiler_path,
							bool isExec);
int check_project_lang(char *lang);
flint_res_string_t build_project(Arena *global_str_arena);
bool set_contains_str(Vector(String *) v, String *elem);
void set_add_str(Vector(String *) v, String *elem);
char *safe_str(String *s);
flint_res_config_t config_init();
Sync_config *read_sync_config(Arena *arena, yyjson_val *val);
void sync_config_free(Sync_config *conf);
void config_free(Config *conf);
flint_err_t read_flint_composition(char *composition_path, Config *conf);
flint_err_t write_flint_composition(char *composition_path, Config *conf);
String *safe_get_string(Arena *arena, yyjson_val *obj, const char *key);
Vector(String *) parse_string_array(Arena *arena, yyjson_val *obj, char *key);
flint_res_dep_map_t read_lock_file(Arena *arena);
bool lock_contains(Arena *arena, Cmap(Dependency *) lock, char *libURL);
flint_err_t write_lock_file(Cmap(Dependency *) lock);

bool set_contains(Vector(const char *) v, const char *elem);
void set_add(Vector(const char *) v, const char *elem);
flint_res_dep_t clone_lib(Arena *arena, char *libURL, const char *hash);
bool is_mybuild_config_present(char *filename);
flint_err_t init_project();
String *collect_src_files(Arena *str_arena, String *path);
String *collect_files(Arena *str_arena, String *path, String *type);
Vector(String *) string_split_lines(Arena *arena, String *str);
String *get_current_working_dir(Arena *arena);
flint_err_t generate_compile_commands(Config *conf);
char *get_repo_name(Arena *arena, const char *git_url);
char *get_version_number(Arena *arena, const char *git_url);
char *get_modified_url(Arena *arena, const char *git_url);
char *get_lib_hash(Arena *arena, char *target_dir);
char *arena_strdup(Arena *arena, const char *str);
char *get_tag_from_hash(Arena *arena, const char *target_dir,
						const char *ref_hash);
flint_res_dep_t clone_lib_hashed(Arena *arena, char *libURL,
								 const char *ref_hash);
flint_err_t add_lib(char *libURL);
flint_err_t remove_library_partial(Arena *arena, char *libURL);
flint_err_t remove_library(char *repo_name);
flint_err_t run_project(Arena *global_str_arena);
flint_err_t sync_lib(char *elem);
const char *get_filename_without_path(const char *path);
flint_err_t cli(int argc, char *argv[], Arena *global_str_arena);
long long get_file_modified_time(const char *path);
bool are_headers_newer(const char *d_file_path, long long obj_time);
bool directory_exists(const char *path);
String *get_flags(Arena *str_arena, yyjson_val *root, String *type);
int copy_file(const char *src_path, const char *dest_path);
bool file_exists(const char *file_name);
flint_err_t add_local_lib(int lib_count, char **lib_link);
flint_err_t add_flag(int lib_count, char **lib_link);
int remove_directory(Arena *arena, const char *path);
char *read_current_version_from_file(Arena *arena);
void update_version_file(char *version);
Vector(String *) remove_excludes(Arena *arena, Vector(String *) collected,
								 Vector(String *) excludes);
flint_err_t list_deps();
void expand_line(const char *src, FILE *out, yyjson_val *vars);
int gen_header_from_tmpl(const char *in_path, const char *out_path);

flint_err_t
traverse_dir(Arena *arena, String *dir_path, Vector(const char *) src_dirs,
			 Vector(const char *) hdr_dirs, Vector(const char *) static_libs,
			 Vector(const char *) shared_libs, Vector(String *) excludes_dirs);

void get_src_vec(Config *conf, Vector(const char *) src_arr,
				 Vector(char *) source_files, String *cwd);
void get_header_vec(Config *conf, Vector(const char *) header_arr,
					Vector(char *) header_vec, String *cwd);
void get_stat_lib_vec(Config *conf, Vector(const char *) src_arr,
					  Vector(char *) source_files, String *cwd);
void get_shared_lib_vec(Config *conf, Vector(const char *) src_arr,
						Vector(char *) source_files, String *cwd);
void clear_cache();

flint_err_t fetch_library(Config *current_config, char *libURL, char *hash,
						  Cmap(Dependency *) lock, bool sync);
String *join_vec_to_str(Arena *arena, Vector(String *) v, const char *sep);
bool is_platform_element(char *entry, char **out_flag);
flint_res_sync_config_t get_remote_composition(Arena *arena, char *fragment);

#endif // FLINT_H
