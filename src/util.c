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

void
*xrealloc(void *ptr, size_t size)
{
	void *const new = realloc(ptr, size);
	if (!new && size)
		abort();
	return new;
}
