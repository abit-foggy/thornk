#include "include/scan.h"
#include "include/config.h"

SubdirLevel g_subdir_stack[32];
int g_subdir_depth = 0;
char g_visited_dirs[MAX_VISITED_DIRS][256];
int g_nvisited = 0;
CompositeTarget g_composites[MAX_COMPOSITES];
int g_ncomposites = 0;
char g_direct_members[MAX_DIRECT][128];
int g_ndirect = 0;

void scan_reset(void) {
    g_cfg_count = 0;
    g_subdir_depth = 0;
    for (int i = 0; i < 32; i++) g_subdir_stack[i].count = 0;
    g_nvisited = 0;
    g_ncomposites = 0;
    g_ndirect = 0;
}

PithValue *scan_find_kbuild_file(PithValue *root_val, PithValue *scope_val) {
    const char *root = root_val ? pithStringData(root_val) : ".";
    const char *scope = scope_val ? pithStringData(scope_val) : "";
    char path1[1024], path2[1024];
    if (scope && *scope) {
        snprintf(path1, sizeof(path1), "%s/%s/Kbuild", root, scope);
        snprintf(path2, sizeof(path2), "%s/%s/Makefile", root, scope);
    } else {
        snprintf(path1, sizeof(path1), "%s/Kbuild", root);
        snprintf(path2, sizeof(path2), "%s/Makefile", root);
    }
    if (access(path1, F_OK) == 0) return pithNewString(path1);
    if (access(path2, F_OK) == 0) return pithNewString(path2);
    return pithNewString("");
}

static char *trim_inplace(char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    size_t l = strlen(s);
    while (l && (s[l - 1] == ' ' || s[l - 1] == '\t' || s[l - 1] == '\r' || s[l - 1] == '\n'))
        s[--l] = '\0';
    return s;
}

static int read_logical_line(FILE *fp, char *out, size_t out_sz) {
    out[0] = '\0';
    char line_buf[2048];
    int line_started = 0;

    while (fgets(line_buf, sizeof(line_buf), fp)) {
        size_t len = strlen(line_buf);
        int has_slash = 0;
        size_t p = len;
        while (p > 0 && (line_buf[p - 1] == '\r' || line_buf[p - 1] == '\n' || line_buf[p - 1] == ' ' || line_buf[p - 1] == '\t')) {
            p--;
        }
        if (p > 0 && line_buf[p - 1] == '\\') {
            has_slash = 1;
            line_buf[p - 1] = ' ';
            line_buf[p] = '\0';
        }
        char *s = line_buf;
        if (!line_started) {
            s = trim_inplace(line_buf);
            if (!*s || *s == '#') continue;
            line_started = 1;
        }
        strncat(out, s, out_sz - strlen(out) - 1);
        if (!has_slash) return 1;
    }
    return line_started;
}

static void normalize_path(const char *scope, const char *dir, char *out, size_t out_sz) {
    char cleaned[256];
    strncpy(cleaned, dir, sizeof(cleaned) - 1);
    cleaned[sizeof(cleaned) - 1] = '\0';
    size_t clen = strlen(cleaned);
    if (clen > 0 && cleaned[clen - 1] == '/') cleaned[clen - 1] = '\0';

    if (scope && *scope) {
        snprintf(out, out_sz, "%s/%s", scope, cleaned);
    } else {
        snprintf(out, out_sz, "%s", cleaned);
    }
}

int scan_subdirs(PithValue *makefile_path_val, PithValue *scope_val) {
    if (!makefile_path_val) return 0;
    const char *mpath = pithStringData(makefile_path_val);
    const char *scope = scope_val ? pithStringData(scope_val) : "";

    FILE *fp = fopen(mpath, "r");
    if (!fp) return 0;

    SubdirLevel *level = &g_subdir_stack[g_subdir_depth];
    level->count = 0;

    char line[16384];
    while (read_logical_line(fp, line, sizeof(line))) {
        char *s = trim_inplace(line);
        if (!*s || *s == '#') continue;

        char *eq = strstr(s, ":=");
        if (!eq) eq = strstr(s, "+=");
        if (!eq) eq = strchr(s, '=');
        if (!eq) continue;

        char varname[128];
        size_t vlen = eq - s;
        if (vlen >= sizeof(varname)) vlen = sizeof(varname) - 1;
        memcpy(varname, s, vlen);
        varname[vlen] = '\0';
        char *var = trim_inplace(varname);

        char *rhs = eq + 1;
        if (*rhs == '=') rhs++;

        char resolved_var[256];
        if (strstr(var, "$(")) {
            PithValue *pv = config_eval_expr(pithNewString(var));
            strncpy(resolved_var, pithStringData(pv), sizeof(resolved_var) - 1);
            resolved_var[sizeof(resolved_var) - 1] = '\0';
        } else {
            strncpy(resolved_var, var, sizeof(resolved_var) - 1);
            resolved_var[sizeof(resolved_var) - 1] = '\0';
        }

        if (strcmp(resolved_var, "obj-y") != 0 && strcmp(resolved_var, "obj-m") != 0 &&
            strcmp(resolved_var, "subdirs-y") != 0 && strcmp(resolved_var, "subdirs-m") != 0) {
            continue;
        }

        char *tok = strtok(rhs, " \t\r\n\\");
        while (tok) {
            char exp_tok[512];
            expand_token_vars(tok, exp_tok, sizeof(exp_tok));
            if (strstr(exp_tok, "/") && !strstr(exp_tok, ".o")) {
                char norm[256];
                normalize_path(scope, exp_tok, norm, sizeof(norm));
                if (level->count < MAX_SUBDIRS) {
                    strncpy(level->subdirs[level->count], norm, sizeof(level->subdirs[0]) - 1);
                    level->subdirs[level->count][sizeof(level->subdirs[0]) - 1] = '\0';
                    level->count++;
                }
            }
            tok = strtok(NULL, " \t\r\n\\");
        }
    }
    fclose(fp);
    return level->count;
}

PithValue *scan_get_subdir(int idx) {
    SubdirLevel *level = &g_subdir_stack[g_subdir_depth];
    if (idx < 0 || idx >= level->count) return pithNewString("");
    return pithNewString(level->subdirs[idx]);
}

void scan_push_subdir_level(void) {
    if (g_subdir_depth < 31) {
        g_subdir_depth++;
        g_subdir_stack[g_subdir_depth].count = 0;
    }
}

void scan_pop_subdir_level(void) {
    if (g_subdir_depth > 0) {
        g_subdir_depth--;
    }
}

int scan_is_dir_visited(PithValue *path_val) {
    if (!path_val) return 0;
    const char *dir = pithStringData(path_val);
    for (int i = 0; i < g_nvisited; i++) {
        if (strcmp(g_visited_dirs[i], dir) == 0) return 1;
    }
    return 0;
}

int scan_mark_dir_visited(PithValue *path_val) {
    if (!path_val || g_nvisited >= MAX_VISITED_DIRS) return 0;
    const char *dir = pithStringData(path_val);
    for (int i = 0; i < g_nvisited; i++) {
        if (strcmp(g_visited_dirs[i], dir) == 0) return 0;
    }
    strncpy(g_visited_dirs[g_nvisited], dir, sizeof(g_visited_dirs[0]) - 1);
    g_visited_dirs[g_nvisited][sizeof(g_visited_dirs[0]) - 1] = '\0';
    g_nvisited++;
    return 1;
}

int scan_composites(PithValue *makefile_path_val, PithValue *scope_val) {
    if (!makefile_path_val) return 0;
    const char *mpath = pithStringData(makefile_path_val);
    const char *scope = scope_val ? pithStringData(scope_val) : "";

    FILE *fp = fopen(mpath, "r");
    if (!fp) return 0;

    int initial_composites = g_ncomposites;
    char line[16384];

    while (read_logical_line(fp, line, sizeof(line))) {
        char *s = trim_inplace(line);
        if (!*s || *s == '#') continue;

        char *eq = strstr(s, ":=");
        if (!eq) eq = strstr(s, "+=");
        if (!eq) eq = strchr(s, '=');
        if (!eq) continue;

        char varname[128];
        size_t vlen = eq - s;
        if (vlen >= sizeof(varname)) vlen = sizeof(varname) - 1;
        memcpy(varname, s, vlen);
        varname[vlen] = '\0';
        char *var = trim_inplace(varname);

        char *rhs = eq + 1;
        if (*rhs == '=') rhs++;

        char resolved_var[256];
        if (strstr(var, "$(")) {
            PithValue *pv = config_eval_expr(pithNewString(var));
            strncpy(resolved_var, pithStringData(pv), sizeof(resolved_var) - 1);
            resolved_var[sizeof(resolved_var) - 1] = '\0';
        } else {
            strncpy(resolved_var, var, sizeof(resolved_var) - 1);
            resolved_var[sizeof(resolved_var) - 1] = '\0';
        }

        int is_objs = 0;
        int is_y = 0;
        char tgt_base[128];
        tgt_base[0] = '\0';

        char *dash_objs_y = strstr(resolved_var, "-objs-y");
        char *dash_objs = strstr(resolved_var, "-objs");
        char *dash_y = strstr(resolved_var, "-y");
        if (dash_objs_y && strlen(dash_objs_y) == 7) {
            is_objs = 1;
            size_t blen = dash_objs_y - resolved_var;
            if (blen >= sizeof(tgt_base)) blen = sizeof(tgt_base) - 1;
            memcpy(tgt_base, resolved_var, blen);
            tgt_base[blen] = '\0';
        } else if (dash_objs && strlen(dash_objs) == 5) {
            is_objs = 1;
            size_t blen = dash_objs - resolved_var;
            if (blen >= sizeof(tgt_base)) blen = sizeof(tgt_base) - 1;
            memcpy(tgt_base, resolved_var, blen);
            tgt_base[blen] = '\0';
        } else if (dash_y && strlen(dash_y) == 2 && strcmp(resolved_var, "obj-y") != 0 && strcmp(resolved_var, "subdirs-y") != 0) {
            is_y = 1;
            size_t blen = dash_y - resolved_var;
            if (blen >= sizeof(tgt_base)) blen = sizeof(tgt_base) - 1;
            memcpy(tgt_base, resolved_var, blen);
            tgt_base[blen] = '\0';
        }

        if (!is_objs && !is_y) continue;
        if (strchr(tgt_base, ':') || strchr(tgt_base, ' ') || strchr(tgt_base, '\t')) continue;
        if (strlen(tgt_base) == 0) continue;

        int is_append = (strstr(s, "+=") != NULL);
        int found_idx = -1;
        for (int c = 0; c < g_ncomposites; c++) {
            if (strcmp(g_composites[c].target, tgt_base) == 0 &&
                strcmp(g_composites[c].scope, scope) == 0) {
                found_idx = c;
                break;
            }
        }

        char old_members[MAX_MEMBERS][128];
        int nold = 0;
        if (found_idx == -1 && g_ncomposites < MAX_COMPOSITES) {
            found_idx = g_ncomposites++;
            strncpy(g_composites[found_idx].target, tgt_base, sizeof(g_composites[0].target) - 1);
            g_composites[found_idx].target[sizeof(g_composites[0].target) - 1] = '\0';
            strncpy(g_composites[found_idx].scope, scope, sizeof(g_composites[0].scope) - 1);
            g_composites[found_idx].scope[sizeof(g_composites[0].scope) - 1] = '\0';
            g_composites[found_idx].nmembers = 0;
            g_composites[found_idx].is_module = 0;
        } else if (found_idx != -1) {
            nold = g_composites[found_idx].nmembers;
            memcpy(old_members, g_composites[found_idx].members, sizeof(old_members));
            if (!is_append) {
                g_composites[found_idx].nmembers = 0;
            }
        }

        if (found_idx != -1) {
            CompositeTarget *ct = &g_composites[found_idx];
            char *tok = strtok(rhs, " \t\r\n\\");
            while (tok) {
                char exp_tok[512];
                expand_token_vars(tok, exp_tok, sizeof(exp_tok));

                const char *cpstart = strstr(exp_tok, "$(");
                const char *cpend = NULL;
                if (cpstart) {
                    cpend = strstr(exp_tok, "-objs)");
                    if (!cpend) cpend = strstr(exp_tok, "-y)");
                }
                if (cpstart && cpend) {
                    char cvname[128];
                    size_t cvlen = cpend - (cpstart + 2);
                    if (cvlen < sizeof(cvname)) {
                        memcpy(cvname, cpstart + 2, cvlen);
                        cvname[cvlen] = '\0';
                        if (strcmp(cvname, tgt_base) == 0 && !is_append) {
                            for (int m = 0; m < nold; m++) {
                                if (ct->nmembers < MAX_MEMBERS) {
                                    strncpy(ct->members[ct->nmembers], old_members[m], sizeof(ct->members[0]) - 1);
                                    ct->members[ct->nmembers][sizeof(ct->members[0]) - 1] = '\0';
                                    ct->nmembers++;
                                }
                            }
                        } else {
                            for (int c = 0; c < g_ncomposites; c++) {
                                if (strcmp(g_composites[c].target, cvname) == 0 &&
                                    strcmp(g_composites[c].scope, scope) == 0) {
                                    for (int m = 0; m < g_composites[c].nmembers; m++) {
                                        if (ct->nmembers < MAX_MEMBERS) {
                                            strncpy(ct->members[ct->nmembers], g_composites[c].members[m], sizeof(ct->members[0]) - 1);
                                            ct->members[ct->nmembers][sizeof(ct->members[0]) - 1] = '\0';
                                            ct->nmembers++;
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                    }
                } else if (strstr(exp_tok, ".o") && !strstr(exp_tok, "$")) {
                    if (ct->nmembers < MAX_MEMBERS) {
                        strncpy(ct->members[ct->nmembers], exp_tok, sizeof(ct->members[0]) - 1);
                        ct->members[ct->nmembers][sizeof(ct->members[0]) - 1] = '\0';
                        ct->nmembers++;
                    }
                }
                tok = strtok(NULL, " \t\r\n\\");
            }
        }
    }
    fclose(fp);

    /* Second pass to expand nested composite targets like $(foo-y) in member lists */
    for (int c = initial_composites; c < g_ncomposites; c++) {
        CompositeTarget *ct = &g_composites[c];
        char final_members[MAX_MEMBERS][128];
        int nfinal = 0;
        for (int m = 0; m < ct->nmembers; m++) {
            char *mem = ct->members[m];
            if (strstr(mem, ".o")) {
                if (nfinal < MAX_MEMBERS) {
                    strncpy(final_members[nfinal], mem, sizeof(final_members[0]) - 1);
                    final_members[nfinal][sizeof(final_members[0]) - 1] = '\0';
                    nfinal++;
                }
            }
        }
        memcpy(ct->members, final_members, sizeof(ct->members));
        ct->nmembers = nfinal;
    }

    return g_ncomposites - initial_composites;
}

int scan_get_composite_count(void) {
    return g_ncomposites;
}

PithValue *scan_get_composite_target(int idx) {
    if (idx < 0 || idx >= g_ncomposites) return pithNewString("");
    return pithNewString(g_composites[idx].target);
}

PithValue *scan_get_composite_scope(int idx) {
    if (idx < 0 || idx >= g_ncomposites) return pithNewString("");
    return pithNewString(g_composites[idx].scope);
}

int scan_get_composite_is_module(int idx) {
    if (idx < 0 || idx >= g_ncomposites) return 0;
    return g_composites[idx].is_module;
}

int scan_get_composite_member_count(int idx) {
    if (idx < 0 || idx >= g_ncomposites) return 0;
    return g_composites[idx].nmembers;
}

PithValue *scan_get_composite_member(int idx, int m_idx) {
    if (idx < 0 || idx >= g_ncomposites) return pithNewString("");
    if (m_idx < 0 || m_idx >= g_composites[idx].nmembers) return pithNewString("");
    return pithNewString(g_composites[idx].members[m_idx]);
}

int scan_directs(PithValue *makefile_path_val, PithValue *scope_val) {
    if (!makefile_path_val) return 0;
    const char *mpath = pithStringData(makefile_path_val);
    const char *scope = scope_val ? pithStringData(scope_val) : "";

    FILE *fp = fopen(mpath, "r");
    if (!fp) return 0;

    g_ndirect = 0;
    char line[16384];

    while (read_logical_line(fp, line, sizeof(line))) {
        char *s = trim_inplace(line);
        if (!*s || *s == '#') continue;

        char *eq = strstr(s, ":=");
        if (!eq) eq = strstr(s, "+=");
        if (!eq) eq = strchr(s, '=');
        if (!eq) continue;

        char varname[128];
        size_t vlen = eq - s;
        if (vlen >= sizeof(varname)) vlen = sizeof(varname) - 1;
        memcpy(varname, s, vlen);
        varname[vlen] = '\0';
        char *var = trim_inplace(varname);

        char *rhs = eq + 1;
        if (*rhs == '=') rhs++;

        char resolved_var[256];
        if (strstr(var, "$(")) {
            PithValue *pv = config_eval_expr(pithNewString(var));
            strncpy(resolved_var, pithStringData(pv), sizeof(resolved_var) - 1);
            resolved_var[sizeof(resolved_var) - 1] = '\0';
        } else {
            strncpy(resolved_var, var, sizeof(resolved_var) - 1);
            resolved_var[sizeof(resolved_var) - 1] = '\0';
        }

        if (strcmp(resolved_var, "obj-y") != 0 && strcmp(resolved_var, "obj-m") != 0 &&
            strcmp(resolved_var, "lib-y") != 0) {
            continue;
        }

        char *tok = strtok(rhs, " \t\r\n\\");
        while (tok) {
            char exp_tok[512];
            expand_token_vars(tok, exp_tok, sizeof(exp_tok));

            const char *pstart = strstr(exp_tok, "$(");
            const char *pend = NULL;
            if (pstart) {
                pend = strstr(exp_tok, "-objs)");
                if (!pend) pend = strstr(exp_tok, "-y)");
            }
            if (pstart && pend) {
                char vname[128];
                size_t vnlen = pend - (pstart + 2);
                if (vnlen < sizeof(vname)) {
                    memcpy(vname, pstart + 2, vnlen);
                    vname[vnlen] = '\0';
                    for (int c = 0; c < g_ncomposites; c++) {
                        if (strcmp(g_composites[c].target, vname) == 0 &&
                            strcmp(g_composites[c].scope, scope) == 0) {
                            for (int m = 0; m < g_composites[c].nmembers; m++) {
                                if (g_ndirect < MAX_DIRECT) {
                                    strncpy(g_direct_members[g_ndirect], g_composites[c].members[m], sizeof(g_direct_members[0]) - 1);
                                    g_direct_members[g_ndirect][sizeof(g_direct_members[0]) - 1] = '\0';
                                    g_ndirect++;
                                }
                            }
                            break;
                        }
                    }
                }
            } else if (strstr(exp_tok, ".o") && !strstr(exp_tok, "$")) {
                char tok_base[128];
                strncpy(tok_base, exp_tok, sizeof(tok_base) - 1);
                tok_base[sizeof(tok_base) - 1] = '\0';
                size_t tblen = strlen(tok_base);
                if (tblen >= 2 && tok_base[tblen - 2] == '.') tok_base[tblen - 2] = '\0';

                int is_composite = 0;
                int comp_idx = -1;
                for (int c = 0; c < g_ncomposites; c++) {
                    if (strcmp(g_composites[c].target, tok_base) == 0 &&
                        strcmp(g_composites[c].scope, scope) == 0) {
                        is_composite = 1;
                        comp_idx = c;
                        break;
                    }
                }
                if (is_composite) {
                    if (strcmp(resolved_var, "obj-y") == 0 || strcmp(resolved_var, "lib-y") == 0) {
                        for (int m = 0; m < g_composites[comp_idx].nmembers; m++) {
                            if (g_ndirect < MAX_DIRECT) {
                                strncpy(g_direct_members[g_ndirect], g_composites[comp_idx].members[m], sizeof(g_direct_members[0]) - 1);
                                g_direct_members[g_ndirect][sizeof(g_direct_members[0]) - 1] = '\0';
                                g_ndirect++;
                            }
                        }
                    }
                } else if (g_ndirect < MAX_DIRECT) {
                    strncpy(g_direct_members[g_ndirect], exp_tok, sizeof(g_direct_members[0]) - 1);
                    g_direct_members[g_ndirect][sizeof(g_direct_members[0]) - 1] = '\0';
                    g_ndirect++;
                }
            }
            tok = strtok(NULL, " \t\r\n\\");
        }
    }
    fclose(fp);
    return g_ndirect;
}

int scan_get_direct_count(void) {
    return g_ndirect;
}

PithValue *scan_get_direct_member(int idx) {
    if (idx < 0 || idx >= g_ndirect) return pithNewString("");
    return pithNewString(g_direct_members[idx]);
}
