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

typedef struct {
	blk_t top;
	blk_t mid;
	blk_t bot;
} split_t;

void
sort_quicksort(state_t *state);

#endif // QUICKSORT_H
