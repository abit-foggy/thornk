#ifndef THORNK_RESOLVE_H
#define THORNK_RESOLVE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pith.h>

PithValue *resolve_qualify_target(PithValue *scope_val, PithValue *target_val);
PithValue *resolve_source(PithValue *root_dir_val, PithValue *scope_val, PithValue *obj_val);

#endif /* THORNK_RESOLVE_H */
