#include <stdio.h>
#include <stdlib.h>

void
complain_about_insufficient_memory_and_exit(void)
{
	fputs("Not enough memory!\n", stderr);
	abort();
}

void *
xmalloc(size_t size)
{
	void *p = malloc(size);
	if (p == NULL) {
		complain_about_insufficient_memory_and_exit();
	}
	return p;
}

void *
xrealloc(void *src, size_t size)
{
	void *p = realloc(src, size);
	if (p == NULL) {
		complain_about_insufficient_memory_and_exit();
	}
	return p;
}
