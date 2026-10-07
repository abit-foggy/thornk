#include "include/post.h"

int post_is_spec_output(void) {
    size_t l = strlen(g_out_file);
    if (l >= 6 && strcmp(g_out_file + l - 6, ".build") == 0) return 1;
    if (l >= 6 && strcmp(g_out_file + l - 6, ".thorn") == 0) return 1;
    return 0;
}

char g_spec_file[512] = "";

PithValue *post_get_spec_file(void) {
    if (g_spec_file[0] != '\0') {
        return pithNewString(g_spec_file);
    }
    if (post_is_spec_output()) {
        return pithNewString(g_out_file);
    }
    char spec_path[512];
    char *slash = strrchr(g_out_file, '/');
    if (slash) {
        size_t dlen = (size_t)(slash - g_out_file);
        snprintf(spec_path, sizeof(spec_path), "%.*s/thorn.build", (int)dlen, g_out_file);
    } else {
        snprintf(spec_path, sizeof(spec_path), "thorn.build");
    }
    return pithNewString(spec_path);
}

PithValue *post_get_out_dir(void) {
    char out_dir[512] = ".";
    char *slash = strrchr(g_out_file, '/');
    if (slash) {
        size_t dlen = (size_t)(slash - g_out_file);
        if (dlen == 0) {
            snprintf(out_dir, sizeof(out_dir), "/");
        } else {
            snprintf(out_dir, sizeof(out_dir), "%.*s", (int)dlen, g_out_file);
        }
    }
    return pithNewString(out_dir);
}

int post_run_thorn(PithValue *spec_path_val, PithValue *out_dir_val) {
    if (!spec_path_val) return 0;
    const char *spec_path = pithStringData(spec_path_val);
    const char *out_dir = out_dir_val ? pithStringData(out_dir_val) : ".";

    char thorn_bin[512] = "";
    const char *env_thorn = getenv("THORN_BIN");
    if (env_thorn && access(env_thorn, X_OK) == 0) {
        strncpy(thorn_bin, env_thorn, sizeof(thorn_bin) - 1);
    } else if (access("/home/foggy/.local/bin/thorn", X_OK) == 0) {
        strncpy(thorn_bin, "/home/foggy/.local/bin/thorn", sizeof(thorn_bin) - 1);
    } else if (access("/home/foggy/thorn/out/thorn", X_OK) == 0) {
        strncpy(thorn_bin, "/home/foggy/thorn/out/thorn", sizeof(thorn_bin) - 1);
    } else {
        strncpy(thorn_bin, "thorn", sizeof(thorn_bin) - 1);
    }

    if (!getenv("PITH_TCCDIR") && access("/home/foggy/thorn/vendor/pith/vendor/tcc/libtcc1.a", F_OK) == 0) {
        setenv("PITH_TCCDIR", "/home/foggy/thorn/vendor/pith/vendor/tcc", 1);
    }

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%s -f \"%s\" --engine ninja --out-dir \"%s\"",
             thorn_bin, spec_path, out_dir);
    int rc = system(cmd);
    if (rc != 0) return 0;

    char default_ninja[512];
    if (strcmp(out_dir, ".") == 0 || strcmp(out_dir, "") == 0) {
        snprintf(default_ninja, sizeof(default_ninja), "build.ninja");
    } else {
        snprintf(default_ninja, sizeof(default_ninja), "%s/build.ninja", out_dir);
    }

    if (strcmp(g_out_file, default_ninja) != 0 && strcmp(g_out_file, "") != 0) {
        rename(default_ninja, g_out_file);
    }

    return 1;
}

int post_vmlinux(PithValue *ninja_path_val, PithValue *kernel_dir_val, PithValue *arch_val) {
    if (!ninja_path_val) return 0;
    const char *ninja_path = pithStringData(ninja_path_val);
    const char *kdir = kernel_dir_val ? pithStringData(kernel_dir_val) : ".";
    const char *arch = arch_val ? pithStringData(arch_val) : "x86";

    FILE *fp = fopen(ninja_path, "r");
    if (!fp) return 0;

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = malloc(sz + 1);
    if (!buf) {
        fclose(fp);
        return 0;
    }
    size_t r = fread(buf, 1, sz, fp);
    buf[r] = '\0';
    fclose(fp);

    /* Collect all built-in.a.a archive targets */
    /* Target format: build <prefix>.../built-in.a.a: ar ... */
    char *builtins[4096];
    int nbuiltins = 0;

    char *line = buf;
    while (*line) {
        char *next = strchr(line, '\n');
        if (next) *next = '\0';

        if (strncmp(line, "build ", 6) == 0 && strstr(line, "built-in.a.a: ar ")) {
            char *colon = strchr(line, ':');
            if (colon && nbuiltins < 4096) {
                *colon = '\0';
                char *target = line + 6;
                while (*target == ' ') target++;
                builtins[nbuiltins++] = strdup(target);
            }
        }

        if (!next) break;
        line = next + 1;
    }

    /* Only postprocess if there are built-in.a.a targets and vmlinux.lds.S exists */
    char lds_src[1024];
    snprintf(lds_src, sizeof(lds_src), "%s/arch/%s/kernel/vmlinux.lds.S", kdir, arch);
    if (nbuiltins == 0 || access(lds_src, F_OK) != 0) {
        for (int i = 0; i < nbuiltins; i++) free(builtins[i]);
        free(buf);
        return 0;
    }

    /* Re-read original file and strip original 'default ...' line if any */
    fp = fopen(ninja_path, "r");
    if (!fp) {
        for (int i = 0; i < nbuiltins; i++) free(builtins[i]);
        free(buf);
        return 0;
    }

    char *out_buf = malloc(sz + 1024 * 1024);
    if (!out_buf) {
        fclose(fp);
        for (int i = 0; i < nbuiltins; i++) free(builtins[i]);
        free(buf);
        return 0;
    }
    out_buf[0] = '\0';
    size_t out_len = 0;

    char line_buf[8192];
    char current_src_dir[1024] = "";
    char current_src_file[1024] = "";
    int in_as_cpp_rule = 0;

    while (fgets(line_buf, sizeof(line_buf), fp)) {
        if (strncmp(line_buf, "default ", 8) == 0) {
            continue; /* Strip default line */
        }
        if (strncmp(line_buf, "rule as_cpp", 11) == 0) {
            in_as_cpp_rule = 1;
        } else if (in_as_cpp_rule && strncmp(line_buf, "  command = ", 12) == 0) {
            snprintf(line_buf, sizeof(line_buf),
                     "  command = $cc -MD -MF $out.d -D__ASSEMBLY__ -fno-PIE $asflags $cflags -c $in -o $out\n");
            in_as_cpp_rule = 0;
        } else if (in_as_cpp_rule && line_buf[0] != ' ') {
            in_as_cpp_rule = 0;
        }
        if (strncmp(line_buf, "build ", 6) == 0) {
            current_src_dir[0] = '\0';
            current_src_file[0] = '\0';
            char *colon = strchr(line_buf, ':');
            if (colon && (strstr(colon, " cc ") || strstr(colon, " as_cpp "))) {
                char *src = strstr(colon, " /");
                if (src) {
                    src++;
                    char *nl = strchr(src, '\n');
                    size_t flen = nl ? (size_t)(nl - src) : strlen(src);
                    if (flen < sizeof(current_src_file)) {
                        memcpy(current_src_file, src, flen);
                        current_src_file[flen] = '\0';
                    }
                    char *slash = strrchr(src, '/');
                    if (slash && (!nl || slash < nl)) {
                        size_t dlen = (size_t)(slash - src);
                        if (dlen < sizeof(current_src_dir)) {
                            memcpy(current_src_dir, src, dlen);
                            current_src_dir[dlen] = '\0';
                        }
                    }
                }
            }
        } else if (strncmp(line_buf, "  cflags = ", 11) == 0 && current_src_dir[0]) {
            char *nl = strchr(line_buf, '\n');
            if (nl) *nl = '\0';
            char parent_dir[1024];
            strncpy(parent_dir, current_src_dir, sizeof(parent_dir) - 1);
            parent_dir[sizeof(parent_dir) - 1] = '\0';
            char *pslash = strrchr(parent_dir, '/');
            if (pslash) *pslash = '\0';
            char extra[2200];
            if (pslash && strlen(parent_dir) > strlen(kdir)) {
                snprintf(extra, sizeof(extra), " -I%s -I%s", current_src_dir, parent_dir);
            } else {
                snprintf(extra, sizeof(extra), " -I%s", current_src_dir);
            }
            strncat(line_buf, extra, sizeof(line_buf) - strlen(line_buf) - 1);
            if (strstr(current_src_file, "jitterentropy.c")) {
                strncat(line_buf, " -O0", sizeof(line_buf) - strlen(line_buf) - 1);
            }
            strncat(line_buf, "\n", sizeof(line_buf) - strlen(line_buf) - 1);
            current_src_dir[0] = '\0';
            current_src_file[0] = '\0';
        }

        size_t llen = strlen(line_buf);
        memcpy(out_buf + out_len, line_buf, llen);
        out_len += llen;
        out_buf[out_len] = '\0';
    }
    fclose(fp);

    /* Format cpp_lds rule, arch/x86/kernel/vmlinux.lds edge, link_vmlinux rule, and vmlinux target */
    char append_buf[1024 * 1024];
    size_t alen = 0;

    alen += snprintf(append_buf + alen, sizeof(append_buf) - alen,
        "\n# --- vmlinux linking & linker script preprocessing ---\n"
        "rule cpp_lds\n"
        "  command = $cc -E -P -C -Ux86 -D__ASSEMBLY__ -DLINKER_SCRIPT "
        "-I %s/include -I %s/include/uapi -I %s/arch/%s/include -I %s/arch/%s/include/uapi "
        "-I %s/arch/%s/include/generated -I %s/arch/%s/include/generated/uapi -I %s/include/generated/uapi "
        "-I include -I include/uapi -I arch/%s/include -I arch/%s/include/uapi "
        "-I arch/%s/include/generated -I arch/%s/include/generated/uapi -I include/generated/uapi "
        "-include %s/include/linux/kconfig.h "
        "-imacros %s/include/generated/autoconf.h $in -o $out\n"
        "  description = LDS $out\n\n",
        kdir, kdir, kdir, arch, kdir, arch,
        kdir, arch, kdir, arch, kdir,
        arch, arch,
        arch, arch,
        kdir,
        kdir);

    alen += snprintf(append_buf + alen, sizeof(append_buf) - alen,
        "build arch/%s/kernel/vmlinux.lds: cpp_lds %s/arch/%s/kernel/vmlinux.lds.S\n\n",
        arch, kdir, arch);

    alen += snprintf(append_buf + alen, sizeof(append_buf) - alen,
        "rule gen_kallsyms\n"
        "  command = true > $out.syms && %s/scripts/kallsyms --all-symbols --absolute-percpu $out.syms > $out.S && "
        "$cc -nostdinc -I%s/arch/%s/include -I%s/arch/%s/include/generated -I%s/include "
        "-I%s/arch/%s/include/uapi -I%s/arch/%s/include/generated/uapi -I%s/include/uapi -I%s/include/generated/uapi "
        "-include %s/include/linux/kconfig.h -D__KERNEL__ -DCONFIG_64BIT=1 -mcmodel=kernel -fno-PIE "
        "-D__ASSEMBLY__ -c $out.S -o $out\n"
        "  description = KSYMS $out\n\n",
        kdir, kdir, arch, kdir, arch, kdir,
        kdir, arch, kdir, arch, kdir, kdir,
        kdir);

    alen += snprintf(append_buf + alen, sizeof(append_buf) - alen,
        "build .tmp_kallsyms.o: gen_kallsyms\n\n");

    alen += snprintf(append_buf + alen, sizeof(append_buf) - alen,
        "rule link_vmlinux\n"
        "  command = ld -m elf_x86_64 -z max-page-size=0x200000 --whole-archive $in --no-whole-archive $kallsyms -T $lds -o $out\n"
        "  description = LINK $out\n\n");

    alen += snprintf(append_buf + alen, sizeof(append_buf) - alen,
        "build vmlinux: link_vmlinux");

    for (int i = 0; i < nbuiltins; i++) {
        alen += snprintf(append_buf + alen, sizeof(append_buf) - alen, " %s", builtins[i]);
    }

    alen += snprintf(append_buf + alen, sizeof(append_buf) - alen,
        " || arch/%s/kernel/vmlinux.lds .tmp_kallsyms.o\n"
        "  lds = arch/%s/kernel/vmlinux.lds\n"
        "  kallsyms = .tmp_kallsyms.o\n\n"
        "default vmlinux\n",
        arch, arch);

    /* Write back updated ninja file */
    fp = fopen(ninja_path, "w");
    if (fp) {
        fputs(out_buf, fp);
        fputs(append_buf, fp);
        fclose(fp);
    }

    for (int i = 0; i < nbuiltins; i++) free(builtins[i]);
    free(buf);
    free(out_buf);
    return 1;
}
