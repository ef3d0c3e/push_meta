#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <assert.h>

void*
xmalloc(size_t size);
void*
xrealloc(void *ptr, size_t size);

#endif // UTIL_H
