
// Interpolation header for PointSize

#ifndef __INTERP_H
#define __INTERP_H

/***************************************************************************/
/* Basic types */

#include "tdef.h"

/***************************************************************************/
/* interpolation */

/**
 * Select which method to use for computing pixels.
 */
/* #define USE_INTERP_MASK_1 */
/* #define USE_INTERP_MASK_2 */
#define USE_INTERP_MASK_3

#define INTERP_32_MASK_1(v) ((v) & 0xFF00FFU)
#define INTERP_32_MASK_2(v) ((v) & 0x00FF00U)
#define INTERP_32_UNMASK_1(v) ((v) & 0xFF00FFU)
#define INTERP_32_UNMASK_2(v) ((v) & 0x00FF00U)
#define INTERP_32_HNMASK (~0x808080U)

#define INTERP_32_GEN2(a, b) \
static inline u32 interp_32_##a##b(u32 p1, u32 p2) \
{ \
	return INTERP_32_UNMASK_1((INTERP_32_MASK_1(p1)*a + INTERP_32_MASK_1(p2)*b) / 16) \
		| INTERP_32_UNMASK_2((INTERP_32_MASK_2(p1)*a + INTERP_32_MASK_2(p2)*b) / 16); \
}

#define INTERP_32_GEN3(a, b, c) \
static inline u32 interp_32_##a##b##c(u32 p1, u32 p2, u32 p3) \
{ \
	return INTERP_32_UNMASK_1((INTERP_32_MASK_1(p1)*a + INTERP_32_MASK_1(p2)*b + INTERP_32_MASK_1(p3)*c) / 16) \
		| INTERP_32_UNMASK_2((INTERP_32_MASK_2(p1)*a + INTERP_32_MASK_2(p2)*b + INTERP_32_MASK_2(p3)*c) / 16); \
}

static inline u32 interp_32_11(u32 p1, u32 p2)
{
#ifdef USE_INTERP_MASK_1
	return INTERP_32_UNMASK_1((INTERP_32_MASK_1(p1) + INTERP_32_MASK_1(p2)) / 2)
		| INTERP_32_UNMASK_2((INTERP_32_MASK_2(p1) + INTERP_32_MASK_2(p2)) / 2);
#else
	/*
	 * This function compute (a + b) / 2 for any rgb nibble, using the
	 * the formula (a + b) / 2 = ((a ^ b) >> 1) + (a & b).
	 * To extend this formula to a serie of packed nibbles the formula is
	 * implemented as (((v0 ^ v1) >> 1) & MASK) + (v0 & v1) where MASK
	 * is used to clear the high bit of all the packed nibbles.
	 */
	return (((p1 ^ p2) >> 1) & INTERP_32_HNMASK) + (p1 & p2);
#endif
}

static inline u32 interp_32_211(u32 p1, u32 p2, u32 p3)
{
#ifdef USE_INTERP_MASK_2
	return INTERP_32_UNMASK_1((INTERP_32_MASK_1(p1)*2 + INTERP_32_MASK_1(p2) + INTERP_32_MASK_1(p3)) / 4)
		| INTERP_32_UNMASK_2((INTERP_32_MASK_2(p1)*2 + INTERP_32_MASK_2(p2) + INTERP_32_MASK_2(p3)) / 4);
#else
	return interp_32_11(p1, interp_32_11(p2, p3));
#endif
}

static inline u32 interp_32_31(u32 p1, u32 p2)
{
#ifdef USE_INTERP_MASK_2
	return INTERP_32_UNMASK_1((INTERP_32_MASK_1(p1)*3 + INTERP_32_MASK_1(p2)) / 4)
		| INTERP_32_UNMASK_2((INTERP_32_MASK_2(p1)*3 + INTERP_32_MASK_2(p2)) / 4);
#else
	return interp_32_11(p1, interp_32_11(p1, p2));
#endif
}

static inline u32 interp_32_521(u32 p1, u32 p2, u32 p3)
{
#ifdef USE_INTERP_MASK_3
	return INTERP_32_UNMASK_1((INTERP_32_MASK_1(p1)*5 + INTERP_32_MASK_1(p2)*2 + INTERP_32_MASK_1(p3)) / 8)
		| INTERP_32_UNMASK_2((INTERP_32_MASK_2(p1)*5 + INTERP_32_MASK_2(p2)*2 + INTERP_32_MASK_2(p3)) / 8);
#else
	return interp_32_11(p1, interp_32_11(p2, interp_32_11(p1, p3)));
#endif
}

static inline u32 interp_32_431(u32 p1, u32 p2, u32 p3)
{
#ifdef USE_INTERP_MASK_3
	return INTERP_32_UNMASK_1((INTERP_32_MASK_1(p1)*4 + INTERP_32_MASK_1(p2)*3 + INTERP_32_MASK_1(p3)) / 8)
		| INTERP_32_UNMASK_2((INTERP_32_MASK_2(p1)*4 + INTERP_32_MASK_2(p2)*3 + INTERP_32_MASK_2(p3)) / 8);
#else
	return interp_32_11(p1, interp_32_11(p2, interp_32_11(p2, p3)));
#endif
}

static inline u32 interp_32_53(u32 p1, u32 p2)
{
#ifdef USE_INTERP_MASK_3
	return INTERP_32_UNMASK_1((INTERP_32_MASK_1(p1)*5 + INTERP_32_MASK_1(p2)*3) / 8)
		| INTERP_32_UNMASK_2((INTERP_32_MASK_2(p1)*5 + INTERP_32_MASK_2(p2)*3) / 8);
#else
	return interp_32_11(p1, interp_32_11(p2, interp_32_11(p1, p2)));
#endif
}

static inline u32 interp_32_332(u32 p1, u32 p2, u32 p3)
{
#ifdef USE_INTERP_MASK_3
	return INTERP_32_UNMASK_1((INTERP_32_MASK_1(p1)*3 + INTERP_32_MASK_1(p2)*3 + INTERP_32_MASK_1(p3)*2) / 8)
		| INTERP_32_UNMASK_2((INTERP_32_MASK_2(p1)*3 + INTERP_32_MASK_2(p2)*3 + INTERP_32_MASK_2(p3)*2) / 8);
#else
	u32 t = interp_32_11(p1, p2);
	return interp_32_11(t, interp_32_11(p3, t));
#endif
}

static inline u32 interp_32_611(u32 p1, u32 p2, u32 p3)
{
#ifdef USE_INTERP_MASK_3
	return INTERP_32_UNMASK_1((INTERP_32_MASK_1(p1)*6 + INTERP_32_MASK_1(p2) + INTERP_32_MASK_1(p3)) / 8)
		| INTERP_32_UNMASK_2((INTERP_32_MASK_2(p1)*6 + INTERP_32_MASK_2(p2) + INTERP_32_MASK_2(p3)) / 8);
#else
	return interp_32_11(p1, interp_32_11(p1, interp_32_11(p2, p3)));
#endif
}

static inline u32 interp_32_71(u32 p1, u32 p2)
{
#ifdef USE_INTERP_MASK_3
	return INTERP_32_UNMASK_1((INTERP_32_MASK_1(p1)*7 + INTERP_32_MASK_1(p2)) / 8)
		| INTERP_32_UNMASK_2((INTERP_32_MASK_2(p1)*7 + INTERP_32_MASK_2(p2)) / 8);
#else
	return interp_32_11(p1, interp_32_11(p1, interp_32_11(p1, p2)));
#endif
}

INTERP_32_GEN3(6, 5, 5)
INTERP_32_GEN3(7, 5, 4)
INTERP_32_GEN3(7, 6, 3)
INTERP_32_GEN3(7, 7, 2)
INTERP_32_GEN3(8, 5, 3)
INTERP_32_GEN3(9, 4, 3)
INTERP_32_GEN3(9, 6, 1)
INTERP_32_GEN3(10, 3, 3)
INTERP_32_GEN3(10, 5, 1)
INTERP_32_GEN3(11, 3, 2)
INTERP_32_GEN2(11, 5)
INTERP_32_GEN3(12, 3, 1)
INTERP_32_GEN2(13, 3)
INTERP_32_GEN3(14, 1, 1)
INTERP_32_GEN2(15, 1)
INTERP_32_GEN2(9, 7)

int interp_32_diff(u32 p1, u32 p2);

#endif
