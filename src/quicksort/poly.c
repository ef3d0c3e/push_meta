#include <float.h>
#include <math.h>
#include <quicksort/quicksort.h>
#include <string.h>
#include <strings.h>

quicksort_data_t
quicksort_poly(quicksort_poly_t poly)
{
	return (quicksort_data_t){
		.poly = poly,
		.sort = quicksort_poly_impl,
		.plots = NULL,
		.plots_size = 0,
	};
}

static inline int
cmp(const void* x, const void* y)
{
	return *(const int*)x - *(const int*)y;
}

typedef struct
{
	int* tmp_buf;
	size_t* cache;
	float coeffs[10];
} poly;

static char*
get_plot_desc(const quicksort_data_t* data, state_t* state, blk_t blk, const char* tag)
{
	char *desc, *values;
	values = xmalloc(sizeof(char) * blk.size * 12);
	char* ptr = values;
	for (size_t i = 0; i < blk.size; ++i)
		ptr += sprintf(ptr, i == 0 ? "%d" : ",%d", blk_value(state, blk.dest, i));
	asprintf(&desc,
	         "poly,%s,max_depth=%zu,neighborhood_radius=%zu,neighborhood_depth=%zu,bruteforce_"
	         "size=%zu,%s,%s",
	         tag,
	         data->poly.max_depth,
	         data->poly.neighborhood_radius,
	         data->poly.neighborhood_depth,
	         data->poly.bruteforce_size,
			 blk_dest_name(blk.dest),
	         values);
	free(values);

	return desc;
}

static inline size_t
cost_function(quicksort_data_t* data,
              state_t* state,
              blk_t blk,
              int p1,
              int p2,
              size_t depth_override)
{
	state_t new = state_clone(state);
	new.search_depth += 1;

	const split_t split = blk_split(&new, blk, p1, p2);
	quicksort_poly_impl(data, &new, split.bot, depth_override);
	quicksort_poly_impl(data, &new, split.mid, depth_override);
	quicksort_poly_impl(data, &new, split.top, depth_override);
	const size_t cost = new.op_count;
	state_destroy(&new);
	return cost;
}

static inline size_t
cost_cached(quicksort_data_t* data,
            state_t* state,
            blk_t blk,
            poly* poly,
            size_t i1,
            size_t i2,
            size_t depth_override)
{
	if (i2 >= blk.size)
		i2 = blk.size - 1;
	if (i1 >= i2)
		i1 = i2;
	const size_t id = i1 + i2 * blk.size;
	if (poly->cache[id] == SIZE_MAX) {
		const int p1 = poly->tmp_buf[i1];
		const int p2 = poly->tmp_buf[i2];
		poly->cache[id] = cost_function(data, state, blk, p1, p2, depth_override);
	}
	return poly->cache[id];
}

static float
sample_smoothed(quicksort_data_t* data,
                state_t* state,
                blk_t blk,
                poly* poly,
                size_t ci1,
                size_t ci2,
                int R,
                size_t depth_override)
{
	// Sampling pattern
	const int pattern[][2] = {
		{ 0, 0 },                    // center
		{ -R, 0 },     { R, 0 },     // horizontal axis
		{ 0, -R },     { 0, R },     // vertical axis
		{ -R, -R },    { R, R },     // main diagonal
		{ -R, R },     { R, -R },    // anti-diagonal
		{ -R / 2, 0 }, { R / 2, 0 }, // half-radius horizontal
		{ 0, -R / 2 }, { 0, R / 2 }, // half-radius vertical
	};
	const int n_pattern = sizeof(pattern) / sizeof(pattern[0]);

	double sum = 0.0;
	int count = 0;
	for (int k = 0; k < n_pattern; ++k) {
		const long ni1 = (long)ci1 + pattern[k][0];
		const long ni2 = (long)ci2 + pattern[k][1];
		if (ni1 < 0 || ni2 < 0)
			continue;
		if ((size_t)ni1 >= blk.size)
			continue;
		if ((size_t)ni2 >= blk.size)
			continue;
		if ((size_t)ni1 > (size_t)ni2)
			continue; // triangular constraint
		const double cost =
		  (double)cost_cached(data, state, blk, poly, (size_t)ni1, (size_t)ni2, depth_override);
		sum += cost;
		++count;
	}
	return count > 0 ? (float)(sum / count) : 0.f;
}

// Build the Vandermonde row for a degree-3 bivariate polynomial
// Basis: [1, u, v, u², uv, v², u³, u²v, uv², v³]  (10 terms)
static inline void
basis(float u, float v, float phi[10])
{
	phi[0] = 1.f;
	phi[1] = u;
	phi[2] = v;
	phi[3] = u * u;
	phi[4] = u * v;
	phi[5] = v * v;
	phi[6] = u * u * u;
	phi[7] = u * u * v;
	phi[8] = u * v * v;
	phi[9] = v * v * v;
}

// Solve the 10×10 normal equations A^T A x = A^T b via Gaussian elimination
// with partial pivoting. Operates in-place on M (augmented matrix [ATA | ATb]).
static inline void
solve_10x10(float M[10][11], float x[10])
{
	const int N = 10;
	for (int col = 0; col < N; ++col) {
		// Partial pivot
		int pivot = col;
		for (int row = col + 1; row < N; ++row)
			if (fabsf(M[row][col]) > fabsf(M[pivot][col]))
				pivot = row;
		// Swap rows
		for (int k = 0; k <= N; ++k) {
			float tmp = M[col][k];
			M[col][k] = M[pivot][k];
			M[pivot][k] = tmp;
		}
		if (fabsf(M[col][col]) < 1e-12f)
			continue; // singular — skip
		// Eliminate
		for (int row = 0; row < N; ++row) {
			if (row == col)
				continue;
			const float factor = M[row][col] / M[col][col];
			for (int k = col; k <= N; ++k)
				M[row][k] -= factor * M[col][k];
		}
	}
	for (int i = 0; i < N; ++i)
		x[i] = M[i][N] / M[i][i];
}

static size_t
triangular_lhs(size_t n_pts, float us[], float vs[], uint64_t* rng)
{
	const int G = (int)ceilf(sqrtf(2.f * (float)n_pts)) + 1;

	// Collect all valid cells then shuffle to pick n_pts of them
	int cells[G * G][2];
	size_t n_cells = 0;
	for (int i = 0; i < G; ++i) {
		for (int j = i; j < G; ++j) // j >= i  →  v >= u
		{
			cells[n_cells][0] = i;
			cells[n_cells][1] = j;
			++n_cells;
		}
	}

	// Suffle to pick first n_pts cells
	const size_t pick = n_pts < n_cells ? n_pts : n_cells;
	for (size_t k = 0; k < pick; ++k) {
		// xorshift64 for cheap randomness
		*rng ^= *rng << 13;
		*rng ^= *rng >> 7;
		*rng ^= *rng << 17;
		const size_t j = k + (size_t)(*rng % (uint64_t)(n_cells - k));
		int tmp0 = cells[k][0], tmp1 = cells[k][1];
		cells[k][0] = cells[j][0];
		cells[k][1] = cells[j][1];
		cells[j][0] = tmp0;
		cells[j][1] = tmp1;
	}

	// Jitter within each chosen cell, enforcing u <= v
	for (size_t k = 0; k < pick; ++k) {
		const float cell_size = 1.f / (float)G;
		*rng ^= *rng << 13;
		*rng ^= *rng >> 7;
		*rng ^= *rng << 17;
		const float ju = (float)(*rng >> 11) * (1.f / (float)(1ULL << 53));
		*rng ^= *rng << 13;
		*rng ^= *rng >> 7;
		*rng ^= *rng << 17;
		const float jv = (float)(*rng >> 11) * (1.f / (float)(1ULL << 53));

		float u = ((float)cells[k][0] + ju) * cell_size;
		float v = ((float)cells[k][1] + jv) * cell_size;
		u = fmaxf(0.f, fminf(u, 1.f));
		v = fmaxf(u, fminf(v, 1.f)); // enforce u <= v
		us[k] = u;
		vs[k] = v;
	}
	return pick;
}

static inline float
poly_eval(const poly* poly, float u, float v)
{
	const float* c = poly->coeffs;
	return c[0] + c[1] * u + c[2] * v + c[3] * u * u + c[4] * u * v + c[5] * v * v +
	       c[6] * u * u * u + c[7] * u * u * v + c[8] * u * v * v + c[9] * v * v * v;
}

poly
build_poly(quicksort_data_t* data, state_t* state, blk_t blk, size_t depth_override, int* tmp_buf)
{
	poly poly = {
		.tmp_buf = tmp_buf,
		.cache = xmalloc(sizeof(size_t) * blk.size * blk.size),
		.coeffs = { 0.f },
	};

	bzero(poly.coeffs, sizeof(float) * 10);
	for (size_t i = 0; i < blk.size * blk.size; ++i)
		poly.cache[i] = SIZE_MAX;

	const size_t n = blk.size;

	// Compute sample points
	float us[30], vs[30];
	uint64_t rng = 0xdeadbeefcafe1234ULL ^ (uint64_t)blk.size ^ ((uint64_t)blk.dest << 7);
	const size_t actual_pts = triangular_lhs(30, us, vs, &rng);

	// Build samples
	const int box_radius = 4;
	float ys[30];

	for (size_t k = 0; k < actual_pts; ++k) {
		// Map normalized [0,1] coords to index space
		const size_t ci1 = (size_t)(us[k] * (float)(n - 1) + 0.5f);
		const size_t ci2 = (size_t)(vs[k] * (float)(n - 1) + 0.5f);
		ys[k] = sample_smoothed(data, state, blk, &poly, ci1, ci2, box_radius, depth_override);
	}

	// Normalize y values for numerical stability (mean=0, std=1)
	float ymean = 0.f;
	for (size_t k = 0; k < actual_pts; ++k)
		ymean += ys[k];
	ymean /= (float)actual_pts;
	float ystd = 0.f;
	for (size_t k = 0; k < actual_pts; ++k)
		ystd += (ys[k] - ymean) * (ys[k] - ymean);
	ystd = sqrtf(ystd / (float)actual_pts);
	if (ystd < 1.f)
		ystd = 1.f;
	for (size_t k = 0; k < actual_pts; ++k)
		ys[k] = (ys[k] - ymean) / ystd;

	// Build matrix [A^T A | A^T b]
	float M[10][11];
	bzero(M, sizeof(M));

	for (size_t k = 0; k < actual_pts; ++k) {
		float phi[10];
		basis(us[k], vs[k], phi);
		for (int i = 0; i < 10; ++i) {
			for (int j = 0; j < 10; ++j)
				M[i][j] += phi[i] * phi[j];
			M[i][10] += phi[i] * ys[k];
		}
	}

	float coeffs_normalized[10];
	solve_10x10(M, coeffs_normalized);

	// Denormalize coefficients back to original cost scale
	// p(u,v) = ymean + ystd * p_normalized(u,v)
	poly.coeffs[0] = ymean + ystd * coeffs_normalized[0];
	for (int i = 1; i < 10; ++i)
		poly.coeffs[i] = ystd * coeffs_normalized[i];

	/*
	if (blk.size == 500) {
	    size_t* plot = xmalloc(sizeof(size_t) * 500 * 500);
	    bzero(plot, sizeof(size_t) * 500 * 500);
	    for (size_t y = 0; y < 500; ++y) {
	        for (size_t x = 0; x < 500; ++x) {
	            if (x > y)
	                continue;


	            plot[x + (500 - y - 1) * 500] = (size_t)poly_eval(&poly, (float)x / 500.f,
	(float)y / 500.f);
	        }
	    }
	    quicksort_data_add_plot(data,
	                            (quicksort_plot_t){
	                              .data = plot,
	                              .desc = strdup("poly"),
	                              .type = PLOT_SIZE,
	                              .size = { 500, 500 },
	                            });
	}

	if (blk.size == 500) {
	    float* plot = xmalloc(sizeof(float) * 500 * 500);
	    bzero(plot, sizeof(float) * 500 * 500);
	    for (size_t y = 0; y < 500; ++y) {
	        for (size_t x = 0; x < 500; ++x) {
	            if (x > y)
	                continue;

	            const float pval = poly_eval(&poly, (float)x / 500.f, (float)y / 500.f);
	            const float rval = (float)cost_cached(data, state, blk, &poly, x, y,
	depth_override); plot[x + (500 - y - 1) * 500] = fabsf(pval - rval) / rval;
	        }
	    }
	    quicksort_data_add_plot(data,
	                            (quicksort_plot_t){
	                              .data = plot,
	                              .desc = strdup("poly_err"),
	                              .type = PLOT_FLOAT,
	                              .size = { 500, 500 },
	                            });
	}
	*/

	return poly;
}

// Find polynomial minimum within domain
void
poly_minimize(const poly* p, const float dom[4], float* u_out, float* v_out)
{
	const int GRID = 200;
	float best = FLT_MAX;
	*u_out = (dom[0] + dom[1]) * 0.5f;
	*v_out = (dom[2] + dom[3]) * 0.5f;

	for (int i = 0; i < GRID; ++i) {
		for (int j = i; j < GRID; ++j) {
			float u = dom[0] + (dom[1] - dom[0]) * (float)i / (GRID - 1);
			float v = dom[2] + (dom[3] - dom[2]) * (float)j / (GRID - 1);
			if (u > v)
				continue; // triangular constraint

			float val = poly_eval(p, u, v);
			if (val < best) {
				best = val;
				*u_out = u;
				*v_out = v;
			}
		}
	}
}

void
scan_neighborhood(quicksort_data_t* data,
                  state_t* state,
                  blk_t blk,
                  poly* poly,
                  size_t* i1,
                  size_t* i2)
{
	size_t best = SIZE_MAX;
	size_t best_pivots[2] = { *i2, *i2 };

	const size_t radius = data->poly.neighborhood_radius;
	const size_t side = radius * 2 + 1;
	size_t i;
#pragma omp parallel for schedule(dynamic) private(i) shared(poly)
	for (i = 0; i < (2 * radius + 1) * (2 * radius + 1); ++i) {
		const int p1 = (int)*i1 - (int)radius + (int)(i % side);
		const int p2 = (int)*i2 - (int)radius + (int)(i / side);
		if (p1 < 0 || p2 < 0 || p2 < p1 || (size_t)p1 >= blk.size || (size_t)p2 >= blk.size)
			continue;

		const size_t cost = cost_cached(
		  data, state, blk, poly, (size_t)p1, (size_t)p2, data->poly.neighborhood_depth);
#pragma omp critical
		if (cost < best) {
			best = cost;
			best_pivots[0] = (size_t)p1;
			best_pivots[1] = (size_t)p2;
		}
	}
	if (blk.size == 500)
		printf("best = %zu\n", best);
	*i1 = best_pivots[0];
	*i2 = best_pivots[1];
}

void
get_pivots(quicksort_data_t* data, state_t* state, blk_t blk, int* pivots, size_t depth_override)
{
	int* tmp_buf = xmalloc(sizeof(int) * blk.size);
	for (size_t i = 0; i < blk.size; ++i)
		tmp_buf[i] = blk_value(state, blk.dest, i);
	qsort(tmp_buf, blk.size, sizeof(int), cmp);

	pivots[0] = tmp_buf[(size_t)(.25f * (float)(blk.size - 1) + .5f)];
	pivots[1] = tmp_buf[(size_t)(.60f * (float)(blk.size - 1) + .5f)];

	// Bruteforce
	if (blk.size < data->poly.bruteforce_size) {
		size_t best = SIZE_MAX;
		size_t i;

		size_t* plot = NULL;
		size_t best_idx[2] = { 0, 0 };
		if (state->search_depth == 0) {
			plot = xmalloc(sizeof(size_t) * blk.size * blk.size);
			bzero(plot, sizeof(size_t) * blk.size * blk.size);
		}
#pragma omp parallel for schedule(dynamic) private(i) shared(data, state)
		for (i = 0; i < blk.size * blk.size; ++i) {
			const int p1 = tmp_buf[i % blk.size];
			const int p2 = tmp_buf[i / blk.size];
			if (p2 <= p1)
				continue;
			const size_t cost = cost_function(data, state, blk, p1, p2, depth_override);
			if (plot)
				plot[i % blk.size + (blk.size - i / blk.size - 1) * blk.size] = cost;
#pragma omp critical
			if (cost < best) {
				best = cost;
				pivots[0] = p1;
				pivots[1] = p2;
				best_idx[0] = i % blk.size;
				best_idx[1] = i / blk.size;
			}
		}
		if (plot) {
			quicksort_plot_t* p =
			  quicksort_data_add_plot(data,
			                          (quicksort_plot_t){
			                            .desc = get_plot_desc(data, state, blk, "bruteforce"),
			                            .data = plot,
			                            .size = { blk.size, blk.size },
			                            .type = PLOT_SIZE,
			                          });
			char* converge;
			asprintf(&converge,
			         "converge,%zu,%zu,%zu",
			         best_idx[0],
			         best_idx[1],
			         best);
			quicksort_plot_add_value(p, converge);
		}
	}
	// Compute poly surrogate & minimize
	else if (state->search_depth <= data->poly.max_depth ||
	         (state->search_depth < depth_override && depth_override != SIZE_MAX)) {
		poly poly = build_poly(data, state, blk, depth_override, tmp_buf);
		const float domain[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
		float u, v;
		poly_minimize(&poly, domain, &u, &v);
		if (blk.size == 500)
			printf("%f %f\n", u, v);
		size_t i1 = (size_t)(u * (float)(blk.size - 1) + .5f);
		size_t i2 = (size_t)(v * (float)(blk.size - 1) + .5f);
		if (blk.size == 500)
			printf("%zu %zu\n", i1, i2);
		scan_neighborhood(data, state, blk, &poly, &i1, &i2);
		if (blk.size == 500)
			printf("%zu %zu\n", i1, i2);
		pivots[0] = tmp_buf[i1];
		pivots[1] = tmp_buf[i2];
		free(poly.cache);
	}

	free(tmp_buf);
}

void
quicksort_poly_impl(quicksort_data_t* data, state_t* state, blk_t blk, size_t depth_override)
{

	if (blk.size == 0)
		return;
	if (blk.size == 500 && state->search_depth == 0) {
		// plot_pivots(data, state, blk, (const int[4]){ 0, 500, 0, 500 });
	}

	// Normalize direction
	if (blk.dest == BLK_A_BOT && state->sa.size == blk.size)
		blk.dest = BLK_A_TOP;
	else if (blk.dest == BLK_B_BOT && state->sb.size == blk.size)
		blk.dest = BLK_B_TOP;

	// Sort manually for small blocks
	if (blk.size == 1) {
		blk_move(state, blk.dest, BLK_A_TOP);
		return;
	} else if (blk.size == 2) {
		blk_sort_2(state, blk);
		return;
	} else if (blk.size == 3) {
		blk_sort_3(state, blk);
		return;
	}

	// Choose pivots & split
	int pivots[2];
	get_pivots(data, state, blk, pivots, depth_override);
	const split_t split = blk_split(state, blk, pivots[0], pivots[1]);
	quicksort_poly_impl(data, state, split.bot, depth_override);
	quicksort_poly_impl(data, state, split.mid, depth_override);
	quicksort_poly_impl(data, state, split.top, depth_override);
}
