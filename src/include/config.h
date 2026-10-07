#ifndef THORNK_CONFIG_H
#define THORNK_CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pith.h>

#define MAX_CONFIG_ENTRIES 16384

extern char g_cfg_keys[MAX_CONFIG_ENTRIES][128];
extern char g_cfg_vals[MAX_CONFIG_ENTRIES][256];
extern int g_cfg_count;
extern char g_arch[64];

int config_parse_file(PithValue *path_val);
PithValue *config_get(PithValue *key_val);
int config_is_set(PithValue *key_val);
PithValue *config_eval_expr(PithValue *expr_val);
int config_is_expr_enabled(PithValue *expr_val);
void expand_token_vars(const char *in, char *out, size_t out_sz);

#endif /* THORNK_CONFIG_H */
