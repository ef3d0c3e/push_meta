#include <optimizer/optimizer.h>
#include <quicksort/quicksort.h>
#include <stdio.h>
#include <string.h>
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

static int random_state = 2043930778; // Fixed for reproducibility
static inline uint32_t
random_int(uint32_t* state)
{
	uint32_t x = *state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;

	return *state = x;
}

static inline void
generate(state_t* state, uint32_t* random_state, size_t num)
{
}

typedef struct
{
	uint32_t random_state;
	size_t generate;
	int list;
	const char* method;
} options_t;

static void
print_help(const char* program)
{
	fprintf(stderr,
	        "%1$s -- A meta solver for the Push_Swap problem\n"
	        "\n"
	        "Usage:\n"
	        "	%1$s [OPTIONS] COMMAND ARGUMENTS\n"
	        "\n"
	        "Example:\n"
	        "	%1$s list 3 4 2 1 # Sort a list passed in arguments\n"
	        "	%1$s generate 500 # Sort a generated list\n"
	        "\n"
	        "Commands:\n"
	        "	generate|gen NUM	Generate a random list from a seed\n"
	        "	list VALUES		Sort the list provided in arguments\n"
	        "\n"
	        "Options:\n"
	        "	-s, --seed NUM		Use a specific seed for `generate'\n"
	        "	-m, --method METHOD	Use a specific sorting method, available method:\n"
	        "		- 'nm', 'Nelder-Mead': (default)\n"
	        "",
	        program);
}

int
main(int ac, char** av)
{
	if (ac < 2) {
		fprintf(stderr, "USAGE: %s [OPTIONS]\n", av[0]);
		return 1;
	}

	// Parse arguments
	options_t opts = {
		.random_state = 2043930778,
		.generate = 0,
		.list = 0,
		.method = "nm",
	};
	for (int i = 1; i < ac;) {
		// Show help
		if (!strcmp(av[i], "-h") || !strcmp(av[i], "--help")) {
			print_help(av[0]);
			exit(0);
		}
		// Parse random state
		else if (!strcmp(av[i], "-s") || !strcmp(av[i], "--seed")) {
			if (i + 1 >= ac) {
				fprintf(stderr, "Expected an integer after `%s'\n", av[i]);
				exit(1);
			}
			char* end;
			opts.random_state = (uint32_t)strtoul(av[i + 1], &end, 10);
			if (*end) {
				fprintf(stderr, "Invalid integer after `%s'\n", av[i]);
				exit(1);
			}
			i += 2;
		} else if (!strcmp(av[i], "-m") || !strcmp(av[i], "--method")) {
			if (i + 1 >= ac) {
				fprintf(stderr, "Expected an sort method after `%s'\n", av[i]);
				exit(1);
			}

			if (!strcmp(av[i + 1], "nm") || !strcmp(av[i + 1], "Nelder-Mead")) {
				opts.method = "nm";
			} else {
				fprintf(stderr, "Unknown sorting method `%s'\n", av[i + 1]);
				exit(1);
			}
			i += 2;
		}
		// Generate
		else if (!strcmp(av[i], "gen") || !strcmp(av[i], "generate")) {
			if (i + 1 >= ac) {
				fprintf(stderr, "Expected an integer after `generate'\n");
				exit(1);
			}
			char* end;
			opts.generate = strtoul(av[i + 1], &end, 10);
			if (*end || opts.generate == 0) {
				fprintf(stderr, "Invalid integer after `generate'\n");
				exit(1);
			}
			i += 2;
			if (i < ac) {
				fprintf(stderr, "Unexpected arguments after `generate'\n");
				exit(1);
			}
		}
		// Read list
		else if (!strcmp(av[i], "list")) {
			opts.list = i + 1;
			break;
		} else {
			fprintf(stderr, "Unknown option `%s'\n", av[i]);
			exit(1);
		}
	}

	// Build state
	const size_t state_capacity = opts.list ? (size_t)(ac - opts.list) : opts.generate;
	state_t state = state_new(state_capacity);
	if (opts.list) {
		for (int i = opts.list; i < ac; ++i) {
			char* end;
			const int val = (int)strtol(av[i], &end, 10);
			for (size_t j = 0; j < state.sa.size; ++j) {
				if (state.sa.data[j] == val) {
					fprintf(stderr, "Duplicate value `%d` in state at position %d", val, i);
					exit(1);
				}
			}
			state.sa.data[state.sa.size++] = val;
		}
	} else if (opts.generate) {
		for (size_t i = 0; i < state_capacity; ++i) {
			while (1) {
				int val = (int)(((uint32_t)random_int(&opts.random_state)) % (uint32_t)state_capacity);
				int valid = 1;
				for (size_t j = 0; j < state.sa.size; ++j) {
					if (state.sa.data[j] == val) {
						valid = 0;
						break;
					}
				}
				if (valid) {
					state.sa.data[state.sa.size++] = val;
					break;
				}
			}
		}
	} else
		assert(0);

	// Build data
	quicksort_data_t data;
	if (!strcmp(opts.method, "nm"))
		data = quicksort_nm((quicksort_nm_t){
		  .max_depth = 3,
		  .max_iters = 50,
		  .tol = 0.01f,
		  .initial_scale = 0.55f,
		  .final_radius = 2,
		});
	else
		abort();

	struct timespec start, end;
	char time[256];

	clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	sort_quicksort(&data, &state);
	clock_gettime(CLOCK_MONOTONIC_RAW, &end);

	assert(state.sb.size == 0);
	assert(state.sa.size == state.sa.capacity);
	assert(stack_is_sorted(&state.sa));

	format_time(start, end, time);
	printf("Base sort in `%zu' instructions in %s.\n", state.saves_size - 1, time);

	quicksort_write_plots(&data);

	optimizer_conf_t cfg = {
		.search_width = 1000,
		.search_depth = 4,
	};

	clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	state_t optimized = optimize(&state, cfg);
	clock_gettime(CLOCK_MONOTONIC_RAW, &end);

	format_time(start, end, time);
	printf("Optimized in `%zu` instructions in %s.\n", optimized.op_count, time);

	quicksort_data_free(&data);
	state_destroy(&optimized);
	state_destroy(&state);

	/*
	    if (ac < 2) {
	        fprintf(stderr, "USAGE: %s NUMBERS\n", av[0]);
	        return 1;
	    }
	    state_t state = state_new((size_t)ac - 1ul);

	    for (int i = 1; i < ac; ++i) {
	        state.sa.data[state.sa.size++] = atoi(av[i]);
	    }

	    quicksort_config_t qcfg = {
	        .search_depth = 2,

	        .nm_max_iters = 50,
	        .nm_tol = 0.01f,
	        .nm_initial_scale = 0.55f,
	        .nm_final_radius = 2,
	    };
	    struct timespec start, end;
	    char time[256];

	    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	    sort_quicksort(&qcfg, &state, quicksort_nm_impl);
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
	    */
	return 0;
}
