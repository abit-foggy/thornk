#include "include/config.h"
#include "include/scan.h"
#include "include/resolve.h"
#include "include/post.h"
#include "include/clean.h"

/* CLI configuration */
char g_kernel_dir[512] = ".";
static char g_config_path[512] = "";
char g_out_file[512] = "build.ninja";
static char g_cli_args[64][512];
static int g_ncli_args = 0;

PithValue *format_stat(PithValue *label_val, int val) {
    const char *label = label_val ? pithStringData(label_val) : "";
    char buf[256];
    snprintf(buf, sizeof(buf), "%s%d", label, val);
    return pithNewString(buf);
}

int print_stat(PithValue *label_val, int val) {
    const char *label = label_val ? pithStringData(label_val) : "";
    printf("%s%d\n", label, val);
    return 0;
}

void add_cli_arg(PithValue *val) {
    if (!val || g_ncli_args >= 64) return;
    const char *s = pithStringData(val);
    strncpy(g_cli_args[g_ncli_args], s, sizeof(g_cli_args[0]) - 1);
    g_cli_args[g_ncli_args][sizeof(g_cli_args[0]) - 1] = '\0';
    g_ncli_args++;
}

void process_cli_args(void) {
    if (g_ncli_args < 1) return;
    strncpy(g_kernel_dir, g_cli_args[0], sizeof(g_kernel_dir) - 1);
    snprintf(g_config_path, sizeof(g_config_path), "%s/.config", g_kernel_dir);
    strncpy(g_arch, "x86", sizeof(g_arch) - 1);
    strncpy(g_out_file, "build.ninja", sizeof(g_out_file) - 1);

    /* Legacy 2-argument mode: thornk <kbuild_file> <output_spec> */
    if (g_ncli_args == 2 && g_cli_args[1][0] != '-') {
        strncpy(g_out_file, g_cli_args[1], sizeof(g_out_file) - 1);
        return;
    }

    for (int i = 1; i < g_ncli_args; i++) {
        if (strcmp(g_cli_args[i], "--config") == 0 && i + 1 < g_ncli_args) {
            strncpy(g_config_path, g_cli_args[++i], sizeof(g_config_path) - 1);
        } else if (strcmp(g_cli_args[i], "--arch") == 0 && i + 1 < g_ncli_args) {
            strncpy(g_arch, g_cli_args[++i], sizeof(g_arch) - 1);
        } else if (strcmp(g_cli_args[i], "--out") == 0 && i + 1 < g_ncli_args) {
            strncpy(g_out_file, g_cli_args[++i], sizeof(g_out_file) - 1);
        } else if (strcmp(g_cli_args[i], "--spec") == 0 && i + 1 < g_ncli_args) {
            strncpy(g_spec_file, g_cli_args[++i], sizeof(g_spec_file) - 1);
        } else if (strcmp(g_cli_args[i], "--clean-legacy") == 0 ||
                   strcmp(g_cli_args[i], "--remove-legacy") == 0) {
            g_clean_legacy = 1;
        } else if (strcmp(g_cli_args[i], "--clean-only") == 0 ||
                   strcmp(g_cli_args[i], "--remove-only") == 0) {
            g_clean_legacy = 1;
            g_clean_only = 1;
        } else if (strcmp(g_cli_args[i], "--keep-legacy") == 0 ||
                   strcmp(g_cli_args[i], "--no-clean-legacy") == 0) {
            g_clean_legacy = -1;
        }
    }
}

PithValue *get_kernel_dir(void) {
    return pithNewString(g_kernel_dir);
}

PithValue *get_config_path(void) {
    return pithNewString(g_config_path);
}

PithValue *get_arch(void) {
    return pithNewString(g_arch);
}

PithValue *get_out_file(void) {
    return pithNewString(g_out_file);
}

/* Kconfig delegates */
void kconfig_reset(void) {
    scan_reset();
}

int parse_kconfig_file(PithValue *path_val) {
    return config_parse_file(path_val);
}

PithValue *get_kconfig(PithValue *key_val) {
    return config_get(key_val);
}

int is_config_set(PithValue *key_val) {
    return config_is_set(key_val);
}

PithValue *eval_expr(PithValue *expr_val) {
    return config_eval_expr(expr_val);
}

int is_expr_enabled(PithValue *expr_val) {
    return config_is_expr_enabled(expr_val);
}

/* Scanning delegates */
PithValue *find_kbuild_file(PithValue *root_val, PithValue *scope_val) {
    return scan_find_kbuild_file(root_val, scope_val);
}

int scan_kbuild_subdirs(PithValue *makefile_path_val, PithValue *scope_val) {
    return scan_subdirs(makefile_path_val, scope_val);
}

PithValue *get_scanned_subdir(int idx) {
    return scan_get_subdir(idx);
}

void push_subdir_level(void) {
    scan_push_subdir_level();
}

void pop_subdir_level(void) {
    scan_pop_subdir_level();
}

int is_dir_visited(PithValue *path_val) {
    return scan_is_dir_visited(path_val);
}

int mark_dir_visited(PithValue *path_val) {
    return scan_mark_dir_visited(path_val);
}

int scan_composite_objects(PithValue *makefile_path_val, PithValue *scope_val) {
    return scan_composites(makefile_path_val, scope_val);
}

int get_composite_count(void) {
    return scan_get_composite_count();
}

PithValue *get_composite_target(int idx) {
    return scan_get_composite_target(idx);
}

PithValue *get_composite_scope(int idx) {
    return scan_get_composite_scope(idx);
}

int get_composite_is_module(int idx) {
    return scan_get_composite_is_module(idx);
}

int get_composite_member_count(int idx) {
    return scan_get_composite_member_count(idx);
}

PithValue *get_composite_member(int idx, int m_idx) {
    return scan_get_composite_member(idx, m_idx);
}

int scan_direct_objects(PithValue *makefile_path_val, PithValue *scope_val) {
    return scan_directs(makefile_path_val, scope_val);
}

int get_direct_count(void) {
    return scan_get_direct_count();
}

PithValue *get_direct_member(int idx) {
    return scan_get_direct_member(idx);
}

/* Resolution delegates */
PithValue *qualify_target(PithValue *scope_val, PithValue *target_val) {
    return resolve_qualify_target(scope_val, target_val);
}

PithValue *resolve_source_path(PithValue *root_dir_val, PithValue *scope_val, PithValue *obj_val) {
    return resolve_source(root_dir_val, scope_val, obj_val);
}

/* Post-processing delegates */
int is_spec_output(void) {
    return post_is_spec_output();
}

PithValue *get_spec_file(void) {
    return post_get_spec_file();
}

PithValue *get_out_dir(void) {
    return post_get_out_dir();
}

int run_thorn(PithValue *spec_path_val, PithValue *out_dir_val) {
    return post_run_thorn(spec_path_val, out_dir_val);
}

int postprocess_vmlinux(PithValue *ninja_path_val, PithValue *kernel_dir_val, PithValue *arch_val) {
    return post_vmlinux(ninja_path_val, kernel_dir_val, arch_val);
}

/* Cleanup delegates */
int is_clean_only(void) {
    return clean_is_only();
}

int process_legacy_cleanup(void) {
    return clean_process_legacy(g_kernel_dir);
}

/* Modular units included in translation */
#include "config.c"
#include "scan.c"
#include "resolve.c"
#include "post.c"
#include "clean.c"

