#ifndef THORNK_POST_H
#define THORNK_POST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pith.h>

extern char g_out_file[512];
extern char g_spec_file[512];

int post_is_spec_output(void);
PithValue *post_get_spec_file(void);
PithValue *post_get_out_dir(void);
int post_run_thorn(PithValue *spec_path_val, PithValue *out_dir_val);
int post_vmlinux(PithValue *ninja_path_val, PithValue *kernel_dir_val, PithValue *arch_val);

#endif /* THORNK_POST_H */
