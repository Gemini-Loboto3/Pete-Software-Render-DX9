
// HQ2x header for PointSize

#ifndef __HQ2X_H
#define __HQ2X_H

#include "interp.h"

void hq2x_32_def(u32* dst0, u32* dst1, const u32* src0, const u32* src1, const u32* src2, unsigned count);
void hq2x32(u8 *srcPtr, u32 srcPitch, u8 *dstPtr, u32 dstPitch, int width, int height);

#endif
