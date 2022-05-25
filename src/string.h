#pragma once
#include <stdarg.h>
#include "common.h"

void putc(u8 c);
void puts(u8 *s);
void puth(u32 v);
void putd(s32 v);
void printf(char *fmt, ...);

void *memcpy(void *restrict d, const void *restrict s, size_t n);
void *memset(void *s, int c, size_t n);
int memcmp(const void *restrict dest, const void *restrict src, size_t n);
