#include <quicksort/quicksort.h>
#include <optimizer/optimizer.h>
#include <stdio.h>

int
main(int ac, char** av)
{
	if (ac < 2) {
		fprintf(stderr, "USAGE: %s NUMBERS\n", av[0]);
		return 1;
	}
	state_t state = state_new((size_t)ac - 1ULL);

	for (int i = 1; i < ac; ++i) {
		state.sa.data[state.sa.size++] = atoi(av[i]);
	}

	sort_quicksort(&state);
	assert(state.sb.size == 0);
	assert(state.sa.size == state.sa.capacity);
	// for (size_t i = 0; i < state.sa.size; ++i)
	//{
	//	printf("%d\n", state.sa.data[i]);
	// }
	ps(&state);
	assert(stack_is_sorted(&state.sa));

	printf("Base sort in `%zu' instructions.\n", state.saves_size - 1);

	optimizer_conf_t cfg = {
		.search_width = 1000,
		.search_depth = 4,
	};
	optimize(&state, cfg);

	state_destroy(&state);
	return 0;
}
