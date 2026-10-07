#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include "include/prepare.h"

int g_is_prepare_cmd = 0;
int g_prepare_defconfig = 0;
char g_prepare_kdir[512] = ".";
char g_prepare_arch[64] = "x86";

static int mkdir_p(const char *path) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len == 0) return 0;
    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755);
}

static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return -1; }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        fwrite(buf, 1, n, out);
    }
    fclose(in);
    fclose(out);
    return 0;
}

static int prepare_defconfig(const char *kdir, const char *arch) {
    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s/.config", kdir);
    if (access(config_path, F_OK) == 0) {
        return 0;
    }

    char cand[512] = "";
    if (strcmp(arch, "x86") == 0 || strcmp(arch, "x86_64") == 0) {
        char c1[512], c2[512], c3[512];
        snprintf(c1, sizeof(c1), "%s/arch/x86/configs/x86_64_defconfig", kdir);
        snprintf(c2, sizeof(c2), "%s/arch/x86/configs/i386_defconfig", kdir);
        snprintf(c3, sizeof(c3), "%s/arch/x86/configs/x86_defconfig", kdir);
        if (access(c1, F_OK) == 0) strncpy(cand, c1, sizeof(cand) - 1);
        else if (access(c2, F_OK) == 0) strncpy(cand, c2, sizeof(cand) - 1);
        else if (access(c3, F_OK) == 0) strncpy(cand, c3, sizeof(cand) - 1);
    } else {
        char c1[512], c2[512];
        snprintf(c1, sizeof(c1), "%s/arch/%s/configs/%s_defconfig", kdir, arch, arch);
        snprintf(c2, sizeof(c2), "%s/arch/%s/defconfig", kdir, arch);
        if (access(c1, F_OK) == 0) strncpy(cand, c1, sizeof(cand) - 1);
        else if (access(c2, F_OK) == 0) strncpy(cand, c2, sizeof(cand) - 1);
    }

    if (cand[0] && copy_file(cand, config_path) == 0) {
        printf("thornk: generated .config from %s\n", cand);
        return 0;
    }
    return -1;
}

static int prepare_autoconf(const char *kdir) {
    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s/.config", kdir);
    FILE *in = fopen(config_path, "r");
    if (!in) return -1;

    char gen_dir[512];
    snprintf(gen_dir, sizeof(gen_dir), "%s/include/generated", kdir);
    mkdir_p(gen_dir);

    char out_path[512];
    snprintf(out_path, sizeof(out_path), "%s/include/generated/autoconf.h", kdir);
    FILE *out = fopen(out_path, "w");
    if (!out) { fclose(in); return -1; }

    fprintf(out, "/*\n * Automatically generated file; DO NOT EDIT.\n * Linux Kernel Configuration\n */\n");
    fprintf(out, "#ifndef __LINUX_AUTOCONF_H__\n");
    fprintf(out, "#define __LINUX_AUTOCONF_H__\n\n");

    char line[1024];
    while (fgets(line, sizeof(line), in)) {
        char *p = line;
        while (*p && (*p == ' ' || *p == '\t')) p++;
        if (!*p || *p == '#' || *p == '\n') continue;

        char *end = p + strlen(p) - 1;
        while (end >= p && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) {
            *end = '\0';
            end--;
        }

        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = p;
        const char *val = eq + 1;

        if (strcmp(val, "y") == 0) {
            fprintf(out, "#define %s 1\n", key);
        } else if (strcmp(val, "m") == 0) {
            fprintf(out, "#define %s_MODULE 1\n", key);
        } else {
            fprintf(out, "#define %s %s\n", key, val);
        }
    }

    fprintf(out, "#ifndef CONFIG_BUILD_SALT\n#define CONFIG_BUILD_SALT \"\"\n#endif\n");
    fprintf(out, "\n#endif /* __LINUX_AUTOCONF_H__ */\n");
    fclose(in);
    fclose(out);
    printf("thornk: generated include/generated/autoconf.h\n");
    return 0;
}

static int prepare_version_headers(const char *kdir, const char *arch) {
    char makefile_path[512];
    snprintf(makefile_path, sizeof(makefile_path), "%s/Makefile", kdir);
    FILE *fp = fopen(makefile_path, "r");
    int version = 6, patchlevel = 12, sublevel = 0;
    if (fp) {
        char line[256];
        int lines = 0;
        while (fgets(line, sizeof(line), fp) && lines++ < 30) {
            int val;
            if (sscanf(line, "VERSION = %d", &val) == 1) version = val;
            else if (sscanf(line, "PATCHLEVEL = %d", &val) == 1) patchlevel = val;
            else if (sscanf(line, "SUBLEVEL = %d", &val) == 1) sublevel = val;
        }
        fclose(fp);
    }

    unsigned int version_code = (version << 16) + (patchlevel << 8) + sublevel;

    /* 1. version.h */
    char uapi_dir[512];
    snprintf(uapi_dir, sizeof(uapi_dir), "%s/include/generated/uapi/linux", kdir);
    mkdir_p(uapi_dir);

    char vpath[512];
    snprintf(vpath, sizeof(vpath), "%s/version.h", uapi_dir);
    FILE *vf = fopen(vpath, "w");
    if (vf) {
        fprintf(vf, "#define LINUX_VERSION_CODE %u\n", version_code);
        fprintf(vf, "#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))\n");
        fprintf(vf, "#define LINUX_VERSION_MAJOR %d\n", version);
        fprintf(vf, "#define LINUX_VERSION_PATCHLEVEL %d\n", patchlevel);
        fprintf(vf, "#define LINUX_VERSION_SUBLEVEL %d\n", sublevel);
        fclose(vf);
    }

    char vpath2[512];
    snprintf(vpath2, sizeof(vpath2), "%s/include/generated/version.h", kdir);
    FILE *vf2 = fopen(vpath2, "w");
    if (vf2) {
        fprintf(vf2, "#define LINUX_VERSION_CODE %u\n", version_code);
        fprintf(vf2, "#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))\n");
        fprintf(vf2, "#define LINUX_VERSION_MAJOR %d\n", version);
        fprintf(vf2, "#define LINUX_VERSION_PATCHLEVEL %d\n", patchlevel);
        fprintf(vf2, "#define LINUX_VERSION_SUBLEVEL %d\n", sublevel);
        fclose(vf2);
    }

    /* 2. utsversion.h */
    char utspath[512];
    snprintf(utspath, sizeof(utspath), "%s/include/generated/utsversion.h", kdir);
    FILE *uf = fopen(utspath, "w");
    if (uf) {
        fprintf(uf, "#define UTS_VERSION \"#1 SMP PREEMPT_DYNAMIC thornk\"\n");
        fclose(uf);
    }

    /* 3. compile.h */
    char comppath[512];
    snprintf(comppath, sizeof(comppath), "%s/include/generated/compile.h", kdir);
    FILE *cf = fopen(comppath, "w");
    if (cf) {
        const char *mach = (strcmp(arch, "x86") == 0 || strcmp(arch, "x86_64") == 0) ? "x86_64" : arch;
        const char *cc = getenv("CC");
        const char *compiler = (cc && strstr(cc, "gcc")) ? "gcc" : "clang";
        fprintf(cf, "#define UTS_MACHINE \"%s\"\n", mach);
        fprintf(cf, "#define LINUX_COMPILE_BY \"thornk\"\n");
        fprintf(cf, "#define LINUX_COMPILE_HOST \"freestanding\"\n");
        fprintf(cf, "#define LINUX_COMPILER \"%s\"\n", compiler);
        fclose(cf);
    }

    /* 4. utsrelease.h */
    char relpath[512];
    snprintf(relpath, sizeof(relpath), "%s/include/generated/utsrelease.h", kdir);
    FILE *rf = fopen(relpath, "w");
    if (rf) {
        fprintf(rf, "#define UTS_RELEASE \"%d.%d.%d\"\n", version, patchlevel, sublevel);
        fclose(rf);
    }

    printf("thornk: generated version, utsversion, utsrelease, and compile headers\n");
    return 0;
}

static int parse_offsets_s(const char *asm_file, const char *out_header, const char *guard_name) {
    FILE *in = fopen(asm_file, "r");
    if (!in) return -1;
    FILE *out = fopen(out_header, "w");
    if (!out) { fclose(in); return -1; }

    fprintf(out, "/*\n * DO NOT MODIFY.\n *\n * This file was generated by thornk\n */\n");
    fprintf(out, "#ifndef %s\n", guard_name);
    fprintf(out, "#define %s\n\n", guard_name);

    char line[1024];
    while (fgets(line, sizeof(line), in)) {
        char *arrow = strstr(line, "->");
        if (!arrow) continue;
        char *p = arrow + 2;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '\n' || *p == '\r') continue;

        char name[128] = "";
        char val[128] = "";
        char comment[256] = "";

        int ni = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && *p != '"' && ni < 127) {
            name[ni++] = *p++;
        }
        name[ni] = '\0';

        while (*p == ' ' || *p == '\t') p++;

        if (*p == '$' || *p == '#') p++;

        int vi = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && *p != '"' && vi < 127) {
            val[vi++] = *p++;
        }
        val[vi] = '\0';

        while (*p == ' ' || *p == '\t') p++;

        int ci = 0;
        while (*p && *p != '\n' && *p != '\r' && *p != '"' && ci < 255) {
            comment[ci++] = *p++;
        }
        while (ci > 0 && (comment[ci - 1] == ' ' || comment[ci - 1] == '\t')) ci--;
        comment[ci] = '\0';

        if (name[0] && val[0]) {
            if (comment[0]) {
                fprintf(out, "#define %s %s /* %s */\n", name, val, comment);
            } else {
                fprintf(out, "#define %s %s\n", name, val);
            }
        }
    }

    fprintf(out, "\n#endif /* %s */\n", guard_name);
    fclose(in);
    fclose(out);
    return 0;
}

static int prepare_bounds_h(const char *kdir, const char *arch, const char *cc) {
    char bounds_c[512];
    snprintf(bounds_c, sizeof(bounds_c), "%s/kernel/bounds.c", kdir);

    char out_path[512];
    snprintf(out_path, sizeof(out_path), "%s/include/generated/bounds.h", kdir);

    if (access(bounds_c, F_OK) != 0) {
        FILE *f = fopen(out_path, "w");
        if (f) {
            fprintf(f, "#ifndef __LINUX_BOUNDS_H__\n#define __LINUX_BOUNDS_H__\n#endif\n");
            fclose(f);
        }
        return 0;
    }

    char tmp_asm[128] = "/tmp/thornk_bounds.s";
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "%s -S -nostdinc -D__KERNEL__ "
        "-I%s/include -I%s/arch/%s/include -I%s/include/uapi -I%s/arch/%s/include/uapi "
        "-I%s/include/generated "
        "-imacros %s/include/generated/autoconf.h "
        "-fno-asynchronous-unwind-tables -fno-stack-protector -O2 "
        "%s -o %s 2>/dev/null",
        cc, kdir, kdir, arch, kdir, kdir, arch, kdir, kdir, bounds_c, tmp_asm);

    int ret = system(cmd);
    if (ret == 0 && access(tmp_asm, F_OK) == 0) {
        parse_offsets_s(tmp_asm, out_path, "__LINUX_BOUNDS_H__");
        unlink(tmp_asm);
        printf("thornk: generated include/generated/bounds.h\n");
    } else {
        FILE *f = fopen(out_path, "w");
        if (f) {
            fprintf(f, "#ifndef __LINUX_BOUNDS_H__\n#define __LINUX_BOUNDS_H__\n#endif\n");
            fclose(f);
        }
    }
    return 0;
}

static int prepare_asm_offsets_h(const char *kdir, const char *arch, const char *cc) {
    char asm_c[512];
    snprintf(asm_c, sizeof(asm_c), "%s/arch/%s/kernel/asm-offsets.c", kdir, arch);

    char gen_asm_dir[512];
    snprintf(gen_asm_dir, sizeof(gen_asm_dir), "%s/arch/%s/include/generated/asm", kdir, arch);
    mkdir_p(gen_asm_dir);

    char out_path[512];
    snprintf(out_path, sizeof(out_path), "%s/arch/%s/include/generated/asm/asm-offsets.h", kdir, arch);

    if (access(asm_c, F_OK) != 0) {
        FILE *f = fopen(out_path, "w");
        if (f) {
            fprintf(f, "#ifndef __ASM_OFFSETS_H__\n#define __ASM_OFFSETS_H__\n#endif\n");
            fclose(f);
        }
        return 0;
    }

    char tmp_asm[128] = "/tmp/thornk_asm_offsets.s";
    char cmd[2048];
    const char *extra_cflags = (strcmp(arch, "x86") == 0 || strcmp(arch, "x86_64") == 0) ?
        "-mcmodel=kernel -mno-sse -mno-mmx " : "";

    snprintf(cmd, sizeof(cmd),
        "%s -S -nostdinc -D__KERNEL__ %s"
        "-I%s/include -I%s/arch/%s/include -I%s/include/uapi -I%s/arch/%s/include/uapi "
        "-I%s/include/generated "
        "-imacros %s/include/generated/autoconf.h "
        "-imacros %s/include/generated/bounds.h "
        "-fno-asynchronous-unwind-tables -fno-stack-protector -O2 "
        "%s -o %s 2>/dev/null",
        cc, extra_cflags, kdir, kdir, arch, kdir, kdir, arch, kdir, kdir, kdir, asm_c, tmp_asm);

    int ret = system(cmd);
    if (ret != 0 && strlen(extra_cflags) > 0) {
        snprintf(cmd, sizeof(cmd),
            "%s -S -nostdinc -D__KERNEL__ "
            "-I%s/include -I%s/arch/%s/include -I%s/include/uapi -I%s/arch/%s/include/uapi "
            "-I%s/include/generated "
            "-imacros %s/include/generated/autoconf.h "
            "-imacros %s/include/generated/bounds.h "
            "-fno-asynchronous-unwind-tables -fno-stack-protector -O2 "
            "%s -o %s 2>/dev/null",
            cc, kdir, kdir, arch, kdir, kdir, arch, kdir, kdir, kdir, asm_c, tmp_asm);
        ret = system(cmd);
    }

    if (ret == 0 && access(tmp_asm, F_OK) == 0) {
        parse_offsets_s(tmp_asm, out_path, "__ASM_OFFSETS_H__");
        unlink(tmp_asm);
        printf("thornk: generated arch/%s/include/generated/asm/asm-offsets.h\n", arch);
    } else {
        FILE *f = fopen(out_path, "w");
        if (f) {
            fprintf(f, "#ifndef __ASM_OFFSETS_H__\n#define __ASM_OFFSETS_H__\n#endif\n");
            fclose(f);
        }
    }
    return 0;
}

static void gen_asm_wrappers(const char *src_dir, const char *arch_dir, const char *gen_dir, const char *prefix) {
    DIR *d = opendir(src_dir);
    if (!d) return;
    mkdir_p(gen_dir);
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (!strstr(de->d_name, ".h")) continue;
        char arch_file[512], gen_file[512];
        snprintf(arch_file, sizeof(arch_file), "%s/%s", arch_dir, de->d_name);
        snprintf(gen_file, sizeof(gen_file), "%s/%s", gen_dir, de->d_name);
        if (access(arch_file, F_OK) != 0 && access(gen_file, F_OK) != 0) {
            FILE *f = fopen(gen_file, "w");
            if (f) {
                fprintf(f, "#include <%s/%s>\n", prefix, de->d_name);
                fclose(f);
            }
        }
    }
    closedir(d);
}

static int prepare_asm_wrappers(const char *kdir, const char *arch) {
    char src[512], adir[512], gdir[512];
    snprintf(src, sizeof(src), "%s/include/asm-generic", kdir);
    snprintf(adir, sizeof(adir), "%s/arch/%s/include/asm", kdir, arch);
    snprintf(gdir, sizeof(gdir), "%s/arch/%s/include/generated/asm", kdir, arch);
    gen_asm_wrappers(src, adir, gdir, "asm-generic");

    snprintf(src, sizeof(src), "%s/include/uapi/asm-generic", kdir);
    snprintf(adir, sizeof(adir), "%s/arch/%s/include/uapi/asm", kdir, arch);
    snprintf(gdir, sizeof(gdir), "%s/arch/%s/include/generated/uapi/asm", kdir, arch);
    gen_asm_wrappers(src, adir, gdir, "asm-generic");
    return 0;
}

int thornk_prepare(const char *kdir, const char *arch, bool defconfig) {
    if (!kdir || !*kdir) kdir = ".";
    if (!arch || !*arch) arch = "x86";

    printf("thornk: preparing kernel headers for %s (arch: %s)\n", kdir, arch);

    const char *cc = getenv("CC");
    if (!cc || !*cc) cc = "cc";

    if (defconfig) {
        prepare_defconfig(kdir, arch);
    }

    prepare_autoconf(kdir);
    prepare_version_headers(kdir, arch);
    prepare_bounds_h(kdir, arch, cc);
    prepare_asm_offsets_h(kdir, arch, cc);
    prepare_asm_wrappers(kdir, arch);

    printf("thornk: successfully prepared kernel headers\n");
    return 0;
}

int prepare_is_cmd(void) {
    return g_is_prepare_cmd;
}

int prepare_run(void) {
    return thornk_prepare(g_prepare_kdir, g_prepare_arch, g_prepare_defconfig != 0);
}

int prepare_auto_if_needed(const char *kdir, const char *arch) {
    if (!kdir || !*kdir) kdir = ".";
    if (!arch || !*arch) arch = "x86";

    char autoconf_h[512], asm_offsets_h[512];
    snprintf(autoconf_h, sizeof(autoconf_h), "%s/include/generated/autoconf.h", kdir);
    snprintf(asm_offsets_h, sizeof(asm_offsets_h), "%s/arch/%s/include/generated/asm/asm-offsets.h", kdir, arch);

    if (access(autoconf_h, F_OK) != 0 || access(asm_offsets_h, F_OK) != 0) {
        char config_path[512];
        snprintf(config_path, sizeof(config_path), "%s/.config", kdir);
        bool need_defconfig = (access(config_path, F_OK) != 0);
        return thornk_prepare(kdir, arch, need_defconfig);
    }
    return 0;
}
