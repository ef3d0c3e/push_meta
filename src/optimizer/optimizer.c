#include <optimizer/optimizer.h>
#include <string.h>
#include <strings.h>

static enum stack_op ops[12] = {
	STACK_OP_NOP, STACK_OP_SA, STACK_OP_SB, STACK_OP_SS,  STACK_OP_PA,  STACK_OP_PB,
	STACK_OP_RA,  STACK_OP_RB, STACK_OP_RR, STACK_OP_RRA, STACK_OP_RRB, STACK_OP_RRR,
};

enum
{
	OPS_LEN = sizeof(ops) / sizeof(ops[0])
};

/** Find the last offset in `orig_state` that is equal to `state` */
static inline size_t
find_future(const state_t* orig_state, const state_t* state, const optimizer_conf_t* cfg)
{
	assert(state->saves_size != 0);

	size_t best = 0;
	for (size_t i = 0; i < cfg->search_width; ++i) {
		if (i + state->saves_size - 1 >= orig_state->saves_size)
			break;
		if (state->sa.size != orig_state->saves[i + state->saves_size - 1].sz_a ||
		    state->sb.size != orig_state->saves[i + state->saves_size - 1].sz_b)
			continue;
		const int* orig_a = orig_state->saves[i + state->saves_size - 1].data;
		const int* orig_b = orig_a + orig_state->saves[i + state->saves_size - 1].sz_a;
		if (!memcmp(state->sa.data, orig_a, sizeof(int) * state->sa.size) &&
		    !memcmp(state->sb.data, orig_b, sizeof(int) * state->sb.size))
		{
			best = i;
		}
	}
	return best;
}

/** Store the result of an optimization pass on an instruction */
typedef struct
{
	/** Current cost */
	size_t cur_cost;

	/** Numbwer of skipped ops -*/
	size_t skip;
	/** Value of the skip */
	size_t value;
	/** Length of the skip */
	size_t len;
	/** Ops for the skip */
	enum stack_op ops[];
} skip_data_t;

static skip_data_t*
skip_data_new(const state_t* state, const optimizer_conf_t* cfg)
{
	skip_data_t* data = xmalloc(
	  (sizeof(skip_data_t) + cfg->search_depth * sizeof(enum stack_op)) * state->saves_size);
	bzero(data,
	      (sizeof(skip_data_t) + cfg->search_depth * sizeof(enum stack_op)) * state->saves_size);
	return data;
}

static inline void
backtrack(const state_t* orig_state, /* Original state */
          state_t* state, /* Bifurcated state */
          size_t start, /* Index in original state */
          const optimizer_conf_t* cfg, /* Optimizer configuration */
          size_t depth, /* Current search depth */
          skip_data_t* skip_data /* Result */)
{
	// Try all instructions
	for (size_t i = 0; i < OPS_LEN; ++i) {
		// Skip impossible instructions
		switch (ops[i]) {
			case STACK_OP_SA:
				if (state->sa.size < 2)
					continue;
				break;
			case STACK_OP_SB:
				if (state->sb.size < 2)
					continue;
				break;
			case STACK_OP_SS:
				if (state->sa.size < 2 || state->sb.size < 2)
					continue;
				break;
			case STACK_OP_PA:
				if (state->sb.size == 0)
					continue;
				break;
			case STACK_OP_PB:
				if (state->sa.size == 0)
					continue;
				break;
			case STACK_OP_RA: case STACK_OP_RRA:
				if (state->sa.size < 2) continue; break;
			case STACK_OP_RB: case STACK_OP_RRB:
				if (state->sb.size < 2) continue; break;
			case STACK_OP_RR: case STACK_OP_RRR:
				if (state->sa.size < 2 || state->sb.size < 2) continue; break;
			default:
				break;
		}

		// Evaluate instruction
		//printf("op  = %s\n", op_name(ops[i]));
		skip_data->cur_cost += ops[i] != STACK_OP_NOP;
		state_op(state, ops[i], 0);
		const size_t skip = find_future(orig_state, state, cfg);

		// Update if better than current skip
		if (skip_data->cur_cost < skip && skip > 0) {
			skip_data->skip = skip;
			skip_data->len = depth;
			skip_data->value = skip - skip_data->cur_cost;
			for (size_t i = 0; i < depth; ++i)
				skip_data->ops[i] = state->saves[start + i].op;
		}

		// Recurse
		if (depth < cfg->search_depth)
			backtrack(orig_state, state, start, cfg, depth + 1, skip_data);
		state_undo(state);
		skip_data->cur_cost -= ops[i] != STACK_OP_NOP;
	}
}

void
optimize(const state_t* state, optimizer_conf_t cfg)
{
	skip_data_t* skip_data = skip_data_new(state, &cfg);
	for (size_t i = 0; i < state->saves_size; ++i) {
		skip_data_t *const data = (skip_data_t *)((char*)skip_data + (sizeof(skip_data_t) + cfg.search_depth * sizeof(enum stack_op)) * i);
		
		// Bifurcate & Evaluate
		state_t bi = state_bifurcate(state, i);
		backtrack(state, &bi, i, &cfg, 1, data);
		state_destroy(&bi);

		size_t value = data->skip;
		for (size_t j = 0; j < data->len; ++j)
		{
			value -= data->ops[j] != STACK_OP_NOP;
		}
		printf("Found skip at %zu: %zu %zu value=%zu\n", i, data->skip, data->len, value);
		for (size_t j = 0; j < data->len; ++j)
		{
			printf("%s\n", op_name(data->ops[j]));
		}
	}
	free(skip_data);
}
