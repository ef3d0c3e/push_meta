#include <quicksort/quicksort.h>
#include <optimizer/optimizer.h>
#include <stdio.h>
#include <time.h>

static inline void
format_time(struct timespec start, struct timespec end, char buf[256])
{
	time_t sec = end.tv_sec - start.tv_sec;
	long nsec = end.tv_nsec - start.tv_nsec;
	if (nsec < 0) {
		--sec;
		nsec += 1000000000L;
	}
	const double diff_us = (double)sec * 1e6 + (double)nsec * 1e-3;
	if (diff_us < 1000)
		snprintf(buf, 256, "%.3F us", diff_us);
	else if (diff_us < 1000000)
		snprintf(buf, 256, "%.3F ms", diff_us / 1000.0);
	else
		snprintf(buf, 256, "%.3F secs", diff_us / 1000000.0);
}

int
main(int ac, char** av)
{
	if (ac < 2) {
		fprintf(stderr, "USAGE: %s NUMBERS\n", av[0]);
		return 1;
	}
	state_t state = state_new((size_t)ac - 1ul);

	for (int i = 1; i < ac; ++i) {
		state.sa.data[state.sa.size++] = atoi(av[i]);
	}

	quicksort_config_t qcfg = {
		.search_depth = 3,

		.nm_max_iters = 50,
		.nm_tol = 0.01f,
		.nm_initial_scale = 0.55f,
		.nm_final_radius = 2,
	};
	struct timespec start, end;
	char time[256];

	clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	sort_quicksort(&qcfg, &state);
	clock_gettime(CLOCK_MONOTONIC_RAW, &end);

	assert(state.sb.size == 0);
	assert(state.sa.size == state.sa.capacity);
	assert(stack_is_sorted(&state.sa));

	format_time(start, end, time);
	printf("Base sort in `%zu' instructions in %s.\n", state.saves_size - 1, time);

	optimizer_conf_t cfg = {
		.search_width = 1000,
		.search_depth = 4,
	};

	clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	state_t optimized = optimize(&state, cfg);
	clock_gettime(CLOCK_MONOTONIC_RAW, &end);

	format_time(start, end, time);
	printf("Optimized in `%zu` instructions in %s.\n", optimized.op_count, time);

	state_destroy(&optimized);
	state_destroy(&state);
	return 0;
}
