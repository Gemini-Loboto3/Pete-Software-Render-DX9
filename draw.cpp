/***************************************************************************
						  draw.c  -  description
							 -------------------
	begin                : Sun Oct 28 2001
	copyright            : (C) 2001 by Pete Bernert
	email                : BlackDove@addcom.de
 ***************************************************************************/

 /***************************************************************************
  *                                                                         *
  *   This program is free software; you can redistribute it and/or modify  *
  *   it under the terms of the GNU General Public License as published by  *
  *   the Free Software Foundation; either version 2 of the License, or     *
  *   (at your option) any later version. See also the license.txt file for *
  *   additional informations.                                              *
  *                                                                         *
  ***************************************************************************/

  //*************************************************************************//
  // History of changes:
  //
  // 2008/05/17 - Pete  
  // - added "visual rumble" stuff to buffer swap func
  //
  // 2007/10/27 - MxC
  // - added HQ2X/HQ3X MMX versions, and fixed stretching
  //
  // 2005/06/11 - MxC
  // - added HQ2X,HQ3X,Scale3X screen filters
  //
  // 2004/01/31 - Pete
  // - added zn stuff
  //
  // 2003/01/31 - stsp
  // - added zn stuff
  //
  // 2003/12/30 - Stefan Sperling <stsp@guerila.com>
  // - improved XF86VM fullscreen switching a little (refresh frequency issues).
  //
  // 2002/12/30 - Pete
  // - added Scale2x display mode - Scale2x (C) 2002 Andrea Mazzoleni - http://scale2x.sourceforge.net
  //
  // 2002/12/29 - Pete
  // - added gun cursor display
  //
  // 2002/12/21 - linuzappz
  // - some more messages for DGA2 errors
  // - improved XStretch funcs a little
  // - fixed non-streched modes for DGA2
  //
  // 2002/11/10 - linuzappz
  // - fixed 5bit masks for 2xSai/etc
  //
  // 2002/11/06 - Pete
  // - added 2xSai, Super2xSaI, SuperEagle
  //
  // 2002/08/09 - linuzappz
  // - added DrawString calls for DGA2 (FPS display)
  //
  // 2002/03/10 - lu
  // - Initial SDL-only blitting function
  // - Initial SDL stretch function (using an undocumented SDL 1.2 func)
  // - Boht are triggered by -D_SDL -D_SDL2
  //
  // 2002/02/18 - linuzappz
  // - NoStretch, PIC and Scanlines support for DGA2 (32bit modes untested)
  // - Fixed PIC colors in CreatePic for 16/15 bit modes
  //
  // 2002/02/17 - linuzappz
  // - Added DGA2 support, support only with no strecthing disabled (also no FPS display)
  //
  // 2002/01/13 - linuzappz
  // - Added timing for the szDebugText (to 2 secs)
  //
  // 2002/01/05 - Pete
  // - fixed linux stretch centering (no more garbled screens)
  //
  // 2001/12/30 - Pete
  // - Added linux fullscreen desktop switching (non-SDL version, define USE_XF86VM in Makefile)
  //
  // 2001/12/19 - syo
  // - support refresh rate change
  // - added  wait VSYNC
  //
  // 2001/12/16 - Pete
  // - Added Windows FPSE RGB24 mode switch
  //
  // 2001/12/05 - syo (syo68k@geocities.co.jp)
  // - modified for "Use system memory" option
  //   (Pete: fixed "system memory" save state pic surface)
  //
  // 2001/11/11 - lu
  // - SDL additions
  //
  // 2001/10/28 - Pete
  // - generic cleanup for the Peops release
  //
  //*************************************************************************//

#include "stdafx.h"

#define _IN_DRAW

#include "externals.h"
#include "gpu.h"
#include "draw.h"
#include "prim.h"
#include "menu.h"
#include "lodepng.h"

////////////////////////////////////////////////////////////////////////////////////
// misc globals
////////////////////////////////////////////////////////////////////////////////////
int            iResX;
int            iResY;
long           lLowerpart;
BOOL           bIsFirstFrame = TRUE;
BOOL           bCheckMask = FALSE;
unsigned short sSetMask = 0;
unsigned long  lSetMask = 0;
int            iDesktopCol = 16;
int            iShowFPS = 0;
int            iWinSize;
int            iFiltering = 0;
int            iUseNoStretchBlt = 0;
int            iFastFwd = 0;
int            iDebugMode = 0;
int            iFVDisplay = 0;
PSXPoint_t     ptCursorPoint[8];
unsigned short usCursorActive = 0;

////////////////////////////////////////////////////////////////////////
// own swap buffer func (window/fullscreen)
////////////////////////////////////////////////////////////////////////

sDX            DX;
static         D3DSURFACE_DESC ddsd;
GUID           guiDev;
BOOL           bDeviceOK;
HWND           hWGPU;
int			   iSysMemory = 0;
int            iFPSEInterface = 0;
int			   iRefreshRate;
BOOL		   bVsync = FALSE;
BOOL		   bVsync_Key = FALSE;

void(*BlitScreen) (unsigned char *, long, long);
HRESULT CreateVertex(void);

////////////////////////////////////////////////////////////////////////
void SaveSurface(LPDIRECT3DSURFACE9 pSurface, LPCSTR filename, int width, int height)
{
	D3DLOCKED_RECT Rect;
	D3DSURFACE_DESC desc;

	pSurface->LockRect(&Rect, NULL, D3DLOCK_READONLY);
	pSurface->GetDesc(&desc);

	if (width == -1) width = desc.Width;
	if (height == -1) height = desc.Height;

	// save the actual file
	unsigned char *temp = new unsigned char[width * height * 4];
	for (int y = 0, p = 0; y < height; y++)
	{
		unsigned char *row = &((unsigned char*)Rect.pBits)[Rect.Pitch * y];
		for (int x = 0; x < width; x++, p += 4, row += 4)
		{
			temp[p + 0] = row[2];
			temp[p + 1] = row[1];
			temp[p + 2] = row[0];
			temp[p + 3] = 0xff;
		}
	}
	lodepng_encode32_file(filename, (const unsigned char*)temp, width, height);
	delete[] temp;

	pSurface->UnlockRect();
}

void SaveTexture(LPDIRECT3DTEXTURE9 pSurface, LPCSTR filename, int width, int height)
{
	D3DLOCKED_RECT Rect;
	pSurface->LockRect(0, &Rect, NULL, D3DLOCK_DONOTWAIT);//D3DLOCK_READONLY);

	// save the actual file
	unsigned char *temp = new unsigned char[width * height * 4];
	for (int y = 0, p = 0; y < height; y++)
	{
		unsigned char *row = &((unsigned char*)Rect.pBits)[Rect.Pitch * y];
		for (int x = 0; x < width; x++, p += 4, row += 4)
		{
			temp[p + 0] = row[2];
			temp[p + 1] = row[1];
			temp[p + 2] = row[0];
			temp[p + 3] = 0xff;
		}
	}
	lodepng_encode32_file(filename, (const unsigned char*)temp, width, height);
	delete[] temp;

	pSurface->UnlockRect(0);
}

////////////////////////////////////////////////////////////////////////
static __inline void WaitVBlank(void)
{
	//if (bVsync_Key)
		//DX.Device->WaitForVBlank(0);
}

////////////////////////////////////////////////////////////////////////
int iColorMode = 0;
#define D3DCOLOR_XBGR(b, g, r)		((r) | ((g)<<8) | ((b)<<8) | 0xff000000)

D3DCOLOR ClutToRGB(unsigned short lu)
{
	unsigned int r, g, b;
	// RGB555 conversion
	if (iColorMode == 0)
	{
		r = lu & 0x1f;         r = (r << 3) | (r >> 2);
		g = (lu >> 5) & 0x1f;  g = (g << 3) | (g >> 2);
		b = (lu >> 10) & 0x1f; b = (b << 3) | (b >> 2);
	}
	// RGB888 conversion
	else
	{
		r = lu & 0x1f;         r <<= 3;
		g = (lu >> 5) & 0x1f;  g <<= 3;
		b = (lu >> 10) & 0x1f; b <<= 3;
	}
	return D3DCOLOR_XRGB(r, g, b);
}

D3DCOLOR ClutToBGR(unsigned short lu)
{
	int r, g, b;
	// RGB555 conversion
	if (iColorMode == 0)
	{
		r = lu & 0x1f;         r = (r << 3) | (r >> 2);
		g = (lu >> 5) & 0x1f;  g = (g << 3) | (g >> 2);
		b = (lu >> 10) & 0x1f; b = (b << 3) | (b >> 2);
	}
	// RGB888 conversion
	else
	{
		r = lu & 0x1f;         r <<= 3;
		g = (lu >> 5) & 0x1f;  g <<= 3;
		b = (lu >> 10) & 0x1f; b <<= 3;
	}
	return D3DCOLOR_XBGR(b, g, r);
}

void BlitScreenRGB(unsigned char * surf, long x, long y)  // BLIT IN 32bit COLOR MODE
{
	unsigned char * pD; unsigned long lu; unsigned short s;
	unsigned int startxy;
	short row, column;
	short dx = (short)PreviousPSXDisplay.Range.x1;
	short dy = (short)PreviousPSXDisplay.DisplayMode.y;

#if 1
	//memset(surf, 0xff, 1024 * 512 * 4);
	//unsigned int *sr = (unsigned int*)surf;
	//for (int i = 0; i < 1024 * 512; i++)
	//	*sr++ = 0xffff00ff;
	//return;
#endif

	if (iDebugMode && iFVDisplay)
	{
		dx = 1024;
		dy = iGPUHeight;
		x = 0; y = 0;

		for (column = 0; column < dy; column++)
		{
			startxy = ((1024)*(column + y)) + x;
			for (row = 0; row < dx; row++)
			{
				s = psxVuw[startxy++];
				*((unsigned long *)((surf)+(column*ddsd.Width * 4) + row * 4)) =
					((((s << 19) & 0xf80000) | ((s << 6) & 0xf800) | ((s >> 7) & 0xf8)) & 0xffffff) | 0xff000000;
			}
		}
		return;
	}


	if (PreviousPSXDisplay.Range.y0)	// centering needed?
	{
		surf += PreviousPSXDisplay.Range.y0*ddsd.Width * 4;
		dy -= PreviousPSXDisplay.Range.y0;
	}

	surf += PreviousPSXDisplay.Range.x0 << 2;

	if (PSXDisplay.RGB24)
	{
		if (iFPSEInterface)
		{
			for (column = 0; column < dy; column++)
			{
				startxy = ((1024)*(column + y)) + x;
				pD = (unsigned char *)&psxVuw[startxy];

				for (row = 0; row < dx; row++)
				{
					lu = *((unsigned long *)pD);
					*((unsigned long *)((surf)+(column*ddsd.Width * 4) + row * 4)) =
						0xff000000 | (BLUE(lu) << 16) | (GREEN(lu) << 8) | (RED(lu));
					pD += 3;
				}
			}
		}
		else
		{
			for (column = 0; column < dy; column++)
			{
				startxy = ((1024)*(column + y)) + x;
				pD = (unsigned char *)&psxVuw[startxy];

				for (row = 0; row < dx; row++)
				{
					lu = *((unsigned long *)pD);
					*((unsigned long *)((surf)+(column*ddsd.Width * 4) + row * 4)) =
						0xff000000 | (RED(lu) << 16) | (GREEN(lu) << 8) | (BLUE(lu));
					pD += 3;
				}
			}
		}
	}
	else
	{
		for (column = 0; column < dy; column++)
		{
			startxy = ((1024)*(column + y)) + x;
			for (row = 0; row < dx; row++)
			{
				s = psxVuw[startxy++];
				*((unsigned long *)((surf)+(column*ddsd.Width * 4) + row * 4)) = ClutToRGB(s);
					//((((s << 19) & 0xf80000) | ((s << 6) & 0xf800) | ((s >> 7) & 0xf8)) & 0xffffff) | 0xff000000;
			}
		}
	}
}

void BlitScreenBGR(unsigned char * surf, long x, long y)  // BLIT IN 32bit COLOR MODE
{
	unsigned char * pD; unsigned long lu; unsigned short s;
	unsigned int startxy;
	short row, column;
	short dx = (short)PreviousPSXDisplay.Range.x1;
	short dy = (short)PreviousPSXDisplay.DisplayMode.y;

	if (iDebugMode && iFVDisplay)
	{
		dx = 1024;
		dy = iGPUHeight;
		x = 0; y = 0;

		for (column = 0; column < dy; column++)
		{
			startxy = ((1024)*(column + y)) + x;
			for (row = 0; row < dx; row++)
			{
				s = psxVuw[startxy++];
				*((unsigned long *)((surf)+(column*ddsd.Width * 4) + row * 4)) =
					((((s << 19) & 0xf80000) | ((s << 6) & 0xf800) | ((s >> 7) & 0xf8)) & 0xffffff) | 0xff000000;
			}
		}
		return;
	}


	if (PreviousPSXDisplay.Range.y0)	// centering needed?
	{
		surf += PreviousPSXDisplay.Range.y0*ddsd.Width * 4;
		dy -= PreviousPSXDisplay.Range.y0;
	}

	surf += PreviousPSXDisplay.Range.x0 << 2;

	if (PSXDisplay.RGB24)
	{
		if (iFPSEInterface)
		{
			for (column = 0; column < dy; column++)
			{
				startxy = ((1024)*(column + y)) + x;
				pD = (unsigned char *)&psxVuw[startxy];

				for (row = 0; row < dx; row++)
				{
					lu = *((unsigned long *)pD);
					*((unsigned long *)((surf)+(column*ddsd.Width * 4) + row * 4)) =
						0xff000000 | (BLUE(lu)) | (GREEN(lu) << 8) | (RED(lu) << 16);
					pD += 3;
				}
			}
		}
		else
		{
			for (column = 0; column < dy; column++)
			{
				startxy = ((1024)*(column + y)) + x;
				pD = (unsigned char *)&psxVuw[startxy];

				for (row = 0; row < dx; row++)
				{
					lu = *((unsigned long *)pD);
					*((unsigned long *)((surf)+(column*ddsd.Width * 4) + row * 4)) =
						0xff000000 | (RED(lu)) | (GREEN(lu) << 8) | (BLUE(lu) << 16);
					pD += 3;
				}
			}
		}
	}
	else
	{
		for (column = 0; column < dy; column++)
		{
			startxy = ((1024)*(column + y)) + x;
			for (row = 0; row < dx; row++)
			{
				s = psxVuw[startxy++];
				*((unsigned long *)((surf)+(column*ddsd.Width * 4) + row * 4)) = ClutToBGR(s);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////
void DoClearScreenBuffer(void)                         // CLEAR DX BUFFER
{
	if (iFiltering >= 2)
	{
		D3DLOCKED_RECT Rect;
		DX.DDTRender->LockRect(0, &Rect, NULL, D3DLOCK_DISCARD);
		memset(Rect.pBits, 0, Rect.Pitch * 512);
		DX.DDTRender->UnlockRect(0);
	}
	else DX.Device->ColorFill(DX.DDSRender, NULL, D3DCOLOR_XRGB(0, 0, 0));
}

////////////////////////////////////////////////////////////////////////

void DoClearFrontBuffer(void)                         // CLEAR PRIMARY BUFFER
{
	if (iFiltering >= 2)
	{
		D3DLOCKED_RECT Rect;
		DX.DDTRender->LockRect(0, &Rect, NULL, D3DLOCK_DISCARD);
		memset(Rect.pBits, 0, Rect.Pitch * 512);
		DX.DDTRender->UnlockRect(0);
	}
	else DX.Device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0, 0);
}

////////////////////////////////////////////////////////////////////////

void NoStretchedBlit(void)
{
	static int iOldDX = 0;
	static int iOldDY = 0;

	int iDX, iDY;
	int iX = iResX - PreviousPSXDisplay.DisplayMode.x;
	int iY = iResY - PreviousPSXDisplay.DisplayMode.y;

	/*
	float fXS,fYS,fS;
	fXS=(float)iResX/(float)PreviousPSXDisplay.DisplayMode.x;
	fYS=(float)iResY/(float)PreviousPSXDisplay.DisplayMode.y;
	if(fXS<fYS) fS=fXS; else fS=fYS;
	*/

	if (iX < 0) { iX = 0; iDX = iResX; }
	else { iX = iX / 2; iDX = PreviousPSXDisplay.DisplayMode.x; }

	if (iY < 0) { iY = 0; iDY = iResY; }
	else { iY = iY / 2; iDY = PreviousPSXDisplay.DisplayMode.y; }

	if (iOldDX != iDX || iOldDY != iDY)
	{
	#if !USE_DX9
		DDBLTFX     ddbltfx;
		ddbltfx.dwSize = sizeof(ddbltfx);
		ddbltfx.dwFillColor = 0x00000000;
		IDirectDrawSurface_Blt(DX.DDSPrimary, NULL, NULL, NULL, DDBLT_COLORFILL, &ddbltfx);
	#else
		//DX.Device->ColorFill(DX.DDSPrimary, NULL, 0x000000);
	#endif
		iOldDX = iDX; iOldDY = iDY;
	}

	if (iWindowMode)
	{
		RECT ScreenRect, ViewportRect;
		POINT Point = { 0, 0 };
		ClientToScreen(DX.hWnd, &Point);
		Point.x += iX; Point.y += iY;

		ScreenRect.left = Point.x;
		ScreenRect.top = Point.y;
		ScreenRect.right = iDX + Point.x;
		ScreenRect.bottom = iDY + Point.y;

		ViewportRect.left = 0;
		ViewportRect.top = 0;
		ViewportRect.right = iDX;
		ViewportRect.bottom = iDY;

		WaitVBlank();
	#if !USE_DX9
		IDirectDrawSurface_Blt(DX.DDSPrimary, &ScreenRect, DX.DDSRender, &ViewportRect,
							   DDBLT_WAIT, NULL);
	#else
		//DX.Device->StretchRect(DX.DDSRender, &ViewportRect, DX.DDSPrimary, NULL/*&ScreenRect*/, D3DTEXTUREFILTERTYPE::D3DTEXF_ANISOTROPIC);
	#endif
	}
	else
	{
		RECT ScreenRect, ViewportRect;

		ScreenRect.left = iX;
		ScreenRect.top = iY;
		ScreenRect.right = iDX + iX;
		ScreenRect.bottom = iDY + iY;

		ViewportRect.left = 0;
		ViewportRect.top = 0;
		ViewportRect.right = iDX;
		ViewportRect.bottom = iDY;

		WaitVBlank();
	#if !USE_DX9
		IDirectDrawSurface_Blt(DX.DDSPrimary, &ScreenRect, DX.DDSRender, &ViewportRect, DDBLT_WAIT, NULL);
	#else
		//DX.Device->StretchRect(DX.DDSRender, &ViewportRect, DX.DDSPrimary, &ScreenRect, D3DTEXTUREFILTERTYPE::D3DTEXF_ANISOTROPIC);
	#endif
	}
	//if (DX.DDSScreenPic) DisplayPic();
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

void ShowGunCursor(unsigned char * surf)
{
	unsigned short dx = (unsigned short)PreviousPSXDisplay.Range.x1;
	unsigned short dy = (unsigned short)PreviousPSXDisplay.DisplayMode.y;
	int x, y, iPlayer, sx, ex, sy, ey;

	if (PreviousPSXDisplay.Range.y0)                       // centering needed?
	{
		surf += PreviousPSXDisplay.Range.y0*ddsd.Width * 4;
		dy -= PreviousPSXDisplay.Range.y0;
	}

	if (iUseNoStretchBlt >= 3 && iUseNoStretchBlt < 13)                               // 2xsai is twice as big, of course
	{
		dx *= 2; dy *= 2;
	}
	else if (iUseNoStretchBlt >= 13)
	{
		dx *= 3; dy *= 3;
	}

	if (iDesktopCol == 32)                                   // 32 bit color depth
	{
		const unsigned long crCursorColor32[8] = { 0xffff0000, 0xff00ff00, 0xff0000ff, 0xffff00ff, 0xffffff00, 0xff00ffff, 0xffffffff, 0xff7f7f7f };

		surf += PreviousPSXDisplay.Range.x0 << 2;               // -> add x left border

		for (iPlayer = 0; iPlayer < 8; iPlayer++)                  // -> loop all possible players
		{
			if (usCursorActive&(1 << iPlayer))                   // -> player active?
			{
				const int ty = (ptCursorPoint[iPlayer].y*dy) / 256;  // -> calculate the cursor pos in the current display
				const int tx = (ptCursorPoint[iPlayer].x*dx) / 512;
				sx = tx - 5; if (sx < 0) { if (sx & 1) sx = 1; else sx = 0; }
				sy = ty - 5; if (sy < 0) { if (sy & 1) sy = 1; else sy = 0; }
				ex = tx + 6; if (ex > dx) ex = dx;
				ey = ty + 6; if (ey > dy) ey = dy;

				for (x = tx, y = sy; y < ey; y += 2)                    // -> do dotted y line
					*((unsigned long *)((surf)+(y*ddsd.Width * 4) + x * 4)) = crCursorColor32[iPlayer];
				for (y = ty, x = sx; x < ex; x += 2)                    // -> do dotted x line
					*((unsigned long *)((surf)+(y*ddsd.Width * 4) + x * 4)) = crCursorColor32[iPlayer];
			}
		}
	}
	else                                                  // 16 bit color depth
	{
		const unsigned short crCursorColor16[8] = { 0xf800, 0x07c0, 0x001f, 0xf81f, 0xffc0, 0x07ff, 0xffff, 0x7bdf };

		surf += PreviousPSXDisplay.Range.x0 << 1;               // -> same stuff as above

		for (iPlayer = 0; iPlayer < 8; iPlayer++)
		{
			if (usCursorActive&(1 << iPlayer))
			{
				const int ty = (ptCursorPoint[iPlayer].y*dy) / 256;
				const int tx = (ptCursorPoint[iPlayer].x*dx) / 512;
				sx = tx - 5; if (sx < 0) { if (sx & 1) sx = 1; else sx = 0; }
				sy = ty - 5; if (sy < 0) { if (sy & 1) sy = 1; else sy = 0; }
				ex = tx + 6; if (ex > dx) ex = dx;
				ey = ty + 6; if (ey > dy) ey = dy;

				for (x = tx, y = sy; y < ey; y += 2)
					*((unsigned short *)((surf)+(y*ddsd.Width * 4) + x * 2)) = crCursorColor16[iPlayer];
				for (y = ty, x = sx; x < ex; x += 2)
					*((unsigned short *)((surf)+(y*ddsd.Width * 4) + x * 2)) = crCursorColor16[iPlayer];
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////
void DoBufferSwap(void)		// SWAP BUFFERS
{							// (we don't swap... we blit only)
	long x, y;
	D3DLOCKED_RECT Rect;

	if (iFiltering >= 2)
	{
		if (FAILED(DX.DDTRender->LockRect(0, &Rect, NULL, D3DLOCK_DISCARD/*D3DLOCK_READONLY*/)))
		{
			DX.DDTRender->UnlockRect(0);
			return;
		}
	}
	else
	{
		if (FAILED(DX.DDSRender->LockRect(&Rect, NULL, D3DLOCK_DONOTWAIT | D3DLOCK_DISCARD)))
		{
			DX.DDSRender->UnlockRect();
			return;
		}
	}
	DX.DDSRender->GetDesc(&ddsd);

	//----------------------------------------------------//

	x = PSXDisplay.DisplayPosition.x;
	y = PSXDisplay.DisplayPosition.y;

	//----------------------------------------------------//

	BlitScreen((unsigned char *)Rect.pBits, x, y);      // fill DDSRender surface

	if (usCursorActive) ShowGunCursor((unsigned char *)Rect.pBits);

	if (iFiltering >= 2)
	{
		DX.DDTRender->UnlockRect(0);
		//D3DXSaveTextureToFile("test.png", D3DXIMAGE_FILEFORMAT::D3DXIFF_PNG, DX.DDTRender, NULL);
	}
	else DX.DDSRender->UnlockRect();

	//if (ulKeybits & KEY_SHOWFPS) DisplayText();              // paint menu text

	//if (pExtraBltFunc)
	//{
	//	pExtraBltFunc();
	//	return;
	//}

	if (iWindowMode)
	{
		RECT ScreenRect, ViewportRect;
		POINT Point = { 0, 0 };
		ClientToScreen(DX.hWnd, &Point);

		ScreenRect.left = Point.x;
		ScreenRect.top = Point.y;
		ScreenRect.right = iResX + Point.x;
		ScreenRect.bottom = iResY + Point.y;

		ViewportRect.left = 0;
		ViewportRect.top = 0;

		if (iDebugMode)
		{
			if (iFVDisplay)
			{
				ViewportRect.right = 1024;
				ViewportRect.bottom = iGPUHeight;
			}
			else
			{
				ViewportRect.right = PreviousPSXDisplay.DisplayMode.x;
				ViewportRect.bottom = PreviousPSXDisplay.DisplayMode.y;
			}
		}
		else
		{
			ViewportRect.right = PreviousPSXDisplay.DisplayMode.x;
			ViewportRect.bottom = PreviousPSXDisplay.DisplayMode.y;

			if (iRumbleTime)
			{
				ScreenRect.left += ((rand()*iRumbleVal) / RAND_MAX) - (iRumbleVal / 2);
				ScreenRect.right += ((rand()*iRumbleVal) / RAND_MAX) - (iRumbleVal / 2);
				ScreenRect.top += ((rand()*iRumbleVal) / RAND_MAX) - (iRumbleVal / 2);
				ScreenRect.bottom += ((rand()*iRumbleVal) / RAND_MAX) - (iRumbleVal / 2);
				iRumbleTime--;
			}

		}

//		if (iUseScanLines == 2)                                // stupid nvidia scanline mode
//		{
//			RECT HelperRect = { 0, 0, iResX, iResY };
//
//			WaitVBlank();
//#if !USE_DX9
//			IDirectDrawSurface_Blt(DX.DDSHelper, &HelperRect, DX.DDSRender, &ViewportRect, DDBLT_WAIT, NULL);
//			IDirectDrawSurface_Blt(DX.DDSPrimary, &ScreenRect, DX.DDSHelper, &HelperRect, DDBLT_WAIT, NULL);
//#else
//			//DX.Device->StretchRect(DX.DDSRender, &ViewportRect, DX.DDSHelper,  &HelperRect, D3DTEXTUREFILTERTYPE::D3DTEXF_ANISOTROPIC);
//			//DX.Device->StretchRect(DX.DDSHelper, &HelperRect,   DX.DDSPrimary, &ScreenRect, D3DTEXTUREFILTERTYPE::D3DTEXF_ANISOTROPIC);
//#endif
//		}
//		else
		{
			WaitVBlank();
#if !USE_DX9
			IDirectDrawSurface_Blt(DX.DDSPrimary, &ScreenRect, DX.DDSRender, &ViewportRect, DDBLT_WAIT, NULL);
#else
			//DX.Device->StretchRect(DX.DDSRender, &ViewportRect, DX.DDSPrimary, &ScreenRect, D3DTEXTUREFILTERTYPE::D3DTEXF_ANISOTROPIC);
#endif
		}
	}
	else
	{
		RECT ScreenRect = { 0, 0, iResX, iResY },
			ViewportRect = { 0, 0, PreviousPSXDisplay.DisplayMode.x,
			PreviousPSXDisplay.DisplayMode.y };

		if (iDebugMode && iFVDisplay)
		{
			ViewportRect.right = 1024;
			ViewportRect.bottom = iGPUHeight;
		}
		else
		{
			if (iRumbleTime)
			{
				ScreenRect.left += ((rand()*iRumbleVal) / RAND_MAX) - (iRumbleVal / 2);
				ScreenRect.right += ((rand()*iRumbleVal) / RAND_MAX) - (iRumbleVal / 2);
				ScreenRect.top += ((rand()*iRumbleVal) / RAND_MAX) - (iRumbleVal / 2);
				ScreenRect.bottom += ((rand()*iRumbleVal) / RAND_MAX) - (iRumbleVal / 2);
				iRumbleTime--;
			}
		}

		//if (iUseScanLines == 2)		// stupid nvidia scanline mode
		//{
		//	WaitVBlank();
		//#if !USE_DX9
		//	IDirectDrawSurface_Blt(DX.DDSHelper, &ScreenRect, DX.DDSRender, &ViewportRect, DDBLT_WAIT, NULL);
		//	IDirectDrawSurface_Blt(DX.DDSPrimary, &ScreenRect, DX.DDSHelper, &ScreenRect, DDBLT_WAIT, NULL);
		//#else
		//#endif
		//}
		//else
		{
			WaitVBlank();
		#if !USE_DX9
			IDirectDrawSurface_Blt(DX.DDSPrimary, &ScreenRect, DX.DDSRender, &ViewportRect, DDBLT_WAIT, NULL);
		#else
		#endif
		}
	}

	//if (DX.DDSScreenPic)
	//	DisplayPic();
}

////////////////////////////////////////////////////////////////////////
// GAMMA
////////////////////////////////////////////////////////////////////////

int iUseGammaVal = 2048;

void DXSetGamma(void)
{
	float g;
	if (iUseGammaVal == 2048) return;

	g = (float)iUseGammaVal;
	if (iUseGammaVal > 512) g = ((g - 512.f) * 2.f) + 512.f;
	g = 0.5f + (g / 1024.0f);

	D3DGAMMARAMP ramp;
	float f;
	for (int i = 0; i < 256; i++)
	{
		f = ((float)i * 256.f) * g;
		if (f > 65535.f) f = 65535;
		ramp.red[i] = ramp.green[i] = ramp.blue[i] = (WORD)f;
	}
	DX.Device->SetGammaRamp(0, D3DSGR_CALIBRATE, &ramp);
}

////////////////////////////////////////////////////////////////////////
// SCAN LINE STUFF
////////////////////////////////////////////////////////////////////////
void MoveScanLineArea(HWND hwnd)
{
}

////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////
HRESULT CreateVertex(void)
{
	TLVERTEX* vertices;

	// Texture coordinates
	float sizex = 1.0f, sizey = 1.0f;

	// If texture is larger than DOSBox FB
	sizex = ((float)PSXDisplay.DisplayMode.x - 0.5f) / 1024.f;
	sizey = ((float)PSXDisplay.DisplayMode.y - 0.5f) / 512.f;
	const float x0 = 0.5f / 1024.f;
	const float y0 = 0.5f / 512.f;

	// Lock the vertex buffer
	DX.vertexBuffer->Lock(0, 0, (void**)&vertices, 0);

	//Setup vertices
	vertices[0].position = D3DXVECTOR3(-0.5f, -0.5f, 0.0f);
	vertices[0].diffuse = 0xFFFFFFFF;
	vertices[0].texcoord = D3DXVECTOR2(x0, sizey);
	vertices[1].position = D3DXVECTOR3(-0.5f, 0.5f, 0.0f);
	vertices[1].diffuse = 0xFFFFFFFF;
	vertices[1].texcoord = D3DXVECTOR2(x0, y0);
	vertices[2].position = D3DXVECTOR3(0.5f, -0.5f, 0.0f);
	vertices[2].diffuse = 0xFFFFFFFF;
	vertices[2].texcoord = D3DXVECTOR2(sizex, sizey);
	vertices[3].position = D3DXVECTOR3(0.5f, 0.5f, 0.0f);
	vertices[3].diffuse = 0xFFFFFFFF;
	vertices[3].texcoord = D3DXVECTOR2(sizex, y0);

	// Additional vertices required for some PS effects
	//if (preProcess) {
	//	vertices[4].position = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	//	vertices[4].diffuse = 0xFFFFFF00;
	//	vertices[4].texcoord = D3DXVECTOR2(0.0f, 1.0f);
	//	vertices[5].position = D3DXVECTOR3(0.0f, 1.0f, 0.0f);
	//	vertices[5].diffuse = 0xFFFFFF00;
	//	vertices[5].texcoord = D3DXVECTOR2(0.0f, 0.0f);
	//	vertices[6].position = D3DXVECTOR3(1.0f, 0.0f, 0.0f);
	//	vertices[6].diffuse = 0xFFFFFF00;
	//	vertices[6].texcoord = D3DXVECTOR2(1.0f, 1.0f);
	//	vertices[7].position = D3DXVECTOR3(1.0f, 1.0f, 0.0f);
	//	vertices[7].diffuse = 0xFFFFFF00;
	//	vertices[7].texcoord = D3DXVECTOR2(1.0f, 0.0f);
	//}

	// Unlock the vertex buffer
	DX.vertexBuffer->Unlock();

	DX.iResX = PSXDisplay.DisplayMode.x;
	DX.iResY = PSXDisplay.DisplayMode.y;

	return S_OK;
}


////////////////////////////////////////////////////////////////////////
// MAIN DIRECT DRAW INIT
////////////////////////////////////////////////////////////////////////

BOOL ReStart = FALSE;

int DXinitialize()
{
	IDirect3D9Ex *DD;

	memset(&DX, 0, sizeof(DX));

	if (!iWindowMode)
	{
		iResX = GetSystemMetrics(SM_CXSCREEN);
		iResY = GetSystemMetrics(SM_CYSCREEN);
	}

	// init some DX vars
	DX.hWnd = hWGPU;
	//DX.DDSHelper = 0;
	DX.DDSScreenPic = 0;

	if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &DD)))
	{
		MessageBox(NULL, "This GPU requires DirectX 9!", "Error", MB_OK);
		return 0;
	}

	DX.DD = DD;

	if (FAILED(DX.DD->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &DX.d3ddm)))
	{
		MessageBox(NULL, "DirectX failed: GetAdapterDisplayMode", "Error", MB_OK);
		return 0;
	}

	switch (DX.d3ddm.Format)
	{
	// allowed types
	case D3DFMT_X8B8G8R8:
	case D3DFMT_A8B8G8R8:
		BlitScreen = BlitScreenBGR;
		break;
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
		BlitScreen = BlitScreenRGB;
		break;
	// everything else should fail
	default:
		MessageBox(NULL, "DirectX failed: 32 bit mode rendering is required.", "Error", MB_OK);
		return 0;
	}

	HRESULT hr;
	D3DFORMAT depth_fmt = D3DFMT_D16;
	if (FAILED(hr = DX.DD->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, DX.d3ddm.Format,
													 D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, depth_fmt)))
	{
		if (hr == D3DERR_NOTAVAILABLE)
		{
			MessageBox(NULL, "DirectX failed: CheckDeviceFormat", "Error", MB_OK);
			return 0;
		}
	}

	D3DCAPS9 d3dCaps;
	if (FAILED(DX.DD->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &d3dCaps)))
	{
		MessageBox(NULL, "DirectX failed: GetDeviceCaps", "Error", MB_OK);
		return 0;
	}

	DWORD dwBehaviorFlags = D3DCREATE_PUREDEVICE;
	if (d3dCaps.VertexProcessingCaps != 0) dwBehaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
	else dwBehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;

	// Everything checks out - create a simple, windowed device.
	D3DPRESENT_PARAMETERS d3dpp;
	memset(&d3dpp, 0, sizeof(d3dpp));

	d3dpp.hDeviceWindow = DX.hWnd;
	d3dpp.BackBufferWidth = iResX;
	d3dpp.BackBufferHeight = iResY;
	d3dpp.BackBufferFormat = DX.d3ddm.Format;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.BackBufferCount = 1;
	d3dpp.Windowed = TRUE;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.AutoDepthStencilFormat = depth_fmt;
	if (bVsync)
	{
		d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
		d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	}
	else
	{
		d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
		d3dpp.FullScreen_RefreshRateInHz = 0;
	}

	//if (FAILED(DX.DD->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, DX.hWnd, dwBehaviorFlags, &d3dpp, NULL, &DX.Device)))
	if (FAILED(DX.DD->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, DX.hWnd, dwBehaviorFlags, &d3dpp, &DX.Device)))
	{
		MessageBox(NULL, "DirectX failed: CreateDeviceEx", "Error", MB_OK);
		return 0;
	}

	if (iFiltering >= 2)
	{
		DX.Scaler = new ScalingEffect(DX.Device);
		if (FAILED(DX.Scaler->LoadEffect("plugins\\sshaders\\2xSaI.fx")))
		{
			MessageBox(NULL, "DirectX failed: LoadEffect", "Error", MB_OK);
			return 0;
		}
		if (FAILED(DX.Device->CreateTexture(1024, 512, 0, D3DUSAGE_DYNAMIC, DX.d3ddm.Format, D3DPOOL_DEFAULT, &DX.DDTRender, NULL)))
		{
			MessageBox(NULL, "DirectX failed: CreateTexture", "Error", MB_OK);
			return 0;
		}

		DX.Device->SetFVF(D3DFVF_TLVERTEX);

		// Create vertex buffer
		if (FAILED(DX.Device->CreateVertexBuffer(sizeof(TLVERTEX) * 4, NULL/*D3DUSAGE_WRITEONLY*/, D3DFVF_TLVERTEX, D3DPOOL_DEFAULT, &DX.vertexBuffer, NULL)))
		{
			MessageBox(NULL, "DirectX failed: CreateVertexBuffer", "Error", MB_OK);
			return E_FAIL;
		}

	}

	//////////////////////////////////////////////////////// main surfaces
	if (FAILED(DX.Device->CreateRenderTarget(d3dpp.BackBufferWidth, d3dpp.BackBufferHeight, DX.d3ddm.Format, D3DMULTISAMPLE_NONE, 0, TRUE, &DX.DDSCopy, NULL)))
	{
		MessageBox(NULL, "DirectX failed: CreateRenderTarget", "Error", MB_OK);
		return 0;
	}

	//----------------------------------------------------//
#if 0
	if (FAILED(DX.Device->CreateOffscreenPlainSurface(1024, 512, DX.d3ddm.Format, D3DPOOL_DEFAULT, &DX.DDSRender, NULL)))
#else
	if (FAILED(DX.Device->CreateRenderTarget(1024, 512, DX.d3ddm.Format, D3DMULTISAMPLE_NONE, 0, TRUE, &DX.DDSRender, NULL)))
#endif
	{
		MessageBox(NULL, "DirectX failed: CreateOffscreenPlainSurface", "Error", MB_OK);
		return 0;
	}

	iDesktopCol = 32;

	//////////////////////////////////////////////////////// small screen clean up

	DXSetGamma();
	DX.Device->ColorFill(DX.DDSRender,  NULL, D3DCOLOR_XRGB(0, 0, 0));

	//////////////////////////////////////////////////////// finish init

	bUsingTWin = FALSE;
	InitMenu();                                           // menu init

	if (iShowFPS)                                          // fps on startup
	{
		ulKeybits |= KEY_SHOWFPS;
		szDispBuf[0] = 0;
		BuildDispMenu(0);
	}

	bIsFirstFrame = FALSE;                                // done

	return 0;
}

////////////////////////////////////////////////////////////////////////
// clean up DX stuff
////////////////////////////////////////////////////////////////////////

void DXcleanup()                                       // DX CLEANUP
{
	CloseMenu();                                          // bye display lists

	if (!bIsFirstFrame)
	{
		if (DX.DDSScreenPic)
		{
			DX.DDSScreenPic->Release();
			DX.DDSScreenPic = 0;
		}
		if (DX.DDTRender)
		{
			DX.DDTRender->Release();
			DX.DDTRender = 0;
		}
		if (DX.Scaler)
		{
			//DX.Scaler->KillThis();
			delete DX.Scaler;
			DX.Scaler = 0;
		}
		DX.DDSRender->Release();
		DX.Device->Release();
		DX.DD->Release();

		ReStart = TRUE;
		bIsFirstFrame = TRUE;
	}
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
// generate HICON to be used with window title
HICON HICONFromPng(unsigned char *png, size_t size)
{
	unsigned int w, h;
	unsigned char *out;

	lodepng_decode32(&out, &w, &h, png, size);
	// swap colors
	for (int i = 0, si = w * h; i < si; i++)
	{
		unsigned char r = out[i * 4 + 2];
		unsigned char b = out[i * 4 + 0];
		out[i * 4 + 0] = r;
		out[i * 4 + 2] = b;
	}

	HBITMAP bitmap = ::CreateBitmap(w, h, 1, 32, out);
	HBITMAP hbmMask = ::CreateBitmap(w, h, 1, 1, NULL);

	ICONINFO ii = { 0 };
	ii.fIcon = TRUE;
	ii.xHotspot = 0;
	ii.yHotspot = 0;
	ii.hbmColor = bitmap;
	ii.hbmMask = hbmMask;

	HICON hIcon = ::CreateIconIndirect(&ii);
	::DeleteObject(bitmap);
	::DeleteObject(hbmMask);
	free(out);

	return hIcon;
}

void SetIcon(HWND hWnd)
{
	// load and lock the bitmap (lock is kinda obsolete in win32)
	HRSRC hR = FindResource(hInst, MAKEINTRESOURCE(IDB_ICON), "PNG");
	HGLOBAL hG = LoadResource(hInst, hR);
	unsigned char *icon = (unsigned char*)LockResource(hG);

	HICON hIcon = HICONFromPng(icon, 15788);
	SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
	SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
}

DWORD  dwGPUStyle = 0;                                   // vars to store some windows stuff
HANDLE hGPUMenu = NULL;

unsigned long ulInitDisplay(void)
{
	HDC hdc; RECT r;

	SetIcon(hWGPU);
	if (iWindowMode)                                       // win mode?
	{
		DWORD dw = GetWindowLong(hWGPU, GWL_STYLE);    // -> adjust wnd style
		dwGPUStyle = dw;
		dw &= ~WS_THICKFRAME;
		dw |= WS_BORDER | WS_CAPTION;
		SetWindowLong(hWGPU, GWL_STYLE, dw);

		iResX = LOWORD(iWinSize); iResY = HIWORD(iWinSize);
		ShowWindow(hWGPU, SW_SHOWNORMAL);

		//if (iUseScanLines)
		//	SetWindowPos(hWGPU, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

		MoveWindow(hWGPU,                            // -> move wnd
				   GetSystemMetrics(SM_CXFULLSCREEN) / 2 - iResX / 2,
				   GetSystemMetrics(SM_CYFULLSCREEN) / 2 - iResY / 2,
				   iResX + GetSystemMetrics(SM_CXFIXEDFRAME) + 3,
				   iResY + GetSystemMetrics(SM_CYFIXEDFRAME) + GetSystemMetrics(SM_CYCAPTION) + 3,
				   TRUE);
		UpdateWindow(hWGPU);                         // -> let windows do some update
	}
	else                                                  // no window mode:
	{
		DWORD dw = GetWindowLong(hWGPU, GWL_STYLE);    // -> adjust wnd style
		dwGPUStyle = dw;
		hGPUMenu = GetMenu(hWGPU);

		#define WS_BORDERLESS	(WS_POPUP | WS_VISIBLE)

		dw &= ~(WS_THICKFRAME | WS_BORDER | WS_CAPTION);
		dw |= WS_BORDERLESS;
		SetWindowLong(hWGPU, GWL_STYLE, dw);
		SetMenu(hWGPU, NULL);

		ShowWindow(hWGPU, SW_SHOWMAXIMIZED);          // -> max mode
	}

	r.left = r.top = 0; r.right = iResX; r.bottom = iResY;          // init bkg with black
	hdc = GetDC(hWGPU);
	FillRect(hdc, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
	ReleaseDC(hWGPU, hdc);

	DXinitialize();                                       // init direct draw (not D3D... oh, well)

	if (!iWindowMode)                                  // borderless windows mode?
		ShowWindow(hWGPU, SW_SHOWMAXIMIZED);           // -> maximize again (fixes strange DX behavior)

	return 1;
}

////////////////////////////////////////////////////////////////////////

void CloseDisplay(void)
{
	DXcleanup();                                          // cleanup dx

	SetWindowLong(hWGPU, GWL_STYLE, dwGPUStyle);    // repair window
	if (hGPUMenu) SetMenu(hWGPU, (HMENU)hGPUMenu);
}

////////////////////////////////////////////////////////////////////////

void CreatePic(unsigned char * pMem)
{
	unsigned char * ps; int x, y;
	if (FAILED(DX.Device->CreateOffscreenPlainSurface(128, 128, D3DFORMAT::D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &DX.DDSScreenPic, NULL)))
	{
		DX.DDSScreenPic = 0;
		return;
	}

	D3DLOCKED_RECT Rect;
	DX.DDSScreenPic->LockRect(&Rect, NULL, D3DLOCK_DISCARD);
	ps = (unsigned char*)Rect.pBits;

	unsigned long l;
	for (y = 0; y < 96; y++)
	{
		for (x = 0; x < 128; x++)
		{
			l = *(pMem + 0);
			l |= (*(pMem + 1)) << 8;
			l |= (*(pMem + 2)) << 16;
			l |= 0xff000000;
			pMem += 3;
			*((unsigned long *)(ps + y * Rect.Pitch + x * 4)) = l;
		}
	}

	DX.DDSScreenPic->UnlockRect();
}

///////////////////////////////////////////////////////////////////////////////////////

void DestroyPic(void)
{
	if (DX.DDSScreenPic)
	{
		RECT ScreenRect = { iResX - 128, 0, iResX, 96 };
	#if !USE_DX9
		DDBLTFX     ddbltfx;
	#endif

		if (iWindowMode)
		{
			POINT Point = { 0, 0 };
			ClientToScreen(DX.hWnd, &Point);
			ScreenRect.left += Point.x;
			ScreenRect.right += Point.x;
			ScreenRect.top += Point.y;
			ScreenRect.bottom += Point.y;
		}

	#if !USE_DX9
		ddbltfx.dwSize = sizeof(ddbltfx);
		ddbltfx.dwFillColor = 0x00000000;
		IDirectDrawSurface_Blt(DX.DDSPrimary, &ScreenRect, NULL, NULL, DDBLT_COLORFILL, &ddbltfx);

		IDirectDrawSurface_Release(DX.DDSScreenPic);
	#else
		DX.DDSScreenPic->Release();
		//DX.Device->ColorFill(DX.DDSPrimary, &ScreenRect, D3DCOLOR_XRGB(0, 0, 0));
	#endif
		DX.DDSScreenPic = 0;
	}
}

///////////////////////////////////////////////////////////////////////////////////////

void DisplayPic(void)
{
	RECT ScreenRect = { iResX - 128, 0, iResX, 96 },
		HelperRect = { 0, 0, 128, 96 };
	if (iWindowMode)
	{
		POINT Point = { 0, 0 };
		ClientToScreen(DX.hWnd, &Point);
		ScreenRect.left += Point.x;
		ScreenRect.right += Point.x;
		ScreenRect.top += Point.y;
		ScreenRect.bottom += Point.y;
	}

	// ??? eh... no need to wait here!
	// WaitVBlank();

#if !USE_DX9
	IDirectDrawSurface_Blt(DX.DDSPrimary, &ScreenRect, DX.DDSScreenPic, &HelperRect, DDBLT_WAIT, NULL);
#else
	DX.Device->StretchRect(DX.DDSScreenPic, &HelperRect, DX.DDSRender, &ScreenRect, D3DTEXTUREFILTERTYPE::D3DTEXF_NONE);
#endif
}

/////////////////////////////////////////////////////////////
void ShowGpuPic(void)
{
	HRSRC hR; HGLOBAL hG; unsigned long * pRMem;
	unsigned char * pMem; int x, y; unsigned long * pDMem;

	// turn off any screen pic, if it does already exist
	if (DX.DDSScreenPic) { DestroyPic(); return; }

	if (ulKeybits&KEY_SHOWFPS) { ShowTextGpuPic(); return; }

	// load and lock the bitmap (lock is kinda obsolete in win32)
	hR = FindResource(hInst, MAKEINTRESOURCE(IDB_GPU), RT_BITMAP);
	hG = LoadResource(hInst, hR);

	// get long ptr to bmp data
	pRMem = ((unsigned long *)LockResource(hG)) + 10;

	// change the data upside-down
	pMem = (unsigned char *)malloc(128 * 96 * 3);

	for (y = 0; y < 96; y++)
	{
		pDMem = (unsigned long *)(pMem + (95 - y) * 128 * 3);
		for (x = 0; x < 96; x++) *pDMem++ = *pRMem++;
	}

	// show the pic
	CreatePic(pMem);

	// clean up
	free(pMem);
	DeleteObject(hG);
}

////////////////////////////////////////////////////////////////////////

void ShowTextGpuPic(void)                              // CREATE TEXT SCREEN PIC
{                                                      // gets Text and paints
	unsigned char * pMem; BITMAPINFO bmi;                  // it into a rgb24 bitmap
	HDC hdc, hdcMem; HBITMAP hBmp, hBmpMem; HFONT hFontMem;   // to display it in the gpu
	HBRUSH hBrush, hBrushMem; HPEN hPen, hPenMem;
	char szB[256];
	RECT r = { 0, 0, 128, 96 };                                  // size of bmp... don't change that
	COLORREF crFrame = RGB(128, 255, 128);                  // some example color inits
	COLORREF crBkg = RGB(0, 0, 0);
	COLORREF crText = RGB(0, 255, 0);

	if (DX.DDSScreenPic) DestroyPic();

	//----------------------------------------------------// creation of the dc & bitmap

	hdc = GetDC(NULL);                                   // create a dc
	hdcMem = CreateCompatibleDC(hdc);
	ReleaseDC(NULL, hdc);

	memset(&bmi, 0, sizeof(BITMAPINFO));                    // create a 24bit dib
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = 128;
	bmi.bmiHeader.biHeight = -96;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 24;
	bmi.bmiHeader.biCompression = BI_RGB;
	hBmp = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS,
		(void **)&pMem, NULL, 0);         // pMem will point to 128x96x3 bitmap data

	hBmpMem = (HBITMAP)SelectObject(hdcMem, hBmp);       // sel the bmp into the dc

	//----------------------------------------------------// ok, the following is just a drawing example... change it...
														  // create & select an additional font... whatever you want to paint, paint it in the dc :)
	hBrush = CreateSolidBrush(crBkg);
	hPen = CreatePen(PS_SOLID, 0, crFrame);

	hBrushMem = (HBRUSH)SelectObject(hdcMem, hBrush);
	hPenMem = (HPEN)SelectObject(hdcMem, hPen);
	hFontMem = (HFONT)SelectObject(hdcMem, hGFont);

	SetTextColor(hdcMem, crText);
	SetBkColor(hdcMem, crBkg);

	Rectangle(hdcMem, r.left, r.top, r.right, r.bottom);      // our example: fill rect and paint border
	InflateRect(&r, -3, -2);                                // reduce the text area

	LoadString(hInst, IDS_INFO0 + iMPos, szB, 255);
	DrawText(hdcMem, szB, strlen(szB), &r,                   // paint the text (including clipping and word break)
			 DT_LEFT | DT_WORDBREAK);

	//----------------------------------------------------// ok, now store the pMem data, or just call the gpu func

	CreatePic(pMem);

	//----------------------------------------------------// finished, now we clean up... needed, or you will get resource leaks :)

	SelectObject(hdcMem, hBmpMem);                         // sel old mem dc objects
	SelectObject(hdcMem, hBrushMem);
	SelectObject(hdcMem, hPenMem);
	SelectObject(hdcMem, hFontMem);
	DeleteDC(hdcMem);                                     // delete mem dcs
	DeleteObject(hBmp);
	DeleteObject(hBrush);                                 // delete created objects
	DeleteObject(hPen);
}
///////////////////////////////////////////////////////////////////////
