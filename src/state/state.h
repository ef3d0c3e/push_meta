#ifndef STATE_H
#define STATE_H

#include <stdint.h>
#include <stdlib.h>

#include "../util.h"

/**
 * @brief All defined stack operations
 *
 * These are operation to be performed by the @ref stack_op function
 *
 * # Encoding
 *
 * Here is the operations encoding scheme:
 *  * Lower 2 bits: Stack selector, 0b01 for A, 0b10 for B, 0b11 for A and B
 *  * Next 3 bits: Operator selector:
 *  *- 0b001: Swap
 *  *- 0b010: Push
 *  *- 0b011: Rotate
 *  *- 0b100: Reverse Rotate
 *  *- 0b101: No-Op
 *
 *  Note: Push with stack selector `0b11` is invalid.
 *
 * # Implementation
 *
 * All operations are trivially O(1), except for rotations. By using a deque as
 * the stack, we can amortize the expensive `memcopy` that will happens around
 * every N rotate operations. Therefore rotations are amortized O(1).
 */
enum stack_op
{
	/**
	 * @brief Stack A operand
	 */
	STACK_OP_SEL_A__ = 0b00001,
	/**
	 * @brief Stack B operand
	 */
	STACK_OP_SEL_B__ = 0b00010,
	/**
	 * @brief Operands bitmask
	 */
	STACK_OPERAND__ = 0b00011,
	/**
	 * @brief Swap operation
	 * Valid for operands `A|B`
	 */
	STACK_OP_SWAP__ = 0b00100,
	/**
	 * @brief Push operation
	 * Valid for operands `A^B`
	 */
	STACK_OP_PUSH__ = 0b01000,
	/**
	 * @brief Rotate operation
	 * Valid for operands `A|B`
	 */
	STACK_OP_ROTATE__ = 0b01100,
	/**
	 * @brief Reverse-rotate operation
	 * Valid for operands `A|B`
	 */
	STACK_OP_REV_ROTATE__ = 0b10000,
	/**
	 * @brief No-Operation
	 */
	STACK_OP_NOP__ = 0b10100,
	/**
	 * @brief Operators bitmask
	 */
	STACK_OPERATOR__ = 0b11100,
	/**
	 * @brief Swaps A's top 2 elements
	 */
	STACK_OP_SA = STACK_OP_SWAP__ | STACK_OP_SEL_A__,
	/**
	 * @brief Swaps B's top 2 elements
	 */
	STACK_OP_SB = STACK_OP_SWAP__ | STACK_OP_SEL_B__,
	/**
	 * @brief Performs SA and SB
	 */
	STACK_OP_SS = STACK_OP_SWAP__ | STACK_OP_SEL_A__ | STACK_OP_SEL_B__,
	/**
	 * @brief Moves B's top to A's top
	 */
	STACK_OP_PA = STACK_OP_PUSH__ | STACK_OP_SEL_A__,
	/**
	 * @brief Moves A's top to B's top
	 */
	STACK_OP_PB = STACK_OP_PUSH__ | STACK_OP_SEL_B__,
	/**
	 * @brief Rotates A topwise
	 */
	STACK_OP_RA = STACK_OP_ROTATE__ | STACK_OP_SEL_A__,
	/**
	 * @brief Rotates B topwise
	 */
	STACK_OP_RB = STACK_OP_ROTATE__ | STACK_OP_SEL_B__,
	/**
	 * @brief Performs RA and RB
	 */
	STACK_OP_RR = STACK_OP_ROTATE__ | STACK_OP_SEL_A__ | STACK_OP_SEL_B__,
	/**
	 * @brief Rotates A bottomwise
	 */
	STACK_OP_RRA = STACK_OP_REV_ROTATE__ | STACK_OP_SEL_A__,
	/**
	 * @brief Rotates B bottomwise
	 */
	STACK_OP_RRB = STACK_OP_REV_ROTATE__ | STACK_OP_SEL_B__,
	/**
	 * @brief Performs RRA and RRB
	 */
	STACK_OP_RRR = STACK_OP_REV_ROTATE__ | STACK_OP_SEL_A__ | STACK_OP_SEL_B__,
	/**
	 * @brief Does nothing
	 */
	STACK_OP_NOP = 0b10100,
};

typedef struct state_t state_t;

/**
 * @brief The stack data structure
 *
 * The instructions set requires underlying stacks to be implemented. They are called
 * `stacks` however they act as double-ended queue because of the
 * (reverse-)rotate operations. Since the stacks cannot grow (limited to the
 * initial program input size), the stacks are implemented using a double-ended queue
 * with a static capacity of 3*N (N = input size). The initial buffer is contained
 * within [N, 2N], and will move with pushes, rotates and reverse-rotates. Once
 * it reaches the edges, [0, N] or [2N, 3N], if will be copied back to the
 * center position. Using a 3*N factor allows the use of `memcpy` when re-centering the values,
 * instead of `memmove`. This guarantees that every operation is amortized O(1).
 *
 */
typedef struct
{
	int* const start;
	int* data;
	size_t size;
	/* guard */
	size_t capacity;
} stack_t;

stack_t
stack_new(size_t capacity);
void
stack_destroy(stack_t* stack);
int
stack_is_sorted(const stack_t *stack);

typedef struct
{
	int* data;
	size_t sz_a;
	size_t sz_b;
	enum stack_op op;
} save_t;

save_t
save_new(const struct state_t* state);
void
save_destroy(save_t* save);

typedef struct state_t
{
	stack_t sa;
	stack_t sb;

	save_t* saves;
	size_t saves_size;
	size_t saves_capacity;
	size_t bifurcate_point;
} state_t;

state_t
state_new(size_t capacity);
void
state_destroy(state_t *state);
state_t
state_bifurcate(const state_t* state, size_t history);
void
state_op(state_t* state, enum stack_op op);

#endif // STATE_H
