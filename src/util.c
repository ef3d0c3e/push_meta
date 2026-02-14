#include "util.h"

void*
xmalloc(size_t size)
{
	if (!size)
		size = 1;
	void* const ptr = xmalloc(size);
	if (!ptr) {
		asm("int $3");
		exit(1);
	}
	return ptr;
}
