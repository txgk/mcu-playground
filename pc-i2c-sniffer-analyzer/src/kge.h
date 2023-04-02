#ifndef KGE_H
#define KGE_H
#include <stdlib.h>
#include <string.h>

#define MAX(A, B) (((A) > (B)) ? (A) : (B))
#define MIN(A, B) (((A) < (B)) ? (A) : (B))
#define LEN(A) ((sizeof((A)))/(sizeof(*(A))))
#define ISDIGIT(A) (((A)=='0')||((A)=='1')||((A)=='2')||((A)=='3')||((A)=='4')||((A)=='5')||((A)=='6')||((A)=='7')||((A)=='8')||((A)=='9'))

struct str {
	char *ptr;
	size_t len;
	size_t lim;
};

typedef struct str *str;

void *xmalloc(size_t s);
void *xrealloc(void *d, size_t s);

str str_big(size_t desired_size); // Generate string big enough for desired_size characters.
str str_gen(const char *src_ptr, size_t src_len); // Generate string from characters array.
void str_set(str s, const char *src_ptr, size_t src_len); // Set new characters for string.
void str_cat(str s, const char *src_ptr, size_t src_len); // Append characters to string.
void str_add(str s, char c); // Append character to string.
void str_nul(str s); // Empty string.
void str_free(str s); // Free string.
#endif // KGE_H

#ifdef KGE_IMPL
void *xmalloc(size_t s) {
	void *p = malloc(s);
	if (p == NULL) abort();
	return p;
}

void *xrealloc(void *d, size_t s) {
	void *p = realloc(d, s);
	if (p == NULL) abort();
	return p;
}

str str_big(size_t desired_size) {
	str s = xmalloc(sizeof(struct str));
	s->ptr = xmalloc(sizeof(char) * (desired_size + 1));
	s->ptr[0] = '\0';
	s->len = 0;
	s->lim = desired_size;
	return s;
}

str str_gen(const char *src_ptr, size_t src_len) {
	str s = xmalloc(sizeof(struct str));
	s->ptr = xmalloc(sizeof(char) * (src_len + 1));
	memcpy(s->ptr, src_ptr, sizeof(char) * src_len);
	s->ptr[src_len] = '\0';
	s->len = src_len;
	s->lim = src_len;
	return s;
}

void str_set(str s, const char *src_ptr, size_t src_len) {
	if (src_len > s->lim) {
		s->lim = src_len * 2 + 10;
		s->ptr = xrealloc(s->ptr, sizeof(char) * s->lim);
	}
	memcpy(s->ptr, src_ptr, sizeof(char) * src_len);
	s->ptr[src_len] = '\0';
	s->len = src_len;
}

void str_cat(str s, const char *src_ptr, size_t src_len) {
	if ((s->len + src_len) >= s->lim) {
		s->lim = (s->len + src_len) * 2 + 10;
		s->ptr = xrealloc(s->ptr, sizeof(char) * s->lim);
	}
	memcpy(s->ptr + s->len, src_ptr, sizeof(char) * src_len);
	s->len += src_len;
	s->ptr[s->len] = '\0';
}

void str_add(str s, char c) {
	if (s->len == s->lim) {
		s->lim = s->len * 2 + 10;
		s->ptr = xrealloc(s->ptr, sizeof(char) * s->lim);
	}
	s->ptr[s->len] = c;
	s->ptr[s->len + 1] = '\0';
	s->len += 1;
}

void str_nul(str s) {
	s->ptr[0] = '\0';
	s->len = 0;
}

void str_free(str s) {
	if (s != NULL) {
		free(s->ptr);
		free(s);
	}
}
#endif // KGE_IMPL
