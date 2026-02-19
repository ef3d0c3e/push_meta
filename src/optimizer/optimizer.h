#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include <state/state.h>
#include <stddef.h>

typedef struct
{
	size_t search_width;
	size_t search_depth;
} optimizer_conf_t;

state_t
optimize(const state_t* state, optimizer_conf_t cfg);

void
optimizer_test(void);

#endif // OPTIMIZER_H
