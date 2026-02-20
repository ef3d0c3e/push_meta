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

/**
 * @brief Get a value in a block
 *
 * @param state State
 * @param blk Block
 * @param pos Position in `blk`
 *
 * @return The value at @p pos in @p blk
 */
int
blk_value(const state_t* state, enum blk_dest blk, size_t pos);
/**
 * @brief Moves the value from one location to another
 *
 * Move the value at @p from into @p to
 *
 * @param state State
 * @param from Move from this location
 * @param to Move to this location
 */
void
blk_move(state_t* state, enum blk_dest from, enum blk_dest to);
/**
 * @brief Sort a block of two element onto A's top
 *
 * Block `blk` will be moved to A's top, sorted
 *
 * @param state State
 * @param blk A block of 2 elements
 */
void
blk_sort_2(state_t* state, blk_t blk);
/**
 * @brief Sort a block of three element onto A's top
 *
 * Block `blk` will be moved to A's top, sorted
 *
 * @param state State
 * @param blk A block of 3 elements
 */
void
blk_sort_3(state_t* state, blk_t blk);

typedef struct
{
	blk_t top;
	blk_t mid;
	blk_t bot;
} split_t;

/**
 * @brief Split a block into three blocks using a pair of pivots
 *
 * @param state State
 * @param blk Block to split
 * @param p1 First pivot
 * @param p2 Second pivot `p1 <= p2`
 *
 * @retutrn Three blocks made from splitting @p blk
 */
split_t
blk_split(state_t* state, blk_t blk, int p1, int p2);

typedef struct quicksort_data_t quicksort_data_t;

/** @brief Nelder-Mead settings */
typedef struct
{
	size_t max_depth;    /* Maximum search depth */
	size_t max_iters;  /* Maximum number of iterations before giving up/convergance */
	float tol;           /* Simplex radius tolerance in normalized [0,1] space, e.g. 1e-3f */
	float initial_scale; /* Initial simplex scale (fraction of [0,1]), e.g. 0.05f */
	size_t final_radius; /* Final search radius */
} quicksort_nm_t;

/** @brief Create quicksort data for Nelder-Mead */
quicksort_data_t
quicksort_nm(quicksort_nm_t);
void
quicksort_nm_impl(quicksort_data_t* data, state_t* state, blk_t blk);

typedef enum
{
	/** @brief A plot of `float` */
	PLOT_FLOAT,
	/** @brief A plot of `size_t` */
	PLOT_SIZE,
} plot_value_type;

/** @brief Data for plots */
typedef struct
{
	/** @brief Plot description */
	char* desc;
	/** @brief Plot value type */
	plot_value_type type;
	/** @brief Data size */
	size_t size[2];
	/** @brief Plot data */
	void* data;
} quicksort_plot_t;

struct quicksort_data_t
{
	union
	{
		quicksort_nm_t nm;
	};
	void (*sort)(quicksort_data_t*, state_t*, blk_t);

	quicksort_plot_t* plots;
	size_t plots_size;
};

/** @brief Free the quicksort data */
void
quicksort_data_free(quicksort_data_t *data);
/** @brief Add a plot to @p data */
void
quicksort_data_add_plot(quicksort_data_t* data, quicksort_plot_t plot);
/** @brief Write plots to files */
void
quicksort_write_plots(const quicksort_data_t *data);

void
sort_quicksort(quicksort_data_t* data, state_t* state);

#endif // QUICKSORT_H
