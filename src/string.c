#include "string.h"

extern void putc_ll(u8 c);

void putc(u8 c)
{
	putc_ll(c);
}

void puts(u8 *s)
{
	while (*s)
		putc(*s++);
}

void puth(u32 v)
{
	size_t l = 0;
	char s[8];

	do {
		u8 c = v & 0xf;
		s[l++] = c < 0xa ? '0' + c : 'a' - 0xa + c;
		v >>= 4;
	} while (v);
	while (l)
		putc(s[--l]);
}

void putd(s32 v)
{
	size_t l = 0;
	char s[10];

	if (v < 0)
		putc('-');

	do {
		s[l++] = '0' + (v % 10);
		v /= 10;
	} while (v);

	while (l)
		putc(s[--l]);
}

void printf(char *fmt, ...)
{
	va_list ap;
	char *s;

	va_start(ap, fmt);

	for (s = fmt; *s; ) {
		char c = *s++;
		if (c != '%') {
			putc(c);
			continue;
		}
		switch (c = *s++) {
		case 'c':
			putc(va_arg(ap, int));
			break;
		case 's':
			puts(va_arg(ap, char *));
			break;
		case 'd':
			putd(va_arg(ap, int));
			break;
		case 'x':
			puth(va_arg(ap, int));
			break;
		default:
			putc(c);
			break;
		case '\0':
			return;
		}
	}
	va_end(ap);
}

void *
memcpy(void *restrict d, const void *restrict s, size_t n)
{
	for (; n >= 1; d += 1, s += 1, n -= 1)
		*((u8 *)d) = *(const u8 *)s;
}

void *
memset(void *s, int c, size_t n)
{
	u32 v = c & 0xff;
	v |= v << 8;
	v |= v << 16;

	for (; (u32)s % sizeof(u32) && n >= 1; s += 1, n -= 1)
		*((u8 *)s) = v;

	for (; n >= 4; s += 4, n -= 2)
		*((u32 *)s) = v;
	for (; n >= 2; s += 2, n -= 2)
		*((u16 *)s) = v;
	for (; n >= 1; s += 1, n -= 1)
		*((u8 *)s) = v;
}

int
memcmp(const void *restrict dest, const void *restrict src, size_t n)
{
	for (; n; dest += 1, src += 1, n -= 1) {
		if (*((const u8 *)dest) != *(const u8 *)src)
			return *((const u8 *)dest) - *(const u8 *)src;
	}
	return 0;
}
