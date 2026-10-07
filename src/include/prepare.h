#ifndef THORNK_PREPARE_H
#define THORNK_PREPARE_H

#include <stdbool.h>

extern int g_is_prepare_cmd;
extern int g_prepare_defconfig;
extern char g_prepare_kdir[512];
extern char g_prepare_arch[64];

int thornk_prepare(const char *kdir, const char *arch, bool defconfig);
int prepare_is_cmd(void);
int prepare_run(void);
int prepare_auto_if_needed(const char *kdir, const char *arch);

#endif /* THORNK_PREPARE_H */
