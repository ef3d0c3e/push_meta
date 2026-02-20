#include <quicksort/quicksort.h>
#include <string.h>

void
quicksort_data_add_plot(quicksort_data_t *data, quicksort_plot_t plot)
{
	data->plots = xrealloc(data->plots, sizeof(plot) * (data->plots_size + 1));
	data->plots[data->plots_size++] = plot;
}

void
quicksort_data_free(quicksort_data_t *data)
{
	for (size_t i = 0; i < data->plots_size; ++i)
	{
		free(data->plots[i].data);
		free(data->plots[i].desc);
	}
	free(data->plots);
}

void
quicksort_write_plots(const quicksort_data_t *data)
{
	for (size_t i = 0; i < data->plots_size; ++i)
	{
		const quicksort_plot_t *plot = &data->plots[i];

		char buf[256];
		sprintf(buf, "plot_%zu.csv", i);

		FILE *file = fopen(buf, "w");
		fwrite(plot->desc, 1, strlen(plot->desc), file);
		fwrite("\n", 1, 1, file);

		for (size_t y = 0; y < plot->size[1]; ++y)
		{
			for (size_t x = 0; x < plot->size[0]; ++x)
			{
				char buf[64];
				switch (plot->type) {
					case PLOT_FLOAT:
						sprintf(buf, "%f", ((float*)plot->data)[x + y * plot->size[0]]);
						break;
					case PLOT_SIZE:
						sprintf(buf, "%zu", ((size_t*)plot->data)[x + y * plot->size[0]]);
						break;
				}
				if (x != 0)
					fwrite(",", 1, 1, file);
				fwrite(buf, 1, strlen(buf), file);
			}
			fwrite("\n", 1, 1, file);
		}

		fclose(file);
		fprintf(stderr, "Plot `%s' written.\n", buf);
	}
}
