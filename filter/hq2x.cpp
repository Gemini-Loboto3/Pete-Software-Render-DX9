/*
PointSize - Collection of resizers for AviSynth
Copyright (C) 2016 `Orum @ forum.doom9.org

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "hq2x.h"

/***************************************************************************/
/* HQ2x C implementation */

void hq2x_32_def(u32* dst0, u32* dst1, const u32* src0, const u32* src1, const u32* src2, unsigned count)
{
	unsigned i;

	for(i=0;i<count;++i) {
		unsigned char mask;

		u32 c[9];

		c[1] = src0[0];
		c[4] = src1[0];
		c[7] = src2[0];

		if (i>0) {
			c[0] = src0[-1];
			c[3] = src1[-1];
			c[6] = src2[-1];
		} else {
			c[0] = c[1];
			c[3] = c[4];
			c[6] = c[7];
		}

		if (i<count-1) {
			c[2] = src0[1];
			c[5] = src1[1];
			c[8] = src2[1];
		} else {
			c[2] = c[1];
			c[5] = c[4];
			c[8] = c[7];
		}

		mask = 0;

		if (interp_32_diff(c[0], c[4]))
			mask |= 1 << 0;
		if (interp_32_diff(c[1], c[4]))
			mask |= 1 << 1;
		if (interp_32_diff(c[2], c[4]))
			mask |= 1 << 2;
		if (interp_32_diff(c[3], c[4]))
			mask |= 1 << 3;
		if (interp_32_diff(c[5], c[4]))
			mask |= 1 << 4;
		if (interp_32_diff(c[6], c[4]))
			mask |= 1 << 5;
		if (interp_32_diff(c[7], c[4]))
			mask |= 1 << 6;
		if (interp_32_diff(c[8], c[4]))
			mask |= 1 << 7;

#define P(a, b) dst##b[a]
#define MUR interp_32_diff(c[1], c[5])
#define MDR interp_32_diff(c[5], c[7])
#define MDL interp_32_diff(c[7], c[3])
#define MUL interp_32_diff(c[3], c[1])
#define I1(p0) c[p0]
#define I2(i0, i1, p0, p1) interp_32_##i0##i1(c[p0], c[p1])
#define I3(i0, i1, i2, p0, p1, p2) interp_32_##i0##i1##i2(c[p0], c[p1], c[p2])

		switch (mask) {
		#include "hq2x.dat"
		}

#undef P
#undef MUR
#undef MDR
#undef MDL
#undef MUL
#undef I1
#undef I2
#undef I3

		src0 += 1;
		src1 += 1;
		src2 += 1;
		dst0 += 2;
		dst1 += 2;
	}
}

void hq2x32(u8 *srcPtr, u32 srcPitch, u8 *dstPtr, u32 dstPitch, int width, int height)
{
	if(height < 2) return;

	u32 srcPitch32 = (srcPitch >> 2);
	u32 dstPitch32 = (dstPitch >> 2);

	u32 *dst0 = (u32*)dstPtr;
	u32 *dst1 = dst0 + dstPitch32;

	u32 *src0 = (u32*)srcPtr;
	u32 *src1 = src0 + srcPitch32;
	u32 *src2 = src1 + srcPitch32;

	hq2x_32_def(dst0, dst1, src0, src0, src1, width);

	for(int count = height - 2;count > 0;count--) {
		dst0 = dst1 + dstPitch32;
		dst1 = dst0 + dstPitch32;

		hq2x_32_def(dst0, dst1, src0, src1, src2, width);

		src0 = src1;
		src1 = src2;
		src2 += srcPitch32;
	}

	dst0 = dst1 + dstPitch32;
	dst1 = dst0 + dstPitch32;

	hq2x_32_def(dst0, dst1, src0, src1, src1, width);
}
