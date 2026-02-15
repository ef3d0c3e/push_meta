#include "util.h"

void*
xmalloc(size_t size)
{
	if (!size)
		size = 1;
	void* const ptr = malloc(size);
	if (!ptr) {
		abort();
	}
	return ptr;
}
