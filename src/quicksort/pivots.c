#include <math.h>
#include <quicksort/quicksort.h>

static inline int
cmp(const void* x, const void* y)
{
	return *(const int*)x - *(const int*)y;
}

static inline size_t
evaluate_pivots(const quicksort_config_t* cfg, const state_t* state, blk_t blk, int p1, int p2)
{
	state_t new = state_clone(state);

	new.search_depth += 1;

	// Split & Evaluate
	const split_t split = blk_split(&new, blk, p1, p2);
	quicksort_impl(cfg, &new, split.bot);
	quicksort_impl(cfg, &new, split.mid);
	quicksort_impl(cfg, &new, split.top);

	const size_t cost = new.op_count;
	state_destroy(&new);
	return cost;
}

/* Map (u, v) to a valid f1 < f2 pair */
static inline void
uv_to_f(float u, float v, float* out_f1, float* out_f2)
{
	*out_f1 = fmaxf(0.f, u);
	*out_f2 = fminf(1.f, u + (1.f - u) * v);
}

/* Map pivot floating point value to a size_t in the blk_t range */
static inline size_t
f_to_index(float f, size_t n)
{
	if (n == 0)
		return 0;
	const float scaled = f * (float)(n - 1);
	size_t idx = (size_t)fmaxf(0.f, floorf(scaled + 0.5f));
	if (idx >= n)
		idx = n - 1;
	return idx;
}

/* Evaluate pivot index pair (i1, i2) */
static size_t
evaluate_index_cached(const quicksort_config_t* cfg,
                      const state_t* state,
                      blk_t blk,
                      const int* tmp_buf,
                      size_t i1,
                      size_t i2,
                      size_t* cache,
                      size_t n,
                      size_t best_cost)
{
	assert(i1 < n && i2 < n && i1 <= i2);

	if (state->op_count >= best_cost)
		return SIZE_MAX;
	const size_t key = i1 * n + i2;
	if (cache[key] != SIZE_MAX)
		return cache[key];

	const int p1 = tmp_buf[i1];
	const int p2 = tmp_buf[i2];
	cache[key] = evaluate_pivots(cfg, state, blk, p1, p2);
	return cache[key];
}

/* Compute maximum distance of two edges of the simplex */
static inline float
simplex_diameter(const float simplex[3][2])
{
	float max = 0.f;
	for (size_t i = 0; i < 3; ++i) {
		for (size_t j = i + 1; j < 3; ++j) {
			const float d =
			  fmaxf(fabsf(simplex[i][0] - simplex[j][0]), fabsf(simplex[i][1] - simplex[j][1]));
			if (d > max)
				max = d;
		}
	}
	return max;
}

static inline size_t
best_cost(const size_t fvals[3])
{
	size_t best = SIZE_MAX;
	for (size_t i = 0; i < 3; ++i) {
		if (fvals[i] < best)
			best = fvals[i];
	}
	return best;
}

void
optimize_pivots(const quicksort_config_t* cfg,
                const state_t* state,
                blk_t blk,
                const int* tmp_buf,
                float* out_f1,
                float* out_f2)
{
	assert(cfg);
	/* quick exits for degenerate sizes */
	const size_t n = blk.size;
	if (n == 0) {
		*out_f1 = *out_f2 = 0.0f;
		return;
	}
	if (n == 1) {
		*out_f1 = *out_f2 = 0.0f;
		return;
	}

	// Initialize cache
	size_t* cache = NULL;
	cache = xmalloc(sizeof(size_t) * n * n);
	for (size_t i = 0; i < n * n; ++i)
		cache[i] = SIZE_MAX;

	float base_u = 0.33f;
	float base_v = 0.5f;

	float simplex[3][2];
	simplex[0][0] = base_u;
	simplex[0][1] = base_v;
	simplex[1][0] = fminf(1.0f, base_u + cfg->nm_initial_scale);
	simplex[1][1] = base_v;
	simplex[2][0] = base_u;
	simplex[2][1] = fminf(1.0f, base_v + cfg->nm_initial_scale);

	/* Nelder-Mead coefficients */
	const float alpha = 1.0f; /* reflection */
	const float gamma = 2.0f; /* expansion */
	const float rho = 0.5f;   /* contraction */
	const float sigma = 0.5f; /* shrink */

	/* function values */
	size_t fvals[3] = { SIZE_MAX, SIZE_MAX, SIZE_MAX };
	for (size_t i = 0; i < 3; ++i) {
		float f1, f2;
		uv_to_f(simplex[i][0], simplex[i][1], &f1, &f2);
		size_t idx1 = f_to_index(f1, n);
		size_t idx2 = f_to_index(f2, n);
		if (idx2 < idx1)
			idx2 = idx1; /* safety */
		fvals[i] =
		  evaluate_index_cached(cfg, state, blk, tmp_buf, idx1, idx2, cache, n, best_cost(fvals));
	}

	/* Main NM loop */
	for (unsigned iter = 0; iter < cfg->nm_max_iters; ++iter) {
		/* sort simplex vertices by fvals ascending (0 = best) */
		for (int a = 0; a < 2; ++a) {
			int best = a;
			for (int b = a + 1; b < 3; ++b)
				if (fvals[b] < fvals[best])
					best = b;
			if (best != a) {
				/* swap fvals */
				const size_t tmpf = fvals[a];
				fvals[a] = fvals[best];
				fvals[best] = tmpf;
				/* swap points */
				const float tmpu = simplex[a][0];
				const float tmpv = simplex[a][1];
				simplex[a][0] = simplex[best][0];
				simplex[a][1] = simplex[best][1];
				simplex[best][0] = tmpu;
				simplex[best][1] = tmpv;
			}
		}
		const size_t best = best_cost(fvals);

		/* Stop on convergence */
		if (simplex_diameter(simplex) < cfg->nm_tol)
			break;

		/* centroid of best two (exclude worst at index 2) */
		const float centroid[2] = { 0.5f * (simplex[0][0] + simplex[1][0]),
			                        0.5f * (simplex[0][1] + simplex[1][1]) };

		/* reflection: xr = centroid + alpha*(centroid - x_worst) */
		float xr[2] = { centroid[0] + alpha * (centroid[0] - simplex[2][0]),
			            centroid[1] + alpha * (centroid[1] - simplex[2][1]) };
		/* clamp xr to [0,1] */
		xr[0] = xr[0] < 0.0f ? 0.0f : (xr[0] > 1.0f ? 1.0f : xr[0]);
		xr[1] = xr[1] < 0.0f ? 0.0f : (xr[1] > 1.0f ? 1.0f : xr[1]);

		float xr_f1, xr_f2;
		uv_to_f(xr[0], xr[1], &xr_f1, &xr_f2);
		size_t xr_i1 = f_to_index(xr_f1, n);
		size_t xr_i2 = f_to_index(xr_f2, n);
		if (xr_i2 < xr_i1)
			xr_i2 = xr_i1;

		const size_t fr = evaluate_index_cached(
		  cfg, state, blk, tmp_buf, xr_i1, xr_i2, cache, n, best);
		// Expansion
		if (fr < fvals[0]) {
			float xe[2] = { centroid[0] + gamma * (xr[0] - centroid[0]),
				            centroid[1] + gamma * (xr[1] - centroid[1]) };
			xe[0] = xe[0] < 0.0f ? 0.0f : (xe[0] > 1.0f ? 1.0f : xe[0]);
			xe[1] = xe[1] < 0.0f ? 0.0f : (xe[1] > 1.0f ? 1.0f : xe[1]);
			float xe_f1, xe_f2;
			uv_to_f(xe[0], xe[1], &xe_f1, &xe_f2);
			size_t xe_i1 = f_to_index(xe_f1, n);
			size_t xe_i2 = f_to_index(xe_f2, n);
			if (xe_i2 < xe_i1)
				xe_i2 = xe_i1;
			size_t fe = evaluate_index_cached(
			  cfg, state, blk, tmp_buf, xe_i1, xe_i2, cache, n, best);

			if (fe < fr) {
				/* accept expansion */
				simplex[2][0] = xe[0];
				simplex[2][1] = xe[1];
				fvals[2] = fe;
			} else {
				/* accept reflection */
				simplex[2][0] = xr[0];
				simplex[2][1] = xr[1];
				fvals[2] = fr;
			}
		}
		// Reflection
		else if (fr < fvals[1]) {
			/* accept reflection (better than 2nd but not best) */
			simplex[2][0] = xr[0];
			simplex[2][1] = xr[1];
			fvals[2] = fr;
		}
		// Contraction
		else {
			// Between centroid and xr, outside contraction
			if (fr < fvals[2]) {
				float xc[2] = { centroid[0] + rho * (xr[0] - centroid[0]),
					            centroid[1] + rho * (xr[1] - centroid[1]) };
				xc[0] = xc[0] < 0.0f ? 0.0f : (xc[0] > 1.0f ? 1.0f : xc[0]);
				xc[1] = xc[1] < 0.0f ? 0.0f : (xc[1] > 1.0f ? 1.0f : xc[1]);
				float xc_f1, xc_f2;
				uv_to_f(xc[0], xc[1], &xc_f1, &xc_f2);
				const size_t xc_i1 = f_to_index(xc_f1, n);
				size_t xc_i2 = f_to_index(xc_f2, n);
				if (xc_i2 < xc_i1)
					xc_i2 = xc_i1;
				const size_t fc = evaluate_index_cached(
				  cfg, state, blk, tmp_buf, xc_i1, xc_i2, cache, n, best);
				if (fc <= fr) {
					simplex[2][0] = xc[0];
					simplex[2][1] = xc[1];
					fvals[2] = fc;
				}
				// Shrink
				else {
					for (size_t i = 1; i < 3; ++i) {
						simplex[i][0] = simplex[0][0] + sigma * (simplex[i][0] - simplex[0][0]);
						simplex[i][1] = simplex[0][1] + sigma * (simplex[i][1] - simplex[0][1]);
						simplex[i][0] = simplex[i][0] < 0.0f
						                  ? 0.0f
						                  : (simplex[i][0] > 1.0f ? 1.0f : simplex[i][0]);
						simplex[i][1] = simplex[i][1] < 0.0f
						                  ? 0.0f
						                  : (simplex[i][1] > 1.0f ? 1.0f : simplex[i][1]);
						float fi1, fi2;
						uv_to_f(simplex[i][0], simplex[i][1], &fi1, &fi2);
						const size_t idi1 = f_to_index(fi1, n);
						size_t idi2 = f_to_index(fi2, n);
						if (idi2 < idi1)
							idi2 = idi1;
						fvals[i] = evaluate_index_cached(
						  cfg, state, blk, tmp_buf, idi1, idi2, cache, n, best);
					}
				}
			}
			// Between centroin and worst, inside contraction
			else {
				float xc[2] = { centroid[0] + rho * (simplex[2][0] - centroid[0]),
					            centroid[1] + rho * (simplex[2][1] - centroid[1]) };
				xc[0] = xc[0] < 0.0f ? 0.0f : (xc[0] > 1.0f ? 1.0f : xc[0]);
				xc[1] = xc[1] < 0.0f ? 0.0f : (xc[1] > 1.0f ? 1.0f : xc[1]);
				float xc_f1, xc_f2;
				uv_to_f(xc[0], xc[1], &xc_f1, &xc_f2);
				const size_t xc_i1 = f_to_index(xc_f1, n);
				size_t xc_i2 = f_to_index(xc_f2, n);
				if (xc_i2 < xc_i1)
					xc_i2 = xc_i1;
				const size_t fc = evaluate_index_cached(
				  cfg, state, blk, tmp_buf, xc_i1, xc_i2, cache, n, best);
				if (fc < fvals[2]) {
					simplex[2][0] = xc[0];
					simplex[2][1] = xc[1];
					fvals[2] = fc;
				}
				// Shrink
				else {
					for (size_t i = 1; i < 3; ++i) {
						simplex[i][0] = simplex[0][0] + sigma * (simplex[i][0] - simplex[0][0]);
						simplex[i][1] = simplex[0][1] + sigma * (simplex[i][1] - simplex[0][1]);
						simplex[i][0] = simplex[i][0] < 0.0f
						                  ? 0.0f
						                  : (simplex[i][0] > 1.0f ? 1.0f : simplex[i][0]);
						simplex[i][1] = simplex[i][1] < 0.0f
						                  ? 0.0f
						                  : (simplex[i][1] > 1.0f ? 1.0f : simplex[i][1]);
						float fi1, fi2;
						uv_to_f(simplex[i][0], simplex[i][1], &fi1, &fi2);
						const size_t idi1 = f_to_index(fi1, n);
						size_t idi2 = f_to_index(fi2, n);
						if (idi2 < idi1)
							idi2 = idi1;
						fvals[i] = evaluate_index_cached(
						  cfg, state, blk, tmp_buf, idi1, idi2, cache, n, best);
					}
				}
			}
		}
	}

	// Choose best point
	size_t best_idx = 0;
	for (size_t i = 1; i < 3; ++i)
		if (fvals[i] < fvals[best_idx])
			best_idx = i;
	const float best_u = simplex[best_idx][0];
	const float best_v = simplex[best_idx][1];
	float best_f1, best_f2;
	uv_to_f(best_u, best_v, &best_f1, &best_f2);
	const size_t best_i1 = f_to_index(best_f1, n);
	size_t best_i2 = f_to_index(best_f2, n);
	if (best_i2 < best_i1)
		best_i2 = best_i1;

	// Scan best point's neighborhood
	size_t final_i1 = best_i1;
	size_t final_i2 = best_i2;
	const int radius = (int)cfg->nm_final_radius;
	if (radius != 0) {
		size_t best = evaluate_index_cached(
		  cfg, state, blk, tmp_buf, best_i1, best_i2, cache, n, fvals[best_idx]);
		for (int di1 = -radius; di1 <= radius; ++di1) {
			for (int di2 = -radius; di2 <= radius; ++di2) {
				if ((size_t)-di1 > best_i1 || (size_t)-di2 > best_i2)
					continue;
				const size_t ni1 = (size_t)((int)best_i1 + di1);
				const size_t ni2 = (size_t)((int)best_i2 + di2);
				if (ni1 >= n || ni2 >= n || ni2 < ni1)
					continue;
				const size_t c = evaluate_index_cached(
				  cfg, state, blk, tmp_buf, ni1, ni2, cache, n, best);
				if (c < best) {
					best = c;
					final_i1 = ni1;
					final_i2 = ni2;
				}
			}
		}
	}

	*out_f1 = fmaxf(0.f, (float)final_i1 / (float)(n - 1));
	*out_f2 = fminf(1.f, (float)final_i2 / (float)(n - 1));
	free(cache);
}

void
quicksort_pivots(const quicksort_config_t* cfg, const state_t* state, blk_t blk, int* pivots)
{
	int* tmp_buf = xmalloc(sizeof(int) * blk.size);
	for (size_t i = 0; i < blk.size; ++i)
		tmp_buf[i] = blk_value(state, blk.dest, i);
	qsort(tmp_buf, blk.size, sizeof(int), cmp);

	// Use (20%, 80%) as pivots
	if (state->search_depth > cfg->search_depth) {
		pivots[0] = tmp_buf[(20 * blk.size) / 100];
		pivots[1] = tmp_buf[(80 * blk.size) / 100];
	} else {
		float f1, f2;
		optimize_pivots(cfg, state, blk, tmp_buf, &f1, &f2);
		assert(f1 <= f2);
		pivots[0] = tmp_buf[(size_t)(f1 * (float)(blk.size - 1) + .5f)];
		pivots[1] = tmp_buf[(size_t)(f2 * (float)(blk.size - 1) + .5f)];
	}
	free(tmp_buf);
}
