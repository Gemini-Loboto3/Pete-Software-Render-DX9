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

#include "interp.h"

/***************************************************************************/
/* diff */

#define INTERP_Y_LIMIT (0x30 * 4)
#define INTERP_U_LIMIT (0x07 * 4)
#define INTERP_V_LIMIT (0x06 * 8)

int interp_32_diff(u32 p1, u32 p2)
{
	int r, g, b;
	int y, u, v;

	/* assume standard rgb formats */
	if ((p1 & 0xF8F8F8) == (p2 & 0xF8F8F8))
		return 0;

	b = (int)((p1 & 0xFF) - (p2 & 0xFF));
	g = (int)((p1 & 0xFF00) - (p2 & 0xFF00)) >> 8;
	r = (int)((p1 & 0xFF0000) - (p2 & 0xFF0000)) >> 16;

	y = r + g + b;

	if (y < -INTERP_Y_LIMIT || y > INTERP_Y_LIMIT)
		return 1;

	u = r - b;

	if (u < -INTERP_U_LIMIT || u > INTERP_U_LIMIT)
		return 1;

	v = -r + 2*g - b;

	if (v < -INTERP_V_LIMIT || v > INTERP_V_LIMIT)
		return 1;

	return 0;
}
