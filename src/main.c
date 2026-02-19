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

	quicksort_config_t qcfg = {
		.search_depth = 1,

		.nm_max_iters = 50,
		.nm_tol = 1e-3f,
		.nm_initial_scale = 0.5f,
		.nm_final_radius = 2,
	};
	sort_quicksort(&qcfg, &state);
	assert(state.sb.size == 0);
	assert(state.sa.size == state.sa.capacity);
	assert(stack_is_sorted(&state.sa));

	printf("Base sort in `%zu' instructions.\n", state.saves_size - 1);

	//optimizer_conf_t cfg = {
	//	.search_width = 1000,
	//	.search_depth = 4,
	//};
	//state_t optimized = optimize(&state, cfg);
	//printf("optimized in `%zu` instructions.\n", optimized.op_count);

	//state_destroy(&optimized);
	state_destroy(&state);
	return 0;
}
