#include <stdio.h>
#include <stdlib.h>

void *
xmalloc(size_t size)
{
	void *p = malloc(size);
	if (p == NULL) {
		fprintf(stderr, "Not enough memory!\n");
		abort();
	}
	return p;
}
