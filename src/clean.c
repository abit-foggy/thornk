#include "include/clean.h"

int g_clean_legacy = 0;
int g_clean_only = 0;

int clean_scan_legacy(const char *dir, int do_delete, int *count) {
    if (!dir || !*dir) return 0;
    DIR *d = opendir(dir);
    if (!d) return 0;

    struct dirent *ent;
    char path[4096];
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        if (strcmp(ent->d_name, ".git") == 0) continue;

        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
        struct stat st;
        if (lstat(path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            clean_scan_legacy(path, do_delete, count);
        } else if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
            if (strncmp(ent->d_name, "Makefile", 8) == 0 ||
                strncmp(ent->d_name, "Kbuild", 6) == 0) {
                if (count) (*count)++;
                if (do_delete) {
                    unlink(path);
                }
            }
        }
    }
    closedir(d);
    return 0;
}

int clean_process_legacy(const char *kernel_dir) {
    if (!kernel_dir || !*kernel_dir) return 0;

    /* If user explicitly requested keeping legacy files, do nothing */
    if (g_clean_legacy < 0) {
        return 0;
    }

    /* If user passed --clean-legacy, --remove-legacy, or --clean-only */
    if (g_clean_legacy > 0 || g_clean_only > 0) {
        int count = 0;
        clean_scan_legacy(kernel_dir, 1, &count);
        printf("thornk: removed %d legacy Kbuild and Makefile files from %s\n", count, kernel_dir);
        return count;
    }

    /* Otherwise, check if any legacy files exist */
    int count = 0;
    clean_scan_legacy(kernel_dir, 0, &count);
    if (count == 0) {
        return 0;
    }

    /* Prompt user only if running in an interactive terminal */
    if (isatty(fileno(stdin)) && isatty(fileno(stdout))) {
        printf("\nFound %d legacy Kbuild and Makefile files in '%s'.\n", count, kernel_dir);
        printf("Would you like to remove them? [y/N]: ");
        fflush(stdout);

        char buf[128];
        if (fgets(buf, sizeof(buf), stdin)) {
            if (buf[0] == 'y' || buf[0] == 'Y') {
                int deleted = 0;
                clean_scan_legacy(kernel_dir, 1, &deleted);
                printf("thornk: removed %d legacy Kbuild and Makefile files\n", deleted);
                return deleted;
            }
        }
        printf("thornk: kept legacy Kbuild and Makefiles\n");
    }

    return 0;
}

int clean_is_only(void) {
    return g_clean_only;
}
