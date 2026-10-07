#ifndef THORNK_CLEAN_H
#define THORNK_CLEAN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "pith.h"

extern int g_clean_legacy;
extern int g_clean_only;
extern char g_kernel_dir[512];

int clean_scan_legacy(const char *dir, int do_delete, int *count);
int clean_process_legacy(const char *kernel_dir);
int clean_is_only(void);

#endif /* THORNK_CLEAN_H */
