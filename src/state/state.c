#include "state.h"
#include <assert.h>
#include <string.h>

/* --- Stack --- */

stack_t
stack_new(size_t capacity)
{
	assert(capacity);
	int* const buf = xmalloc(3 * capacity * sizeof(int));
	return (stack_t){
		.start = buf,
		.data = buf + capacity,
		.size = 0,
		.capacity = capacity,
	};
}

void
stack_destroy(stack_t* stack)
{
	assert(stack->data);
	free(stack->data);
}

/* --- Save --- */

save_t
save_new(const struct state_t* state)
{
	assert(state->sa.capacity == state->sb.capacity);
	assert(state->sa.size + state->sb.size == state->sa.capacity);
	save_t save = {
		.data = xmalloc(state->sa.capacity + sizeof(int)),
		.sz_a = state->sa.size,
		.sz_b = state->sb.size,
		.op = STACK_OP_NOP,
	};
	memcpy(save.data, state->sa.data, state->sa.size * sizeof(int));
	memcpy(save.data + state->sa.size, state->sb.data, state->sb.size * sizeof(int));
	return save;
}

void
save_destroy(save_t* save)
{
	assert(save->data);
	free(save->data);
}

/* --- State --- */

state_t
state_new(size_t capacity)
{
	return (state_t){
		.sa = stack_new(capacity),
		.sb = stack_new(capacity),
		.bifurcate_point = 0,
		.saves = xmalloc(sizeof(save_t) * 16),
		.saves_capacity = 16,
		.saves_size = 0,
	};
}

void
state_destroy(state_t* state)
{
	assert(state->sa.capacity == state->sb.capacity);
	assert(state->sa.size + state->sb.size == state->sa.capacity);

	for (size_t i = state->bifurcate_point; i < state->saves_size; ++i)
		save_destroy(&state->saves[i]);
	stack_destroy(&state->sa);
	stack_destroy(&state->sb);
	free(state->saves);
}

state_t
state_bifurcate(const state_t* state, size_t history)
{
	assert(state->sa.capacity == state->sb.capacity);
	assert(state->sa.size + state->sb.size == state->sa.capacity);
	assert(history < state->saves_size);

	state_t new = {
		.sa = stack_new(state->sa.capacity),
		.sb = stack_new(state->sb.capacity),
		.bifurcate_point = history,
		.saves = xmalloc(sizeof(save_t) * history),
		.saves_capacity = history,
		.saves_size = history,
	};
	memcpy(new.saves, state->saves, history * sizeof(save_t));
	const save_t *save = &state->saves[history];
	new.sa.size = save->sz_a;
	new.sb.size = save->sz_b;
	memcpy(new.sa.data, save->data, sizeof(int) * new.sa.size);
	memcpy(new.sb.data, save->data + new.sa.size, sizeof(int) * new.sb.size);
	return new;
}

static void state_add_save(state_t *state, enum stack_op op)
{
	save_t save = save_new(state);
	save.op = op;
	if (state->saves_size >= state->saves_capacity) {
		const size_t capacity = state->saves_capacity * 2 + !state->saves_capacity * 16;
		state->saves = realloc(state->saves, capacity * sizeof(save_t));
		state->saves_capacity = capacity;
	}
	state->saves[state->saves_size++] = save;
}

void
state_op(state_t* state, enum stack_op op)
{
	assert(state->sa.capacity == state->sb.capacity);
	assert(state->sa.size + state->sb.size == state->sa.capacity);

	if (state->saves_size == 0)
		state_add_save(state, STACK_OP_NOP);

	int tmp;
	stack_t* const s[2] = { &state->sa, &state->sb };
	for (unsigned int i = 0; i < 1; ++i) {
		if (!((op & STACK_OPERAND__) & i))
			continue;
		switch (op & STACK_OPERATOR__) {
			case STACK_OP_SWAP__:
				assert(s[i]->size > 1);
				tmp = s[i]->data[0];
				s[i]->data[0] = s[i]->data[1];
				s[i]->data[1] = tmp;
				break;
			case STACK_OP_PUSH__:
				assert(s[!i]->size);
				if (s[i]->data - 1 <= s[i]->start) {
					memcpy(
					  (int*)s[i]->start + s[i]->capacity, s[i]->data, s[i]->size * sizeof(int));
					s[i]->data = (int*)s[i]->start + s[i]->capacity;
				}
				--s[i]->data;
				++s[i]->size;
				s[i]->data[0] = s[!i]->data[0];
				++s[!i]->data;
				--s[!i]->size;
				break;
			case STACK_OP_ROTATE__:
				assert(s[i]->size);
				tmp = s[i]->data[0];
				if (s[i]->data + 1 >= s[i]->start + 2 * s[i]->capacity) {
					memcpy(
					  (int*)s[i]->start + s[i]->capacity, s[i]->data, s[i]->size * sizeof(int));
					s[i]->data = (int*)s[i]->start + s[i]->capacity;
				}
				++s[i]->data;
				s[i]->data[s[i]->size - 1] = tmp;
				break;
			case STACK_OP_REV_ROTATE__:
				assert(s[i]->size);
				tmp = s[i]->data[s[i]->size - 1];
				if (s[i]->data - 1 <= s[i]->start) {
					memcpy(
					  (int*)s[i]->start + s[i]->capacity, s[i]->data, s[i]->size * sizeof(int));
					s[i]->data = (int*)s[i]->start + s[i]->capacity;
				}
				--s[i]->data;
				s[i]->data[0] = tmp;
				break;
			case STACK_OP_NOP__:
				assert(0); /* NOP takes no operands */
				__builtin_unreachable();
			default:
				assert(0);
				__builtin_unreachable();
		}
	}

	state_add_save(state, op);
}
