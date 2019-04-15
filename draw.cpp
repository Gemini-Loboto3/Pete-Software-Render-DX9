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
int            iUseScanLines = 0;
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
#if !USE_DX9
static         DDSURFACEDESC9 ddsd;
#else
static			D3DSURFACE_DESC ddsd;
#endif
GUID           guiDev;
BOOL           bDeviceOK;
HWND           hWGPU;
int			   iSysMemory = 0;
int            iFPSEInterface = 0;
int			   iRefreshRate;
BOOL		   bVsync = FALSE;
BOOL		   bVsync_Key = FALSE;

void(*BlitScreen) (unsigned char *, long, long);

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

////////////////////////////////////////////////////////////////////////
static __inline void WaitVBlank(void)
{
	if (bVsync_Key)
	{
	#if !USE_DX9
		IDirectDraw2_WaitForVerticalBlank(DX.DD, DDWAITVB_BLOCKBEGIN, 0);
	#else
		DX.Device->WaitForVBlank(0);
	#endif
	}
}

////////////////////////////////////////////////////////////////////////
void BlitScreen32(unsigned char * surf, long x, long y)  // BLIT IN 32bit COLOR MODE
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
				*((unsigned long *)((surf)+(column*ddsd.Width * 4) + row * 4)) =
					((((s << 19) & 0xf80000) | ((s << 6) & 0xf800) | ((s >> 7) & 0xf8)) & 0xffffff) | 0xff000000;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////
void DoClearScreenBuffer(void)                         // CLEAR DX BUFFER
{
#if !USE_DX9
	DDBLTFX     ddbltfx;

	ddbltfx.dwSize = sizeof(ddbltfx);
	ddbltfx.dwFillColor = 0x00000000;

	IDirectDrawSurface_Blt(DX.DDSRender, NULL, NULL, NULL, DDBLT_COLORFILL, &ddbltfx);

	if (iUseNoStretchBlt >= 3)
	{
		if (pSaISmallBuff) memset(pSaISmallBuff, 0, 512 * 512 * 4);
	}
#else
	DX.Device->ColorFill(DX.DDSRender, NULL, D3DCOLOR_XRGB(0, 0, 0));
#endif
}

////////////////////////////////////////////////////////////////////////

void DoClearFrontBuffer(void)                         // CLEAR PRIMARY BUFFER
{
#if !USE_DX9
	DDBLTFX     ddbltfx;

	ddbltfx.dwSize = sizeof(ddbltfx);
	ddbltfx.dwFillColor = 0x00000000;

	IDirectDrawSurface_Blt(DX.DDSPrimary, NULL, NULL, NULL, DDBLT_COLORFILL, &ddbltfx);
#else
	DX.Device->ColorFill(DX.DDSRender/*DX.DDSPrimary*/, NULL, D3DCOLOR_XRGB(0, 0, 0));
#endif
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
	if (DX.DDSScreenPic) DisplayPic();
}

////////////////////////////////////////////////////////////////////////

//void NoStretchedBlitEx(void)
//{
//	static int iOldDX = 0;
//	static int iOldDY = 0;
//
//	int iDX, iDY, iX, iY; float fXS, fYS, fS;
//
//	if (!PreviousPSXDisplay.DisplayMode.x) return;
//	if (!PreviousPSXDisplay.DisplayMode.y) return;
//
//	fXS = (float)iResX / (float)PreviousPSXDisplay.DisplayMode.x;
//	fYS = (float)iResY / (float)PreviousPSXDisplay.DisplayMode.y;
//	if (fXS < fYS) fS = fXS; else fS = fYS;
//
//	iDX = (int)(PreviousPSXDisplay.DisplayMode.x*fS);
//	iDY = (int)(PreviousPSXDisplay.DisplayMode.y*fS);
//
//	iX = iResX - iDX;
//	iY = iResY - iDY;
//
//	if (iX < 0) iX = 0;
//	else     iX = iX / 2;
//
//	if (iY < 0) iY = 0;
//	else     iY = iY / 2;
//
//	if (iOldDX != iDX || iOldDY != iDY)
//	{
//	#if !USE_DX9
//		DDBLTFX     ddbltfx;
//		ddbltfx.dwSize = sizeof(ddbltfx);
//		ddbltfx.dwFillColor = 0x00000000;
//		IDirectDrawSurface_Blt(DX.DDSPrimary, NULL, NULL, NULL, DDBLT_COLORFILL, &ddbltfx);
//	#else
//		DX.Device->ColorFill(DX.DDSPrimary, NULL, 0x000000);
//	#endif
//		iOldDX = iDX; iOldDY = iDY;
//	}
//
//	if (iWindowMode)
//	{
//		RECT ScreenRect, ViewportRect;
//		POINT Point = { 0, 0 };
//		ClientToScreen(DX.hWnd, &Point);
//		Point.x += iX; Point.y += iY;
//
//		ScreenRect.left = Point.x;
//		ScreenRect.top = Point.y;
//		ScreenRect.right = iDX + Point.x;
//		ScreenRect.bottom = iDY + Point.y;
//
//		ViewportRect.left = 0;
//		ViewportRect.top = 0;
//		ViewportRect.right = PreviousPSXDisplay.DisplayMode.x;
//		ViewportRect.bottom = PreviousPSXDisplay.DisplayMode.y;
//
//		WaitVBlank();
//	#if !USE_DX9
//		IDirectDrawSurface_Blt(DX.DDSPrimary, &ScreenRect, DX.DDSRender, &ViewportRect, DDBLT_WAIT, NULL);
//	#else
//		DX.Device->StretchRect(DX.DDSRender, &ViewportRect, DX.DDSPrimary, &ScreenRect, D3DTEXTUREFILTERTYPE::D3DTEXF_ANISOTROPIC);
//	#endif
//	}
//	else
//	{
//		RECT ScreenRect, ViewportRect;
//
//		ScreenRect.left = iX;
//		ScreenRect.top = iY;
//		ScreenRect.right = iDX + iX;
//		ScreenRect.bottom = iDY + iY;
//
//		ViewportRect.left = 0;
//		ViewportRect.top = 0;
//		ViewportRect.right = PreviousPSXDisplay.DisplayMode.x;
//		ViewportRect.bottom = PreviousPSXDisplay.DisplayMode.y;
//
//		WaitVBlank();
//	#if !USE_DX9
//		IDirectDrawSurface_Blt(DX.DDSPrimary, &ScreenRect, DX.DDSRender, &ViewportRect, DDBLT_WAIT, NULL);
//	#else
//		DX.Device->StretchRect(DX.DDSRender, &ViewportRect, DX.DDSPrimary, &ScreenRect, D3DTEXTUREFILTERTYPE::D3DTEXF_ANISOTROPIC);
//	#endif
//	}
//	if (DX.DDSScreenPic) DisplayPic();
//}

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
#if !USE_DX9
	HRESULT ddrval;
	ddrval = IDirectDrawSurface_Lock(DX.DDSRender, NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);

	if (ddrval == DDERR_SURFACELOST)
	{
		IDirectDrawSurface_Restore(DX.DDSRender);
	}

	if (ddrval != DD_OK)
	{
		IDirectDrawSurface_Unlock(DX.DDSRender, &ddsd);
		return;
	}
#else
	D3DLOCKED_RECT Rect;
	if (FAILED(DX.DDSRender->LockRect(&Rect, NULL, D3DLOCK_DONOTWAIT | D3DLOCK_DISCARD)))
	{
		DX.DDSRender->UnlockRect();
		return;
	}
	DX.DDSRender->GetDesc(&ddsd);
#endif

	//----------------------------------------------------//

	x = PSXDisplay.DisplayPosition.x;
	y = PSXDisplay.DisplayPosition.y;

	//----------------------------------------------------//

	BlitScreen((unsigned char *)Rect.pBits, x, y);      // fill DDSRender surface

	if (usCursorActive) ShowGunCursor((unsigned char *)Rect.pBits);

#if !USE_DX9
	IDirectDrawSurface_Unlock(DX.DDSRender, &ddsd);
#else
	DX.DDSRender->UnlockRect();
#endif

	//SaveSurface(DX.DDSRender, "Test.png");

	if (ulKeybits & KEY_SHOWFPS) DisplayText();              // paint menu text

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

		if (iUseScanLines == 2)                                // stupid nvidia scanline mode
		{
			RECT HelperRect = { 0, 0, iResX, iResY };

			WaitVBlank();
#if !USE_DX9
			IDirectDrawSurface_Blt(DX.DDSHelper, &HelperRect, DX.DDSRender, &ViewportRect, DDBLT_WAIT, NULL);
			IDirectDrawSurface_Blt(DX.DDSPrimary, &ScreenRect, DX.DDSHelper, &HelperRect, DDBLT_WAIT, NULL);
#else
			//DX.Device->StretchRect(DX.DDSRender, &ViewportRect, DX.DDSHelper,  &HelperRect, D3DTEXTUREFILTERTYPE::D3DTEXF_ANISOTROPIC);
			//DX.Device->StretchRect(DX.DDSHelper, &HelperRect,   DX.DDSPrimary, &ScreenRect, D3DTEXTUREFILTERTYPE::D3DTEXF_ANISOTROPIC);
#endif
		}
		else
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

		if (iUseScanLines == 2)		// stupid nvidia scanline mode
		{
			WaitVBlank();
		#if !USE_DX9
			IDirectDrawSurface_Blt(DX.DDSHelper, &ScreenRect, DX.DDSRender, &ViewportRect, DDBLT_WAIT, NULL);
			IDirectDrawSurface_Blt(DX.DDSPrimary, &ScreenRect, DX.DDSHelper, &ScreenRect, DDBLT_WAIT, NULL);
		#else
		#endif
		}
		else
		{
			WaitVBlank();
		#if !USE_DX9
			IDirectDrawSurface_Blt(DX.DDSPrimary, &ScreenRect, DX.DDSRender, &ViewportRect, DDBLT_WAIT, NULL);
		#else
		#endif
		}
	}

	if (DX.DDSScreenPic)
		DisplayPic();
}

////////////////////////////////////////////////////////////////////////
// GAMMA
////////////////////////////////////////////////////////////////////////

int iUseGammaVal = 2048;

void DXSetGamma(void)
{
#if !USE_DX9
	float g;
	if (iUseGammaVal == 2048) return;

	g = (float)iUseGammaVal;
	if (g > 512) g = ((g - 512) * 2) + 512;
	g = 0.5f + ((g) / 1024.0f);

	// some cards will cheat... so we don't trust the caps here
	// if (DD_Caps.dwCaps2 & DDCAPS2_PRIMARYGAMMA)
	{
		float f; DDGAMMARAMP ramp; int i;
		LPDIRECTDRAWGAMMACONTROL DD_Gamma = NULL;

		if FAILED(IDirectDrawSurface_QueryInterface(DX.DDSPrimary, &IID_IDirectDrawGammaControl, (void**)&DD_Gamma))
			return;

		for (i = 0; i < 256; i++)
		{
			f = (((float)(i * 256))*g);
			if (f > 65535) f = 65535;
			ramp.red[i] = ramp.green[i] = ramp.blue[i] = (WORD)f;
		}

		IDirectDrawGammaControl_SetGammaRamp(DD_Gamma, 0, &ramp);
		IDirectDrawGammaControl_Release(DD_Gamma);
	}
#endif
}

////////////////////////////////////////////////////////////////////////
// SCAN LINE STUFF
////////////////////////////////////////////////////////////////////////

//void SetScanLineList(LPDIRECTDRAWCLIPPER Clipper)
//{
//	LPRGNDATA lpCL; RECT * pr; int y; POINT Point = { 0,0 };
//
//	IDirectDrawClipper_SetClipList(Clipper, NULL, 0);
//
//	lpCL = (LPRGNDATA)malloc(sizeof(RGNDATAHEADER) + ((iResY / 2) + 1) * sizeof(RECT));
//	if (iWindowMode) ClientToScreen(DX.hWnd, &Point);
//
//	lpCL->rdh.dwSize = sizeof(RGNDATAHEADER);
//	lpCL->rdh.iType = RDH_RECTANGLES;
//	lpCL->rdh.nCount = iResY / 2;
//	lpCL->rdh.nRgnSize = 0;
//	lpCL->rdh.rcBound.left = Point.x;
//	lpCL->rdh.rcBound.top = Point.y;
//	lpCL->rdh.rcBound.bottom = Point.y + iResY;
//	lpCL->rdh.rcBound.right = Point.x + iResX;
//
//	pr = (RECT *)lpCL->Buffer;
//	for (y = 0; y < iResY; y += 2)
//	{
//		pr->left = Point.x;
//		pr->top = Point.y + y;
//		pr->right = Point.x + iResX;
//		pr->bottom = Point.y + y + 1;
//		pr++;
//	}
//
//	IDirectDrawClipper_SetClipList(Clipper, lpCL, 0);
//
//	free(lpCL);
//}

////////////////////////////////////////////////////////////////////////

void MoveScanLineArea(HWND hwnd)
{
#if !USE_DX9
	LPDIRECTDRAWCLIPPER Clipper; HRESULT h;
	DDBLTFX ddbltfx; RECT r;

	if (FAILED(h = IDirectDraw_CreateClipper(DX.DD, 0, &Clipper, NULL)))
		return;

	IDirectDrawSurface_SetClipper(DX.DDSPrimary, NULL);

	ddbltfx.dwSize = sizeof(ddbltfx);
	ddbltfx.dwFillColor = 0x00000000;

	// Fixed Scanline Mode for FullScreen where desktop was visible
	// in background

	if (iWindowMode)
	{
		GetClientRect(hwnd, &r);
		ClientToScreen(hwnd, (LPPOINT)&r.left);
		r.right += r.left;
		r.bottom += r.top;
	}
	else
	{
		r.left = 0;
		r.top = 0;
		r.right = iResX;
		r.bottom = iResY;
	}

	IDirectDrawSurface_Blt(DX.DDSPrimary, &r, NULL, NULL, DDBLT_COLORFILL, &ddbltfx);

	SetScanLineList(Clipper);

	IDirectDrawSurface_SetClipper(DX.DDSPrimary, Clipper);
	IDirectDrawClipper_Release(Clipper);
#endif
}

////////////////////////////////////////////////////////////////////////
// MAIN DIRECT DRAW INIT
////////////////////////////////////////////////////////////////////////

BOOL ReStart = FALSE;

int DXinitialize()
{
	IDirect3D9Ex *DD;

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

	HRESULT hr;
	D3DFORMAT depth_fmt = D3DFMT_D32F_LOCKABLE;
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
	d3dpp.PresentationInterval =
#if 1
		D3DPRESENT_INTERVAL_IMMEDIATE;
#else
		D3DPRESENT_INTERVAL_DEFAULT;
#endif
	if (FAILED(DX.DD->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, DX.hWnd, dwBehaviorFlags, &d3dpp, NULL, &DX.Device)))
	{
		MessageBox(NULL, "Failed to create D3D device", "Error", MB_OK);
		return 0;
	}

	//////////////////////////////////////////////////////// main surfaces
	//DX.Device->CreateRenderTarget(d3dpp.BackBufferWidth, d3dpp.BackBufferHeight,
	//	DX.d3ddm.Format, D3DMULTISAMPLE_NONE, 0, TRUE, &DX.DDSPrimary, NULL);

	//----------------------------------------------------//
	if (FAILED(DX.Device->CreateOffscreenPlainSurface(1024, 512, DX.d3ddm.Format, D3DPOOL_DEFAULT, &DX.DDSRender, NULL)))
		return 0;

	BlitScreen = BlitScreen32;
	iDesktopCol = 32; //GetSystemMetrics(SM_CYSCREEN);

	//////////////////////////////////////////////////////// small screen clean up

	DXSetGamma();
	//DX.Device->ColorFill(DX.DDSPrimary, NULL, D3DCOLOR_XRGB(0, 0, 0));
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
		//if (DX.DDSHelper)
		//{
			//DX.DDSHelper->Release();
			//DX.DDSHelper = 0;
		//}
		if (DX.DDSScreenPic)
		{
			DX.DDSScreenPic->Release();
			DX.DDSScreenPic = 0;
		}
		DX.DDSRender->Release();
		//DX.DDSPrimary->Release();
		DX.Device->Release();
		DX.DD->Release();

		ReStart = TRUE;
		bIsFirstFrame = TRUE;
	}
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

DWORD  dwGPUStyle = 0;                                   // vars to store some wimdows stuff
HANDLE hGPUMenu = NULL;

unsigned long ulInitDisplay(void)
{
	HDC hdc; RECT r;

	if (iWindowMode)                                       // win mode?
	{
		DWORD dw = GetWindowLong(hWGPU, GWL_STYLE);    // -> adjust wnd style
		dwGPUStyle = dw;
		dw &= ~WS_THICKFRAME;
		dw |= WS_BORDER | WS_CAPTION;
		SetWindowLong(hWGPU, GWL_STYLE, dw);

		iResX = LOWORD(iWinSize); iResY = HIWORD(iWinSize);
		ShowWindow(hWGPU, SW_SHOWNORMAL);

		if (iUseScanLines)
			SetWindowPos(hWGPU, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

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

		dw &= ~(WS_THICKFRAME | WS_BORDER | WS_CAPTION);
		SetWindowLong(hWGPU, GWL_STYLE, dw);
		SetMenu(hWGPU, NULL);

		ShowWindow(hWGPU, SW_SHOWMAXIMIZED);          // -> max mode
	}

	r.left = r.top = 0; r.right = iResX; r.bottom = iResY;          // init bkg with black
	hdc = GetDC(hWGPU);
	FillRect(hdc, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
	ReleaseDC(hWGPU, hdc);

	DXinitialize();                                       // init direct draw (not D3D... oh, well)

	if (!iWindowMode)                                      // fullscreen mode?
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
#if !USE_DX9
	DDSURFACEDESC xddsd; HRESULT ddrval;

	memset(&xddsd, 0, sizeof(DDSURFACEDESC));
	xddsd.dwSize = sizeof(DDSURFACEDESC);
	xddsd.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS;
	// xddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;

	if (iSysMemory)
		xddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	else
		xddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;

	xddsd.dwWidth = 128;
	xddsd.dwHeight = 96;

	if (IDirectDraw_CreateSurface(DX.DD, &xddsd, &DX.DDSScreenPic, NULL))
	#else
	if (FAILED(DX.Device->CreateOffscreenPlainSurface(128, 128, D3DFORMAT::D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &DX.DDSScreenPic, NULL)))
	#endif
	{
		DX.DDSScreenPic = 0;
		return;
	}

#if !USE_DX9
	ddrval = IDirectDrawSurface_Lock(DX.DDSScreenPic, NULL, &xddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);

	if (ddrval == DDERR_SURFACELOST)
	{
		IDirectDrawSurface_Restore(DX.DDSScreenPic);
	}

	if (ddrval != DD_OK)
	{
		IDirectDrawSurface_Unlock(DX.DDSScreenPic, &xddsd);
		return;
	}

	ps = (unsigned char *)xddsd.lpSurface;
#else
	D3DLOCKED_RECT Rect;
	DX.DDSScreenPic->LockRect(&Rect, NULL, D3DLOCK_DISCARD);
	ps = (unsigned char*)Rect.pBits;
#endif

#if 0
	if (iDesktopCol == 16)
	{
		unsigned short s;
		for (y = 0; y < 96; y++)
		{
			for (x = 0; x < 128; x++)
			{
				s = (*(pMem + 0)) >> 3;
				s |= ((*(pMem + 1)) & 0xfc) << 3;
				s |= ((*(pMem + 2)) & 0xf8) << 8;
				pMem += 3;
				*((unsigned short *)(ps + y * Rect.Pitch + x * 2)) = s;
			}
		}
	}
	else if (iDesktopCol == 15)
	{
		unsigned short s;
		for (y = 0; y < 96; y++)
		{
			for (x = 0; x < 128; x++)
			{
				s = (*(pMem + 0)) >> 3;
				s |= ((*(pMem + 1)) & 0xfc) << 2;
				s |= ((*(pMem + 2)) & 0xf8) << 7;
				pMem += 3;
				*((unsigned short *)(ps + y * Rect.Pitch + x * 2)) = s;
			}
		}
	}
	else if (iDesktopCol == 32)
	#endif
	{
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
	}

#if !USE_DX9
	IDirectDrawSurface_Unlock(DX.DDSScreenPic, &xddsd);
#else
	DX.DDSScreenPic->UnlockRect();
#endif
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
	DX.Device->StretchRect(DX.DDSScreenPic, &HelperRect, DX.DDSRender, &ScreenRect, D3DTEXTUREFILTERTYPE::D3DTEXF_ANISOTROPIC);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////

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
