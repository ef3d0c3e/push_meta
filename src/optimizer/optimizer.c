#include <optimizer/optimizer.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

static enum stack_op ops[12] = {
	STACK_OP_NOP, STACK_OP_SA, STACK_OP_SB, STACK_OP_SS,  STACK_OP_PA,  STACK_OP_PB,
	STACK_OP_RA,  STACK_OP_RB, STACK_OP_RR, STACK_OP_RRA, STACK_OP_RRB, STACK_OP_RRR,
};

enum
{
	OPS_LEN = sizeof(ops) / sizeof(ops[0])
};

static inline size_t
sz_min(size_t a, size_t b)
{
	if (a <= b)
		return a;
	return b;
}

/** Find the last index in `orig_state` that is equal to `state` */
static inline size_t
find_future(const state_t* orig_state,
            const state_t* state,
            const optimizer_conf_t* cfg,
            size_t start)
{
	assert(state->saves_size != 0);
	const size_t end = sz_min(start + cfg->search_width, orig_state->saves_size);

	size_t best = 0;
	for (size_t i = start; i < end; ++i) {
		const save_t* orig = &orig_state->saves[i];
		if (state->sa.size != orig->sz_a || state->sb.size != orig->sz_b)
			continue;
		const int* orig_a = orig->data;
		const int* orig_b = orig->data + orig->sz_a;
		if (!memcmp(orig_a, state->sa.data, sizeof(int) * state->sa.size) &&
		    !memcmp(orig_b, state->sb.data, sizeof(int) * state->sb.size))
			best = i;
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

static inline size_t
skip_data_stride(const optimizer_conf_t* cfg)
{
	return (offsetof(skip_data_t, ops) + cfg->search_depth * sizeof(enum stack_op) +
	        _Alignof(skip_data_t) - 1) &
	       ~(_Alignof(skip_data_t) - 1);
}

static skip_data_t*
skip_data_new(const state_t* state, const optimizer_conf_t* cfg)
{
	const size_t size = skip_data_stride(cfg) * state->saves_size;

	skip_data_t* data = xmalloc(size);
	bzero(data, size);
	return data;
}

static inline int
should_prune(const state_t* state, size_t depth, enum stack_op op, enum stack_op* cur_ops)
{
	// Forbid illegal ops
	switch (op) {
		case STACK_OP_SA:
			if (state->sa.size < 2)
				return 1;
			break;
		case STACK_OP_SB:
			if (state->sb.size < 2)
				return 1;
			break;
		case STACK_OP_SS:
			if (state->sa.size < 2 || state->sb.size < 2)
				return 1;
			break;
		case STACK_OP_PA:
			if (state->sb.size == 0)
				return 1;
			break;
		case STACK_OP_PB:
			if (state->sa.size == 0)
				return 1;
			break;
		case STACK_OP_RA:
		case STACK_OP_RRA:
			if (state->sa.size < 2)
				return 1;
			break;
		case STACK_OP_RB:
		case STACK_OP_RRB:
			if (state->sb.size < 2)
				return 1;
			break;
		case STACK_OP_RR:
		case STACK_OP_RRR:
			if (state->sa.size < 2 || state->sb.size < 2)
				return 1;
			break;
		default:
			break;
	}

	// Forbid no-op
	if (depth > 1) {
		switch (cur_ops[depth - 2]) {
			case STACK_OP_SA:
				if (op == STACK_OP_SA)
					return 1;
				break;
			case STACK_OP_SB:
				if (op == STACK_OP_SB)
					return 1;
				break;
			case STACK_OP_SS:
				if (op == STACK_OP_SS)
					return 1;
				break;
			case STACK_OP_PA:
				if (op == STACK_OP_PB)
					return 1;
				break;
			case STACK_OP_PB:
				if (op == STACK_OP_PA)
					return 1;
				break;
			case STACK_OP_RA:
				if (op == STACK_OP_RRA)
					return 1;
				break;
			case STACK_OP_RB:
				if (op == STACK_OP_RRB)
					return 1;
				break;
			case STACK_OP_RR:
				if (op == STACK_OP_RRR)
					return 1;
				break;
			case STACK_OP_RRA:
				if (op == STACK_OP_RA)
					return 1;
				break;
			case STACK_OP_RRB:
				if (op == STACK_OP_RB)
					return 1;
				break;
			case STACK_OP_RRR:
				if (op == STACK_OP_RRB)
					return 1;
				break;
			default:
				break;
		}
	}
	return 0;
}

static inline void
backtrack(const state_t* orig_state,   /* Original state */
          state_t* state,              /* Bifurcated state */
          size_t start,                /* Index in original state */
          const optimizer_conf_t* cfg, /* Optimizer configuration */
          size_t depth,                /* Current search depth */
          skip_data_t* skip_data,      /* Result */
          enum stack_op* cur_ops)
{
	// Try all instructions
	for (size_t i = 0; i < OPS_LEN; ++i) {
		// Skip impossible instructions
		if (should_prune(state, depth, ops[i], cur_ops))
			continue;

		// Evaluate instruction
		skip_data->cur_cost += ops[i] != STACK_OP_NOP;
		cur_ops[depth - 1] = ops[i];
		state_op(state, ops[i]);
		size_t search_from = start + depth;
		size_t skip = find_future(orig_state, state, cfg, search_from);

		if (skip > search_from) {
			size_t original_cost = skip - start;
			if (original_cost > skip_data->cur_cost) {
				size_t value = original_cost - skip_data->cur_cost;

				if (value > skip_data->value) {
					skip_data->skip = skip;
					skip_data->len = depth;
					skip_data->value = value;

					memcpy(skip_data->ops, cur_ops, sizeof(enum stack_op) * depth);
				}
			}
		}

		// Recurse
		if (depth < cfg->search_depth && ops[i] != STACK_OP_NOP)
			backtrack(orig_state, state, start, cfg, depth + 1, skip_data, cur_ops);
		state_undo(state, ops[i]);
		skip_data->cur_cost -= ops[i] != STACK_OP_NOP;
	}
}

typedef enum
{
	DECISION_NEXT = 0,
	DECISION_SKIP = 1
} decision_t;

static enum stack_op*
build_optimal_walk(const state_t* orig_state,
                   void* skip_data_base,
                   const optimizer_conf_t* cfg,
                   size_t* ops_count_out)
{
	const size_t stride = skip_data_stride(cfg);
	const size_t n = orig_state->saves_size - 1;
	*ops_count_out = 0;
	if (orig_state->saves_size == 0 || n == 0 || skip_data_base == NULL || cfg == NULL)
		return NULL;

	size_t* dp = xmalloc((n + 1) * sizeof(size_t));
	bzero(dp, (n + 1) * sizeof(size_t));
	decision_t* decision = xmalloc(n * sizeof(decision_t));
	bzero(decision, n * sizeof(decision_t));

	// Build dp
	for (size_t i = n - 1; i-- > 0;) {
		skip_data_t* const sd = (skip_data_t*)((char*)skip_data_base + i * stride);

		// Default: No skip
		size_t best = dp[i + 1];
		decision_t best_dec = DECISION_NEXT;

		assert(sd->skip <= n);
		// Better: Skip
		if (sd->skip >= i) {
			assert(sd->len <= cfg->search_depth);
			if (sd->value + dp[sd->skip] > best) {
				best = sd->value + dp[sd->skip];
				best_dec = DECISION_SKIP;
			}
		}

		dp[i] = best;
		decision[i] = best_dec;
	}

	enum stack_op* out = xmalloc(orig_state->saves_size * sizeof(enum stack_op));
	size_t out_len = 0;

	// Build walk
	size_t i = 0;
	while (i < n) {
		skip_data_t* sd = (skip_data_t*)((char*)skip_data_base + i * stride);
		assert(sd->len <= cfg->search_depth);

		// Emit skips
		if (decision[i] == DECISION_SKIP && sd->skip > i) {
			for (size_t j = 0; j < sd->len; ++j) {
				assert(out_len < orig_state->saves_size);
				out[out_len++] = sd->ops[j];
			}
			i = sd->skip;
		}
		// Emit original instructions
		else {
			assert(out_len < orig_state->saves_size);
			out[out_len++] = orig_state->saves[i + 1].op;
			i += 1;
		}
	}

	free(dp);
	free(decision);
	*ops_count_out = out_len;
	return out;
}

state_t
optimize(const state_t* state, optimizer_conf_t cfg)
{
	skip_data_t* skip_data = skip_data_new(state, &cfg);

	// Compute skip_data
	size_t i;
#pragma omp parallel for schedule(dynamic) shared(state, cfg, skip_data) private(i)
	for (i = 0; i < state->saves_size - 1; ++i) {
		skip_data_t* const data = (skip_data_t*)((char*)skip_data + i * skip_data_stride(&cfg));

		enum stack_op* ops = xmalloc(sizeof(enum stack_op) * cfg.search_depth);
		// Bifurcate & Evaluate
		state_t bi = state_bifurcate(state, i + 1);
		backtrack(state, &bi, i, &cfg, 1, data, ops);
		state_destroy(&bi);
		free(ops);

		/*
		printf("Found skip at %zu: skip=%zu len=%zu value=%zu\n",
		       i,
		       data->skip,
		       data->len,
		       data->value);

		for (size_t j = 0; j < data->len; ++j) {
		    printf("%s\n", op_name(data->ops[j]));
		}

		if (data->skip) {
		    state_t s = state_bifurcate(state, data->skip + 1);
		    ps(&s);
		    state_destroy(&s);
		}
		printf("\n\n");
		*/
	}

	size_t ops_count;
	enum stack_op* ops = build_optimal_walk(state, skip_data, &cfg, &ops_count);

	state_t final = state_bifurcate(state, state->saves_size > 1 ? 1 : 0);
	final.op_count = 0;
	for (size_t i = 0; i < ops_count; ++i) {
		//printf("op=%s\n", op_name(ops[i]));
		if (ops[i] != STACK_OP_NOP)
			state_op(&final, ops[i]);
	}
	free(ops);
	free(skip_data);

	return final;
}
