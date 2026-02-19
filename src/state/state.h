#ifndef STATE_H
#define STATE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <util.h>

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
	 * Valid for operands `~(A|B)`
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
	STACK_OP_NOP = STACK_OP_NOP__,
};

/**
 * @brief Get the name of an operation
 *
 * @param op Operation to get the name of
 *
 * @return Name of @p op
 */
const char*
op_name(enum stack_op op);

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
	/** Start address of the stack */
	int* const start;
	/** @brief Stack elements, within `[start, start + 2 * capacity]` */
	int* data;
	/** @brief Number of elements in the stack */
	size_t size;
	/* guard */
	size_t capacity;
} stack_t;

/**
 * @brief Create a new stack with an initial capacity
 *
 * @param capacity Capacity of the stack
 *
 * @return A new stack that can hold @p capacity elements
 */
stack_t
stack_new(size_t capacity);
/**
 * @brief Destroy (deallocate) a stack
 *
 * @param stack Stack to deallocate
 */
void
stack_destroy(stack_t* stack);
/**
 * @brief Check if a stack is sorted
 *
 * @param stack Stack to check for
 *
 * @return 1 if @p stack is sorted, `0` otherwise
 */
int
stack_is_sorted(const stack_t *stack);

/**
 * @brief Store a save of state
 *
 * In regular evaluation mode, every operation adds a save to the state.
 * This is how @ref state_bifurcate can restore a previous saved state
 */
typedef struct
{
	/** @brief Saved values, layout: [stack_a, stack_b] */
	int* data;
	/** @brief Size of stack_a */
	size_t sz_a;
	/** @brief Size of stack_b */
	size_t sz_b;
	/** @brief Operation that triggered the save */
	enum stack_op op;
} save_t;

/**
 * @brief Create a new save from the current state
 *
 * @param state State to create a save from
 */
save_t
save_new(const struct state_t* state);
/**
 * @brief Destroy a save
 *
 * @param save Save to destroy
 */
void
save_destroy(save_t* save);

/**
 * @brief The state
 *
 * This struct holds the stacks, save states and other parameters used for sorting
 */
typedef struct state_t
{
	/** @brief A stack */
	stack_t sa;
	/** @brief B stack */
	stack_t sb;

	/** @brief All saves */
	save_t* saves;
	/** @brief Number of saves */
	size_t saves_size;
	/** @brief Capacity of `saves` */
	size_t saves_capacity;
	/** @brief Point of bifurcation, `SIZE_MAX` for none */
	size_t bifurcate_point;

	/** @brief Number of operations evaluated */
	size_t op_count;
	/** @brief Search depth, used by Nelder-Maud to preemptively stop */
	size_t search_depth;
} state_t;

/**
 * @brief Create a new state
 *
 * @param capacity Capacity of the state's stacks
 *
 * @return The newly created state
 */
state_t
state_new(size_t capacity);
/**
 * @brief Destroy (deallocate) a state
 *
 * @param state State to destroy
 */
void
state_destroy(state_t *state);
/**
 * @brief Create a state from a save state
 *
 * @param state State to use for save states
 * @param history Save state ID
 *
 * @return The newly created state.
 *
 * @note The returned state is special in that it won't create new save states when used.
 */
state_t
state_bifurcate(const state_t* state, size_t history);
/**
 * @brief Evaluate an operation on the state
 *
 * @param op Operation to evaluate
 *
 * @note This will trigger a save state if the save is not cloned/bifurcated from another state.
 * This function will increase @p state's op_count.
 */
void
state_op(state_t* state, enum stack_op op);
/**
 * @brief Undo an operation on the state
 *
 * This function will simply evaluate the inverse operation of @p op.
 * E.g `SA` -> `SA`, `RB` -> `RRB`, etc.
 *
 * @param state State to undo on
 * @param op Operation to undo
 *
 * @note This function will decrease @p state's op_count.
 */
void
state_undo(state_t* state, enum stack_op op);
/**
 * @brief Clone the state
 *
 * @param state State to clone
 *
 * @return A clone of @p state
 *
 * @note The returned state will not record saves
 */
state_t
state_clone(const state_t *state);

/**
 * @brief Print the state on stdout
 *
 * This function only prints the current values of the stack
 *
 * @param state State to print
 */
void
print_state(const state_t* state);

#endif // STATE_H
