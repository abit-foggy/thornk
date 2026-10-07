#ifndef THORNK_SCAN_H
#define THORNK_SCAN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pith.h>

#define MAX_SUBDIRS 1024
#define MAX_VISITED_DIRS 16384
#define MAX_COMPOSITES 1024
#define MAX_MEMBERS 512
#define MAX_DIRECT 8192

typedef struct {
    char subdirs[MAX_SUBDIRS][256];
    int count;
} SubdirLevel;

typedef struct {
    char target[128];
    char scope[256];
    char members[MAX_MEMBERS][128];
    int nmembers;
    int is_module;
} CompositeTarget;

extern SubdirLevel g_subdir_stack[32];
extern int g_subdir_depth;
extern char g_visited_dirs[MAX_VISITED_DIRS][256];
extern int g_nvisited;
extern CompositeTarget g_composites[MAX_COMPOSITES];
extern int g_ncomposites;
extern char g_direct_members[MAX_DIRECT][128];
extern int g_ndirect;

void scan_reset(void);
PithValue *scan_find_kbuild_file(PithValue *root_val, PithValue *scope_val);

int scan_subdirs(PithValue *makefile_path_val, PithValue *scope_val);
PithValue *scan_get_subdir(int idx);
void scan_push_subdir_level(void);
void scan_pop_subdir_level(void);

int scan_mark_dir_visited(PithValue *path_val);
int scan_is_dir_visited(PithValue *path_val);

int scan_composites(PithValue *makefile_path_val, PithValue *scope_val);
int scan_get_composite_count(void);
PithValue *scan_get_composite_target(int idx);
PithValue *scan_get_composite_scope(int idx);
int scan_get_composite_is_module(int idx);
int scan_get_composite_member_count(int idx);
PithValue *scan_get_composite_member(int idx, int m_idx);

int scan_directs(PithValue *makefile_path_val, PithValue *scope_val);
int scan_get_direct_count(void);
PithValue *scan_get_direct_member(int idx);

#endif /* THORNK_SCAN_H */
