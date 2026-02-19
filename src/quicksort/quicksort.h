#ifndef QUICKSORT_H
#define QUICKSORT_H

#include <state/state.h>

/**
 * @brief A location
 *
 *    A           B
 * +-----+     +-----+
 * | TOP |     | TOP |
 * | ... |     | ... |
 * |     |     |     |
 * +-----+     +-----+
 * |     |     |     |
 * |     |     |     |
 * |     |     |     |
 * |     |     |     |
 * +-----+     +-----+
 * |     |     |     |
 * | ... |     | ... |
 * | BOT |     | BOT |
 * +-----+     +-----+
 */
enum blk_dest
{
	//
	BLK_SEL__ = 0b10,
	BLK_A__ = 0b00,
	BLK_B__ = 0b10,
	//
	BLK_POS__ = 0b01,
	BLK_TOP__ = 0b00,
	BLK_BOT__ = 0b01,
	//
	BLK_A_TOP = BLK_A__ | BLK_TOP__, // 00
	BLK_A_BOT = BLK_A__ | BLK_BOT__, // 01
	BLK_B_TOP = BLK_B__ | BLK_TOP__, // 10
	BLK_B_BOT = BLK_B__ | BLK_BOT__, // 11
};

typedef struct
{
	size_t size;
	enum blk_dest dest;
} blk_t;

int
blk_value(const state_t* state, enum blk_dest blk, size_t pos);

typedef struct
{
	blk_t top;
	blk_t mid;
	blk_t bot;
} split_t;

split_t
blk_split(state_t* state, blk_t blk, int p1, int p2);

typedef struct
{
	size_t search_depth;

	unsigned nm_max_iters;  /* Maximum number of iterations before giving up/convergance */
	float nm_tol;           /* Simplex radius tolerance in normalized [0,1] space, e.g. 1e-3f */
	float nm_initial_scale; /* Initial simplex scale (fraction of [0,1]), e.g. 0.05f */
	size_t nm_final_radius; /* Final search radius */
} quicksort_config_t;

void
quicksort_pivots(const quicksort_config_t* cfg, const state_t* state, blk_t blk, int* pivots);
void
quicksort_impl(const quicksort_config_t* cfg, state_t* state, blk_t blk);
void
sort_quicksort(const quicksort_config_t* cfg, state_t* state);

#endif // QUICKSORT_H
