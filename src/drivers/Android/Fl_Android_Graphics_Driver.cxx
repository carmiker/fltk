//
// "$Id$"
//
// Graphics routines for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2018 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     http://www.fltk.org/COPYING.php
//
// Please report all bugs and problems on the following page:
//
//     http://www.fltk.org/str.php
//


#include "../../config_lib.h"
#include "Fl_Android_Application.H"
#include "Fl_Android_Graphics_Driver.H"
#include "Fl_Android_Screen_Driver.H"
#include <FL/Fl.H>
#include <FL/platform.H>
#include <errno.h>


/*
 * By linking this module, the following static method will instantiate the
 * Windows GDI Graphics driver as the main display driver.
 */
Fl_Graphics_Driver *Fl_Graphics_Driver::newMainGraphicsDriver()
{
  return new Fl_Android_Graphics_Driver();
}


Fl_Android_Graphics_Driver::Fl_Android_Graphics_Driver() :
        pStride(0), pBits(0)
{
}


Fl_Android_Graphics_Driver::~Fl_Android_Graphics_Driver()
{
}


void Fl_Android_Graphics_Driver::make_current(Fl_Window *win)
{
  // The Stride is the offset between lines in the graphics buffer
  pStride = Fl_Android_Application::graphics_buffer().stride;
  // Bits is the memory address of the top left corner of the window
  pBits = ((uint16_t*)(Fl_Android_Application::graphics_buffer().bits))
          + win->x_root() + pStride * win->y_root();

  // TODO: set the clipping area
  // set the clipping area to the physical screen size in window coordinates
  pWindowRegion.set(-win->x(), -win->y(), 600, 800);
  pWindowRegion.intersect_with(Fl_Rect_Region(0, 0, win->w(), win->h()));

  pDesktopWindowRegion.set(pWindowRegion);

  // remove all window rectangles that are positioned on top of this window
  // TODO: this region is expensive to calculate. Cache it for each window and recalculate when windows move, show, hide, or change order
  Fl_Window *wTop = Fl::first_window();
  while (wTop) {
    if (wTop==win) break;
    Fl_Rect_Region r(wTop->x()-win->x(), wTop->y()-win->y(), wTop->w(), wTop->h());
    pDesktopWindowRegion.subtract(r);
    wTop = Fl::next_window(wTop);
  }
  pClippingRegion.set(pDesktopWindowRegion);
}


static uint16_t  make565(int red, int green, int blue)
{
    return (uint16_t)( ((red   << 8) & 0xf800) |
                       ((green << 3) & 0x07e0) |
                       ((blue  >> 3) & 0x001f) );
}

extern unsigned fl_cmap[256];


uint16_t Fl_Android_Graphics_Driver::make565(Fl_Color crgba)
{
  if (crgba<0x00000100) crgba = fl_cmap[crgba];
    return (uint16_t)( ((crgba >>16) & 0xf800) |
                       ((crgba >>13) & 0x07e0) |
                       ((crgba >>11) & 0x001f) );
}


void Fl_Android_Graphics_Driver::rectf_unscaled(float x, float y, float w, float h)
{
  for (const auto &it: pClippingRegion.overlapping(Fl_Rect_Region(x, y, w, h))) {
    Fl_Rect_Region &s = it->clipped_rect();
    rectf_unclipped(s.x(), s.y(), s.w(), s.h());
  }
}


void Fl_Android_Graphics_Driver::rectf_unclipped(float x, float y, float w, float h)
{
  if (w<=0 || h<=0) return;

  uint16_t cc = make565(color());
  int32_t ss = pStride;
  uint16_t *bits = pBits;
  uint32_t xx = (uint32_t)x;
  uint32_t yy = (uint32_t)y;
  uint32_t ww = (uint32_t)w;
  uint32_t hh = (uint32_t)h;
  for (uint32_t iy = 0; iy<hh; ++iy) {
    uint16_t *d = bits + (iy+yy)*ss + xx;
    for (uint32_t ix = ww; ix>0; --ix) {
      *d++ = cc;
    }
  }
}


void Fl_Android_Graphics_Driver::xyline_unscaled(float x, float y, float x1)
{
  float w;
  if (x1>x) {
    w = x1-x;
  } else {
    w = x-x1;
    x = x1;
  }
  for (const auto &it: pClippingRegion.overlapping(Fl_Rect_Region(x, y, w, 1))) {
    Fl_Rect_Region &s = it->clipped_rect();
    xyline_unclipped(s.x(), s.y(), s.right());
  }
}


void Fl_Android_Graphics_Driver::xyline_unclipped(float x, float y, float x1)
{
  uint16_t cc = make565(color());
  float w = x1-x;
  int32_t ss = pStride;
  uint16_t *bits = pBits;
  uint32_t xx = (uint32_t)x;
  uint32_t yy = (uint32_t)y;
  uint32_t ww = (uint32_t)w;
  uint16_t *d = bits + yy*ss + xx;
  for (uint32_t ix = ww; ix>0; --ix) {
    *d++ = cc;
  }
}

void Fl_Android_Graphics_Driver::yxline_unscaled(float x, float y, float y1)
{
  float h;
  if (y1>y) {
    h = y1-y;
  } else {
    h = y-y1;
    y = y1;
  }
  for (const auto &it: pClippingRegion.overlapping(Fl_Rect_Region(x, y, 1, h))) {
    Fl_Rect_Region &s = it->clipped_rect();
    yxline_unclipped(s.x(), s.y(), s.bottom());
  }
}

void Fl_Android_Graphics_Driver::yxline_unclipped(float x, float y, float y1)
{
  uint16_t cc = make565(color());
  float h = y1-y;
  int32_t ss = pStride;
  uint16_t *bits = pBits;
  uint32_t xx = (uint32_t)x;
  uint32_t yy = (uint32_t)y;
  uint32_t hh = (uint32_t)h;
  uint16_t *d = bits + yy*ss + xx;
  for (uint32_t iy = hh; iy>0; --iy) {
    *d = cc;
    d += ss;
  }
}



#if 0

// Code used to switch output to an off-screen window.  See macros in
// win32.H which save the old state in local variables.

typedef struct { BYTE a; BYTE b; BYTE c; BYTE d; } FL_BLENDFUNCTION;
typedef BOOL (WINAPI* fl_alpha_blend_func)
(HDC,int,int,int,int,HDC,int,int,int,int,FL_BLENDFUNCTION);
static fl_alpha_blend_func fl_alpha_blend = NULL;
static FL_BLENDFUNCTION blendfunc = { 0, 0, 255, 1};

/* Reference to the current device context
 For back-compatibility only. The preferred procedure to get this reference is
 Fl_Surface_Device::surface()->driver()->gc().
 */
HDC fl_gc = 0;

void Fl_GDI_Graphics_Driver::global_gc()
{
  fl_gc = (HDC)gc();
}

/*
 * This function checks if the version of Windows that we
 * curently run on supports alpha blending for bitmap transfers
 * and finds the required function if so.
 */
char Fl_GDI_Graphics_Driver::can_do_alpha_blending() {
  static char been_here = 0;
  static char can_do = 0;
  // do this test only once
  if (been_here) return can_do;
  been_here = 1;
  // load the library that implements alpha blending
  HMODULE hMod = LoadLibrary("MSIMG32.DLL");
  // give up if that doesn't exist (Win95?)
  if (!hMod) return 0;
  // now find the blending function inside that dll
  fl_alpha_blend = (fl_alpha_blend_func)GetProcAddress(hMod, "AlphaBlend");
  // give up if we can't find it (Win95)
  if (!fl_alpha_blend) return 0;
  // we have the call, but does our display support alpha blending?
  // get the desktop's device context
  HDC dc = GetDC(0L);
  if (!dc) return 0;
  // check the device capabilities flags. However GetDeviceCaps
  // does not return anything useful, so we have to do it manually:

  HBITMAP bm = CreateCompatibleBitmap(dc, 1, 1);
  HDC new_gc = CreateCompatibleDC(dc);
  int save = SaveDC(new_gc);
  SelectObject(new_gc, bm);
  /*COLORREF set = */ SetPixel(new_gc, 0, 0, 0x01010101);
  BOOL alpha_ok = fl_alpha_blend(dc, 0, 0, 1, 1, new_gc, 0, 0, 1, 1, blendfunc);
  RestoreDC(new_gc, save);
  DeleteDC(new_gc);
  DeleteObject(bm);
  ReleaseDC(0L, dc);

  if (alpha_ok) can_do = 1;
  return can_do;
}

HDC fl_makeDC(HBITMAP bitmap) {
  HDC new_gc = CreateCompatibleDC((HDC)Fl_Graphics_Driver::default_driver().gc());
  SetTextAlign(new_gc, TA_BASELINE|TA_LEFT);
  SetBkMode(new_gc, TRANSPARENT);
#if USE_COLORMAP
  if (fl_palette) SelectPalette(new_gc, fl_palette, FALSE);
#endif
  SelectObject(new_gc, bitmap);
  return new_gc;
}

void Fl_GDI_Graphics_Driver::copy_offscreen(int x, int y, int w, int h, Fl_Offscreen bitmap, int srcx, int srcy) {
  HDC new_gc = CreateCompatibleDC(gc_);
  int save = SaveDC(new_gc);
  SelectObject(new_gc, bitmap);
  BitBlt(gc_, x*scale_, y*scale_, w*scale_, h*scale_, new_gc, srcx*scale_, srcy*scale_, SRCCOPY);
  RestoreDC(new_gc, save);
  DeleteDC(new_gc);
}

BOOL Fl_GDI_Graphics_Driver::alpha_blend_(int x, int y, int w, int h, HDC src_gc, int srcx, int srcy, int srcw, int srch) {
  return fl_alpha_blend(gc_, x, y, w, h, src_gc, srcx, srcy, srcw, srch, blendfunc);
}

#if ! defined(FL_DOXYGEN)
void Fl_GDI_Graphics_Driver::copy_offscreen_with_alpha(int x,int y,int w,int h,HBITMAP bitmap,int srcx,int srcy) {
  HDC new_gc = CreateCompatibleDC(gc_);
  int save = SaveDC(new_gc);
  SelectObject(new_gc, bitmap);
  BOOL alpha_ok = 0;
  // first try to alpha blend
  if ( can_do_alpha_blending() ) {
    alpha_ok = alpha_blend_(x, y, w, h, new_gc, srcx, srcy, w, h);
  }
  // if that failed (it shouldn't), still copy the bitmap over, but now alpha is 1
  if (!alpha_ok) {
    BitBlt(gc_, x, y, w, h, new_gc, srcx, srcy, SRCCOPY);
  }
  RestoreDC(new_gc, save);
  DeleteDC(new_gc);
}

void Fl_GDI_Graphics_Driver::translate_all(int x, int y) {
  const int stack_height = 10;
  if (depth == -1) {
    origins = new POINT[stack_height];
    depth = 0;
  }
  if (depth >= stack_height)  {
    Fl::warning("Fl_Copy/Image_Surface: translate stack overflow!");
    depth = stack_height - 1;
  }
  GetWindowOrgEx((HDC)gc(), origins+depth);
  SetWindowOrgEx((HDC)gc(), origins[depth].x - x*scale_, origins[depth].y - y*scale_, NULL);
  depth++;
}

void Fl_GDI_Graphics_Driver::untranslate_all() {
  if (depth > 0) depth--;
  SetWindowOrgEx((HDC)gc(), origins[depth].x, origins[depth].y, NULL);
}
#endif

void Fl_GDI_Graphics_Driver::add_rectangle_to_region(Fl_Region r, int X, int Y, int W, int H) {
  Fl_Region R = XRectangleRegion(X, Y, W, H);
  CombineRgn(r, r, R, RGN_OR);
  XDestroyRegion(R);
}

void Fl_GDI_Graphics_Driver::transformed_vertex0(float x, float y) {
  if (!n || x != p[n-1].x || y != p[n-1].y) {
    if (n >= p_size) {
      p_size = p ? 2*p_size : 16;
      p = (POINT*)realloc((void*)p, p_size*sizeof(*p));
    }
    p[n].x = x;
    p[n].y = y;
    n++;
  }
}

void Fl_GDI_Graphics_Driver::fixloop() {  // remove equal points from closed path
  while (n>2 && p[n-1].x == p[0].x && p[n-1].y == p[0].y) n--;
}

Fl_Region Fl_GDI_Graphics_Driver::XRectangleRegion(int x, int y, int w, int h) {
  if (Fl_Surface_Device::surface() == Fl_Display_Device::display_device()) return CreateRectRgn(x,y,x+w,y+h);
  // because rotation may apply, the rectangle becomes a polygon in device coords
  POINT pt[4] = { {x, y}, {x + w, y}, {x + w, y + h}, {x, y + h} };
  LPtoDP((HDC)fl_graphics_driver->gc(), pt, 4);
  return CreatePolygonRgn(pt, 4, ALTERNATE);
}

void Fl_GDI_Graphics_Driver::XDestroyRegion(Fl_Region r) {
  DeleteObject(r);
}


typedef BOOL(WINAPI* flTypeImmAssociateContextEx)(HWND, HIMC, DWORD);
extern flTypeImmAssociateContextEx flImmAssociateContextEx;
typedef HIMC(WINAPI* flTypeImmGetContext)(HWND);
extern flTypeImmGetContext flImmGetContext;
typedef BOOL(WINAPI* flTypeImmSetCompositionWindow)(HIMC, LPCOMPOSITIONFORM);
extern flTypeImmSetCompositionWindow flImmSetCompositionWindow;
typedef BOOL(WINAPI* flTypeImmReleaseContext)(HWND, HIMC);
extern flTypeImmReleaseContext flImmReleaseContext;


void Fl_GDI_Graphics_Driver::reset_spot()
{
}

void Fl_GDI_Graphics_Driver::set_spot(int font, int size, int X, int Y, int W, int H, Fl_Window *win)
{
  if (!win) return;
  Fl_Window* tw = win;
  while (tw->parent()) tw = tw->window(); // find top level window

  if (!tw->shown())
    return;

  HIMC himc = flImmGetContext(fl_xid(tw));

  if (himc) {
    COMPOSITIONFORM cfs;
    cfs.dwStyle = CFS_POINT;
    cfs.ptCurrentPos.x = X;
    cfs.ptCurrentPos.y = Y - tw->labelsize();
    MapWindowPoints(fl_xid(win), fl_xid(tw), &cfs.ptCurrentPos, 1);
    flImmSetCompositionWindow(himc, &cfs);
    flImmReleaseContext(fl_xid(tw), himc);
  }
}


void Fl_GDI_Graphics_Driver::scale(float f) {
  if (f != scale_) {
    size_ = 0;
    scale_ = f;
//fprintf(LOG,"set scale to %f\n",f);fflush(LOG);
  }
}


/* Rescale region r with factor f and returns the scaled region.
 Region r is returned unchanged if r is null or f is 1.
 The input region is deleted if dr is null.
 */
HRGN Fl_GDI_Graphics_Driver::scale_region(HRGN r, float f, Fl_GDI_Graphics_Driver *dr) {
  if (r && f != 1) {
    DWORD size = GetRegionData(r, 0, NULL);
    RGNDATA *pdata = (RGNDATA*)malloc(size);
    GetRegionData(r, size, pdata);
    if (!dr) DeleteObject(r);
    POINT pt = {0, 0};
    if (dr && dr->depth >= 1) { // account for translation
      GetWindowOrgEx((HDC)dr->gc(), &pt);
      pt.x *= (f - 1);
      pt.y *= (f - 1);
    }
    RECT *rects = (RECT*)&(pdata->Buffer);
    int delta = (f > 1.75 ? 1 : 0) - int(f/2);
    for (DWORD i = 0; i < pdata->rdh.nCount; i++) {
      int x = rects[i].left * f + pt.x;
      int y = rects[i].top * f + pt.y;
      RECT R2;
      R2.left = x + delta;
      R2.top  = y + delta;
      R2.right = int(rects[i].right * f) + pt.x - x + R2.left;
      R2.bottom = int(rects[i].bottom * f) + pt.y - y + R2.top;
      rects[i] = R2;
    }
    r = ExtCreateRegion(NULL, size, pdata);
    free(pdata);
  }
  return r;
}


Fl_Region Fl_GDI_Graphics_Driver::scale_clip(float f) {
  HRGN r = rstack[rstackptr];
  HRGN r2 = scale_region(r, f, this);
  return (r == r2 ? NULL : (rstack[rstackptr] = r2, r));
}

void Fl_GDI_Graphics_Driver::set_current_() {
  restore_clip();
}

#endif

/*

 */

//
// End of "$Id$".
//