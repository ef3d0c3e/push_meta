#include <quicksort/quicksort.h>
#include <stdio.h>

static inline int
blk_value(const state_t* state, enum blk_dest blk, size_t pos)
{
	switch (blk) {
		case BLK_A_TOP:
			assert(state->sa.size > pos);
			return state->sa.data[pos];
			break;
		case BLK_A_BOT:
			assert(state->sa.size > pos);
			return state->sa.data[state->sa.size - pos - 1];
			break;
		case BLK_B_TOP:
			assert(state->sb.size > pos);
			return state->sb.data[pos];
			break;
		case BLK_B_BOT:
			assert(state->sb.size > pos);
			return state->sb.data[state->sb.size - pos - 1];
			break;
		default:
			assert(0);
			__builtin_unreachable();
	}
}

/* Move a single value from a location to another */
static inline void
blk_move(state_t* state, enum blk_dest from, enum blk_dest to)
{
	assert(((from & BLK_SEL__) == BLK_A__ && state->sa.size) ||
	       ((from & BLK_SEL__) == BLK_B__ && state->sb.size));

	static const enum stack_op table[16][4] = {
		[(BLK_A_TOP << 2) | BLK_A_TOP] = { STACK_OP_NOP },
		[(BLK_A_TOP << 2) | BLK_A_BOT] = { STACK_OP_RA, STACK_OP_NOP },
		[(BLK_A_TOP << 2) | BLK_B_TOP] = { STACK_OP_PB, STACK_OP_NOP },
		[(BLK_A_TOP << 2) | BLK_B_BOT] = { STACK_OP_PB, STACK_OP_RB, STACK_OP_NOP },

		[(BLK_A_BOT << 2) | BLK_A_TOP] = { STACK_OP_RRA, STACK_OP_NOP },
		[(BLK_A_BOT << 2) | BLK_A_BOT] = { STACK_OP_NOP },
		[(BLK_A_BOT << 2) | BLK_B_TOP] = { STACK_OP_RRA, STACK_OP_PB, STACK_OP_NOP },
		[(BLK_A_BOT << 2) | BLK_B_BOT] = { STACK_OP_RRA, STACK_OP_PB, STACK_OP_RB, STACK_OP_NOP },

		[(BLK_B_TOP << 2) | BLK_A_TOP] = { STACK_OP_PA, STACK_OP_NOP },
		[(BLK_B_TOP << 2) | BLK_A_BOT] = { STACK_OP_PA, STACK_OP_RA, STACK_OP_NOP },
		[(BLK_B_TOP << 2) | BLK_B_TOP] = { STACK_OP_NOP },
		[(BLK_B_TOP << 2) | BLK_B_BOT] = { STACK_OP_RB, STACK_OP_NOP },

		[(BLK_B_BOT << 2) | BLK_A_TOP] = { STACK_OP_RRB, STACK_OP_PA, STACK_OP_NOP },
		[(BLK_B_BOT << 2) | BLK_A_BOT] = { STACK_OP_RRB, STACK_OP_PA, STACK_OP_RA, STACK_OP_NOP },
		[(BLK_B_BOT << 2) | BLK_B_TOP] = { STACK_OP_RRB, STACK_OP_NOP },
		[(BLK_B_BOT << 2) | BLK_B_BOT] = { STACK_OP_NOP },
	};

	const unsigned int id = (from << 2) | to;
	assert(id < 16);
	for (size_t i = 0; table[id][i] != STACK_OP_NOP; ++i)
		state_op(state, table[id][i], 1);
}

/* --- Small sort --- */
static inline int
blk_rank(const state_t* state, blk_t blk)
{
	assert(blk.size != 0);
	assert(blk.size <= 3);

	if (blk.size == 1)
		return 0;
	if (blk.size == 2)
		return blk_value(state, blk.dest, 0) > blk_value(state, blk.dest, 1);

	const int u = blk_value(state, blk.dest, 0);
	const int v = blk_value(state, blk.dest, 1);
	const int w = blk_value(state, blk.dest, 2);
	return 1 * (u > v && v > w) + 2 * (u > w && w > v) + 3 * (v > u && u > w) +
	       4 * (v > w && w > u) + 5 * (w > u && u > v) + 6 * (w > v && v > u) - 1;
}

/** Move a block of size 2 on A_TOP, sorted */
static inline void
blk_sort_2(state_t* state, blk_t blk)
{
	assert(blk.size == 2);

	// u = blk_value(0), v = blk_value(1)
	static const enum stack_op table[4][2][6] = {
		[BLK_A_TOP] = {
			// u < v
			[0] = {STACK_OP_NOP},
			// u > v
			[1] = {STACK_OP_SA, STACK_OP_NOP},
		},
		[BLK_A_BOT] = {
			// u < v
			[0] = {STACK_OP_RRA, STACK_OP_RRA, STACK_OP_SA, STACK_OP_NOP},
			// u > v
			[1] = {STACK_OP_RRA, STACK_OP_RRA, STACK_OP_NOP},
		},
		[BLK_B_TOP] = {
			// u < v
			[0] = {STACK_OP_PA, STACK_OP_PA, STACK_OP_SA, STACK_OP_NOP},
			// u > v
			[1] = {STACK_OP_PA, STACK_OP_PA, STACK_OP_NOP},
		},
		[BLK_B_BOT] = {
			// u < v
			[0] = {STACK_OP_RRB, STACK_OP_RRB, STACK_OP_PA, STACK_OP_PA, STACK_OP_NOP},
			// u > v
			[1] = {STACK_OP_RRB, STACK_OP_RRB, STACK_OP_PA, STACK_OP_PA, STACK_OP_SA, STACK_OP_NOP},
		},
	};

	const int rank = blk_rank(state, blk);
	assert(rank == 0 || rank == 1);
	for (size_t i = 0; table[blk.dest][rank][i] != STACK_OP_NOP; ++i)
		state_op(state, table[blk.dest][rank][i], 1);
}

/** Move a block of size 3 to A_TOP, sorted */
static inline void
blk_sort_3(state_t* state, blk_t blk)
{
	assert(blk.size == 3);

	// u = blk_value(0), v = blk_value(1), w = blk_value(2)
	static const enum stack_op table[4][6][8] = {
		[BLK_A_TOP] = {
			[0] = {STACK_OP_SA, STACK_OP_RA, STACK_OP_SA, STACK_OP_RRA, STACK_OP_SA, STACK_OP_NOP},
			[1] = {STACK_OP_SA, STACK_OP_RA, STACK_OP_SA, STACK_OP_RRA, STACK_OP_NOP},
			[2] = {STACK_OP_RA, STACK_OP_SA, STACK_OP_RRA, STACK_OP_SA, STACK_OP_NOP},
			[3] = {STACK_OP_RA, STACK_OP_SA, STACK_OP_RRA, STACK_OP_NOP},
			[4] = {STACK_OP_SA, STACK_OP_NOP},
			[5] = {STACK_OP_NOP},
		},
		[BLK_A_BOT] = {
			[0] = {STACK_OP_RRA, STACK_OP_RRA, STACK_OP_RRA, STACK_OP_NOP},
			[1] = {STACK_OP_RRA, STACK_OP_RRA, STACK_OP_RRA, STACK_OP_SA, STACK_OP_NOP},
			[2] = {STACK_OP_RRA, STACK_OP_RRA, STACK_OP_SA, STACK_OP_RRA, STACK_OP_NOP},
			[3] = {STACK_OP_RRA, STACK_OP_RRA, STACK_OP_SA, STACK_OP_RRA, STACK_OP_SA, STACK_OP_NOP},
			[4] = {STACK_OP_RRA, STACK_OP_RRA, STACK_OP_PB, STACK_OP_RRA, STACK_OP_SA, STACK_OP_PA, STACK_OP_NOP},
			[5] = {STACK_OP_RRA, STACK_OP_PB, STACK_OP_RRA, STACK_OP_RRA, STACK_OP_SA, STACK_OP_PA, STACK_OP_NOP},
		},
		[BLK_B_TOP] = {
			[0] = {STACK_OP_PA, STACK_OP_PA, STACK_OP_PA, STACK_OP_NOP},
			[1] = {STACK_OP_PA, STACK_OP_SB, STACK_OP_PA, STACK_OP_PA, STACK_OP_NOP},
			[2] = {STACK_OP_SB, STACK_OP_PA, STACK_OP_PA, STACK_OP_PA, STACK_OP_NOP},
			[3] = {STACK_OP_SB, STACK_OP_PA, STACK_OP_SB, STACK_OP_PA, STACK_OP_PA, STACK_OP_NOP},
			[4] = {STACK_OP_PA, STACK_OP_SB, STACK_OP_PA, STACK_OP_SA, STACK_OP_PA, STACK_OP_NOP},
			[5] = {STACK_OP_SB, STACK_OP_PA, STACK_OP_SB, STACK_OP_PA, STACK_OP_SA, STACK_OP_PA, STACK_OP_NOP},
		},
		[BLK_B_BOT] = {
			[0] = {STACK_OP_RRB, STACK_OP_PA, STACK_OP_RRB, STACK_OP_PA, STACK_OP_RRB, STACK_OP_PA, STACK_OP_NOP},
			[1] = {STACK_OP_RRB, STACK_OP_PA, STACK_OP_RRB, STACK_OP_RRB, STACK_OP_PA, STACK_OP_PA, STACK_OP_NOP},
			[2] = {STACK_OP_RRB, STACK_OP_RRB, STACK_OP_PA, STACK_OP_PA, STACK_OP_RRB, STACK_OP_PA, STACK_OP_NOP},
			[3] = {STACK_OP_RRB, STACK_OP_RRB, STACK_OP_PA, STACK_OP_RRB, STACK_OP_PA, STACK_OP_PA, STACK_OP_NOP},
			[4] = {STACK_OP_RRB, STACK_OP_RRB, STACK_OP_SB, STACK_OP_RRB, STACK_OP_PA, STACK_OP_PA, STACK_OP_PA, STACK_OP_NOP},
			[5] = {STACK_OP_RRB, STACK_OP_RRB, STACK_OP_RRB, STACK_OP_PA, STACK_OP_PA, STACK_OP_PA, STACK_OP_NOP},
		},
	};

	const int rank = blk_rank(state, blk);
	assert(rank < 6);
	for (size_t i = 0; table[blk.dest][rank][i] != STACK_OP_NOP; ++i)
		state_op(state, table[blk.dest][rank][i], 1);
}

/* --- Quicksort --- */
static inline split_t
blk_split(state_t *state, blk_t blk, int p1, int p2)
{
	split_t split = {
		.top = { .size = 0, .dest = blk.dest == BLK_B_BOT ? BLK_B_TOP : BLK_B_BOT},
		.mid = { .size = 0, .dest = (blk.dest & BLK_SEL__) == BLK_B__ ? BLK_A_BOT : BLK_B_TOP},
		.bot = { .size = 0, .dest = blk.dest == BLK_A_TOP ? BLK_A_BOT : BLK_A_TOP},
	};

	while (blk.size)
	{
		const int val = blk_value(state, blk.dest, 0);
		if (val >= p2)
		{
			blk_move(state, blk.dest, split.bot.dest);
			++split.bot.size;
		}
		else if (val >= p1)
		{
			blk_move(state, blk.dest, split.mid.dest);
			++split.mid.size;
		}
		else
		{
			blk_move(state, blk.dest, split.top.dest);
			++split.top.size;
		}
		--blk.size;
	}
	return split;
}

static void
quicksort_impl(state_t* state, blk_t blk)
{
	if (blk.size == 0)
		return;

	// Normalize direction
	if (blk.dest == BLK_A_BOT && state->sa.size == blk.size)
		blk.dest = BLK_A_TOP;
	else if (blk.dest == BLK_B_BOT && state->sb.size == blk.size)
		blk.dest = BLK_B_TOP;

	// Sort manually for small blocks
	if (blk.size == 1) {
		blk_move(state, blk.dest, BLK_A_TOP);
		return;
	}
	else if (blk.size == 2) {
		blk_sort_2(state, blk);
		return;
	}
	else if (blk.size == 3) {
		blk_sort_3(state, blk);
		return;
	}

	// Choose pivots & split
	int pivots[2] = {blk_value(state, blk.dest, blk.size / 3), blk_value(state, blk.dest, 2 * blk.size / 3)};
	if (pivots[0] > pivots[1])
	{
		const int tmp = pivots[0];
		pivots[0] = pivots[1];
		pivots[1] = tmp;
	}
	const split_t split = blk_split(state, blk, pivots[0], pivots[1]);
	quicksort_impl(state, split.bot);
	quicksort_impl(state, split.mid);
	quicksort_impl(state, split.top);
}

void
sort_quicksort(state_t* state)
{
	assert(state->sb.size == 0);
	assert(state->sa.size == state->sa.capacity);

	// Create block
	const blk_t blk = { .dest = BLK_A_TOP, .size = state->sa.size };
	quicksort_impl(state, blk);
}
