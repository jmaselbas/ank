#pragma once
#include <stddef.h>
#include "config.h"

typedef char sym[];

extern sym _text_end;
extern sym _data_start;
extern sym _data_end;
extern sym _bss_start;
extern sym _bss_end;

typedef signed   char          s8;
typedef signed   short int     s16;
typedef signed   int           s32;
typedef signed   long long int s64;
typedef unsigned char          u8;
typedef unsigned short int     u16;
typedef unsigned int           u32;
typedef unsigned long long int u64;

typedef u16 le16;

#define LEN(a) (sizeof(a) / sizeof(*a))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define BIT(x) (1UL << (x))
#define IOMEM(x) ((void *)x)

static inline u32 read32(void *addr)
{
	volatile u32 *p = addr;
	return *p;
}

static inline void write32(void *addr, u32 val)
{
	volatile u32 *p = addr;
	*p = val;
}

static inline u16 read16(void *addr)
{
	volatile u16 *p = addr;
	return *p;
}

static inline void write16(void *addr, u16 val)
{
	volatile u16 *p = addr;
	*p = val;
}

static inline u8 read8(void *addr)
{
	volatile u8 *p = addr;
	return *p;
}

static inline void write8(void *addr, u8 val)
{
	volatile u8 *p = addr;
	*p = val;
}
