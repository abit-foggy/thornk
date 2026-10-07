#include "include/config.h"

char g_cfg_keys[MAX_CONFIG_ENTRIES][128];
char g_cfg_vals[MAX_CONFIG_ENTRIES][256];
int g_cfg_count = 0;
char g_arch[64] = "x86";

static char *config_trim(char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    size_t l = strlen(s);
    while (l && (s[l - 1] == ' ' || s[l - 1] == '\t' || s[l - 1] == '\r' || s[l - 1] == '\n'))
        s[--l] = '\0';
    return s;
}

int config_parse_file(PithValue *path_val) {
    if (!path_val) return 0;
    const char *path = pithStringData(path_val);
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;

    char line[2048];
    while (fgets(line, sizeof(line), fp)) {
        char *s = config_trim(line);
        if (!*s || *s == '#') continue;

        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = config_trim(s);
        char *val = config_trim(eq + 1);

        size_t vlen = strlen(val);
        if (vlen >= 2 && val[0] == '"' && val[vlen - 1] == '"') {
            val[vlen - 1] = '\0';
            val++;
        }

        if (g_cfg_count < MAX_CONFIG_ENTRIES) {
            strncpy(g_cfg_keys[g_cfg_count], key, sizeof(g_cfg_keys[0]) - 1);
            g_cfg_keys[g_cfg_count][sizeof(g_cfg_keys[0]) - 1] = '\0';
            strncpy(g_cfg_vals[g_cfg_count], val, sizeof(g_cfg_vals[0]) - 1);
            g_cfg_vals[g_cfg_count][sizeof(g_cfg_vals[0]) - 1] = '\0';
            g_cfg_count++;
        }
    }
    fclose(fp);
    return g_cfg_count;
}

PithValue *config_get(PithValue *key_val) {
    if (!key_val) return pithNewString("");
    const char *key = pithStringData(key_val);
    for (int i = 0; i < g_cfg_count; i++) {
        if (strcmp(g_cfg_keys[i], key) == 0) {
            return pithNewString(g_cfg_vals[i]);
        }
    }
    return pithNewString("");
}

int config_is_set(PithValue *key_val) {
    if (!key_val) return 0;
    const char *key = pithStringData(key_val);
    for (int i = 0; i < g_cfg_count; i++) {
        if (strcmp(g_cfg_keys[i], key) == 0) {
            return (strcmp(g_cfg_vals[i], "y") == 0 || strcmp(g_cfg_vals[i], "m") == 0);
        }
    }
    return 0;
}

static const char *lookup_var_val(const char *var) {
    for (int i = 0; i < g_cfg_count; i++) {
        if (strcmp(g_cfg_keys[i], var) == 0) {
            return g_cfg_vals[i];
        }
    }
    if (strcmp(var, "SRCARCH") == 0 || strcmp(var, "ARCH") == 0) {
        return g_arch;
    }
    if (strcmp(var, "BITS") == 0) {
        if (strcmp(g_arch, "x86_64") == 0) return "64";
        for (int i = 0; i < g_cfg_count; i++) {
            if (strcmp(g_cfg_keys[i], "CONFIG_64BIT") == 0 && strcmp(g_cfg_vals[i], "y") == 0) {
                return "64";
            }
        }
        return "32";
    }
    return "";
}

static void eval_subst(const char *args, const char *arg_end, char *out, size_t out_sz) {
    const char *c1 = strchr(args, ',');
    if (!c1 || c1 >= arg_end) return;
    const char *c2 = strchr(c1 + 1, ',');
    if (!c2 || c2 >= arg_end) return;

    char from_buf[64], to_buf[64], text_buf[256];
    size_t fsz = c1 - args;
    if (fsz >= sizeof(from_buf)) fsz = sizeof(from_buf) - 1;
    memcpy(from_buf, args, fsz); from_buf[fsz] = '\0';

    size_t tsz = c2 - (c1 + 1);
    if (tsz >= sizeof(to_buf)) tsz = sizeof(to_buf) - 1;
    memcpy(to_buf, c1 + 1, tsz); to_buf[tsz] = '\0';

    size_t xsz = arg_end - (c2 + 1);
    if (xsz >= sizeof(text_buf)) xsz = sizeof(text_buf) - 1;
    memcpy(text_buf, c2 + 1, xsz); text_buf[xsz] = '\0';

    PithValue *ev_from = config_eval_expr(pithNewString(from_buf));
    PithValue *ev_to = config_eval_expr(pithNewString(to_buf));
    PithValue *ev_text = config_eval_expr(pithNewString(text_buf));

    const char *from_str = pithStringData(ev_from);
    const char *to_str = pithStringData(ev_to);
    const char *text_str = pithStringData(ev_text);

    if (!*from_str) {
        strncat(out, text_str, out_sz - strlen(out) - 1);
        return;
    }
    const char *tp = text_str;
    size_t flen = strlen(from_str);
    while (*tp) {
        const char *match = strstr(tp, from_str);
        if (!match) {
            strncat(out, tp, out_sz - strlen(out) - 1);
            break;
        }
        strncat(out, tp, match - tp);
        strncat(out, to_str, out_sz - strlen(out) - 1);
        tp = match + flen;
    }
}

PithValue *config_eval_expr(PithValue *expr_val) {
    if (!expr_val) return pithNewString("");
    const char *expr = pithStringData(expr_val);
    char buf[1024];
    buf[0] = '\0';

    const char *p = expr;
    while (*p) {
        const char *var_start = strstr(p, "$(");
        if (!var_start) {
            strncat(buf, p, sizeof(buf) - strlen(buf) - 1);
            break;
        }
        strncat(buf, p, var_start - p);
        int paren_depth = 0;
        const char *var_end = var_start;
        while (*var_end) {
            if (*var_end == '(') paren_depth++;
            else if (*var_end == ')') {
                paren_depth--;
                if (paren_depth == 0) break;
            }
            var_end++;
        }
        if (!*var_end) {
            strncat(buf, var_start, sizeof(buf) - strlen(buf) - 1);
            break;
        }

        if (strncmp(var_start + 2, "subst ", 6) == 0) {
            eval_subst(var_start + 8, var_end, buf, sizeof(buf));
        } else {
            char varname[128];
            size_t vlen = var_end - (var_start + 2);
            if (vlen >= sizeof(varname)) vlen = sizeof(varname) - 1;
            memcpy(varname, var_start + 2, vlen);
            varname[vlen] = '\0';

            const char *vval = lookup_var_val(varname);
            strncat(buf, vval, sizeof(buf) - strlen(buf) - 1);
        }
        p = var_end + 1;
    }
    return pithNewString(buf);
}

int config_is_expr_enabled(PithValue *expr_val) {
    if (!expr_val) return 0;
    const char *expr = pithStringData(expr_val);
    const char *var_start = strstr(expr, "$(");
    if (!var_start) return 1;

    int paren_depth = 0;
    const char *var_end = var_start;
    while (*var_end) {
        if (*var_end == '(') paren_depth++;
        else if (*var_end == ')') {
            paren_depth--;
            if (paren_depth == 0) break;
        }
        var_end++;
    }
    if (!*var_end) return 0;

    PithValue *res = config_eval_expr(expr_val);
    const char *res_str = pithStringData(res);
    return (strcmp(res_str, "y") == 0 || strcmp(res_str, "m") == 0);
}

void expand_token_vars(const char *in, char *out, size_t out_sz) {
    out[0] = '\0';
    const char *p = in;
    while (*p) {
        const char *vstart = strstr(p, "$(");
        if (!vstart) {
            strncat(out, p, out_sz - strlen(out) - 1);
            break;
        }
        strncat(out, p, vstart - p);
        int paren_depth = 0;
        const char *vend = vstart;
        while (*vend) {
            if (*vend == '(') paren_depth++;
            else if (*vend == ')') {
                paren_depth--;
                if (paren_depth == 0) break;
            }
            vend++;
        }
        if (!*vend) {
            strncat(out, vstart, out_sz - strlen(out) - 1);
            break;
        }

        if (strncmp(vstart + 2, "subst ", 6) == 0) {
            eval_subst(vstart + 8, vend, out, out_sz);
        } else {
            char varname[128];
            size_t vlen = vend - (vstart + 2);
            if (vlen >= sizeof(varname)) vlen = sizeof(varname) - 1;
            memcpy(varname, vstart + 2, vlen);
            varname[vlen] = '\0';

            const char *val = lookup_var_val(varname);
            if (*val != '\0' || strncmp(varname, "CONFIG_", 7) == 0) {
                strncat(out, val, out_sz - strlen(out) - 1);
            } else {
                strncat(out, "$(", out_sz - strlen(out) - 1);
                strncat(out, varname, out_sz - strlen(out) - 1);
                strncat(out, ")", out_sz - strlen(out) - 1);
            }
        }
        p = vend + 1;
    }
}
