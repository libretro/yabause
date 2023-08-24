/*  Copyright 2003-2006 Guillaume Duhamel
    Copyright 2004 Lawrence Sebald
    Copyright 2004-2007 Theo Berkau

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*! \file vidcs.c
    \brief OpenGL video renderer
*/
#if defined(HAVE_LIBGL) || defined(__ANDROID__) || defined(IOS)

#include <math.h>
#define EPSILON (1e-10 )


#include "vidcs.h"
#include "vidshared.h"
#include "debug.h"
#include "vdp2.h"
#include "yabause.h"
#include "ygl.h"
#include "yui.h"
#include "vdp1_compute.h"

#define Y_MAX(a, b) ((a) > (b) ? (a) : (b))
#define Y_MIN(a, b) ((a) < (b) ? (a) : (b))

#define LOG_AREA

#define LOG_CMD

static int renderer_started = 0;
static Vdp2 baseVdp2Regs;
static int drawcell_run = 0;

static int vdp2_interlace = 0;

static int isEnabled(int id, Vdp2* varVdp2Regs);
static void VIDCSVdp2DrawScreens(void);
static void Vdp2SetResolution(u16 TVMD);

static Vdp2 baseVdp2Regs;

static int vdp1_interlace = 0;

int GlWidth = 320;
int GlHeight = 224;

int vdp1cor = 0;
int vdp1cog = 0;
int vdp1cob = 0;

static int vdp2busy = 0;
static int screenDirty = 0;

vdp2rotationparameter_struct  Vdp1ParaA;


static void Vdp2DrawRBG0(Vdp2* varVdp2Regs);

static u32 Vdp2ColorRamGetLineColor(u32 colorindex, int alpha);
static int Vdp2PatternAddrPos(vdp2draw_struct *info, int planex, int x, int planey, int y, Vdp2 *varVdp2Regs);
static void Vdp2DrawPatternPos(vdp2draw_struct *info, YglTexture *texture, int x, int y, int cx, int cy,  int lines, Vdp2 *varVdp2Regs);
static INLINE void ReadVdp2ColorOffset(Vdp2 * regs, vdp2draw_struct *info, int mask);
static INLINE u16 Vdp2ColorRamGetColorRaw(u32 colorindex);
static void FASTCALL Vdp2DrawRotation(RBGDrawInfo * rbg, Vdp2 *varVdp2Regs);
static void Vdp2DrawRotation_in_sync(RBGDrawInfo * rbg, Vdp2 *varVdp2Regs);


static int FASTCALL Vdp2CheckWindowRange(vdp2draw_struct *info, int x, int y, int w, int h, Vdp2 *varVdp2Regs);
static void requestDrawCell(vdp2draw_struct * info, YglTexture *texture, Vdp2* varVdp2Regs);
static void requestDrawCellQuad(vdp2draw_struct * info, YglTexture *texture, Vdp2* varVdp2Regs);
static INLINE u32 Vdp2RotationFetchPixel(vdp2draw_struct *info, int x, int y, int cellw);
static u32 Vdp2ColorRamGetLineColorOffset(u32 colorindex, int alpha, int offset);
static void FASTCALL Vdp2DrawBitmapCoordinateInc(vdp2draw_struct *info, YglTexture *texture,  Vdp2 *varVdp2Regs);
static void FASTCALL Vdp2DrawBitmapLineScroll(vdp2draw_struct *info, YglTexture *texture, int width, int height,  Vdp2 *varVdp2Regs);
static void Vdp2DrawMapPerLine(vdp2draw_struct *info, YglTexture *texture, Vdp2 *varVdp2Regs);
static void Vdp2DrawMapTest(vdp2draw_struct *info, YglTexture *texture, int delayed, Vdp2 *varVdp2Regs);
static int Vdp2CheckCharAccessPenalty(int char_access, int ptn_access);
static int sameVDP2Reg(int id, Vdp2 *a, Vdp2 *b);

static void Vdp2GenLineinfo(vdp2draw_struct *info);
static void Vdp2DrawBackScreen(Vdp2 *varVdp2Regs);
static void Vdp2DrawLineColorScreen(Vdp2 *varVdp2Regs);
static void Vdp2DrawRBG1(Vdp2 *varVdp2Regs);
static void Vdp1SetTextureRatio(int vdp2widthratio, int vdp2heightratio);

static void finishRbgQueue();

static vdp2Lineinfo lineNBG0[512];
static vdp2Lineinfo lineNBG1[512];



#define WA_INSIDE (0)
#define WA_OUTSIDE (1)

extern void YglGenReset();


int VIDCSInit(void);
void VIDCSDeInit(void);
void VIDCSResize(int, int, unsigned int, unsigned int, int);
void VIDCSGetScale(float *, float *, int *, int *);
int VIDCSIsFullscreen(void);
int VIDCSVdp1Reset(void);

extern vdp2rotationparameter_struct  Vdp1ParaA;

void VIDCSVdp1Draw();
void VIDCSVdp1NormalSpriteDraw(vdp1cmd_struct *cmd, u8 * ram, Vdp1 * regs, u8* back_framebuffer);
void VIDCSVdp1ScaledSpriteDraw(vdp1cmd_struct *cmd, u8 * ram, Vdp1 * regs, u8* back_framebuffer);
void VIDCSVdp1DistortedSpriteDraw(vdp1cmd_struct *cmd, u8 * ram, Vdp1 * regs, u8* back_framebuffer);
void VIDCSVdp1PolygonDraw(vdp1cmd_struct *cmd, u8 * ram, Vdp1 * regs, u8* back_framebuffer);
void VIDCSVdp1PolylineDraw(vdp1cmd_struct *cmd, u8 * ram, Vdp1 * regs, u8* back_framebuffer);
void VIDCSVdp1LineDraw(vdp1cmd_struct *cmd, u8 * ram, Vdp1 * regs, u8* back_framebuffer);
void VIDCSVdp1UserClipping(vdp1cmd_struct *cmd, u8 * ram, Vdp1 * regs);
void VIDCSVdp1SystemClipping(vdp1cmd_struct *cmd, u8 * ram, Vdp1 * regs);
void VIDCSVdp1DrawFB(void);
void VIDCSReadColorOffset(void);
extern void VIDCSRender(Vdp2 *varVdp2Regs);
extern void VIDCSRenderVDP1(void);
extern void VIDCSFinsihDraw(void);

 void VIDCSVdp1LocalCoordinate(vdp1cmd_struct *cmd, u8 * ram, Vdp1 * regs);
 int VIDCSVdp2Reset(void);
 void VIDCSVdp2Draw(void);
extern void VIDCSGetGlSize(int *width, int *height);
extern void VIDCSGetNativeResolution(int *width, int *height, int*interlace);
extern void VIDCSSetSettingValueMode(int type, int value);
extern void VIDCSSync();
extern void VIDCSGetNativeResolution(int *width, int *height, int*interlace);
extern void VIDCSVdp2DispOff(void);
extern int VIDCSGenFrameBuffer();
extern void VIDCSGenerateBufferVdp1(vdp1cmd_struct* cmd);

extern u32 FASTCALL Vdp1ReadPolygonColor(vdp1cmd_struct *cmd, Vdp2* varVdp2Regs);

VideoInterface_struct VIDCS = {
VIDCORE_CS,
"Compute Shader Video Interface",
VIDCSInit,
VIDCSDeInit,
VIDCSResize,
VIDCSGetScale,
VIDCSIsFullscreen,
VIDCSVdp1Reset,
VIDCSVdp1Draw,
VIDCSVdp1NormalSpriteDraw,
VIDCSVdp1ScaledSpriteDraw,
VIDCSVdp1DistortedSpriteDraw,
VIDCSVdp1PolygonDraw,
VIDCSVdp1PolylineDraw,
VIDCSVdp1LineDraw,
VIDCSVdp1UserClipping,
VIDCSVdp1SystemClipping,
VIDCSVdp1LocalCoordinate,
VIDCSEraseWriteVdp1,
VIDCSFrameChangeVdp1,
VIDCSGenerateBufferVdp1,
VIDCSVdp2Reset,
VIDCSVdp2Draw,
VIDCSGetGlSize,
VIDCSSetSettingValueMode,
VIDCSSync,
VIDCSGetNativeResolution,
VIDCSVdp2DispOff,
VIDCSRender,
VIDCSRenderVDP1,
VIDCSGenFrameBuffer,
VIDCSFinsihDraw,
VIDCSVdp1DrawFB
};


#define LOG_ASYN

static void FASTCALL Vdp2DrawCell_in_sync(vdp2draw_struct *info, YglTexture *texture, Vdp2 *varVdp2Regs);

#define NB_MSG 128

#define CELL_SINGLE 0x1
#define CELL_QUAD   0x2

static void Vdp2DrawPatternPos(vdp2draw_struct *info, YglTexture *texture, int x, int y, int cx, int cy, int lines, Vdp2 *varVdp2Regs)
{
  u64 cacheaddr = ((u32)(info->alpha_per_line[y>>vdp2_interlace] >> 3) << 27) |
    (info->paladdr << 20) | info->charaddr | info->transparencyenable |
    ((info->patternpixelwh >> 4) << 1) | (((u64)(info->coloroffset >> 8) & 0x07) << 32) | (((u64)(info->idScreen) & 0x07) << 39);
  int priority = info->priority;
  YglCache c;
  vdp2draw_struct tile = *info;
  int winmode = 0;

  tile.dst = 0;
  tile.uclipmode = 0;
  tile.colornumber = info->colornumber;
  tile.mosaicxmask = info->mosaicxmask;
  tile.mosaicymask = info->mosaicymask;
  tile.idScreen = info->idScreen;

  tile.cellw = tile.cellh = info->patternpixelwh;
  tile.flipfunction = info->flipfunction;

  if (info->specialprimode == 1) {
    info->priority = (info->priority & 0xFFFFFFFE) | info->specialfunction;
  }

  cacheaddr |= ((u64)(info->priority) & 0x07) << 42;

  tile.priority = info->priority;

  tile.vertices[0] = x;
  tile.vertices[1] = y;
  tile.vertices[2] = (x + tile.cellw);
  tile.vertices[3] = y;
  tile.vertices[4] = (x + tile.cellh);
  tile.vertices[5] = (y + lines /*(float)info->lineinc*/);
  tile.vertices[6] = x;
  tile.vertices[7] = (y + lines/*(float)info->lineinc*/ );

  // Screen culling
  //if (tile.vertices[0] >= _Ygl->rwidth || tile.vertices[1] >= _Ygl->rheight || tile.vertices[2] < 0 || tile.vertices[5] < 0)
  //{
  //	return;
  //}

  if ((_Ygl->Win0[info->idScreen] != 0 || _Ygl->Win1[info->idScreen] != 0) && info->coordincx == 1.0f && info->coordincy == 1.0f)
  {                                                 // coordinate inc is not supported yet.
    winmode = Vdp2CheckWindowRange(info, x - cx, y - cy, tile.cellw, info->lineinc, varVdp2Regs);
    if (winmode == 0) // all outside, no need to draw
    {
      return;
    }
  }

  tile.cor = info->cor;
  tile.cog = info->cog;
  tile.cob = info->cob;

  if (1 == YglIsCached(YglTM_vdp2, cacheaddr, &c))
  {
    YglCachedQuadOffset(&tile, &c, cx, cy, info->coordincx, info->coordincy, YglTM_vdp2);
    return;
  }

  YglQuadOffset(&tile, texture, &c, cx, cy, info->coordincx, info->coordincy, YglTM_vdp2);
  YglCacheAdd(YglTM_vdp2, cacheaddr, &c);
  switch (info->patternwh)
  {
  case 1:
    requestDrawCell(info, texture, varVdp2Regs);
    break;
  case 2:
    texture->w += 8;
    requestDrawCellQuad(info, texture, varVdp2Regs);
    break;
  }
  info->priority = priority;
}


//////////////////////////////////////////////////////////////////////////////

static void Vdp2PatternAddrUsingPatternname(vdp2draw_struct *info, u16 paternname, Vdp2 *varVdp2Regs )
{
    u16 tmp = paternname;
    info->specialfunction = (info->supplementdata >> 9) & 0x1;
    info->specialcolorfunction = (info->supplementdata >> 8) & 0x1;

    switch (info->colornumber)
    {
    case 0: // in 16 colors
      info->paladdr = ((tmp & 0xF000) >> 12) | ((info->supplementdata & 0xE0) >> 1);
      break;
    default: // not in 16 colors
      info->paladdr = (tmp & 0x7000) >> 8;
      break;
    }

    switch (info->auxmode)
    {
    case 0:
      info->flipfunction = (tmp & 0xC00) >> 10;

      switch (info->patternwh)
      {
      case 1:
        info->charaddr = (tmp & 0x3FF) | ((info->supplementdata & 0x1F) << 10);
        break;
      case 2:
        info->charaddr = ((tmp & 0x3FF) << 2) | (info->supplementdata & 0x3) | ((info->supplementdata & 0x1C) << 10);
        break;
      }
      break;
    case 1:
      info->flipfunction = 0;

      switch (info->patternwh)
      {
      case 1:
        info->charaddr = (tmp & 0xFFF) | ((info->supplementdata & 0x1C) << 10);
        break;
      case 2:
        info->charaddr = ((tmp & 0xFFF) << 2) | (info->supplementdata & 0x3) | ((info->supplementdata & 0x10) << 10);
        break;
      }
      break;
    }

  if (!(varVdp2Regs->VRSIZE & 0x8000))
    info->charaddr &= 0x3FFF;

  info->charaddr *= 0x20; // thanks Runik
}

static void Vdp2PatternAddr(vdp2draw_struct *info, Vdp2 *varVdp2Regs)
{
  info->addr &= 0x7FFFF;
  switch (info->patterndatasize)
  {
  case 1:
  {
    u16 tmp = Vdp2RamReadWord(NULL, Vdp2Ram, info->addr);

    info->addr += 2;
    info->specialfunction = (info->supplementdata >> 9) & 0x1;
    info->specialcolorfunction = (info->supplementdata >> 8) & 0x1;

    switch (info->colornumber)
    {
    case 0: // in 16 colors
      info->paladdr = ((tmp & 0xF000) >> 12) | ((info->supplementdata & 0xE0) >> 1);
      break;
    default: // not in 16 colors
      info->paladdr = (tmp & 0x7000) >> 8;
      break;
    }

    switch (info->auxmode)
    {
    case 0:
      info->flipfunction = (tmp & 0xC00) >> 10;

      switch (info->patternwh)
      {
      case 1:
        info->charaddr = (tmp & 0x3FF) | ((info->supplementdata & 0x1F) << 10);
        break;
      case 2:
        info->charaddr = ((tmp & 0x3FF) << 2) | (info->supplementdata & 0x3) | ((info->supplementdata & 0x1C) << 10);
        break;
      }
      break;
    case 1:
      info->flipfunction = 0;

      switch (info->patternwh)
      {
      case 1:
        info->charaddr = (tmp & 0xFFF) | ((info->supplementdata & 0x1C) << 10);
        break;
      case 2:
        info->charaddr = ((tmp & 0xFFF) << 2) | (info->supplementdata & 0x3) | ((info->supplementdata & 0x10) << 10);
        break;
      }
      break;
    }

    break;
  }
  case 2: {
    u16 tmp1 = Vdp2RamReadWord(NULL, Vdp2Ram, info->addr);
    u16 tmp2 = Vdp2RamReadWord(NULL, Vdp2Ram, info->addr + 2);
    info->addr += 4;
    info->charaddr = tmp2 & 0x7FFF;
    info->flipfunction = (tmp1 & 0xC000) >> 14;
    switch (info->colornumber) {
    case 0:
      info->paladdr = (tmp1 & 0x7F);
      break;
    default:
      info->paladdr = (tmp1 & 0x70);
      break;
    }
    info->specialfunction = (tmp1 & 0x2000) >> 13;
    info->specialcolorfunction = (tmp1 & 0x1000) >> 12;
    break;
  }
  }

  if (!(varVdp2Regs->VRSIZE & 0x8000))
    info->charaddr &= 0x3FFF;

  info->charaddr *= 0x20; // thanks Runik
}


static int Vdp2PatternAddrPos(vdp2draw_struct *info, int planex, int x, int planey, int y, Vdp2 *varVdp2Regs)
{
  u32 addr = info->addr +
    (info->pagewh*info->pagewh*info->planew*planey +
      info->pagewh*info->pagewh*planex +
      info->pagewh*y +
      x)*info->patterndatasize * 2;

  int ptnAddrBk = (((addr >> 16)& 0xF) >> ((varVdp2Regs->VRSIZE >> 15)&0x1)) >> 1;
  if (info->pname_bank[ptnAddrBk] == 0) return 0;

  switch (info->patterndatasize)
  {
  case 1:
  {
    u16 tmp = Vdp2RamReadWord(NULL, Vdp2Ram, addr);

    info->specialfunction = (info->supplementdata >> 9) & 0x1;
    info->specialcolorfunction = (info->supplementdata >> 8) & 0x1;

    switch (info->colornumber)
    {
    case 0: // in 16 colors
      info->paladdr = ((tmp & 0xF000) >> 12) | ((info->supplementdata & 0xE0) >> 1);
      break;
    default: // not in 16 colors
      info->paladdr = (tmp & 0x7000) >> 8;
      break;
    }

    switch (info->auxmode)
    {
    case 0:
      info->flipfunction = (tmp & 0xC00) >> 10;

      switch (info->patternwh)
      {
      case 1:
        info->charaddr = (tmp & 0x3FF) | ((info->supplementdata & 0x1F) << 10);
        break;
      case 2:
        info->charaddr = ((tmp & 0x3FF) << 2) | (info->supplementdata & 0x3) | ((info->supplementdata & 0x1C) << 10);
        break;
      }
      break;
    case 1:
      info->flipfunction = 0;

      switch (info->patternwh)
      {
      case 1:
        info->charaddr = (tmp & 0xFFF) | ((info->supplementdata & 0x1C) << 10);
        break;
      case 2:
        info->charaddr = ((tmp & 0xFFF) << 2) | (info->supplementdata & 0x3) | ((info->supplementdata & 0x10) << 10);
        break;
      }
      break;
    }

    break;
  }
  case 2: {
    u16 tmp1 = Vdp2RamReadWord(NULL, Vdp2Ram, addr);
    u16 tmp2 = Vdp2RamReadWord(NULL, Vdp2Ram, addr + 2);
    info->charaddr = tmp2 & 0x7FFF;
    info->flipfunction = (tmp1 & 0xC000) >> 14;
    switch (info->colornumber) {
    case 0:
      info->paladdr = (tmp1 & 0x7F);
      break;
    default:
      info->paladdr = (tmp1 & 0x70);
      break;
    }
    info->specialfunction = (tmp1 & 0x2000) >> 13;
    info->specialcolorfunction = (tmp1 & 0x1000) >> 12;
    break;
  }
  }

  if (!(varVdp2Regs->VRSIZE & 0x8000))
    info->charaddr &= 0x3FFF;

  info->charaddr *= 0x20; // thanks Runik

  return 1;
}

static void Vdp2DrawRotation_in_sync(RBGDrawInfo * rbg, Vdp2 *varVdp2Regs)
{

  if (rbg == NULL) return;

  vdp2draw_struct *info = &rbg->info;
  int rgb_type = rbg->rgb_type;
  YglTexture *texture = &rbg->texture;

  float i, j;
  int k,l;
  int x, y;
  int cellw, cellh;
  int oldcellx = -1, oldcelly = -1;
  u32 color;
  int vres, hres, vstart;
  int h;
  int v;
  int lineInc = varVdp2Regs->LCTA.part.U & 0x8000 ? 2 : 0;
  vdp2rotationparameter_struct *parameter;
  u32* colpoint = NULL;

  u32 addr;
  u8 alpha = 0x00;
  if (_Ygl->rheight >= 448) lineInc <<= 1;
  vres = rbg->vres * (rbg->info.endLine - rbg->info.startLine)/yabsys.VBlankLineCount;
  vstart = rbg->info.startLine;
  hres = rbg->hres;

  cellw = rbg->info.cellw;
  cellh = rbg->info.cellh;

  x = 0;
  y = 0;
  if (rgb_type == 0)
  {
    rbg->paraA.dx = rbg->paraA.A * rbg->paraA.deltaX + rbg->paraA.B * rbg->paraA.deltaY;
    rbg->paraA.dy = rbg->paraA.D * rbg->paraA.deltaX + rbg->paraA.E * rbg->paraA.deltaY;
    rbg->paraA.Xp = rbg->paraA.A * (rbg->paraA.Px - rbg->paraA.Cx) +
      rbg->paraA.B * (rbg->paraA.Py - rbg->paraA.Cy) +
      rbg->paraA.C * (rbg->paraA.Pz - rbg->paraA.Cz) + rbg->paraA.Cx + rbg->paraA.Mx;
    rbg->paraA.Yp = rbg->paraA.D * (rbg->paraA.Px - rbg->paraA.Cx) +
      rbg->paraA.E * (rbg->paraA.Py - rbg->paraA.Cy) +
      rbg->paraA.F * (rbg->paraA.Pz - rbg->paraA.Cz) + rbg->paraA.Cy + rbg->paraA.My;
  }

  if (rbg->useb)
  {
    rbg->paraB.dx = rbg->paraB.A * rbg->paraB.deltaX + rbg->paraB.B * rbg->paraB.deltaY;
    rbg->paraB.dy = rbg->paraB.D * rbg->paraB.deltaX + rbg->paraB.E * rbg->paraB.deltaY;
    rbg->paraB.Xp = rbg->paraB.A * (rbg->paraB.Px - rbg->paraB.Cx) + rbg->paraB.B * (rbg->paraB.Py - rbg->paraB.Cy)
      + rbg->paraB.C * (rbg->paraB.Pz - rbg->paraB.Cz) + rbg->paraB.Cx + rbg->paraB.Mx;
    rbg->paraB.Yp = rbg->paraB.D * (rbg->paraB.Px - rbg->paraB.Cx) + rbg->paraB.E * (rbg->paraB.Py - rbg->paraB.Cy)
      + rbg->paraB.F * (rbg->paraB.Pz - rbg->paraB.Cz) + rbg->paraB.Cy + rbg->paraB.My;
  }

  rbg->paraA.over_pattern_name = varVdp2Regs->OVPNRA;
  rbg->paraB.over_pattern_name = varVdp2Regs->OVPNRB;


  rbg->info.cellw = rbg->hres;
  rbg->info.cellh = (rbg->vres * (info->endLine - info->startLine))/yabsys.VBlankLineCount;

  if (info->isbitmap) {
    rbg->info.cellw = cellw;
    rbg->info.cellh = cellh ;
  }

  YglQuadRbg0(rbg, NULL, &rbg->c, rbg->rgb_type, YglTM_vdp2, varVdp2Regs);

 //Not optimal. Should be 0 if there is no offset used.
  _Ygl->useLineColorOffset[0] = ((varVdp2Regs->KTCTL & 0x1010)!=0)?_Ygl->linecolorcoef_tex[0]:0;
  _Ygl->useLineColorOffset[1] = ((varVdp2Regs->KTCTL & 0x1010)!=0)?_Ygl->linecolorcoef_tex[1]:0;
  return;
}

  /*------------------------------------------------------------------------------
   Rotate Screen drawing
   ------------------------------------------------------------------------------*/
  static void FASTCALL Vdp2DrawRotation(RBGDrawInfo * rbg, Vdp2 *varVdp2Regs)
  {
    vdp2draw_struct *info = &rbg->info;
    int rgb_type = rbg->rgb_type;
    YglTexture *texture = &rbg->texture;

    int x, y;
    int cellw, cellh;
    int oldcellx = -1, oldcelly = -1;
    int lineInc = varVdp2Regs->LCTA.part.U & 0x8000 ? 2 : 0;
    int screenHeight = _Ygl->rheight;
    int screenWidth  = _Ygl->rwidth;

      if (_Ygl->rheight >= 448) lineInc <<= 1;
      if (_Ygl->rheight >= 448) rbg->vres = (_Ygl->rheight >> 1); else rbg->vres = _Ygl->rheight;
      if (_Ygl->rwidth >= 640) rbg->hres = (_Ygl->rwidth >> 1); else rbg->hres = _Ygl->rwidth;

    rbg->hres *= _Ygl->widthRatio;
    rbg->vres *= _Ygl->heightRatio;

    RBGGenerator_init(_Ygl->width, _Ygl->height);

    info->vertices[0] = 0;
    info->vertices[1] = (screenHeight * info->startLine)/yabsys.VBlankLineCount;
    info->vertices[2] = screenWidth;
    info->vertices[3] = (screenHeight * info->startLine)/yabsys.VBlankLineCount;
    info->vertices[4] = screenWidth;
    info->vertices[5] = (screenHeight * info->endLine)/yabsys.VBlankLineCount;
    info->vertices[6] = 0;
    info->vertices[7] = (screenHeight * info->endLine)/yabsys.VBlankLineCount;
    cellw = info->cellw;
    cellh = info->cellh;
    info->cellw = rbg->hres;
    info->cellh = rbg->vres;
    info->flipfunction = 0;
    info->cor = 0x00;
    info->cog = 0x00;
    info->cob = 0x00;

    if (varVdp2Regs->RPMD != 0) rbg->useb = 1;
    if (rgb_type == 0x04) rbg->useb = 1;

    if (!info->isbitmap)
    {
      oldcellx = -1;
      oldcelly = -1;
      rbg->pagesize = info->pagewh*info->pagewh;
      rbg->patternshift = (2 + info->patternwh);
    }
    else
    {
      oldcellx = 0;
      oldcelly = 0;
      rbg->pagesize = 0;
      rbg->patternshift = 0;
    }

      u64 cacheaddr = 0x90000000BAD;

      rbg->vdp2_sync_flg = -1;
      info->cellw = cellw;
      info->cellh = cellh;

      Vdp2DrawRotation_in_sync(rbg, varVdp2Regs);
  }

#ifdef CELL_ASYNC

static void executeDrawCell();

YabEventQueue *cellq = NULL;
YabEventQueue *cellq_end = NULL;

typedef struct {
  vdp2draw_struct *info;
  YglTexture *texture;
  Vdp2 *varVdp2Regs;
  int order;
} drawCellTask;

static int nbLoop = 0;
static int nbClear = 0;

YglTexture *textureTable[NB_MSG];
vdp2draw_struct *infoTable[NB_MSG];
Vdp2* vdp2RegsTable[NB_MSG];
int orderTable[NB_MSG];

void* Vdp2DrawCell_in_async(void *p)
{
   while(drawcell_run != 0){
     drawCellTask *task = (drawCellTask *)YabWaitEventQueue(cellq);
     if (task != NULL) {
       if (task->order == CELL_SINGLE) {
         Vdp2DrawCell_in_sync(task->info, task->texture, task->varVdp2Regs);
       } else {
         Vdp2DrawCell_in_sync(task->info, task->texture, task->varVdp2Regs);
         task->texture->textdata -= (task->texture->w + 8) * 8 - 8;
         Vdp2DrawCell_in_sync(task->info, task->texture, task->varVdp2Regs);
         task->texture->textdata -= 8;
         task->info->draw_line += 8;
         Vdp2DrawCell_in_sync(task->info, task->texture, task->varVdp2Regs);
         task->texture->textdata -= (task->texture->w + 8) * 8 - 8;
         Vdp2DrawCell_in_sync(task->info, task->texture, task->varVdp2Regs);
       }
       free(task->texture);
       free(task->info);
       free(task->varVdp2Regs);
       free(task);
     }
     YabWaitEventQueue(cellq_end);
   }
   return NULL;
}

static void FASTCALL Vdp2DrawCell(vdp2draw_struct *info, YglTexture *texture, Vdp2 *varVdp2Regs, int order) {
   drawCellTask *task = (drawCellTask*)malloc(sizeof(drawCellTask));

   task->texture = (YglTexture*)malloc(sizeof(YglTexture));
   memcpy(task->texture, texture, sizeof(YglTexture));

   task->info = (vdp2draw_struct*)malloc(sizeof(vdp2draw_struct));
   memcpy(task->info, info, sizeof(vdp2draw_struct));

   task->varVdp2Regs = (Vdp2*)malloc(sizeof(Vdp2));
   memcpy(task->varVdp2Regs, varVdp2Regs, sizeof(Vdp2));

   task->order = order;


   if (drawcell_run == 0) {
     drawcell_run = 1;
     cellq = YabThreadCreateQueue(NB_MSG);
     cellq_end = YabThreadCreateQueue(NB_MSG);
     YabThreadStart(YAB_THREAD_VDP2_NBG0, Vdp2DrawCell_in_async, 0);
   }
   YabAddEventQueue(cellq_end, NULL);
   YabAddEventQueue(cellq, task);
   YabThreadYield();
}

static void executeDrawCell() {
#ifdef CELL_ASYNC
  int i = 0;
  while (i < nbLoop) {
    Vdp2DrawCell(infoTable[i], textureTable[i], vdp2RegsTable[i], orderTable[i]);
    free(infoTable[i]);
    free(textureTable[i]);
    free(vdp2RegsTable[i]);
    i++;
  }
  nbLoop = 0;
#endif
}

static void requestDrawCellOrder(vdp2draw_struct * info, YglTexture *texture, Vdp2* varVdp2Regs, int order) {
  textureTable[nbLoop] = (YglTexture*)malloc(sizeof(YglTexture));
  memcpy(textureTable[nbLoop], texture, sizeof(YglTexture));
  infoTable[nbLoop] = (vdp2draw_struct*)malloc(sizeof(vdp2draw_struct));
  memcpy(infoTable[nbLoop], info, sizeof(vdp2draw_struct));
  vdp2RegsTable[nbLoop] = (Vdp2*)malloc(sizeof(Vdp2));
  memcpy(vdp2RegsTable[nbLoop], varVdp2Regs, sizeof(Vdp2));
  orderTable[nbLoop] = order;
  nbLoop++;
  if (nbLoop >= NB_MSG) {
    LOG_ASYN("Too much drawCell request!\n");
    executeDrawCell();
  }
}

static void requestDrawCell(vdp2draw_struct * info, YglTexture *texture, Vdp2* varVdp2Regs) {
#ifdef CELL_ASYNC
   requestDrawCellOrder(info, texture, varVdp2Regs, CELL_SINGLE);
#else
         Vdp2DrawCell_in_sync(info, texture, varVdp2Regs);
#endif
}

static void requestDrawCellQuad(vdp2draw_struct * info, YglTexture *texture, Vdp2* varVdp2Regs) {
#ifdef CELL_ASYNC
   requestDrawCellOrder(info, texture, varVdp2Regs, CELL_QUAD);
#else
   Vdp2DrawCell_in_sync(info, texture, varVdp2Regs);
   texture->textdata -= (texture->w + 8) * 8 - 8;
   Vdp2DrawCell_in_sync(info, texture, varVdp2Regs);
   texture->textdata -= 8;
   Vdp2DrawCell_in_sync(info, texture, varVdp2Regs);
   texture->textdata -= (texture->w + 8) * 8 - 8;
   Vdp2DrawCell_in_sync(info, texture, varVdp2Regs);
#endif
}
#endif

//////////////////////////////////////////////////////////////////////////////

int VIDCSInit(void)
{
  if(renderer_started)
    return -1;

  if (YglInit(2048, 1024, 8) != 0)
    return -1;

  for (int i=0; i<SPRITE; i++)
    YglReset(_Ygl->vdp2levels[i]);

  _Ygl->vdp1wratio = 1.0;
  _Ygl->vdp1hratio = 1.0;

  _Ygl->vdp1wdensity = 1.0;
  _Ygl->vdp1hdensity = 1.0;

  _Ygl->vdp2wdensity = 1.0;
  _Ygl->vdp2hdensity = 1.0;
  YglChangeResolution(320, 224);

  renderer_started = 1;
  return 0;
}

//////////////////////////////////////////////////////////////////////////////

void VIDCSDeInit(void)
{
  if(!renderer_started)
    return;
#ifdef CELL_ASYNC
  if (drawcell_run == 1) {
    drawcell_run = 0;
    for (int i=0; i<4; i++) {
      YabAddEventQueue(cellq_end, NULL);
      YabAddEventQueue(cellq, NULL);
    }
    YabThreadWait(YAB_THREAD_VDP2_NBG0);
    YabThreadWait(YAB_THREAD_VDP2_NBG1);
    YabThreadWait(YAB_THREAD_VDP2_NBG2);
    YabThreadWait(YAB_THREAD_VDP2_NBG3);
  }
#endif
  YglGenReset();
  YglDeInit();

  renderer_started = 0;
}

int WaitVdp2Async(int sync) {
  int empty = 0;
  if (vdp2busy == 1) {
#ifdef CELL_ASYNC
    if (cellq_end != NULL) {
      empty = 1;
      while (((empty = YaGetQueueSize(cellq_end))!=0) && (sync == 1))
      {
        YabThreadYield();
      }
    }
#endif
    RBGGenerator_onFinish();
    if (empty == 0) vdp2busy = 0;
  }
  return empty;
}

void waitVdp2DrawScreensEnd(int sync) {
  YglCheckFBSwitch(0);
  if (vdp2busy == 1) {
    WaitVdp2Async(sync);
    YglTmPush(YglTM_vdp2);
    if (VIDCore != NULL) {
      VIDCSReadColorOffset();
      VIDCore->composeFB(&Vdp2Lines[0]);
    }
  }
}

void addCSCommands(vdp1cmd_struct* cmd, int type)
{
  //Test game: Sega rally : The aileron at the start
  int ADx = (cmd->CMDXD - cmd->CMDXA);
  int ADy = (cmd->CMDYD - cmd->CMDYA);
  int BCx = (cmd->CMDXC - cmd->CMDXB);
  int BCy = (cmd->CMDYC - cmd->CMDYB);

  int nbStepAD = sqrt(ADx*ADx + ADy*ADy);
  int nbStepBC = sqrt(BCx*BCx + BCy*BCy);

  int nbStep = MAX(nbStepAD, nbStepBC);

  cmd->type = type;

  cmd->nbStep = nbStep;
  if(cmd->nbStep  != 0) {
    // Ici faut voir encore les Ax doivent faire un de plus.
    cmd->uAstepx = (float)ADx/(float)nbStep;
    cmd->uAstepy = (float)ADy/(float)nbStep;
    cmd->uBstepx = (float)BCx/(float)nbStep;
    cmd->uBstepy = (float)BCy/(float)nbStep;
  } else {
    cmd->uAstepx = 0.0;
    cmd->uAstepy = 0.0;
    cmd->uBstepx = 0.0;
    cmd->uBstepy = 0.0;
  }
#ifdef DEBUG_VDP1_CMD
  YuiMsg("Add Distorted\n");
  YuiMsg("\t[%d,%d]\n", cmd->CMDXA, cmd->CMDYA);
  YuiMsg("\t[%d,%d]\n", cmd->CMDXB, cmd->CMDYB);
  YuiMsg("\t[%d,%d]\n", cmd->CMDXC, cmd->CMDYC);
  YuiMsg("\t[%d,%d]\n", cmd->CMDXD, cmd->CMDYD);
  YuiMsg("\n\n");
  YuiMsg("=> %d (%d %d %d %d => %d %d) %f %f %f %f\n", cmd->nbStep, ADx, ADy, BCx, BCy, nbStepAD, nbStepBC, cmd->uAstepx, cmd->uAstepy, cmd->uBstepx, cmd->uBstepy);
  YuiMsg("==============\n");
#endif
  vdp1_add(cmd,0);
}

//////////////////////////////////////////////////////////////////////////////
void VIDCSVdp1Draw()
{
  int i;
  int line = 0;
  Vdp2 *varVdp2Regs = &Vdp2Lines[yabsys.LineCount];
  _Ygl->vpd1_running = 1;

  _Ygl->msb_shadow_count_[_Ygl->drawframe] = 0;

  Vdp1DrawCommands(Vdp1Ram, Vdp1Regs, NULL);

  _Ygl->vpd1_running = 0;
}

//////////////////////////////////////////////////////////////////////////////

void VIDCSVdp1NormalSpriteDraw(vdp1cmd_struct *cmd, u8 * ram, Vdp1 * regs, u8* back_framebuffer)
{
  LOG_CMD("%d\n", __LINE__);

  if (((cmd->CMDPMOD >> 3) & 0x7u) == 5) {
    // hard/vdp2/hon/p09_20.htm#no9_21
    u32 *cclist = (u32 *)&(Vdp2Lines[0].CCRSA);
    cclist[0] &= 0x1Fu;
  }
  cmd->SPCTL = Vdp2Lines[0].SPCTL;
  cmd->type = QUAD;

  vdp1_add(cmd,0);

  LOG_CMD("%d\n", __LINE__);
}

//////////////////////////////////////////////////////////////////////////////

int getBestMode(vdp1cmd_struct* cmd) {
  int ret = DISTORTED;
  // if (
  //   ((cmd->CMDXA - cmd->CMDXD) == 0) &&
  //   ((cmd->CMDYA - cmd->CMDYB) == 0) &&
  //   ((cmd->CMDXB - cmd->CMDXC) == 0) &&
  //   ((cmd->CMDYC - cmd->CMDYD) == 0)
  // ) {
  //   ret = QUAD;
  // }
  return ret;
}

void VIDCSVdp1ScaledSpriteDraw(vdp1cmd_struct *cmd, u8 * ram, Vdp1 * regs, u8* back_framebuffer)
{

  if (((cmd->CMDPMOD >> 3) & 0x7u) == 5) {
    // hard/vdp2/hon/p09_20.htm#no9_21
    u32 *cclist = (u32 *)&(Vdp2Lines[0].CCRSA);
    cclist[0] &= 0x1Fu;
  }

  cmd->SPCTL = Vdp2Lines[0].SPCTL;
  cmd->type = QUAD;
  vdp1_add(cmd,0);

  LOG_CMD("%d\n", __LINE__);
}

void VIDCSVdp1DistortedSpriteDraw(vdp1cmd_struct *cmd, u8 * ram, Vdp1 * regs, u8* back_framebuffer)
{
  LOG_CMD("%d\n", __LINE__);

  if (((cmd->CMDPMOD >> 3) & 0x7u) == 5) {
    // hard/vdp2/hon/p09_20.htm#no9_21
    u32 *cclist = (u32 *)&(Vdp2Lines[0].CCRSA);
    cclist[0] &= 0x1Fu;
  }

  cmd->SPCTL = Vdp2Lines[0].SPCTL;
  if (getBestMode(cmd) == DISTORTED) {
    addCSCommands(cmd,DISTORTED);
  } else {
    cmd->type = QUAD;
    if (cmd->CMDXA <= cmd->CMDXB) cmd->CMDXB += 1;
    else cmd->CMDXB -= 1;
    if (cmd->CMDXD <= cmd->CMDXC) cmd->CMDXC += 1;
    else cmd->CMDXC -= 1;
    if (cmd->CMDYB <= cmd->CMDYC) cmd->CMDYC += 1;
    else cmd->CMDYC -= 1;
    if (cmd->CMDYA <= cmd->CMDYD) cmd->CMDYD += 1;
    else cmd->CMDYD -= 1;
    vdp1_add(cmd,0);
  }

  return;
}

void VIDCSVdp1PolygonDraw(vdp1cmd_struct *cmd, u8 * ram, Vdp1 * regs, u8* back_framebuffer)
{
  cmd->SPCTL = Vdp2Lines[0].SPCTL;
  // cmd->type = POLYGON;
  cmd->COLOR[0] = Vdp1ReadPolygonColor(cmd,&Vdp2Lines[0]);


  if (getBestMode(cmd) == DISTORTED) {
    addCSCommands(cmd,POLYGON);
  } else {
    if (cmd->CMDXA <= cmd->CMDXB) cmd->CMDXB += 1;
    else cmd->CMDXB -= 1;
    if (cmd->CMDXD <= cmd->CMDXC) cmd->CMDXC += 1;
    else cmd->CMDXC -= 1;
    if (cmd->CMDYB <= cmd->CMDYC) cmd->CMDYC += 1;
    else cmd->CMDYC -= 1;
    if (cmd->CMDYA <= cmd->CMDYD) cmd->CMDYD += 1;
    else cmd->CMDYD -= 1;
    cmd->type = QUAD_POLY;
    vdp1_add(cmd,0);
  }
  return;
}

void VIDCSVdp1PolylineDraw(vdp1cmd_struct *cmd, u8 * ram, Vdp1 * regs, u8* back_framebuffer)
{
  LOG_CMD("%d\n", __LINE__);

  cmd->SPCTL = Vdp2Lines[0].SPCTL;
  cmd->COLOR[0] = Vdp1ReadPolygonColor(cmd,&Vdp2Lines[0]);
  cmd->type = POLYLINE;

  vdp1_add(cmd,0);
}

//////////////////////////////////////////////////////////////////////////////

void VIDCSVdp1LineDraw(vdp1cmd_struct *cmd, u8 * ram, Vdp1 * regs, u8* back_framebuffer)
{
  LOG_CMD("%d\n", __LINE__);

  cmd->SPCTL = Vdp2Lines[0].SPCTL;
  cmd->type = LINE;
  cmd->COLOR[0] = Vdp1ReadPolygonColor(cmd,&Vdp2Lines[0]);

  vdp1_add(cmd,0);
}

//////////////////////////////////////////////////////////////////////////////

void VIDCSVdp1UserClipping(vdp1cmd_struct *cmd, u8 * ram, Vdp1 * regs)
{
  if (  (cmd->CMDXC > regs->systemclipX2)
    && (cmd->CMDYC > regs->systemclipY2)
  ) {
    regs->localX = 0;
    regs->localY = 0;
  }

  cmd->type = USER_CLIPPING;
  vdp1_add(cmd,1);
  regs->userclipX1 = cmd->CMDXA;
  regs->userclipY1 = cmd->CMDYA;
  regs->userclipX2 = cmd->CMDXC;
  regs->userclipY2 = cmd->CMDYC;
}

//////////////////////////////////////////////////////////////////////////////

void VIDCSVdp1SystemClipping(vdp1cmd_struct *cmd, u8 * ram, Vdp1 * regs)
{
  if (((cmd->CMDXC) == regs->systemclipX2) && (regs->systemclipY2 == (cmd->CMDYC))) return;
  cmd->type = SYSTEM_CLIPPING;
  vdp1_add(cmd,1);
  regs->systemclipX2 = cmd->CMDXC;
  regs->systemclipY2 = cmd->CMDYC;
}

void VIDCSVdp1DrawFB(void) {
  VIDCSRenderVDP1();
  vdp1_write();
}

static void Vdp2DrawNBG0(Vdp2* varVdp2Regs) {
  vdp2draw_struct info = {0};
  YglTexture texture;
  YglCache tmpc;
  u32 char_access = 0;
  u32 ptn_access = 0;
  info.dst = 0;
  info.uclipmode = 0;
  info.idScreen = NBG0;
  info.coordincx = 1.0f;
  info.coordincy = 1.0f;

  info.cor = 0;
  info.cog = 0;
  info.cob = 0;
  int i;
  info.enable = 0;
  info.startLine = 0;
  info.endLine = (yabsys.VBlankLineCount < 270)?yabsys.VBlankLineCount:270;

  info.cellh = 256 << vdp2_interlace;
  info.specialcolorfunction = 0;

    // NBG0 mode
  for (i=0; i<yabsys.VBlankLineCount; i++) {
    info.display[i] = isEnabled(NBG0, &Vdp2Lines[i]);
    info.enable |= info.display[i];
    info.alpha_per_line[i] = (~Vdp2Lines[i].CCRNA & 0x1F) << 3;
  }
    if (!info.enable) return;

    for (int i=0; i < 4; i++) {
        info.char_bank[i] = 0;
        info.pname_bank[i] = 0;
        for (int j=0; j < 8; j++) {
          if (Vdp2External.AC_VRAM[i][j] == 0x04) {
            info.char_bank[i] = 1;
            char_access |= 1<<j;
          }
          if (Vdp2External.AC_VRAM[i][j] == 0x00) {
            info.pname_bank[i] = 1;
            ptn_access |= (1 << j);
          }
        }
      }

      //ToDo Need to determine if NBG0 shall be disabled due to VRAM access
      //if (char_access == 0) return;

    if (char_access == 0) return;
    if ((info.isbitmap = varVdp2Regs->CHCTLA & 0x2) != 0)
    {
      // Bitmap Mode
      ReadBitmapSize(&info, varVdp2Regs->CHCTLA >> 2, 0x3);
      if (vdp2_interlace) info.cellh *= 2;

      info.x = -((varVdp2Regs->SCXIN0 & 0x7FF) % info.cellw);
      info.y = -((varVdp2Regs->SCYIN0 & 0x7FF) % info.cellh);

      info.charaddr = (varVdp2Regs->MPOFN & 0x7) * 0x20000;
      info.paladdr = (varVdp2Regs->BMPNA & 0x7) << 4;
      info.flipfunction = 0;
      info.specialcolorfunction = (varVdp2Regs->BMPNA & 0x10) >> 4;
      info.specialfunction = (varVdp2Regs->BMPNA >> 5) & 0x01;

      //If RBG0 is used and the VRAM is used for it, check that NBGx is not using reserved area, otherwise, do not display
      int charAddrBk = (((info.charaddr >> 16)& 0xF) >> ((varVdp2Regs->VRSIZE >> 15)&0x1)) >> 1;
      int needUpdate = 0;
      for (int i=0; i<yabsys.VBlankLineCount; i++) {
        if ((Vdp2Lines[i].BGON & 0x10)!=0) {
          //RBG0 is enabled for this line. Check we can display the NBGx
          if(((Vdp2Lines[i].RAMCTL>>(charAddrBk<<1))&0x3) != 0x0){
            //VRAM on the dedicated bank is used by RBG0, it can not be used by NBGx
            needUpdate = 1;
            info.display[i] = 0;
          }
        }
      }
      if (needUpdate != 0) {
        info.enable = 0;
        for (int i=0; i<yabsys.VBlankLineCount; i++) info.enable |= info.display[i];
        if (!info.enable) return;
      }
    }
    else
    {
      // Tile Mode
      if (ptn_access == 0) return;
      info.mapwh = 2;

      ReadPlaneSize(&info, varVdp2Regs->PLSZ);

      info.x = -((varVdp2Regs->SCXIN0 & 0x7FF) % (512 * info.planew));
      info.y = -((varVdp2Regs->SCYIN0 & 0x7FF) % (512 * info.planeh));
      ReadPatternData(&info, varVdp2Regs->PNCN0, varVdp2Regs->CHCTLA & 0x1);
    }

    if ((varVdp2Regs->ZMXN0.all & 0x7FF00) == 0)
      //invalid zoom value
      return;
    else
      info.coordincx = (float)65536 / (varVdp2Regs->ZMXN0.all & 0x7FF00);

    switch (varVdp2Regs->ZMCTL & 0x03)
    {
    case 0:
      info.maxzoom = 1.0f;
      break;
    case 1:
      info.maxzoom = 0.5f;
      //if( info.coordincx < 0.5f )  info.coordincx = 0.5f;
      break;
    case 2:
    case 3:
      info.maxzoom = 0.25f;
      //if( info.coordincx < 0.25f )  info.coordincx = 0.25f;
      break;
    }

    if ((varVdp2Regs->ZMYN0.all & 0x7FF00) == 0)
      //Invalid zoom value
      return;
    else
      info.coordincy = (float)65536 / (varVdp2Regs->ZMYN0.all & 0x7FF00);

    info.PlaneAddr = (void FASTCALL(*)(void *, int, Vdp2*))&Vdp2NBG0PlaneAddr;


  ReadMosaicData(&info, 0x1, varVdp2Regs);

  info.transparencyenable = !(varVdp2Regs->BGON & 0x100);
  info.specialprimode = varVdp2Regs->SFPRMD & 0x3;
  info.specialcolormode = varVdp2Regs->SFCCMD & 0x3;

  if (varVdp2Regs->SFSEL & 0x1)
    info.specialcode = varVdp2Regs->SFCODE >> 8;
  else
    info.specialcode = varVdp2Regs->SFCODE & 0xFF;

  info.colornumber = (varVdp2Regs->CHCTLA & 0x70) >> 4;

  int dest_alpha = ((varVdp2Regs->CCCTL >> 9) & 0x01);

  info.coloroffset = (varVdp2Regs->CRAOFA & 0x7) << 8;
  info.linecheck_mask = 0x01;
  info.priority = varVdp2Regs->PRINA & 0x7;

  if (info.priority == 0)
    return;

  ReadLineScrollData(&info, varVdp2Regs->SCRCTL & 0xFF, varVdp2Regs->LSTA0.all);
  info.lineinfo = lineNBG0;
  Vdp2GenLineinfo(&info);

  if (varVdp2Regs->SCRCTL & 1)
  {
    info.isverticalscroll = 1;
    info.verticalscrolltbl = (varVdp2Regs->VCSTA.all & 0x7FFFE) << 1;
    if (varVdp2Regs->SCRCTL & 0x100)
      info.verticalscrollinc = 8;
    else
      info.verticalscrollinc = 4;
  }
  else
    info.isverticalscroll = 0;

// NBG0 draw
    if (info.isbitmap)
    {
      if (info.coordincx != 1.0f || info.coordincy != 1.0f || VDPLINE_SZ(info.islinescroll)) {
        info.sh = (varVdp2Regs->SCXIN0 & 0x7FF);
        info.sv = (varVdp2Regs->SCYIN0 & 0x7FF);
        info.x = 0;
        info.y = 0;
        info.vertices[0] = 0;
        info.vertices[1] = 0;
        info.vertices[2] = _Ygl->rwidth;
        info.vertices[3] = 0;
        info.vertices[4] = _Ygl->rwidth;
        info.vertices[5] = _Ygl->rheight;
        info.vertices[6] = 0;
        info.vertices[7] = _Ygl->rheight;
        vdp2draw_struct infotmp = info;
        infotmp.cellw = _Ygl->rwidth;
        if (_Ygl->rheight >= 448)
          infotmp.cellh = (_Ygl->rheight >> 1) << vdp2_interlace;
        else
          infotmp.cellh = _Ygl->rheight << vdp2_interlace;
        YglQuad(&infotmp, &texture, &tmpc, YglTM_vdp2);
        Vdp2DrawBitmapCoordinateInc(&info, &texture, varVdp2Regs);
      }
      else {

        int xx, yy;
        int isCached = 0;

        if (info.islinescroll) // Nights Movie
        {
          info.sh = (varVdp2Regs->SCXIN0 & 0x7FF);
          info.sv = (varVdp2Regs->SCYIN0 & 0x7FF);
          info.x = 0;
          info.y = 0;
          info.vertices[0] = 0;
          info.vertices[1] = 0;
          info.vertices[2] = _Ygl->rwidth;
          info.vertices[3] = 0;
          info.vertices[4] = _Ygl->rwidth;
          info.vertices[5] = _Ygl->rheight;
          info.vertices[6] = 0;
          info.vertices[7] = _Ygl->rheight;
          vdp2draw_struct infotmp = info;
          infotmp.cellw = _Ygl->rwidth;
          infotmp.cellh = _Ygl->rheight << vdp2_interlace;
          YglQuad(&infotmp, &texture, &tmpc, YglTM_vdp2);
          Vdp2DrawBitmapLineScroll(&info, &texture, _Ygl->rwidth, _Ygl->rheight, varVdp2Regs);

        }
        else {
          yy = info.y;
          while (yy + info.y < _Ygl->rheight)
          {
            info.draw_line = yy;
            xx = info.x;
            while (xx + info.x < _Ygl->rwidth)
            {
              info.vertices[0] = xx;
              info.vertices[1] = yy;
              info.vertices[2] = (xx + info.cellw);
              info.vertices[3] = yy;
              info.vertices[4] = (xx + info.cellw);
              info.vertices[5] = (yy + info.cellh);
              info.vertices[6] = xx;
              info.vertices[7] = (yy + info.cellh);
              if (isCached == 0)
              {
                YglQuad(&info, &texture, &tmpc, YglTM_vdp2);
                if (info.islinescroll) {
                  Vdp2DrawBitmapLineScroll(&info, &texture, info.cellw, info.cellh, varVdp2Regs);
                } else {
                  requestDrawCell(&info, &texture, varVdp2Regs);
                }
                isCached = 1;
              }
              else {
                YglCachedQuad(&info, &tmpc, YglTM_vdp2);
              }
              xx += info.cellw;
            }
            yy += info.cellh;
          }
        }
      }
    }
    else
    {
      if (info.islinescroll) {
        info.sh = (varVdp2Regs->SCXIN0 & 0x7FF);
        info.sv = (varVdp2Regs->SCYIN0 & 0x7FF);
        info.x = 0;
        info.y = 0;
        info.vertices[0] = 0;
        info.vertices[1] = 0;
        info.vertices[2] = _Ygl->rwidth;
        info.vertices[3] = 0;
        info.vertices[4] = _Ygl->rwidth;
        info.vertices[5] = _Ygl->rheight;
        info.vertices[6] = 0;
        info.vertices[7] = _Ygl->rheight;
        vdp2draw_struct infotmp = info;
        infotmp.cellw = _Ygl->rwidth;
        infotmp.cellh = _Ygl->rheight;

        infotmp.flipfunction = 0;

        YglQuad(&infotmp, &texture, &tmpc, YglTM_vdp2);
        Vdp2DrawMapPerLine(&info, &texture, varVdp2Regs);
      }
      else {
        int delayed = 0;
        // Setting miss of cycle patten need to plus 8 dot vertical
        // If pattern access is defined on T0 for NBG0 or NBG1, there is no limitation
        if (((ptn_access & 0x1)==0) && Vdp2CheckCharAccessPenalty(char_access, ptn_access) != 0) {
          delayed = 1;
        }
        info.x = varVdp2Regs->SCXIN0 & 0x7FF;
        info.y = varVdp2Regs->SCYIN0 & 0x7FF;
        Vdp2DrawMapTest(&info, &texture, delayed, varVdp2Regs);
      }
    }
  executeDrawCell();
}

//////////////////////////////////////////////////////////////////////////////

static void Vdp2DrawNBG1(Vdp2* varVdp2Regs)
{
  vdp2draw_struct info = {0};
  YglTexture texture;
  YglCache tmpc;
  u32 char_access = 0;
  u32 ptn_access = 0;
  info.dst = 0;
  info.idScreen = NBG1;
  info.uclipmode = 0;
  info.cor = 0;
  info.cog = 0;
  info.cob = 0;
  info.specialcolorfunction = 0;
  info.enable = 0;
  info.startLine = 0;
  info.endLine = (yabsys.VBlankLineCount < 270)?yabsys.VBlankLineCount:270;

  for (int i=0; i<yabsys.VBlankLineCount; i++) {
    info.display[i] = isEnabled(NBG1, &Vdp2Lines[i]);
    info.enable |= info.display[i];
    info.alpha_per_line[i] = ((~Vdp2Lines[i].CCRNA & 0x1F00) >> 5);
  }
  if (!info.enable) return;

  for (int i=0; i < 4; i++) {
      info.char_bank[i] = 0;
      info.pname_bank[i] = 0;
      for (int j=0; j < 8; j++) {
        if (Vdp2External.AC_VRAM[i][j] == 0x05) {
          info.char_bank[i] = 1;
          char_access |= 1<<j;
        }
        if (Vdp2External.AC_VRAM[i][j] == 0x01) {
          info.pname_bank[i] = 1;
          ptn_access |= (1 << j);
        }
      }
    }
  //ToDo Need to determine if NBG1 shall be disabled due to VRAM access
  //if (char_access == 0) return;

  info.transparencyenable = !(varVdp2Regs->BGON & 0x200);
  info.specialprimode = (varVdp2Regs->SFPRMD >> 2) & 0x3;

  info.colornumber = (varVdp2Regs->CHCTLA & 0x3000) >> 12;

  if (char_access == 0) return;

  if ((info.isbitmap = varVdp2Regs->CHCTLA & 0x200) != 0)
  {
    //If there is no access to character pattern data, do not display the layer
    ReadBitmapSize(&info, varVdp2Regs->CHCTLA >> 10, 0x3);

    info.x = -((varVdp2Regs->SCXIN1 & 0x7FF) % info.cellw);
    info.y = -((varVdp2Regs->SCYIN1 & 0x7FF) % info.cellh);
    info.charaddr = ((varVdp2Regs->MPOFN & 0x70) >> 4) * 0x20000;
    info.paladdr = (varVdp2Regs->BMPNA & 0x700) >> 4;
    info.flipfunction = 0;
    info.specialfunction = 0;
    info.specialcolorfunction = (varVdp2Regs->BMPNA & 0x1000) >> 4;

    //If RBG0 is used and the VRAM is used for it, check that NBGx is not using reserved area, otherwise, do not display
    int charAddrBk = (((info.charaddr >> 16)& 0xF) >> ((varVdp2Regs->VRSIZE >> 15)&0x1)) >> 1;
    int needUpdate = 0;
    for (int i=0; i<yabsys.VBlankLineCount; i++) {
      if ((Vdp2Lines[i].BGON & 0x10)!=0) {
        //RBG0 is enabled for this line. Check we can display the NBGx
        if(((Vdp2Lines[i].RAMCTL>>(charAddrBk<<1))&0x3) != 0x0){
          //VRAM on the dedicated bank is used by RBG0, it can not be used by NBGx
          needUpdate = 1;
          info.display[i] = 0;
        }
      }
    }
    if (needUpdate != 0) {
      info.enable = 0;
      for (int i=0; i<yabsys.VBlankLineCount; i++) info.enable |= info.display[i];
      if (!info.enable) return;
    }
  }
  else
  {
    if (ptn_access == 0) return;
    info.mapwh = 2;

    ReadPlaneSize(&info, varVdp2Regs->PLSZ >> 2);

    info.x = -((varVdp2Regs->SCXIN1 & 0x7FF) % (512 * info.planew));
    info.y = -((varVdp2Regs->SCYIN1 & 0x7FF) % (512 * info.planeh));

    ReadPatternData(&info, varVdp2Regs->PNCN1, varVdp2Regs->CHCTLA & 0x100);
  }

  info.specialcolormode = (varVdp2Regs->SFCCMD >> 2) & 0x3;

  if (varVdp2Regs->SFSEL & 0x2)
    info.specialcode = varVdp2Regs->SFCODE >> 8;
  else
    info.specialcode = varVdp2Regs->SFCODE & 0xFF;

  ReadMosaicData(&info, 0x2, varVdp2Regs);


  info.coloroffset = (varVdp2Regs->CRAOFA & 0x70) << 4;
  info.linecheck_mask = 0x02;

  if ((varVdp2Regs->ZMXN1.all & 0x7FF00) == 0)
    //invalid zoom value
    return;
  else
    info.coordincx = (float)65536 / (varVdp2Regs->ZMXN1.all & 0x7FF00);

  switch ((varVdp2Regs->ZMCTL >> 8) & 0x03)
  {
  case 0:
    info.maxzoom = 1.0f;
    break;
  case 1:
    info.maxzoom = 0.5f;
    //      if( info.coordincx < 0.5f )  info.coordincx = 0.5f;
    break;
  case 2:
  case 3:
    info.maxzoom = 0.25f;
    //      if( info.coordincx < 0.25f )  info.coordincx = 0.25f;
    break;
  }
  if ((varVdp2Regs->ZMYN1.all & 0x7FF00) == 0)
    info.coordincy = 1.0f;
  else
    info.coordincy = (float)65536 / (varVdp2Regs->ZMYN1.all & 0x7FF00);


  info.priority = (varVdp2Regs->PRINA >> 8) & 0x7;

  info.PlaneAddr = (void FASTCALL(*)(void *, int, Vdp2*))&Vdp2NBG1PlaneAddr;

  if ((info.priority == 0) ||
    (varVdp2Regs->BGON & 0x1 && (varVdp2Regs->CHCTLA & 0x70) >> 4 == 4)) // If NBG0 16M mode is enabled, don't draw
    return;

  ReadLineScrollData(&info, varVdp2Regs->SCRCTL >> 8, varVdp2Regs->LSTA1.all);
  info.lineinfo = lineNBG1;
  Vdp2GenLineinfo(&info);
  if (varVdp2Regs->SCRCTL & 0x100)
  {
    info.isverticalscroll = 1;
    if (varVdp2Regs->SCRCTL & 0x1)
    {
      info.verticalscrolltbl = 4 + ((varVdp2Regs->VCSTA.all & 0x7FFFE) << 1);
      info.verticalscrollinc = 8;
    }
    else
    {
      info.verticalscrolltbl = (varVdp2Regs->VCSTA.all & 0x7FFFE) << 1;
      info.verticalscrollinc = 4;
    }
  }
  else
    info.isverticalscroll = 0;

  if (info.isbitmap)
  {

    if (info.coordincx != 1.0f || info.coordincy != 1.0f) {
      info.sh = (varVdp2Regs->SCXIN1 & 0x7FF);
      info.sv = (varVdp2Regs->SCYIN1 & 0x7FF);
      info.x = 0;
      info.y = 0;
      info.vertices[0] = 0;
      info.vertices[1] = 0;
      info.vertices[2] = _Ygl->rwidth;
      info.vertices[3] = 0;
      info.vertices[4] = _Ygl->rwidth;
      info.vertices[5] = _Ygl->rheight;
      info.vertices[6] = 0;
      info.vertices[7] = _Ygl->rheight;
      vdp2draw_struct infotmp = info;
      infotmp.cellw = _Ygl->rwidth;
      if (_Ygl->rheight >= 448)
        infotmp.cellh = (_Ygl->rheight >> 1);
      else
        infotmp.cellh = _Ygl->rheight;
      YglQuad(&infotmp, &texture, &tmpc, YglTM_vdp2);
      Vdp2DrawBitmapCoordinateInc(&info, &texture, varVdp2Regs);
    }
    else {

      int xx, yy;
      int isCached = 0;

      if (info.islinescroll) // Nights Movie
      {
        info.sh = (varVdp2Regs->SCXIN1 & 0x7FF);
        info.sv = (varVdp2Regs->SCYIN1 & 0x7FF);
        info.x = 0;
        info.y = 0;
        info.vertices[0] = 0;
        info.vertices[1] = 0;
        info.vertices[2] = _Ygl->rwidth;
        info.vertices[3] = 0;
        info.vertices[4] = _Ygl->rwidth;
        info.vertices[5] = _Ygl->rheight;
        info.vertices[6] = 0;
        info.vertices[7] = _Ygl->rheight;
        vdp2draw_struct infotmp = info;
        infotmp.cellw = _Ygl->rwidth;
        infotmp.cellh = _Ygl->rheight;
        YglQuad(&infotmp, &texture, &tmpc, YglTM_vdp2);
        Vdp2DrawBitmapLineScroll(&info, &texture, _Ygl->rwidth, _Ygl->rheight, varVdp2Regs);

      }
      else {
        yy = info.y;
        while (yy + info.y < _Ygl->rheight)
        {
          info.draw_line = yy;
          xx = info.x;
          while (xx + info.x < _Ygl->rwidth)
          {
            info.vertices[0] = xx;
            info.vertices[1] = yy;
            info.vertices[2] = (xx + info.cellw);
            info.vertices[3] = yy;
            info.vertices[4] = (xx + info.cellw);
            info.vertices[5] = (yy + info.cellh);
            info.vertices[6] = xx;
            info.vertices[7] = (yy + info.cellh);
            if (isCached == 0)
            {
              YglQuad(&info, &texture, &tmpc, YglTM_vdp2);
              if (info.islinescroll) {
                Vdp2DrawBitmapLineScroll(&info, &texture, info.cellw, info.cellh, varVdp2Regs);
              }
              else {
                requestDrawCell(&info, &texture, varVdp2Regs);
              }
              isCached = 1;
            }
            else {
              YglCachedQuad(&info, &tmpc, YglTM_vdp2);
            }
            xx += info.cellw;
          }
          yy += info.cellh;
        }
      }
    }
  }
  else {
    if (info.islinescroll) {
      if (char_access == 0) return;
      info.sh = (varVdp2Regs->SCXIN1 & 0x7FF);
      info.sv = (varVdp2Regs->SCYIN1 & 0x7FF);
      info.x = 0;
      info.y = 0;
      info.vertices[0] = 0;
      info.vertices[1] = 0;
      info.vertices[2] = _Ygl->rwidth;
      info.vertices[3] = 0;
      info.vertices[4] = _Ygl->rwidth;
      info.vertices[5] = _Ygl->rheight;
      info.vertices[6] = 0;
      info.vertices[7] = _Ygl->rheight;
      vdp2draw_struct infotmp = info;
      infotmp.cellw = _Ygl->rwidth;
      infotmp.cellh = _Ygl->rheight;
      infotmp.flipfunction = 0;

      YglQuad(&infotmp, &texture, &tmpc, YglTM_vdp2);
      Vdp2DrawMapPerLine(&info, &texture, varVdp2Regs);
    }
    else {
      //Vdp2DrawMap(&info, &texture);
      int delayed = 0;
      // Setting miss of cycle patten need to plus 8 dot vertical
      // If pattern access is defined on T0 for NBG0 or NBG1, there is no limitation
      //If there is no access to pattern data, do not display the layer
      if (((ptn_access & 0x1)==0) && Vdp2CheckCharAccessPenalty(char_access, ptn_access) != 0) {
        delayed = 1;
        // YuiMsg("Penalty %x %x\n", char_access, ptn_access);
      }
      info.x = varVdp2Regs->SCXIN1 & 0x7FF;
      info.y = varVdp2Regs->SCYIN1 & 0x7FF;
      Vdp2DrawMapTest(&info, &texture, delayed, varVdp2Regs);
    }
  }
  executeDrawCell();
}

//////////////////////////////////////////////////////////////////////////////

static void Vdp2DrawNBG2(Vdp2* varVdp2Regs)
{
  vdp2draw_struct info = {0};
  YglTexture texture;
  info.dst = 0;
  info.idScreen = NBG2;
  info.uclipmode = 0;
  info.cor = 0;
  info.cog = 0;
  info.cob = 0;
  info.specialcolorfunction = 0;
  info.enable = 0;
  info.startLine = 0;
  info.endLine = (yabsys.VBlankLineCount < 270)?yabsys.VBlankLineCount:270;

  for (int i=0; i<yabsys.VBlankLineCount; i++) {
    info.display[i] = isEnabled(NBG2, &Vdp2Lines[i]);
    info.enable |= info.display[i];
    info.alpha_per_line[i] = (~Vdp2Lines[i].CCRNB & 0x1F) << 3;
  }
  if (!info.enable) return;

  info.transparencyenable = !(varVdp2Regs->BGON & 0x400);
  info.specialprimode = (varVdp2Regs->SFPRMD >> 4) & 0x3;

  info.colornumber = (varVdp2Regs->CHCTLB & 0x2) >> 1;
  info.mapwh = 2;

  ReadPlaneSize(&info, varVdp2Regs->PLSZ >> 4);
  info.x = -((varVdp2Regs->SCXN2 & 0x7FF) % (512 * info.planew));
  info.y = -((varVdp2Regs->SCYN2 & 0x7FF) % (512 * info.planeh));
  ReadPatternData(&info, varVdp2Regs->PNCN2, varVdp2Regs->CHCTLB & 0x1);

  ReadMosaicData(&info, 0x4, varVdp2Regs);

  info.specialcolormode = (varVdp2Regs->SFCCMD >> 4) & 0x3;

  if (varVdp2Regs->SFSEL & 0x4)
    info.specialcode = varVdp2Regs->SFCODE >> 8;
  else
    info.specialcode = varVdp2Regs->SFCODE & 0xFF;


  info.coloroffset = varVdp2Regs->CRAOFA & 0x700;

  info.linecheck_mask = 0x04;
  info.coordincx = info.coordincy = 1;
  info.priority = varVdp2Regs->PRINB & 0x7;
  info.PlaneAddr = (void FASTCALL(*)(void *, int, Vdp2*))&Vdp2NBG2PlaneAddr;

  if ((info.priority == 0) ||
    (varVdp2Regs->BGON & 0x1 && (varVdp2Regs->CHCTLA & 0x70) >> 4 >= 2)) // If NBG0 2048/32786/16M mode is enabled, don't draw
    return;

  info.islinescroll = 0;
  info.linescrolltbl = 0;
  info.lineinc = 0;
  info.isverticalscroll = 0;

  int delayed = 0;
  {
    int char_access = 0;
    int ptn_access = 0;

    for (int i = 0; i < 4; i++) {
      info.char_bank[i] = 0;
      info.pname_bank[i] = 0;
      for (int j = 0; j < 8; j++) {
        if (Vdp2External.AC_VRAM[i][j] == 0x06) {
          info.char_bank[i] = 1;
          char_access |= (1 << j);
        }
        if (Vdp2External.AC_VRAM[i][j] == 0x02) {
          info.pname_bank[i] = 1;
          ptn_access |= (1 << j);
        }
      }
    }
    if (char_access == 0) return;
    if (ptn_access == 0) return;
    // Setting miss of cycle patten need to plus 8 dot vertical
    if (Vdp2CheckCharAccessPenalty(char_access, ptn_access) != 0) {
      delayed = 1;
    }
  }


  info.x = varVdp2Regs->SCXN2 & 0x7FF;
  info.y = varVdp2Regs->SCYN2 & 0x7FF;
  Vdp2DrawMapTest(&info, &texture, delayed, varVdp2Regs);
  executeDrawCell();

}

//////////////////////////////////////////////////////////////////////////////

static void Vdp2DrawNBG3(Vdp2* varVdp2Regs)
{
  vdp2draw_struct info = {0};
  YglTexture texture;
  info.idScreen = NBG3;
  info.dst = 0;
  info.uclipmode = 0;
  info.cor = 0;
  info.cog = 0;
  info.cob = 0;
  info.specialcolorfunction = 0;
  info.enable = 0;
  info.startLine = 0;
  info.endLine = (yabsys.VBlankLineCount < 270)?yabsys.VBlankLineCount:270;

  for (int i=0; i<yabsys.VBlankLineCount; i++) {
    info.display[i] = isEnabled(NBG3, &Vdp2Lines[i]);
    info.enable |= info.display[i];
    info.alpha_per_line[i] = (~Vdp2Lines[i].CCRNB & 0x1F00) >> 5;
  }
  if (!info.enable) return;
  info.transparencyenable = !(varVdp2Regs->BGON & 0x800);
  info.specialprimode = (varVdp2Regs->SFPRMD >> 6) & 0x3;

  info.colornumber = (varVdp2Regs->CHCTLB & 0x20) >> 5;

  info.mapwh = 2;

  ReadPlaneSize(&info, varVdp2Regs->PLSZ >> 6);
  info.x = -((varVdp2Regs->SCXN3 & 0x7FF) % (512 * info.planew));
  info.y = -((varVdp2Regs->SCYN3 & 0x7FF) % (512 * info.planeh));
  ReadPatternData(&info, varVdp2Regs->PNCN3, varVdp2Regs->CHCTLB & 0x10);

  ReadMosaicData(&info, 0x8, varVdp2Regs);

  info.specialcolormode = (varVdp2Regs->SFCCMD >> 6) & 0x03;
  if (varVdp2Regs->SFSEL & 0x8)
    info.specialcode = varVdp2Regs->SFCODE >> 8;
  else
    info.specialcode = varVdp2Regs->SFCODE & 0xFF;


  info.coloroffset = (varVdp2Regs->CRAOFA & 0x7000) >> 4;

  info.linecheck_mask = 0x08;
  info.coordincx = info.coordincy = 1;

  info.priority = (varVdp2Regs->PRINB >> 8) & 0x7;
  info.PlaneAddr = (void FASTCALL(*)(void *, int, Vdp2*))&Vdp2NBG3PlaneAddr;

  if ((info.priority == 0) ||
    (varVdp2Regs->BGON & 0x1 && (varVdp2Regs->CHCTLA & 0x70) >> 4 == 4) || // If NBG0 16M mode is enabled, don't draw
    (varVdp2Regs->BGON & 0x2 && (varVdp2Regs->CHCTLA & 0x3000) >> 12 >= 2)) // If NBG1 2048/32786 is enabled, don't draw
    return;

  info.islinescroll = 0;
  info.linescrolltbl = 0;
  info.lineinc = 0;
  info.isverticalscroll = 0;


  int delayed = 0;
{
  int char_access = 0;
  int ptn_access = 0;
  for (int i = 0; i < 4; i++) {
    info.char_bank[i] = 0;
    info.pname_bank[i] = 0;
    for (int j = 0; j < 8; j++) {
      if (Vdp2External.AC_VRAM[i][j] == 0x07) {
        info.char_bank[i] = 1;
        char_access |= (1 << j);
      }
      if (Vdp2External.AC_VRAM[i][j] == 0x03) {
        info.pname_bank[i] = 1;
        ptn_access |= (1 << j);
      }
    }
  }
  if (char_access == 0) return;
  if (ptn_access == 0) return;
  // Setting miss of cycle patten need to plus 8 dot vertical
  if (Vdp2CheckCharAccessPenalty(char_access, ptn_access) != 0) {
    delayed = 1;
  }
}

  info.x = varVdp2Regs->SCXN3 & 0x7FF;
  info.y = varVdp2Regs->SCYN3 & 0x7FF;
  Vdp2DrawMapTest(&info, &texture, delayed, varVdp2Regs);
  executeDrawCell();
}

//////////////////////////////////////////////////////////////////////////////

static void Vdp2DrawRBG0_part( RBGDrawInfo *rgb, Vdp2* varVdp2Regs)
{
  vdp2draw_struct* info = &rgb->info;

  info->dst = 0;
  info->idScreen = RBG0;
  info->uclipmode = 0;
  info->cor = 0;
  info->cog = 0;
  info->cob = 0;
  info->specialcolorfunction = 0;
  info->enable = 0;
  info->RotWin = NULL;
  info->RotWinMode = 0;

  info->enable = ((varVdp2Regs->BGON & 0x10)!=0);
  if (!info->enable) {
    free(rgb);
    return;
  }
  // //If no VRAM access is granted to RBG0, just abort.
  if ((varVdp2Regs->RAMCTL & 0xFF) == 0) {
    LOG("No RAMCTL for RBG0\n");
    free(rgb);
    return;
  }

  for (int i=info->startLine; i<info->endLine; i++) {
    info->display[i] = info->enable;
    // Color calculation ratio
    rgb->alpha[i] = (~(Vdp2Lines[i].CCRR & 0x1F)) << 3;
    info->alpha_per_line[i] = rgb->alpha[i];
  }

  info->priority = varVdp2Regs->PRIR & 0x7;

  LOG_AREA("RGB0 prio = %d\n", info->priority);

  if (info->priority == 0) {
    free(rgb);
    return;
  }

  info->transparencyenable = !(varVdp2Regs->BGON & 0x1000);

  info->specialprimode = (varVdp2Regs->SFPRMD >> 8) & 0x3;

  info->colornumber = (varVdp2Regs->CHCTLB & 0x7000) >> 12;

  LOG_AREA("RGB0 colornumber = %d\n", info->colornumber);

  info->islinescroll = 0;
  info->linescrolltbl = 0;
  info->lineinc = 0;

  Vdp2ReadRotationTable(0, &rgb->paraA, varVdp2Regs, Vdp2Ram);
  Vdp2ReadRotationTable(1, &rgb->paraB, varVdp2Regs, Vdp2Ram);

  //rgb->paraA.PlaneAddr = (void FASTCALL(*)(void *, int, Vdp2*))&Vdp2ParameterAPlaneAddr;
  //rgb->paraB.PlaneAddr = (void FASTCALL(*)(void *, int, Vdp2*))&Vdp2ParameterBPlaneAddr;
  rgb->paraA.charaddr = (varVdp2Regs->MPOFR & 0x7) * 0x20000;
  rgb->paraB.charaddr = (varVdp2Regs->MPOFR & 0x70) * 0x2000;
  ReadPlaneSizeR(&rgb->paraA, varVdp2Regs->PLSZ >> 8);
  ReadPlaneSizeR(&rgb->paraB, varVdp2Regs->PLSZ >> 12);

if (varVdp2Regs->RPMD == 0x03)
  {
    //printf("RPMD 0x3\n");
    // Enable Window0(RPW0E)?
    if (((varVdp2Regs->WCTLD >> 1) & 0x01) == 0x01)
    {
      info->RotWin = _Ygl->win[0];
      // RPW0A( inside = 0, outside = 1 )
      info->RotWinMode = (varVdp2Regs->WCTLD & 0x01);
      // Enable Window1(RPW1E)?
    }
    else if (((varVdp2Regs->WCTLD >> 3) & 0x01) == 0x01)
    {
      info->RotWin = _Ygl->win[1];
      // RPW1A( inside = 0, outside = 1 )
      info->RotWinMode = ((varVdp2Regs->WCTLD >> 2) & 0x01);
      // Bad Setting Both Window is disabled
    }
  }

  rgb->paraA.screenover = (varVdp2Regs->PLSZ >> 10) & 0x03;
  rgb->paraB.screenover = (varVdp2Regs->PLSZ >> 14) & 0x03;

  // Figure out which Rotation Parameter we're uqrt
  switch (varVdp2Regs->RPMD & 0x3)
  {
  case 0:
    // Parameter A
    info->rotatenum = 0;
    info->PlaneAddr = (void FASTCALL(*)(void *, int, Vdp2*))&Vdp2ParameterAPlaneAddr;
    break;
  case 1:
    // Parameter B
    info->rotatenum = 1;
    info->PlaneAddr = (void FASTCALL(*)(void *, int, Vdp2*))&Vdp2ParameterBPlaneAddr;
    break;
  case 2:
    // Parameter A+B switched via coefficients
    // FIX ME(need to figure out which Parameter is being used)
  case 3:
  default:
    // Parameter A+B switched via rotation parameter window
    // FIX ME(need to figure out which Parameter is being used)
    info->rotatenum = 0;
    info->PlaneAddr = (void FASTCALL(*)(void *, int, Vdp2*))&Vdp2ParameterAPlaneAddr;
    break;
  }

  info->isbitmap = ((varVdp2Regs->CHCTLB & 0x200) != 0);

  if (info->isbitmap)
  {

    // Bitmap Mode
    ReadBitmapSize(info, varVdp2Regs->CHCTLB >> 10, 0x1);
    if (info->rotatenum == 0)
      // Parameter A
      info->charaddr = (varVdp2Regs->MPOFR & 0x7) * 0x20000;
    else
      // Parameter B
      info->charaddr = (varVdp2Regs->MPOFR & 0x70) * 0x2000;

    //If no VRAM access is granted to RBG0 character pattern table , just abort.
      int charAddrBk = (((info->charaddr >> 16)& 0xF) >> ((varVdp2Regs->VRSIZE >> 15)&0x1)) >> 1;
      if (((varVdp2Regs->RAMCTL>>(charAddrBk<<1))&0x3) != 0x3) {
        free(rgb);
        return;
      }

    info->paladdr = (varVdp2Regs->BMPNB & 0x7) << 4;
    info->flipfunction = 0;
    info->specialfunction = 0;
  }
  else
  {
    int i;
    // Tile Mode
    info->mapwh = 4;

    if (info->rotatenum == 0)
      // Parameter A
      ReadPlaneSize(info, varVdp2Regs->PLSZ >> 8);
    else
      // Parameter B
      ReadPlaneSize(info, varVdp2Regs->PLSZ >> 12);

    ReadPatternData(info, varVdp2Regs->PNCR, varVdp2Regs->CHCTLB & 0x100);

    rgb->paraA.ShiftPaneX = 8 + rgb->paraA.planew;
    rgb->paraA.ShiftPaneY = 8 + rgb->paraA.planeh;
    rgb->paraB.ShiftPaneX = 8 + rgb->paraB.planew;
    rgb->paraB.ShiftPaneY = 8 + rgb->paraB.planeh;

    rgb->paraA.MskH = (8 * 64 * rgb->paraA.planew) - 1;
    rgb->paraA.MskV = (8 * 64 * rgb->paraA.planeh) - 1;
    rgb->paraB.MskH = (8 * 64 * rgb->paraB.planew) - 1;
    rgb->paraB.MskV = (8 * 64 * rgb->paraB.planeh) - 1;

    rgb->paraA.MaxH = 8 * 64 * rgb->paraA.planew * 4;
    rgb->paraA.MaxV = 8 * 64 * rgb->paraA.planeh * 4;
    rgb->paraB.MaxH = 8 * 64 * rgb->paraB.planew * 4;
    rgb->paraB.MaxV = 8 * 64 * rgb->paraB.planeh * 4;

    if (rgb->paraA.screenover == OVERMODE_512)
    {
      rgb->paraA.MaxH = 512;
      rgb->paraA.MaxV = 512;
    }

    if (rgb->paraB.screenover == OVERMODE_512)
    {
      rgb->paraB.MaxH = 512;
      rgb->paraB.MaxV = 512;
    }

    for (i = 0; i < 16; i++)
    {
	  Vdp2ParameterAPlaneAddr(info, i, varVdp2Regs);
      rgb->paraA.PlaneAddrv[i] = info->addr;
	  Vdp2ParameterBPlaneAddr(info, i, varVdp2Regs);
      rgb->paraB.PlaneAddrv[i] = info->addr;
    }
  }

  ReadMosaicData(info, 0x10, varVdp2Regs);

  info->specialcolormode = (varVdp2Regs->SFCCMD >> 8) & 0x03;
  if (varVdp2Regs->SFSEL & 0x10)
    info->specialcode = varVdp2Regs->SFCODE >> 8;
  else
    info->specialcode = varVdp2Regs->SFCODE & 0xFF;

  info->coloroffset = (varVdp2Regs->CRAOFB & 0x7) << 8;

  info->linecheck_mask = 0x10;
  info->coordincx = info->coordincy = 1;

  Vdp2DrawRotation(rgb, varVdp2Regs);
  free(rgb);
}


static void Vdp2DrawRBG0(Vdp2* varVdp2Regs)
{
  int nbZone = 1;
  int lastLine = 0;
  int line;
  int max = (yabsys.VBlankLineCount >= 270)?270:yabsys.VBlankLineCount;
  RBGDrawInfo *rgb;
  for (line = 2; line<max; line++) {
    if (!sameVDP2Reg(RBG0, &Vdp2Lines[line-1], &Vdp2Lines[line])) {
      rgb = (RBGDrawInfo *)calloc(1, sizeof(RBGDrawInfo));
      rgb->rgb_type = 0x0;
      rgb->info.startLine = lastLine;
      rgb->info.endLine = line;
      lastLine = line;
      LOG_AREA("RBG0 Draw from %d to %d %x\n", rgb->info.startLine, rgb->info.endLine, varVdp2Regs->BGON);
      Vdp2DrawRBG0_part(rgb, &Vdp2Lines[rgb->info.startLine]);
    }
  }
  rgb = (RBGDrawInfo *)calloc(1, sizeof(RBGDrawInfo));
  rgb->rgb_type = 0x0;
  rgb->info.startLine = lastLine;
  rgb->info.endLine = line;
  LOG_AREA("RBG0 Draw from %d to %d %x\n", rgb->info.startLine, rgb->info.endLine, varVdp2Regs->BGON);
  Vdp2DrawRBG0_part(rgb, &Vdp2Lines[rgb->info.startLine]);

}

#define ceilf(a) ((a)+0.99999f)


//////////////////////////////////////////////////////////////////////////////

static int _VIDCSIsFullscreen;

void VIDCSResize(int originx, int originy, unsigned int w, unsigned int h, int on)
{
  if ((originx == _Ygl->originx)&&
     (originy == _Ygl->originy)&&
     (w == GlWidth)&&
     (h == GlHeight)&&
     (_VIDCSIsFullscreen == on)) return;

  _VIDCSIsFullscreen = on;

  GlWidth = w;
  GlHeight = h;

  YglChangeResolution(_Ygl->rwidth, _Ygl->rheight);

  _Ygl->originx = originx;
  _Ygl->originy = originy;

  glViewport(originx, originy, GlWidth, GlHeight);

}

void VIDCSGetScale(float *xRatio, float *yRatio, int *xUp, int *yUp) {
  double w = 0;
  double h = 0;
  int x,y = 0;
  double dar = (double)GlWidth/(double)GlHeight;
  double par = 4.0/3.0;

  int Intw = (int)(floor((float)GlWidth/(float)_Ygl->width));
  int Inth = (int)(floor((float)GlHeight/(float)_Ygl->height));
  int Int  = 1;
  int modeScreen = _Ygl->stretch;
  #ifndef __LIBRETRO__
  if (yabsys.isRotated) par = 1.0/par;
  #endif
  if (Intw == 0) {
    modeScreen = 0;
    Intw = 1;
  }
  if (Inth == 0) {
    modeScreen = 0;
    Inth = 1;
  }
  Int = (Inth<Intw)?Inth:Intw;

  switch(modeScreen) {
    case 0:
      w = (dar>par)?(double)GlHeight*par:GlWidth;
      h = (dar>par)?(double)GlHeight:(double)GlWidth/par;
      x = (GlWidth-w)/2;
      y = (GlHeight-h)/2;
      break;
    case 1:
      w = GlWidth;
      h = GlHeight;
      x = 0;
      y = 0;
      break;
    case 2:
      w = Int * _Ygl->width;
      h = Int * _Ygl->height;
      x = (GlWidth-w)/2;
      y = (GlHeight-h)/2;
      break;
    default:
       break;
   }

  *xRatio = w / _Ygl->rwidth;
  *yRatio = h / _Ygl->rheight;
  *xUp = x;
  *yUp = y;
}
//////////////////////////////////////////////////////////////////////////////

int VIDCSIsFullscreen(void) {
  return _VIDCSIsFullscreen;
}

//////////////////////////////////////////////////////////////////////////////

int VIDCSVdp1Reset(void)
{
  return 0;
}

void VIDCSReadColorOffset(void) {
  u8 offset[enBGMAX+1] = {0x1, 0x2, 0x4, 0x8, 0x10, 0x1, 0x40, 0x20};
  int line_shift = 0;
  if (_Ygl->rheight > 256) {
    line_shift = 1;
  }
  else {
    line_shift = 0;
  }

  u32 * linebuf = YglGetPerlineBuf();
  for (int line = 0; line < _Ygl->rheight; line++) {
    Vdp2 * lVdp2Regs = &Vdp2Lines[line >> line_shift];
    int b_cor = lVdp2Regs->COBR & 0xFF;
    int b_cog = lVdp2Regs->COBG & 0xFF;
    int b_cob = lVdp2Regs->COBB & 0xFF;
    int a_cor = lVdp2Regs->COAR & 0xFF;
    int a_cog = lVdp2Regs->COAG & 0xFF;
    int a_cob = lVdp2Regs->COAB & 0xFF;
    if (lVdp2Regs->COBR & 0x100)
      b_cor |= 0xFFFFFF00;
    if (lVdp2Regs->COBG & 0x100)
      b_cog |= 0xFFFFFF00;
    if (lVdp2Regs->COBB & 0x100)
      b_cob |= 0xFFFFFF00;
    if (lVdp2Regs->COAR & 0x100)
      a_cor |= 0xFFFFFF00;
    if (lVdp2Regs->COAG & 0x100)
      a_cog |= 0xFFFFFF00;
    if (lVdp2Regs->COAB & 0x100)
      a_cob |= 0xFFFFFF00;
    int colOffB =
       (((int)(128.0f + (b_cob / 2.0)) & 0xFF) << 16)
    | (((int)(128.0f + (b_cog / 2.0)) & 0xFF) << 8)
    | (((int)(128.0f + (b_cor / 2.0)) & 0xFF) << 0);
    int colOffA =
      (((int)(128.0f + (a_cob / 2.0)) & 0xFF) << 16)
    | (((int)(128.0f + (a_cog / 2.0)) & 0xFF) << 8)
    | (((int)(128.0f + (a_cor / 2.0)) & 0xFF) << 0);
    for(int id = 0; id<enBGMAX+1; id++){
      if (isEnabled(id,lVdp2Regs) == 0) {
        linebuf[line+512*id] = 0x0;
      } else {
        if (lVdp2Regs->CLOFEN & offset[id]) {
          // color offset enable
          if (lVdp2Regs->CLOFSL & offset[id])
          {
            // color offset B
            linebuf[line+512*id] = colOffB;
          }
          else
          {
            // color offset A
            linebuf[line+512*id] = colOffA;
          }
        }
        else {
          linebuf[line+512*id] = 0x00808080;
        }
      }
    }
  }
  YglSetPerlineBuf(linebuf);

}

//////////////////////////////////////////////////////////////////////////////

void VIDCSVdp1LocalCoordinate(vdp1cmd_struct *cmd, u8 * ram, Vdp1 * regs)
{
  regs->localX = cmd->CMDXA;
  regs->localY = cmd->CMDYA;
}

//////////////////////////////////////////////////////////////////////////////

int VIDCSVdp2Reset(void)
{
  return 0;
}

//////////////////////////////////////////////////////////////////////////////

void VIDCSVdp2Draw(void)
{
  YglCheckFBSwitch(1);
  //varVdp2Regs = Vdp2RestoreRegs(0, Vdp2Lines);
  //if (varVdp2Regs == NULL) varVdp2Regs = Vdp2Regs;
  Vdp2SetResolution(Vdp2Lines[0].TVMD);
  if (_Ygl->rwidth > YglTM_vdp2->width) {
    int new_width = _Ygl->rwidth;
    int new_height = YglTM_vdp2->height;
    YglTMDeInit(&YglTM_vdp2);
    YglTM_vdp2 = YglTMInit(new_width, new_height);
  }
  YglTmPull(YglTM_vdp2, 0);

  if (Vdp2Regs->TVMD & 0x8000) {
    VIDCSVdp2DrawScreens();
    screenDirty = 1;
    vdp2busy = 1;
  } else {
    if (screenDirty != 0)
      vdp2busy = 1;
    screenDirty = 0;
  }

  /* It would be better to reset manualchange in a Vdp1SwapFrameBuffer
  function that would be called here and during a manual change */
  //Vdp1External.manualchange = 0;
}

//////////////////////////////////////////////////////////////////////////////

#define VDP2_DRAW_LINE 0
static void VIDCSVdp2DrawScreens(void)
{
  u64 before;
  u64 now;
  u32 difftime;
  char str[64];

LOG_ASYN("===================================\n");

  _Ygl->useLineColorOffset[0] = 0;
  _Ygl->useLineColorOffset[1] = 0;

  Vdp2GenerateWindowInfo(&Vdp2Lines[VDP2_DRAW_LINE]);

  if (Vdp1Regs->TVMR & 0x02) {
    Vdp2ReadRotationTable(0, &Vdp1ParaA, &Vdp2Lines[VDP2_DRAW_LINE], Vdp2Ram);
  }
  Vdp2DrawBackScreen(&Vdp2Lines[VDP2_DRAW_LINE]);
  Vdp2DrawLineColorScreen(&Vdp2Lines[VDP2_DRAW_LINE]);

  Vdp2DrawRBG0(&Vdp2Lines[VDP2_DRAW_LINE]);
  Vdp2DrawNBG3(&Vdp2Lines[VDP2_DRAW_LINE]);
  Vdp2DrawNBG2(&Vdp2Lines[VDP2_DRAW_LINE]);
  Vdp2DrawNBG1(&Vdp2Lines[VDP2_DRAW_LINE]);
  Vdp2DrawNBG0(&Vdp2Lines[VDP2_DRAW_LINE]);
  Vdp2DrawRBG1(&Vdp2Lines[VDP2_DRAW_LINE]);

LOG_ASYN("*********************************\n");

  A0_Updated = 0;
  A1_Updated = 0;
  B0_Updated = 0;
  B1_Updated = 0;
}

//////////////////////////////////////////////////////////////////////////////

static void Vdp2SetResolution(u16 TVMD)
{
  int width = 1, height = 1;
  int wratio = 1, hratio = 1;

  // Horizontal Resolution
  switch (TVMD & 0x7)
  {
  case 0:
    width = 320;
    wratio = 1;
    break;
  case 1:
    width = 352;
    wratio = 1;
    break;
  case 2:
    width = 640;
    wratio = 2;
    break;
  case 3:
    width = 704;
      wratio = 2;
    break;
  case 4:
    width = 320;
    wratio = 1;
    break;
  case 5:
    width = 352;
    wratio = 1;
    break;
  case 6:
    width = 640;
    wratio = 2;
    break;
  case 7:
    width = 704;
    wratio = 2;
    break;
  }

  // Vertical Resolution
  switch ((TVMD >> 4) & 0x3)
  {
  case 0:
    height = 224;
    break;
  case 1: height = 240;
    break;
  case 2: height = 256;
    break;
  default: height = 256;
    break;
  }

  hratio = 1;

  // Check for interlace
  switch ((TVMD >> 6) & 0x3)
  {
  case 3: // Double-density Interlace
    height *= 2;
    hratio = 2;
    vdp2_interlace = 1;
    break;
  case 2: // Single-density Interlace
  case 0: // Non-interlace
  default:
    vdp2_interlace = 0;
    break;
  }

  float oldvdp1wd = _Ygl->vdp1wdensity;
  float oldvdp1hd = _Ygl->vdp1hdensity;

  float oldvdp2wd = _Ygl->vdp2wdensity;
  float oldvdp2hd = _Ygl->vdp2hdensity;

  Vdp1SetTextureRatio(wratio, hratio);

  int change = 0;

  change |= (oldvdp1wd != _Ygl->vdp1wdensity);
  change |= (oldvdp1hd != _Ygl->vdp1hdensity);
  change |= (oldvdp2wd != _Ygl->vdp2wdensity);
  change |= (oldvdp2hd != _Ygl->vdp2hdensity);

  change |= (width != _Ygl->rwidth);
  change |= (height != _Ygl->rheight);

  if (change != 0)YglChangeResolution(width, height);
}

//////////////////////////////////////////////////////////////////////////////

void VIDCSGetGlSize(int *width, int *height)
{
  *width = GlWidth;
  *height = GlHeight;
}

void VIDCSGetNativeResolution(int *width, int *height, int*interlace)
{
  *width = _Ygl->rwidth;
  *height = _Ygl->rheight;
  *interlace = vdp2_interlace;
}

void VIDCSVdp2DispOff()
{
}

void VIDCSSetSettingValueMode(int type, int value) {

  switch (type) {
  case VDP_SETTING_FILTERMODE:
    _Ygl->aamode = (AAMODE)value;
    break;
  case VDP_SETTING_UPSCALMODE:
    _Ygl->upmode = (UPMODE)value;
    break;
  case VDP_SETTING_RESOLUTION_MODE:
    if (_Ygl->resolution_mode != (RESOLUTION_MODE)value) {
       _Ygl->resolution_mode = (RESOLUTION_MODE)value;
       YglChangeResolution(_Ygl->rwidth, _Ygl->rheight);
    }
    break;
  case VDP_SETTING_POLYGON_MODE:
    if ((POLYGONMODE)value == GPU_TESSERATION && _Ygl->polygonmode != GPU_TESSERATION) {
      int maj, min;
      glGetIntegerv(GL_MAJOR_VERSION, &maj);
      glGetIntegerv(GL_MINOR_VERSION, &min);
#if defined(_OGL3_)
      if ((maj >=4) && (min >=2)) {
        if (glPatchParameteri) {
          _Ygl->polygonmode = (POLYGONMODE)value;
        } else {
          YuiMsg("GPU tesselation is not possible - fallback on CPU tesselation\n");
          _Ygl->polygonmode = CPU_TESSERATION;
        }
      } else {
        YuiMsg("GPU tesselation is not possible - fallback on CPU tesselation\n");
        _Ygl->polygonmode = CPU_TESSERATION;
      }
#else
      _Ygl->polygonmode = CPU_TESSERATION;
#endif
    } else {


      _Ygl->polygonmode = (POLYGONMODE)value;
    }
  break;
  case VDP_SETTING_ASPECT_RATIO:
    _Ygl->stretch = (RATIOMODE)value;
  break;
  case VDP_SETTING_WIREFRAME:
    _Ygl->wireframe_mode = value;
  break;
  case VDP_SETTING_MESH_MODE:
    _Ygl->meshmode = (MESHMODE)value;
  break;
  case VDP_SETTING_BANDING_MODE:
    _Ygl->bandingmode = (BANDINGMODE)value;
  break;
  default:
  return;
  }
}

//////////////////////////////////////////////////////////////////////////////

u32 FASTCALL Vdp1ReadPolygonColor(vdp1cmd_struct *cmd, Vdp2* varVdp2Regs)
{
  return VDP1COLOR(0x0, cmd->CMDCOLR);
}

static void Vdp1SetTextureRatio(int vdp2widthratio, int vdp2heightratio)
{
  int vdp1w = 1;
  int vdp1h = 1;

  // may need some tweaking
  if (Vdp1Regs->TVMR & 0x1) VDP1_MASK = 0xFF;
  else VDP1_MASK = 0xFFFF;

  // Figure out which vdp1 screen mode to use
  switch (Vdp1Regs->TVMR & 7)
  {
  case 0:
  case 2:
  case 3:
    vdp1w = 1;
    break;
  case 1:
    vdp1w = 2;
    break;
  default:
    vdp1w = 1;
    vdp1h = 1;
    break;
  }

  // Is double-interlace enabled?
  if (Vdp1Regs->FBCR & 0x8) {
    vdp1h = 2;
    vdp1_interlace = (Vdp1Regs->FBCR & 0x4) ? 2 : 1;
  }
  else {
    vdp1_interlace = 0;
  }
  _Ygl->vdp1wdensity = vdp1w;
  _Ygl->vdp1hdensity = vdp1h;

  _Ygl->vdp2wdensity = vdp2widthratio;
  _Ygl->vdp2hdensity = vdp2heightratio;
}

//////////////////////////////////////////////////////////////////////////////
static u16 Vdp2ColorRamGetColorRaw(u32 colorindex) {
  switch (Vdp2Internal.ColorMode)
  {
  case 0:
  case 1:
  {
    colorindex <<= 1;
    return T2ReadWord(Vdp2ColorRam, colorindex & 0xFFF);
  }
  case 2:
  {
    colorindex <<= 2;
    colorindex &= 0xFFF;
    return T2ReadWord(Vdp2ColorRam, colorindex);
  }
  default: break;
  }
  return 0;
}

static u32 Vdp2ColorRamGetLineColorOffset(u32 colorindex, int alpha, int offset)
{
  int flag = 0xFFF;
  switch (Vdp2Internal.ColorMode)
  {
  case 0:
    flag &= 0x380;
  case 1:
  {
    u32 tmp;
    flag &= 0x780;
    //Line color offset from rotation table might be applicable here
    if (offset != 0) colorindex = (colorindex&flag) | (offset&0x7F);
    colorindex <<= 1;
    tmp = T2ReadWord(Vdp2ColorRam, colorindex & 0xFFF);
    return SAT2YAB1(alpha, tmp);
  }
  case 2:
  {
    u32 tmp1, tmp2;
    colorindex <<= 2;
    colorindex &= 0xFFF;
    tmp1 = T2ReadWord(Vdp2ColorRam, colorindex & 0xFFF);
    tmp2 = T2ReadWord(Vdp2ColorRam, (colorindex + 2) & 0xFFF);
    //Line color offset from rotation table are not applicable here
    return SAT2YAB2(alpha, tmp1, tmp2);
  }
  default: break;
  }
  return 0;
}

static u32 Vdp2ColorRamGetLineColor(u32 colorindex, int alpha) {
  return Vdp2ColorRamGetLineColorOffset(colorindex, alpha,0);
}
//////////////////////////////////////////////////////////////////////////////
// Window
static int useRotWin = 0;
int WinS[enBGMAX+1];
int WinS_mode[enBGMAX+1];

void Vdp2GenerateWindowInfo(Vdp2 *varVdp2Regs)
{
  int HShift;
  int v = 0;
  u32 LineWinAddr;
  int upWindow = 0;
  u32 val = 0;

  int Win0[enBGMAX+1];
  int Win0_mode[enBGMAX+1];
  int Win1[enBGMAX+1];
  int Win1_mode[enBGMAX+1];
  int Win_op[enBGMAX+1];

  if (((varVdp2Regs->WCTLD & 0xA)!=0x0) != useRotWin) {
    useRotWin = ((varVdp2Regs->WCTLD & 0xA)!=0x0);
    _Ygl->needWinUpdate |= 1;
  }

  Win0[NBG0] = (varVdp2Regs->WCTLA >> 1) & 0x01;
  Win1[NBG0] = (varVdp2Regs->WCTLA >> 3) & 0x01;
  WinS[NBG0] = (varVdp2Regs->WCTLA >> 5) & 0x01;
  Win0[NBG1] = (varVdp2Regs->WCTLA >> 9) & 0x01;
  Win1[NBG1] = (varVdp2Regs->WCTLA >> 11) & 0x01;
  WinS[NBG1] = (varVdp2Regs->WCTLA >> 13) & 0x01;

  Win0[NBG2] = (varVdp2Regs->WCTLB >> 1) & 0x01;
  Win1[NBG2] = (varVdp2Regs->WCTLB >> 3) & 0x01;
  WinS[NBG2] = (varVdp2Regs->WCTLB >> 5) & 0x01;
  Win0[NBG3] = (varVdp2Regs->WCTLB >> 9) & 0x01;
  Win1[NBG3] = (varVdp2Regs->WCTLB >> 11) & 0x01;
  WinS[NBG3] = (varVdp2Regs->WCTLB >> 13) & 0x01;

  Win0[RBG0] = (varVdp2Regs->WCTLC >> 1) & 0x01;
  Win1[RBG0] = (varVdp2Regs->WCTLC >> 3) & 0x01;
  WinS[RBG0] = (varVdp2Regs->WCTLC >> 5) & 0x01;
  Win0[SPRITE] = (varVdp2Regs->WCTLC >> 9) & 0x01;
  Win1[SPRITE] = (varVdp2Regs->WCTLC >> 11) & 0x01;
  WinS[SPRITE] = (varVdp2Regs->WCTLC >> 13) & 0x01;

  Win0_mode[NBG0] = (varVdp2Regs->WCTLA) & 0x01;
  Win1_mode[NBG0] = (varVdp2Regs->WCTLA >> 2) & 0x01;
  WinS_mode[NBG0] = (varVdp2Regs->WCTLA >> 4) & 0x01;
  Win0_mode[NBG1] = (varVdp2Regs->WCTLA >> 8) & 0x01;
  Win1_mode[NBG1] = (varVdp2Regs->WCTLA >> 10) & 0x01;
  WinS_mode[NBG1] = (varVdp2Regs->WCTLA >> 12) & 0x01;

  Win0_mode[NBG2] = (varVdp2Regs->WCTLB) & 0x01;
  Win1_mode[NBG2] = (varVdp2Regs->WCTLB >> 2) & 0x01;
  WinS_mode[NBG2] = (varVdp2Regs->WCTLB >> 4) & 0x01;
  Win0_mode[NBG3] = (varVdp2Regs->WCTLB >> 8) & 0x01;
  Win1_mode[NBG3] = (varVdp2Regs->WCTLB >> 10) & 0x01;
  WinS_mode[NBG3] = (varVdp2Regs->WCTLB >> 12) & 0x01;

  Win0_mode[RBG0] = (varVdp2Regs->WCTLC) & 0x01;
  Win1_mode[RBG0] = (varVdp2Regs->WCTLC >> 2) & 0x01;
  WinS_mode[RBG0] = (varVdp2Regs->WCTLC >> 4) & 0x01;
  Win0_mode[SPRITE] = (varVdp2Regs->WCTLC >> 8) & 0x01;
  Win1_mode[SPRITE] = (varVdp2Regs->WCTLC >> 10) & 0x01;
  WinS_mode[SPRITE] = (varVdp2Regs->WCTLC >> 12) & 0x01;

  Win_op[NBG0] = (varVdp2Regs->WCTLA >> 7) & 0x01;
  Win_op[NBG1] = (varVdp2Regs->WCTLA >> 15) & 0x01;
  Win_op[NBG2] = (varVdp2Regs->WCTLB >> 7) & 0x01;
  Win_op[NBG3] = (varVdp2Regs->WCTLB >> 15) & 0x01;
  Win_op[RBG0] = (varVdp2Regs->WCTLC >> 7) & 0x01;
  Win_op[SPRITE] = (varVdp2Regs->WCTLC >> 15) & 0x01;

  Win0_mode[SPRITE+1] = (varVdp2Regs->WCTLD >> 8) & 0x01;
  Win0[SPRITE+1] = (varVdp2Regs->WCTLD >> 9) & 0x01;
  Win1_mode[SPRITE+1] = (varVdp2Regs->WCTLD >> 10) & 0x01;
  Win1[SPRITE+1] = (varVdp2Regs->WCTLD >> 11) & 0x01;
  WinS_mode[SPRITE+1] = (varVdp2Regs->WCTLD >> 12) & 0x01;
  WinS[SPRITE+1] = (varVdp2Regs->WCTLD >> 13) & 0x01;
  Win_op[SPRITE+1] = (varVdp2Regs->WCTLD >> 15) & 0x01;

  Win0[RBG1] = Win0[NBG0];
  Win0_mode[RBG1] = Win0_mode[NBG0];
  Win1[RBG1] = Win1[NBG0];
  Win1_mode[RBG1] = Win1_mode[NBG0];
  WinS[RBG1] = WinS[NBG0];
  WinS_mode[RBG1] = WinS_mode[NBG0];
  Win_op[RBG1] = Win_op[NBG0];


  for (int i=0; i<enBGMAX+1; i++) {
    if (Win0[i] != _Ygl->Win0[i]) _Ygl->needWinUpdate |= 1;
    if (Win1[i] != _Ygl->Win1[i]) _Ygl->needWinUpdate |= 1;
    if (WinS[i] != _Ygl->WinS[i]) _Ygl->needWinUpdate |= 1;
    if (Win0_mode[i] != _Ygl->Win0_mode[i]) _Ygl->needWinUpdate |= 1;
    if (Win1_mode[i] != _Ygl->Win1_mode[i]) _Ygl->needWinUpdate |= 1;
    if (WinS_mode[i] != _Ygl->WinS_mode[i]) _Ygl->needWinUpdate |= 1;
    if (Win_op[i] != _Ygl->Win_op[i]) _Ygl->needWinUpdate |= 1;
  #ifdef WINDOW_DEBUG
    //DEBUG
    if ((Win0[i] == 1) || (Win1[i] == 1) || (WinS[i] == 1))
      YuiMsg("Windows are used on layer %d (WO:%d, W1:%d, WS:%d, WS mode %s, WS op %s)\n", i, Win0[i], Win1[i], WinS[i], (WinS_mode[i]==0)?"INSIDE":"OUTSIDE", (Win_op[i]==0)?"OR":"AND");
    else
      YuiMsg("Windows are not used on layer %d\n", i);
  #endif
  }
  memcpy(&_Ygl->Win0[0], &Win0[0], (enBGMAX+1)*sizeof(int));
  memcpy(&_Ygl->Win1[0], &Win1[0], (enBGMAX+1)*sizeof(int));
  memcpy(&_Ygl->WinS[0], &WinS[0], (enBGMAX+1)*sizeof(int));
  memcpy(&_Ygl->Win0_mode[0], &Win0_mode[0], (enBGMAX+1)*sizeof(int));
  memcpy(&_Ygl->Win1_mode[0], &Win1_mode[0], (enBGMAX+1)*sizeof(int));
  memcpy(&_Ygl->WinS_mode[0], &WinS_mode[0], (enBGMAX+1)*sizeof(int));
  memcpy(&_Ygl->Win_op[0], &Win_op[0], (enBGMAX+1)*sizeof(int));

  if( _Ygl->win[0] == NULL ){
    _Ygl->win[0] = (u32*)malloc(512 * 4);
  }
  if( _Ygl->win[1] == NULL ){
    _Ygl->win[1] = (u32*)malloc(512 * 4);
  }

  HShift = 0;
  if (_Ygl->rwidth >= 640) HShift = 0; else HShift = 1;

  // Line Table mode
  if ((varVdp2Regs->LWTA0.part.U & 0x8000))
  {
    // start address
    LineWinAddr = (u32)((((varVdp2Regs->LWTA0.part.U & 0x07) << 15) | (varVdp2Regs->LWTA0.part.L >> 1)) << 2);
    for (v = 0; v < _Ygl->rheight; v++) {
      if (v >= varVdp2Regs->WPSY0 && v <= varVdp2Regs->WPEY0) {
        short HStart = Vdp2RamReadWord(NULL, Vdp2Ram, LineWinAddr + (v << 2)) & 0xFFFF;
        short HEnd = Vdp2RamReadWord(NULL, Vdp2Ram, LineWinAddr + (v << 2) + 2) & 0xFFFF;
        if ((HEnd < HStart) || (HEnd < 0)) val = 0x000000FF; //END < START
        else {
          val = (HStart>>HShift) | ((HEnd>>HShift) << 16);
        }
      } else {
        val = 0x000000FF; //END < START
      }
      if (val != _Ygl->win[0][v]) {
        _Ygl->win[0][v] = val;
        _Ygl->needWinUpdate = 1;
      }
    }
    // Parameter Mode
  }
  else {
    for (v = 0; v < _Ygl->rheight; v++) {
      if (v >= varVdp2Regs->WPSY0 && v <= varVdp2Regs->WPEY0) {
        if (((short)varVdp2Regs->WPEY0 < (short)varVdp2Regs->WPSY0)  || ((short)varVdp2Regs->WPEY0 < 0)) val = 0x000000FF; //END < START
        else {
          val = (varVdp2Regs->WPSX0 >>HShift) | ((varVdp2Regs->WPEX0>>HShift) << 16);
        }
      } else {
        val = 0x000000FF; //END > START
      }
      if (val != _Ygl->win[0][v]) {
        _Ygl->win[0][v] = val;
        _Ygl->needWinUpdate = 1;
      }
    }
  }
  // Line Table mode
  if ((varVdp2Regs->LWTA1.part.U & 0x8000))
  {
    // start address
    LineWinAddr = (u32)((((varVdp2Regs->LWTA1.part.U & 0x07) << 15) | (varVdp2Regs->LWTA1.part.L >> 1)) << 2);
    for (v = 0; v < _Ygl->rheight; v++) {
      if (v >= varVdp2Regs->WPSY1 && v <= varVdp2Regs->WPEY1) {
        short HStart = Vdp2RamReadWord(NULL, Vdp2Ram, LineWinAddr + (v << 2)) & 0xFFFF;
        short HEnd = Vdp2RamReadWord(NULL, Vdp2Ram, LineWinAddr + (v << 2) + 2) & 0xFFFF;
        if ((HEnd < HStart) || (HEnd < 0)) val = 0x000000FF; //END < START
        else {
          val = (HStart>>HShift) | ((HEnd>>HShift) << 16);
        }
      } else {
        val = 0x000000FF; //END < START
      }
      if (val != _Ygl->win[1][v]) {
        _Ygl->win[1][v] = val;
        _Ygl->needWinUpdate = 1;
      }
    }
    // Parameter Mode
  }
  else {
    for (v = 0; v < _Ygl->rheight; v++) {
      if (v >= varVdp2Regs->WPSY1 && v <= varVdp2Regs->WPEY1) {
        if (((short)varVdp2Regs->WPEY1 < (short)varVdp2Regs->WPSY1) || ((short)varVdp2Regs->WPEY1 < 0)) val = 0x000000FF; //END < START
        else {
          val = (varVdp2Regs->WPSX1 >>HShift) | ((varVdp2Regs->WPEX1>>HShift) << 16);
        }
      } else {
        val = 0x000000FF; //END < START
      }
      if (val != _Ygl->win[1][v]) {
        _Ygl->win[1][v] = val;
        _Ygl->needWinUpdate = 1;
      }
    }
  }
}

// 0 .. outside,1 .. inside
static INLINE int Vdp2CheckWindow(vdp2draw_struct *info, int x, int y, int area, u32* win)
{
  if (y < 0) return 0;
  if (y >= _Ygl->rheight) return 0;
  int upLx = win[y] & 0xFFFF;
  int upRx = (win[y] >> 16) & 0xFFFF;
  // inside
  if (area == 1)
  {
    if (win[y] == 0) return 0;
    if (x >= upLx && x <= upRx)
    {
      return 1;
    }
    else {
      return 0;
    }
    // outside
  }
  else {
    if (win[y] == 0) return 1;
    if (x < upLx) return 1;
    if (x > upRx) return 1;
    return 0;
  }
  return 0;
}

// 0 .. all outsize, 1~3 .. partly inside, 4.. all inside
static int FASTCALL Vdp2CheckWindowRange(vdp2draw_struct *info, int x, int y, int w, int h, Vdp2 *varVdp2Regs)
{
  int rtn = 0;

  if (_Ygl->Win0[info->idScreen]  != 0 && _Ygl->Win1[info->idScreen]  == 0)
  {
    rtn += Vdp2CheckWindow(info, x, y, _Ygl->Win0_mode[info->idScreen], _Ygl->win[0]);
    rtn += Vdp2CheckWindow(info, x + w, y, _Ygl->Win0_mode[info->idScreen], _Ygl->win[0]);
    rtn += Vdp2CheckWindow(info, x + w, y + h, _Ygl->Win0_mode[info->idScreen], _Ygl->win[0]);
    rtn += Vdp2CheckWindow(info, x, y + h, _Ygl->Win0_mode[info->idScreen], _Ygl->win[0]);
    return rtn;
  }
  else if (_Ygl->Win0[info->idScreen]  == 0 && _Ygl->Win1[info->idScreen]  != 0)
  {
    rtn += Vdp2CheckWindow(info, x, y, _Ygl->Win1_mode[info->idScreen], _Ygl->win[1]);
    rtn += Vdp2CheckWindow(info, x + w, y, _Ygl->Win1_mode[info->idScreen], _Ygl->win[1]);
    rtn += Vdp2CheckWindow(info, x + w, y + h, _Ygl->Win1_mode[info->idScreen], _Ygl->win[1]);
    rtn += Vdp2CheckWindow(info, x, y + h, _Ygl->Win1_mode[info->idScreen], _Ygl->win[1]);
    return rtn;
  }
  else if (_Ygl->Win0[info->idScreen]  != 0 && _Ygl->Win1[info->idScreen]  != 0)
  {
    if (_Ygl->Win_op[info->idScreen] == 0)
    {
      rtn += (Vdp2CheckWindow(info, x, y, _Ygl->Win0_mode[info->idScreen], _Ygl->win[0]) &
        Vdp2CheckWindow(info, x, y, _Ygl->Win1_mode[info->idScreen], _Ygl->win[1]));
      rtn += (Vdp2CheckWindow(info, x + w, y, _Ygl->Win0_mode[info->idScreen], _Ygl->win[0])&
        Vdp2CheckWindow(info, x + w, y, _Ygl->Win1_mode[info->idScreen], _Ygl->win[1]));
      rtn += (Vdp2CheckWindow(info, x + w, y + h, _Ygl->Win0_mode[info->idScreen], _Ygl->win[0])&
        Vdp2CheckWindow(info, x + w, y + h, _Ygl->Win1_mode[info->idScreen], _Ygl->win[1]));
      rtn += (Vdp2CheckWindow(info, x, y + h, _Ygl->Win0_mode[info->idScreen], _Ygl->win[0]) &
        Vdp2CheckWindow(info, x, y + h, _Ygl->Win1_mode[info->idScreen], _Ygl->win[1]));
      return rtn;
    }
    else {
      rtn += (Vdp2CheckWindow(info, x, y, _Ygl->Win0_mode[info->idScreen], _Ygl->win[0]) |
        Vdp2CheckWindow(info, x, y, _Ygl->Win1_mode[info->idScreen], _Ygl->win[1]));
      rtn += (Vdp2CheckWindow(info, x + w, y, _Ygl->Win0_mode[info->idScreen], _Ygl->win[0]) |
        Vdp2CheckWindow(info, x + w, y, _Ygl->Win1_mode[info->idScreen], _Ygl->win[1]));
      rtn += (Vdp2CheckWindow(info, x + w, y + h, _Ygl->Win0_mode[info->idScreen], _Ygl->win[0]) |
        Vdp2CheckWindow(info, x + w, y + h, _Ygl->Win1_mode[info->idScreen], _Ygl->win[1]));
      rtn += (Vdp2CheckWindow(info, x, y + h, _Ygl->Win0_mode[info->idScreen], _Ygl->win[0]) |
        Vdp2CheckWindow(info, x, y + h, _Ygl->Win1_mode[info->idScreen], _Ygl->win[1]));
      return rtn;
    }
  }
  return 0;
}

static void Vdp2GenLineinfo(vdp2draw_struct *info)
{
  int bound = 0;
  int i;
  u16 val1, val2;
  int index = 0;
  int lineindex = 0;
  if (info->lineinc == 0 || info->islinescroll == 0) return;

  if (VDPLINE_SY(info->islinescroll)) bound += 0x04;
  if (VDPLINE_SX(info->islinescroll)) bound += 0x04;
  if (VDPLINE_SZ(info->islinescroll)) bound += 0x04;

  for (i = 0; i < _Ygl->rheight; i += info->lineinc)
  {
    index = 0;
    if (VDPLINE_SX(info->islinescroll))
    {
      info->lineinfo[lineindex].LineScrollValH = Vdp2RamReadWord(NULL, Vdp2Ram, info->linescrolltbl + (i / info->lineinc)*bound);
      if ((info->lineinfo[lineindex].LineScrollValH & 0x400)) info->lineinfo[lineindex].LineScrollValH |= 0xF800; else info->lineinfo[lineindex].LineScrollValH &= 0x07FF;
      index += 4;
    }
    else {
      info->lineinfo[lineindex].LineScrollValH = 0;
    }

    if (VDPLINE_SY(info->islinescroll))
    {
      info->lineinfo[lineindex].LineScrollValV = Vdp2RamReadWord(NULL, Vdp2Ram, info->linescrolltbl + (i / info->lineinc)*bound + index);
      if ((info->lineinfo[lineindex].LineScrollValV & 0x400)) info->lineinfo[lineindex].LineScrollValV |= 0xF800; else info->lineinfo[lineindex].LineScrollValV &= 0x07FF;
      index += 4;
    }
    else {
      info->lineinfo[lineindex].LineScrollValV = 0;
    }

    if (VDPLINE_SZ(info->islinescroll))
    {
      val1 = Vdp2RamReadWord(NULL, Vdp2Ram, info->linescrolltbl + (i / info->lineinc)*bound + index);
      val2 = Vdp2RamReadWord(NULL, Vdp2Ram, info->linescrolltbl + (i / info->lineinc)*bound + index + 2);
      info->lineinfo[lineindex].CoordinateIncH = (((int)((val1) & 0x07) << 8) | (int)((val2) >> 8));
      index += 4;
    }
    else {
      info->lineinfo[lineindex].CoordinateIncH = 0x0100;
    }

    lineindex++;
  }
}

INLINE void Vdp2SetSpecialPriority(vdp2draw_struct *info, u8 dot, u32 *prio, u32 * cramindex ) {
  *prio = info->priority;
  if (info->specialprimode == 2) {
    *prio = info->priority & 0xE;
    if (info->specialfunction & 1) {
      if (PixelIsSpecialPriority(info->specialcode, dot))
      {
        *prio |= 1;
      }
    }
  }
}

static INLINE u32 Vdp2GetCCOn(vdp2draw_struct *info, u8 dot, u32 cramindex, Vdp2 *varVdp2Regs) {

  const int CCMD = ((varVdp2Regs->CCCTL >> 8) & 0x01);  // hard/vdp2/hon/p12_14.htm#CCMD_
  int cc = 1;
  if (CCMD == 0) {  // Calculate Rate mode
    switch (info->specialcolormode)
    {
    case 1: if (info->specialcolorfunction == 0) { cc = 0; } break;
    case 2:
      if (info->specialcolorfunction == 0) { cc = 0; }
      else { if ((info->specialcode & (1 << ((dot & 0xF) >> 1))) == 0) { cc = 0; } }
      break;
   case 3:
     if (((Vdp2ColorRamGetColorRaw(cramindex) & 0x8000) == 0)) { cc = 0; }
     break;
    }
  }
  else {  // Calculate Add mode
    switch (info->specialcolormode)
    {
    case 1:
      if (info->specialcolorfunction == 0) { cc = 0; }
      break;
    case 2:
      if (info->specialcolorfunction == 0) { cc = 0; }
      else { if ((info->specialcode & (1 << ((dot & 0xF) >> 1))) == 0) { cc = 0; } }
      break;
   case 3:
     if (((Vdp2ColorRamGetColorRaw(cramindex) & 0x8000) == 0)) { cc = 0; }
     break;
    }
  }
  return cc;
}


static INLINE u32 Vdp2GetPixel4bpp(vdp2draw_struct *info, u32 addr, YglTexture *texture, Vdp2 *varVdp2Regs) {

  u32 cramindex;
  u16 dotw = Vdp2RamReadWord(NULL, Vdp2Ram, addr);
  u8 dot;
  u32 cc;
  u32 priority = 0;

  dot = (dotw & 0xF000) >> 12;
  if (!(dot & 0xF) && info->transparencyenable) {
    *texture->textdata++ = 0x00000000;
  } else {
    cramindex = (info->coloroffset + ((info->paladdr << 4) | (dot & 0xF)));
    Vdp2SetSpecialPriority(info, dot, &priority, &cramindex);
    cc = Vdp2GetCCOn(info, dot, cramindex, varVdp2Regs);
    *texture->textdata++ = VDP2COLOR(info->idScreen, info->alpha, priority, cc, cramindex);
  }

  cc = 1;
  dot = (dotw & 0xF00) >> 8;
  if (!(dot & 0xF) && info->transparencyenable) {
    *texture->textdata++ = 0x00000000;
  }
  else {
    cramindex = (info->coloroffset + ((info->paladdr << 4) | (dot & 0xF)));
    Vdp2SetSpecialPriority(info, dot, &priority, &cramindex);
    cc = Vdp2GetCCOn(info, dot, cramindex, varVdp2Regs);
    *texture->textdata++ = VDP2COLOR(info->idScreen, info->alpha, priority, cc, cramindex);
  }

  cc = 1;
  dot = (dotw & 0xF0) >> 4;
  if (!(dot & 0xF) && info->transparencyenable) {
    *texture->textdata++ = 0x00000000;
  }
  else {
    cramindex = (info->coloroffset + ((info->paladdr << 4) | (dot & 0xF)));
    Vdp2SetSpecialPriority(info, dot, &priority, &cramindex);
    cc = Vdp2GetCCOn(info, dot, cramindex, varVdp2Regs);
    *texture->textdata++ = VDP2COLOR(info->idScreen, info->alpha, priority, cc, cramindex);
  }

  cc = 1;
  dot = (dotw & 0xF);
  if (!(dot & 0xF) && info->transparencyenable) {
    *texture->textdata++ = 0x00000000;
  }
  else {
    cramindex = (info->coloroffset + ((info->paladdr << 4) | (dot & 0xF)));
    Vdp2SetSpecialPriority(info, dot, &priority, &cramindex);
    cc = Vdp2GetCCOn(info, dot, cramindex, varVdp2Regs);
    *texture->textdata++ = VDP2COLOR(info->idScreen, info->alpha, priority, cc, cramindex);
  }
  return 0;
}

static INLINE u32 Vdp2GetPixel8bpp(vdp2draw_struct *info, u32 addr, YglTexture *texture, Vdp2* varVdp2Regs) {

  u32 cramindex;
  u16 dotw = Vdp2RamReadWord(NULL, Vdp2Ram, addr);
  u8 dot;
  u32 cc;
  u32 priority = 0;

  cc = 1;
  dot = (dotw & 0xFF00)>>8;
  if (!(dot & 0xFF) && info->transparencyenable) *texture->textdata++ = 0x00000000;
  else {
    cramindex = info->coloroffset + ((info->paladdr << 4) | (dot & 0xFF));
    Vdp2SetSpecialPriority(info, dot, &priority, &cramindex);
    cc = Vdp2GetCCOn(info, dot, cramindex, varVdp2Regs);
    *texture->textdata++ = VDP2COLOR(info->idScreen, info->alpha, priority, cc, cramindex);
  }
  cc = 1;
  dot = (dotw & 0xFF);
  if (!(dot & 0xFF) && info->transparencyenable) *texture->textdata++ = 0x00000000;
  else {
    cramindex = info->coloroffset + ((info->paladdr << 4) | (dot & 0xFF));
    Vdp2SetSpecialPriority(info, dot, &priority, &cramindex);
    cc = Vdp2GetCCOn(info, dot, cramindex, varVdp2Regs);
    *texture->textdata++ = VDP2COLOR(info->idScreen, info->alpha, priority, cc, cramindex);
  }
  return 0;
}


static INLINE u32 Vdp2GetPixel16bpp(vdp2draw_struct *info, u32 addr, Vdp2* varVdp2Regs) {
  u32 cramindex;
  u8 cc;
  u16 dot = Vdp2RamReadWord(NULL, Vdp2Ram, addr);
  u32 priority = 0;
  if ((dot == 0) && info->transparencyenable) return 0x00000000;
  else {
    cramindex = info->coloroffset + dot;
    Vdp2SetSpecialPriority(info, dot, &priority, &cramindex);
    cc = Vdp2GetCCOn(info, dot, cramindex, varVdp2Regs);
    return VDP2COLOR(info->idScreen, info->alpha, priority, cc, cramindex);
  }
}

static INLINE u32 Vdp2GetPixel16bppbmp(vdp2draw_struct *info, u32 addr, Vdp2 *varVdp2Regs) {
  u32 color;
  u16 dot = Vdp2RamReadWord(NULL, Vdp2Ram, addr);
//if (info->patternwh == 2) printf("%x\n", dot);
//Ca deconne ici
  int cc = Vdp2GetCCOn(info, dot, 0, varVdp2Regs);
  if (!(dot & 0x8000) && info->transparencyenable) color = 0x00000000;
  else color = VDP2COLOR(info->idScreen, info->alpha, info->priority, cc, RGB555_TO_RGB24(dot));
  return color;
}

static INLINE u32 Vdp2GetPixel32bppbmp(vdp2draw_struct *info, u32 addr, Vdp2 *varVdp2Regs) {
  u32 color;
  u16 dot1, dot2;
  int cc;
  dot1 = Vdp2RamReadWord(NULL, Vdp2Ram, addr);
  dot2 = Vdp2RamReadWord(NULL, Vdp2Ram, addr+2);

  cc = Vdp2GetCCOn(info, 0, 0, varVdp2Regs);

  if (!(dot1 & 0x8000) && info->transparencyenable) color = 0x00000000;
  else color = VDP2COLOR(info->idScreen, info->alpha, info->priority, cc, (((dot1 & 0xFF) << 16) | (dot2 & 0xFF00) | (dot2 & 0xFF)));
  return color;
}

static void FASTCALL Vdp2DrawCellInterlace(vdp2draw_struct *info, YglTexture *texture, Vdp2 *varVdp2Regs) {
  int i, j, h, addr, inc;
  unsigned int *start;
  unsigned int color;
  switch (info->colornumber)
  {
  case 0: // 4 BPP
    for (i = 0; i < info->cellh; i++)
    {
      info->alpha = info->alpha_per_line[(info->draw_line + i)>>vdp2_interlace];
      for (j = 0; j < info->cellw; j += 4)
      {
        Vdp2GetPixel4bpp(info, info->charaddr, texture, varVdp2Regs);
        info->charaddr += 2;
      }
      texture->textdata += texture->w;
    }
    break;
  case 1: // 8 BPP
    for (i = 0; i < info->cellh; i++)
    {
      info->alpha = info->alpha_per_line[(info->draw_line + i)>>vdp2_interlace];
      for (j = 0; j < info->cellw; j += 2)
      {

        Vdp2GetPixel8bpp(info, info->charaddr, texture, varVdp2Regs);
        info->charaddr += 2;
      }
      texture->textdata += texture->w;
    }
    break;
  case 2: // 16 BPP(palette)
    for (i = 0; i < info->cellh; i++)
    {
      info->alpha = info->alpha_per_line[(info->draw_line + i)>>vdp2_interlace];
      for (j = 0; j < info->cellw; j++)
      {
        *texture->textdata++ = Vdp2GetPixel16bpp(info, info->charaddr, varVdp2Regs);
        info->charaddr += 2;
      }
      texture->textdata += texture->w;
    }
    break;
  case 3: // 16 BPP(RGB)
    addr = info->charaddr;
    inc = (info->cellw + texture->w);
    start = texture->textdata;
    texture->textdata += (info->cellw + texture->w)*info->cellh/2;
    info->charaddr += (!vdp2_is_odd_frame)?0:2*info->cellw;
    for (i = 0; i < info->cellh/2; i+=2)
    {
      info->alpha = info->alpha_per_line[(info->draw_line + i)>>vdp2_interlace];
      for (j = 0; j < info->cellw; j++)
      {
        color = Vdp2GetPixel16bppbmp(info, info->charaddr, varVdp2Regs);
        *(texture->textdata+inc) = color;
        *texture->textdata = color;
        *texture->textdata++;
        info->charaddr += 2;
      }
      info->charaddr += 2*info->cellw;
      texture->textdata += inc;
    }
    info->charaddr = addr;
    info->charaddr += (!vdp2_is_odd_frame)?2*info->cellw:0;
    texture->textdata = start;
    for (i = 0; i < info->cellh/2; i+=2)
    {
      info->alpha = info->alpha_per_line[(info->draw_line + i)>>vdp2_interlace];
      for (j = 0; j < info->cellw; j++)
      {
        color = Vdp2GetPixel16bppbmp(info, info->charaddr, varVdp2Regs);
        *(texture->textdata+inc) = color;
        *texture->textdata = color;
        *texture->textdata++;
        info->charaddr += 2;
      }
      info->charaddr += 2*info->cellw;
      texture->textdata += inc;
    }
    break;
  case 4: // 32 BPP
    for (i = 0; i < info->cellh; i++)
    {
      info->alpha = info->alpha_per_line[(info->draw_line + i)>>vdp2_interlace];
      for (j = 0; j < info->cellw; j++)
      {
        *texture->textdata++ = Vdp2GetPixel32bppbmp(info, info->charaddr, varVdp2Regs);
        info->charaddr += 4;
      }
      texture->textdata += texture->w;
    }
    break;
  }
}

static u32 getAlpha(vdp2draw_struct *info, int id) {
  int idx = info->draw_line + id;
  if (idx < 0) idx = 0;
  if ((idx>>vdp2_interlace) > yabsys.VBlankLineCount) idx = yabsys.VBlankLineCount<<vdp2_interlace;
  return info->alpha_per_line[idx>>vdp2_interlace];
}

static void FASTCALL Vdp2DrawCell_in_sync(vdp2draw_struct *info, YglTexture *texture, Vdp2 *varVdp2Regs)
{
  int i, j;


//   if ((vdp2_interlace == 1) && (_Ygl->rheight > 448)) {
//     // Weird... Partly fix True Pinball in case of interlace only but it is breaking Zen Nihon Pro Wres, so use the bad test of the height
//     Vdp2DrawCellInterlace(info, texture, varVdp2Regs);
//     return;
//   }
  switch (info->colornumber)
  {
  case 0: // 4 BPP
    for (i = 0; i < info->cellh; i++)
    {
      info->alpha = getAlpha(info, i);
      for (j = 0; j < info->cellw; j += 4)
      {
        Vdp2GetPixel4bpp(info, info->charaddr, texture, varVdp2Regs);
        info->charaddr += 2;
      }
      texture->textdata += texture->w;
    }
    break;
  case 1: // 8 BPP
    for (i = 0; i < info->cellh; i++)
    {
      info->alpha = getAlpha(info, i);
      for (j = 0; j < info->cellw; j += 2)
      {
        Vdp2GetPixel8bpp(info, info->charaddr, texture, varVdp2Regs);
        info->charaddr += 2;
      }
      texture->textdata += texture->w;
    }
    break;
  case 2: // 16 BPP(palette)
    for (i = 0; i < info->cellh; i++)
    {
      info->alpha = getAlpha(info, i);
      for (j = 0; j < info->cellw; j++)
      {
        *texture->textdata++ = Vdp2GetPixel16bpp(info, info->charaddr, varVdp2Regs);
        info->charaddr += 2;
      }
      texture->textdata += texture->w;
    }
    break;
  case 3: // 16 BPP(RGB)
    for (i = 0; i < info->cellh; i++)
    {
      info->alpha = getAlpha(info, i);
      for (j = 0; j < info->cellw; j++)
      {
        *texture->textdata++ = Vdp2GetPixel16bppbmp(info, info->charaddr, varVdp2Regs);
        info->charaddr += 2;
      }
      texture->textdata += texture->w;
    }
    break;
  case 4: // 32 BPP
    for (i = 0; i < info->cellh; i++)
    {
      info->alpha = getAlpha(info, i);
      for (j = 0; j < info->cellw; j++)
      {
        *texture->textdata++ = Vdp2GetPixel32bppbmp(info, info->charaddr, varVdp2Regs);
        info->charaddr += 4;
      }
      texture->textdata += texture->w;
    }
    break;
  }
}


static void FASTCALL Vdp2DrawBitmapLineScroll(vdp2draw_struct *info, YglTexture *texture, int width, int height,  Vdp2 *varVdp2Regs)
{
  int i, j;

  for (i = 0; i < height; i++)
  {
    int sh, sv;
    u32 baseaddr;
    vdp2Lineinfo * line;
    info->draw_line = i;
    info->alpha = info->alpha_per_line[info->draw_line>>vdp2_interlace];
    baseaddr = (u32)info->charaddr;
    line = &(info->lineinfo[i*info->lineinc]);

    if (VDPLINE_SX(info->islinescroll))
      sh = line->LineScrollValH + info->sh;
    else
      sh = info->sh;

    if (VDPLINE_SY(info->islinescroll))
      sv = line->LineScrollValV + info->sv;
    else
      sv = i + info->sv;

    sv &= (info->cellh - 1);
    sh &= (info->cellw - 1);
    if ((line->LineScrollValH >= 0) && (line->LineScrollValH < sh) && (sv >0)) {
      sv -= 1;
    }
    switch (info->colornumber) {
    case 0:
      baseaddr += (((sh + sv * info->cellw) >> 2) << 1);
      for (j = 0; j < width; j += 4)
      {
        Vdp2GetPixel4bpp(info, baseaddr, texture, varVdp2Regs);
        baseaddr += 2;
      }
      break;
    case 1:
      baseaddr += sh + sv * info->cellw;
      for (j = 0; j < width; j += 2)
      {
        Vdp2GetPixel8bpp(info, baseaddr, texture, varVdp2Regs);
        baseaddr += 2;
      }
      break;
    case 2:
      baseaddr += ((sh + sv * info->cellw) << 1);
      for (j = 0; j < width; j++)
      {
        *texture->textdata++ = Vdp2GetPixel16bpp(info, baseaddr, varVdp2Regs);
        baseaddr += 2;

      }
      break;
    case 3:
      baseaddr += ((sh + sv * info->cellw) << 1);
      for (j = 0; j < width; j++)
      {
        *texture->textdata++ = Vdp2GetPixel16bppbmp(info, baseaddr, varVdp2Regs);
        baseaddr += 2;
      }
      break;
    case 4:
      baseaddr += ((sh + sv * info->cellw) << 2);
      for (j = 0; j < width; j++)
      {
        //if (info->isverticalscroll){
        //	sv += T1ReadLong(Vdp2Ram, info->verticalscrolltbl+(j>>3) ) >> 16;
        //}
        *texture->textdata++ = Vdp2GetPixel32bppbmp(info, baseaddr, varVdp2Regs);
        baseaddr += 4;
      }
      break;
    }
    texture->textdata += texture->w;
  }
}


static void FASTCALL Vdp2DrawBitmapCoordinateInc(vdp2draw_struct *info, YglTexture *texture,  Vdp2 *varVdp2Regs)
{
  u32 color;
  int i, j;

  int incv = 1.0 / info->coordincy*256.0;
  int inch = 1.0 / info->coordincx*256.0;

  int lineinc = 1;
  int linestart = 0;

  int height = _Ygl->rheight;

  // Is double-interlace enabled?
/*
  if ((vdp1_interlace != 0) || (height >= 448)) {
    lineinc = 2;
  }

  if (vdp1_interlace != 0) {
    linestart = vdp1_interlace - 1;
  }
*/

  for (i = linestart; i < lineinc*height; i += lineinc)
  {
    int sh, sv;
    int v;
    u32 baseaddr;
    vdp2Lineinfo * line;
    baseaddr = (u32)info->charaddr;
    line = &(info->lineinfo[i*info->lineinc]);

    info->draw_line = i;

    v = (i*incv) >> 8;
    if (VDPLINE_SZ(info->islinescroll))
      inch = line->CoordinateIncH;

    if (inch == 0) inch = 1;

    if (VDPLINE_SX(info->islinescroll))
      sh = info->sh + line->LineScrollValH;
    else
      sh = info->sh;

    if (VDPLINE_SY(info->islinescroll))
      sv = info->sv + line->LineScrollValV;
    else
      sv = v + info->sv;

    //sh &= (info->cellw - 1);
    sv &= (info->cellh - 1);

    switch (info->colornumber) {
    case 0:
      baseaddr = baseaddr + (sh >> 1) + (sv * (info->cellw >> 1));
      for (j = 0; j < _Ygl->rwidth; j++)
      {
        u32 h = ((j*inch) >> 8);
        u32 addr = (baseaddr + (h >> 1));
        if (addr >= 0x80000) {
          *texture->textdata++ = 0x0000;
        }
        else {
          int cc = 1;
          u8 dot = Vdp2RamReadByte(NULL, Vdp2Ram, addr);
          u32 alpha = info->alpha_per_line[info->draw_line>>vdp2_interlace];
          if (!(h & 0x01)) dot = dot >> 4;
          if (!(dot & 0xF) && info->transparencyenable) *texture->textdata++ = 0x00000000;
          else {
            color = (info->coloroffset + ((info->paladdr << 4) | (dot & 0xF)));
            switch (info->specialcolormode)
            {
            case 1: if (info->specialcolorfunction == 0) { cc = 0; } break;
            case 2:
              if (info->specialcolorfunction == 0) { cc = 0; }
              else { if ((info->specialcode & (1 << ((dot & 0xF) >> 1))) == 0) { cc = 0; } }
              break;
            case 3:
              if (((Vdp2ColorRamGetColorRaw(color) & 0x8000) == 0)) { cc = 0; }
              break;
            }
            *texture->textdata++ = VDP2COLOR(info->idScreen, alpha, info->priority, cc, color);
          }
        }
      }
      break;
    case 1:
      baseaddr += sh + sv * info->cellw;

      for (j = 0; j < _Ygl->rwidth; j++)
      {
        int h = ((j*inch) >> 8);
        u32 alpha = info->alpha_per_line[info->draw_line>>vdp2_interlace];
        u8 dot = Vdp2RamReadByte(NULL, Vdp2Ram, baseaddr + h);
        if (!dot && info->transparencyenable) {
          *texture->textdata++ = 0; continue;
        }
        else {
          int cc = 1;
          color = info->coloroffset + ((info->paladdr << 4) | (dot & 0xFF));
          switch (info->specialcolormode)
          {
          case 1: if (info->specialcolorfunction == 0) { cc = 0; } break;
          case 2:
            if (info->specialcolorfunction == 0) { cc = 0; }
            else { if ((info->specialcode & (1 << ((dot & 0xF) >> 1))) == 0) { cc = 0; } }
            break;
          case 3:
            if (((Vdp2ColorRamGetColorRaw(color) & 0x8000) == 0)) { cc = 0; }
            break;
          }
          *texture->textdata++ = VDP2COLOR(info->idScreen, alpha, info->priority, cc, color);
        }
      }

      break;
    case 2:
      baseaddr += ((sh + sv * info->cellw) << 1);
      for (j = 0; j < _Ygl->rwidth; j++)
      {
        int h = ((j*inch) >> 8) << 1;
        *texture->textdata++ = Vdp2GetPixel16bpp(info, baseaddr + h, varVdp2Regs);

      }
      break;
    case 3:
      baseaddr += ((sh + sv * info->cellw) << 1);
      for (j = 0; j < _Ygl->rwidth; j++)
      {
        int h = ((j*inch) >> 8) << 1;
        *texture->textdata++ = Vdp2GetPixel16bppbmp(info, baseaddr + h, varVdp2Regs);
      }
      break;
    case 4:
      baseaddr += ((sh + sv * info->cellw) << 2);
      for (j = 0; j < _Ygl->rwidth; j++)
      {
        int h = (j*inch >> 8) << 2;
        *texture->textdata++ = Vdp2GetPixel32bppbmp(info, baseaddr + h, varVdp2Regs);
      }
      break;
    }
    texture->textdata += texture->w;
  }
}

//////////////////////////////////////////////////////////////////////////////

static INLINE u32 Vdp2RotationFetchPixel(vdp2draw_struct *info, int x, int y, int cellw)
{
  u32 dot;
  u32 cramindex;
  u32 alpha = info->alpha_per_line[info->draw_line>>vdp2_interlace];
  u8 lowdot = 0x00;
  u32 priority = 0;
  switch (info->colornumber)
  {
  case 0: // 4 BPP
    dot = Vdp2RamReadByte(NULL, Vdp2Ram, (info->charaddr + (((y * cellw) + x) >> 1) ));
    if (!(x & 0x1)) dot >>= 4;
    if (!(dot & 0xF) && info->transparencyenable) return 0x00000000;
    else {
      int cc = 1;
      cramindex = (info->coloroffset + ((info->paladdr << 4) | (dot & 0xF)));
      Vdp2SetSpecialPriority(info, dot, &priority, &cramindex);
      switch (info->specialcolormode)
      {
      case 1: if (info->specialcolorfunction == 0) { cc = 0; } break;
      case 2:
        if (info->specialcolorfunction == 0) { cc = 0; }
        else { if ((info->specialcode & (1 << ((dot & 0xF) >> 1))) == 0) { cc = 0; } }
        break;
      case 3:
        if (((Vdp2ColorRamGetColorRaw(cramindex) & 0x8000) == 0)) { cc = 0; }
        break;
      }
      return   VDP2COLOR(info->idScreen, alpha, priority, cc, cramindex);
    }
  case 1: // 8 BPP
    dot = Vdp2RamReadByte(NULL, Vdp2Ram, (info->charaddr + (y * cellw) + x));
    if (!(dot & 0xFF) && info->transparencyenable) return 0x00000000;
    else {
      int cc = 1;
      cramindex = info->coloroffset + ((info->paladdr << 4) | (dot & 0xFF));
      Vdp2SetSpecialPriority(info, dot, &priority, &cramindex);
      switch (info->specialcolormode)
      {
      case 1: if (info->specialcolorfunction == 0) { cc = 0; } break;
      case 2:
        if (info->specialcolorfunction == 0) { cc = 0; }
        else { if ((info->specialcode & (1 << ((dot & 0xF) >> 1))) == 0) { cc = 0; } }
        break;
      case 3:
        if (((Vdp2ColorRamGetColorRaw(cramindex) & 0x8000) == 0)) { cc = 0; }
        break;
      }
      return   VDP2COLOR(info->idScreen, alpha, priority, cc, cramindex);
    }
  case 2: // 16 BPP(palette)
    dot = Vdp2RamReadWord(NULL, Vdp2Ram, (info->charaddr + ((y * cellw) + x) * 2));
    if ((dot == 0) && info->transparencyenable) return 0x00000000;
    else {
      int cc = 1;
      cramindex = (info->coloroffset + dot);
      Vdp2SetSpecialPriority(info, dot, &priority, &cramindex);
      switch (info->specialcolormode)
      {
      case 1: if (info->specialcolorfunction == 0) { cc = 0; } break;
      case 2:
        if (info->specialcolorfunction == 0) { cc = 0; }
        else { if ((info->specialcode & (1 << ((dot & 0xF) >> 1))) == 0) { cc = 0; } }
        break;
      case 3:
        if (((Vdp2ColorRamGetColorRaw(cramindex) & 0x8000) == 0)) { cc = 0; }
        break;
      }
      return   VDP2COLOR(info->idScreen, alpha, priority, cc, cramindex);
    }
  case 3: // 16 BPP(RGB)
    dot = Vdp2RamReadWord(NULL, Vdp2Ram, (info->charaddr + ((y * cellw) + x) * 2));
    if (!(dot & 0x8000) && info->transparencyenable) return 0x00000000;
    else return VDP2COLOR(info->idScreen, alpha, info->priority, 1, RGB555_TO_RGB24(dot & 0xFFFF));
  case 4: // 32 BPP
    dot = Vdp2RamReadLong(NULL, Vdp2Ram, (info->charaddr + ((y * cellw) + x) * 4));
    if (!(dot & 0x80000000) && info->transparencyenable) return 0x00000000;
    else return VDP2COLOR(info->idScreen, alpha, info->priority, 1, dot & 0xFFFFFF);
  default:
    return 0;
  }
}

static int getPriority(int id, Vdp2 *a) {
  switch (id) {
  case NBG0:
      return (a->PRINA & 0x7);
  case NBG1:
    return ((a->PRINA >> 8) & 0x7);
  default:
    return 0;
  }
}
//////////////////////////////////////////////////////////////////////////////

static void Vdp2DrawMapPerLine(vdp2draw_struct *info, YglTexture *texture, Vdp2 *varVdp2Regs) {

  int lineindex = 0;

  int sx; //, sy;
  int mapx, mapy;
  int planex, planey;
  int pagex, pagey;
  int charx, chary;
  int dot_on_planey;
  int dot_on_pagey;
  int dot_on_planex;
  int dot_on_pagex;
  int h, v;
  const int planeh_shift = 9 + (info->planeh - 1);
  const int planew_shift = 9 + (info->planew - 1);
  const int plane_shift = 9;
  const int plane_mask = 0x1FF;
  const int page_shift = 9 - 7 + (64 / info->pagewh);
  const int page_mask = 0x0f >> ((info->pagewh / 32) - 1);

  int preplanex = -1;
  int preplaney = -1;
  int prepagex = -1;
  int prepagey = -1;
  int mapid = 0;
  int premapid = -1;
  int scaleh = 0;

  info->patternpixelwh = 8 * info->patternwh;
  info->draww = _Ygl->rwidth;

  if (_Ygl->rheight >= 448)
    scaleh = 1;

  const int incv = 1.0 / info->coordincy*256.0;
  const int res_shift = 0;

  int linemask = 0;
  switch (info->lineinc) {
  case 1:
    linemask = 0;
    break;
  case 2:
    linemask = 0x01;
    break;
  case 4:
    linemask = 0x03;
    break;
  case 8:
    linemask = 0x07;
    break;
  }


  for (v = 0; v < _Ygl->rheight; v += 1) {  // ToDo: info->coordincy

    int targetv = 0;

    if (VDPLINE_SX(info->islinescroll)) {
      sx = info->sh + info->lineinfo[lineindex<<res_shift].LineScrollValH;
    }
    else {
      sx = info->sh;
    }

    if (VDPLINE_SY(info->islinescroll)) {
      targetv = info->sv + (v&linemask) + info->lineinfo[lineindex<<res_shift].LineScrollValV;
    }
    else {
      targetv = info->sv + ((v*incv)>>8);
    }

    if (info->isverticalscroll) {
      // this is *wrong*, vertical scroll use a different value per cell
      // info->verticalscrolltbl should be incremented by info->verticalscrollinc
      // each time there's a cell change and reseted at the end of the line...
      // or something like that :)
      targetv += Vdp2RamReadLong(NULL, Vdp2Ram, info->verticalscrolltbl) >> 16;
    }

    if (VDPLINE_SZ(info->islinescroll)) {
      info->coordincx = info->lineinfo[lineindex<<res_shift].CoordinateIncH / 256.0f;
      if (info->coordincx == 0) {
        info->coordincx = _Ygl->rwidth;
      }
      else {
        info->coordincx = 1.0f / info->coordincx;
      }
    }

    if (info->coordincx < info->maxzoom) info->coordincx = info->maxzoom;

    // determine which chara shoud be used.
    //mapy   = (v+sy) / (512 * info->planeh);
    mapy = (targetv) >> planeh_shift;
    //int dot_on_planey = (v + sy) - mapy*(512 * info->planeh);
    dot_on_planey = (targetv)-(mapy << planeh_shift);
    mapy = mapy & 0x01;
    //planey = dot_on_planey / 512;
    planey = dot_on_planey >> plane_shift;
    //int dot_on_pagey = dot_on_planey - planey * 512;
    dot_on_pagey = dot_on_planey & plane_mask;
    planey = planey & (info->planeh - 1);
    //pagey = dot_on_pagey / (512 / info->pagewh);
    pagey = dot_on_pagey >> page_shift;
    //chary = dot_on_pagey - pagey*(512 / info->pagewh);
    chary = dot_on_pagey & page_mask;
    if (pagey < 0) pagey = info->pagewh - 1 + pagey;

    int inch = 1.0 / info->coordincx*256.0;

    for (int j = 0; j < info->draww; j += 1) {

      int h = ((j*inch) >> 8);

      //mapx = (h + sx) / (512 * info->planew);
      mapx = (h + sx) >> planew_shift;
      //int dot_on_planex = (h + sx) - mapx*(512 * info->planew);
      dot_on_planex = (h + sx) - (mapx << planew_shift);
      mapx = mapx & 0x01;

      mapid = info->mapwh * mapy + mapx;
      if (mapid != premapid) {
        info->PlaneAddr(info, mapid, varVdp2Regs);
        premapid = mapid;
      }

      //planex = dot_on_planex / 512;
      planex = dot_on_planex >> plane_shift;
      //int dot_on_pagex = dot_on_planex - planex * 512;
      dot_on_pagex = dot_on_planex & plane_mask;
      planex = planex & (info->planew - 1);
      //pagex = dot_on_pagex / (512 / info->pagewh);
      pagex = dot_on_pagex >> page_shift;
      //charx = dot_on_pagex - pagex*(512 / info->pagewh);
      charx = dot_on_pagex & page_mask;
      if (pagex < 0) pagex = info->pagewh - 1 + pagex;

      if (planex != preplanex || pagex != prepagex || planey != preplaney || pagey != prepagey) {
        if (Vdp2PatternAddrPos(info, planex, pagex, planey, pagey, varVdp2Regs) == 0) continue;
        preplanex = planex;
        preplaney = planey;
        prepagex = pagex;
        prepagey = pagey;
      }
      info->draw_line = v>>scaleh;
      info->priority = getPriority(info->idScreen, &Vdp2Lines[v>>scaleh]); //MapPerLine is called only for NBG0 and NBG1
      int priority = info->priority;
      if (info->specialprimode == 1) {
        info->priority = (info->priority & 0xFFFFFFFE) | info->specialfunction;
      }

      int x = charx;
      int y = chary;

      if (info->patternwh == 1)
      {
        x &= 8 - 1;
        y &= 8 - 1;

        // vertical flip
        if (info->flipfunction & 0x2)
          y = 8 - 1 - y;

        // horizontal flip
        if (info->flipfunction & 0x1)
          x = 8 - 1 - x;
      }
      else
      {
        if (info->flipfunction)
        {
          y &= 16 - 1;
          if (info->flipfunction & 0x2)
          {
            if (!(y & 8))
              y = 8 - 1 - y + 16;
            else
              y = 16 - 1 - y;
          }
          else if (y & 8)
            y += 8;

          if (info->flipfunction & 0x1)
          {
            if (!(x & 8))
              y += 8;

            x &= 8 - 1;
            x = 8 - 1 - x;
          }
          else if (x & 8)
          {
            y += 8;
            x &= 8 - 1;
          }
          else
            x &= 8 - 1;
        }
        else
        {
          y &= 16 - 1;
          if (y & 8)
            y += 8;
          if (x & 8)
            y += 8;
          x &= 8 - 1;
        }
      }
      *(texture->textdata++) = Vdp2RotationFetchPixel(info, x, y, info->cellw);
      info->priority = priority;
    }
    if((v & linemask) == linemask) lineindex++;
    texture->textdata += texture->w;
  }

}

static void Vdp2DrawMapTest(vdp2draw_struct *info, YglTexture *texture, int delayed, Vdp2 *varVdp2Regs) {

  int lineindex = 0;

  int sx = 0; //, sy;
  int mapx = 0, mapy = 0;
  int planex = 0, planey = 0;
  int pagex = 0, pagey = 0;
  int charx = 0, chary = 0;
  int dot_on_planey = 0;
  int dot_on_pagey = 0;
  int dot_on_planex = 0;
  int dot_on_pagex = 0;
  int h, v;
  int cell_count = 0;

  const int planeh_shift = 9 + (info->planeh - 1);
  const int planew_shift = 9 + (info->planew - 1);
  const int plane_shift = 9;
  const int plane_mask = 0x1FF;
  const int page_shift = 9 - 7 + (64 / info->pagewh);
  const int page_mask = 0x0f >> ((info->pagewh / 32) - 1);

  info->patternpixelwh = 8 * info->patternwh;
  info->draww = (int)((float)_Ygl->rwidth / info->coordincx);
  info->drawh = (int)((float)_Ygl->rheight / info->coordincy);
  info->lineinc = info->patternpixelwh;

  //info->coordincx = 1.0f;

  for (v = -info->patternpixelwh; v < info->drawh + info->patternpixelwh; v += info->patternpixelwh) {
    int targetv = 0;
    sx = info->x;

    if (!info->isverticalscroll) {
      targetv = info->y + v;
      // determine which chara shoud be used.
      //mapy   = (v+sy) / (512 * info->planeh);
      mapy = (targetv) >> planeh_shift;
      //int dot_on_planey = (v + sy) - mapy*(512 * info->planeh);
      dot_on_planey = (targetv)-(mapy * (1 << planeh_shift));
      mapy = mapy & 0x01;
      //planey = dot_on_planey / 512;
      planey = dot_on_planey >> plane_shift;
      //int dot_on_pagey = dot_on_planey - planey * 512;
      dot_on_pagey = dot_on_planey & plane_mask;
      planey = planey & (info->planeh - 1);
      //pagey = dot_on_pagey / (512 / info->pagewh);
      pagey = dot_on_pagey >> page_shift;
      //chary = dot_on_pagey - pagey*(512 / info->pagewh);
      chary = dot_on_pagey & page_mask;
      if (pagey < 0) pagey = info->pagewh - 1 + pagey;
    }
    else {
      cell_count = 0;
    }
    for (h = -info->patternpixelwh; h < info->draww + info->patternpixelwh; h += info->patternpixelwh) {

      if (info->isverticalscroll) {
        targetv = info->y + v + (Vdp2RamReadLong(NULL, Vdp2Ram, info->verticalscrolltbl + cell_count) >> 16);
        cell_count += info->verticalscrollinc;
        // determine which chara shoud be used.
        //mapy   = (v+sy) / (512 * info->planeh);
        mapy = (targetv) >> planeh_shift;
        //int dot_on_planey = (v + sy) - mapy*(512 * info->planeh);
        dot_on_planey = (targetv)-(mapy << planeh_shift);
        mapy = mapy & 0x01;
        //planey = dot_on_planey / 512;
        planey = dot_on_planey >> plane_shift;
        //int dot_on_pagey = dot_on_planey - planey * 512;
        dot_on_pagey = dot_on_planey & plane_mask;
        planey = planey & (info->planeh - 1);
        //pagey = dot_on_pagey / (512 / info->pagewh);
        pagey = dot_on_pagey >> page_shift;
        //chary = dot_on_pagey - pagey*(512 / info->pagewh);
        chary = dot_on_pagey & page_mask;
        if (pagey < 0) pagey = info->pagewh - 1 + pagey;
      }

      //mapx = (h + sx) / (512 * info->planew);
      mapx = (h + sx) >> planew_shift;
      //int dot_on_planex = (h + sx) - mapx*(512 * info->planew);
      dot_on_planex = (h + sx) - (mapx * (1<<planew_shift));
      mapx = mapx & 0x01;
      //planex = dot_on_planex / 512;
      planex = dot_on_planex >> plane_shift;
      //int dot_on_pagex = dot_on_planex - planex * 512;
      dot_on_pagex = dot_on_planex & plane_mask;
      planex = planex & (info->planew - 1);
      //pagex = dot_on_pagex / (512 / info->pagewh);
      pagex = dot_on_pagex >> page_shift;
      //charx = dot_on_pagex - pagex*(512 / info->pagewh);
      charx = dot_on_pagex & page_mask;

      info->PlaneAddr(info, info->mapwh * mapy + mapx, varVdp2Regs);
      if (Vdp2PatternAddrPos(info, planex, pagex, planey, pagey, varVdp2Regs) != 0) {
        //Only draw if there is a valid character pattern VRAM access for the current layer
        int charAddrBk = (((info->charaddr >> 16)& 0xF) >> ((varVdp2Regs->VRSIZE >> 15)&0x1)) >> 1;
        if (info->char_bank[charAddrBk] == 1) {
          int x = h - charx;
          int y = v - chary;
          info->draw_line =  y;
          if (delayed && (h == -info->patternpixelwh)) continue;
          Vdp2DrawPatternPos(info, texture, x+delayed*8, y, 0, 0, info->lineinc, varVdp2Regs);
        }
      }
    }
    lineindex++;
  }

}

//////////////////////////////////////////////////////////////////////////////

static u32 FASTCALL DoNothing(UNUSED void *info, u32 pixel)
{
  return pixel;
}

//////////////////////////////////////////////////////////////////////////////

static u32 FASTCALL DoColorOffset(void *info, u32 pixel)
{
  return pixel;
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void ReadVdp2ColorOffset(Vdp2 * regs, vdp2draw_struct *info, int mask)
{
  if (regs->CLOFEN & mask)
  {
    // color offset enable
    if (regs->CLOFSL & mask)
    {
      // color offset B
      info->cor = regs->COBR & 0xFF;
      if (regs->COBR & 0x100)
        info->cor |= 0xFFFFFF00;

      info->cog = regs->COBG & 0xFF;
      if (regs->COBG & 0x100)
        info->cog |= 0xFFFFFF00;

      info->cob = regs->COBB & 0xFF;
      if (regs->COBB & 0x100)
        info->cob |= 0xFFFFFF00;
    }
    else
    {
      // color offset A
      info->cor = regs->COAR & 0xFF;
      if (regs->COAR & 0x100)
        info->cor |= 0xFFFFFF00;

      info->cog = regs->COAG & 0xFF;
      if (regs->COAG & 0x100)
        info->cog |= 0xFFFFFF00;

      info->cob = regs->COAB & 0xFF;
      if (regs->COAB & 0x100)
        info->cob |= 0xFFFFFF00;
    }
    info->PostPixelFetchCalc = &DoColorOffset;
  }
  else { // color offset disable

    info->PostPixelFetchCalc = &DoNothing;
    info->cor = 0;
    info->cob = 0;
    info->cog = 0;

  }
}


static void Vdp2DrawBackScreen(Vdp2 *varVdp2Regs)
{
  u32 scrAddr;
  int dot;
  u32 * linebuf;
  vdp2draw_struct info = {0};

  static int line[512 * 4];

  if (varVdp2Regs->VRSIZE & 0x8000)
    scrAddr = (((varVdp2Regs->BKTAU & 0x7) << 16) | varVdp2Regs->BKTAL) * 2;
  else
    scrAddr = (((varVdp2Regs->BKTAU & 0x3) << 16) | varVdp2Regs->BKTAL) * 2;

#if defined(__ANDROID__) || defined(_OGLES3_) || defined(_OGLES31_) || defined(_OGL3_)
  if ((varVdp2Regs->BKTAU & 0x8000) != 0 ) {
    // per line background color
    u32* back_pixel_data = YglGetBackColorPointer();
    if (back_pixel_data != NULL) {
      int line_shift = 0;
      if (_Ygl->rheight > 256) {
        line_shift = 1;
      }
      else {
        line_shift = 0;
      }
      for (int i = 0; i < _Ygl->rheight; i++) {
        u8 r, g, b, a;
        ReadVdp2ColorOffset(&Vdp2Lines[i >> line_shift], &info, 0x20);
        dot = Vdp2RamReadWord(NULL, Vdp2Ram, (scrAddr + 2 * i));
        r = Y_MAX(((dot & 0x1F) << 3) + info.cor, 0);
        g = Y_MAX((((dot & 0x3E0) >> 5) << 3) + info.cog, 0);
        b = Y_MAX((((dot & 0x7C00) >> 10) << 3) + info.cob, 0);
        a = ((~varVdp2Regs->CCRLB & 0x1F00) >> 5)|NONE;
        *back_pixel_data++ = (a << 24) | ((b&0xFF) << 16) | ((g&0xFF) << 8) | (r&0xFF);
      }
      YglSetBackColor(_Ygl->rheight);
    }
  }
  else {
    dot = Vdp2RamReadWord(NULL, Vdp2Ram, scrAddr);
    YglSetClearColor(
      (float)(((dot & 0x1F) << 3) + info.cor) / (float)(0xFF),
      (float)((((dot & 0x3E0) >> 5) << 3) + info.cog) / (float)(0xFF),
      (float)((((dot & 0x7C00) >> 10) << 3) + info.cob) / (float)(0xFF)
    );
  }
#else
  if (varVdp2Regs->BKTAU & 0x8000)
  {
    int y;

    for (y = 0; y < _Ygl->rheight; y++)
    {
      dot = Vdp2RamReadWord(NULL, Vdp2Ram, scrAddr);
      scrAddr += 2;

      lineColors[3 * y + 0] = (dot & 0x1F) << 3;
      lineColors[3 * y + 1] = (dot & 0x3E0) >> 2;
      lineColors[3 * y + 2] = (dot & 0x7C00) >> 7;
      line[4 * y + 0] = 0;
      line[4 * y + 1] = y;
      line[4 * y + 2] = _Ygl->rwidth;
      line[4 * y + 3] = y;
    }

    glColorPointer(3, GL_UNSIGNED_BYTE, 0, lineColors);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(2, GL_INT, 0, line);
    glEnableClientState(GL_VERTEX_ARRAY);
    glDrawArrays(GL_LINES, 0, _Ygl->rheight * 2);
    glDisableClientState(GL_COLOR_ARRAY);
    glColor3ub(0xFF, 0xFF, 0xFF);
  }
  else
  {
    dot = Vdp2RamReadWord(NULL, Vdp2Ram, scrAddr);

    glColor3ub((dot & 0x1F) << 3, (dot & 0x3E0) >> 2, (dot & 0x7C00) >> 7);

    line[0] = 0;
    line[1] = 0;
    line[2] = _Ygl->rwidth;
    line[3] = 0;
    line[4] = _Ygl->rwidth;
    line[5] = _Ygl->rheight;
    line[6] = 0;
    line[7] = _Ygl->rheight;

    glVertexPointer(2, GL_INT, 0, line);
    glEnableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 8);
    glColor3ub(0xFF, 0xFF, 0xFF);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}
#endif
  }

//////////////////////////////////////////////////////////////////////////////
// 11.3 Line Color insertion
//  7.1 Line Color Screen
static void Vdp2DrawLineColorScreen(Vdp2 *varVdp2Regs)
{

  u32 cacheaddr = 0xFFFFFFFF;
  int inc = 0;
  int line_cnt = _Ygl->rheight;
  int i;
  u32 * line_pixel_data;
  u32 addr;

  if (varVdp2Regs->LNCLEN == 0) return;

  line_pixel_data = YglGetLineColorScreenPointer();
  if (line_pixel_data == NULL) {
    return;
  }

  if ((varVdp2Regs->LCTA.part.U & 0x8000)) {
    inc = 0x02; // color per line
  }
  else {
    inc = 0x00; // single color
  }

  u8 alpha = ((~varVdp2Regs->CCRLB & 0x1F) << 3) | NONE;

  addr = (varVdp2Regs->LCTA.all & 0x7FFFF)<<1;
  for (i = 0; i < line_cnt; i++) {
    u16 LineColorRamAdress = Vdp2RamReadWord(NULL, Vdp2Ram, addr);
    *(line_pixel_data) = Vdp2ColorRamGetLineColor(LineColorRamAdress, alpha);
    line_pixel_data++;
    addr += inc;
  }

  YglSetLineColorScreen(line_pixel_data, line_cnt);

}

//////////////////////////////////////////////////////////////////////////////

static int Vdp2CheckCharAccessPenalty(int char_access, int ptn_access) {
  if (_Ygl->rwidth >= 640) {
    //if (char_access < ptn_access) {
    //  return -1;
    //}
    if (ptn_access & 0x01) { // T0
      // T0-T2
      if ((char_access & 0x07) != 0) {
        if (char_access < ptn_access) {
          return -1;
        }
        return 0;
      }
    }

    if (ptn_access & 0x02) { // T1
      // T1-T3
      if ((char_access & 0x0E) != 0) {
        if (char_access < ptn_access) {
          return -1;
        }
        return 0;
      }
    }

    if (ptn_access & 0x04) { // T2
      // T0,T2,T3
      if ((char_access & 0x0D) != 0) {
        if (char_access < ptn_access) {
          return -1;
        }
        return 0;
      }
    }

    if (ptn_access & 0x08) { // T3
      // T0,T1,T3
      if ((char_access & 0xB) != 0) {
        if (char_access < ptn_access) {
          return -1;
        }
        return 0;
      }
    }
    return -1;
  }
  else {

    if (ptn_access & 0x01) { // T0
      // T0-T2, T4-T7
      if ((char_access & 0xF7) != 0) {
        return 0;
      }
    }

    if (ptn_access & 0x02) { // T1
      // T0-T3, T5-T7
      if ((char_access & 0xEF) != 0) {
        return 0;
      }
    }

    if (ptn_access & 0x04) { // T2
      // T0-T3, T6-T7
      if ((char_access & 0xCF) != 0) {
        return 0;
      }
    }

    if (ptn_access & 0x08) { // T3
      // T0-T3, T7
      if ((char_access & 0x8F) != 0) {
        return 0;
      }
    }

    if (ptn_access & 0x10) { // T4
      // T0-T3
      if ((char_access & 0x0F) != 0) {
        return 0;
      }
    }

    if (ptn_access & 0x20) { // T5
      // T1-T3
      if ((char_access & 0x0E) != 0) {
        return 0;
      }
    }

    if (ptn_access & 0x40) { // T6
      // T2,T3
      if ((char_access & 0x0C) != 0) {
        return 0;
      }
    }

    if (ptn_access & 0x80) { // T7
      // T3
      if ((char_access & 0x08) != 0) {
        return 0;
      }
    }
    return -1;
  }
  return 0;
}

//////////////////////////////////////////////////////////////////////////////

static void Vdp2DrawRBG1_part(RBGDrawInfo *rgb, Vdp2* varVdp2Regs)
{
  YglTexture texture;
  YglCache tmpc;
  vdp2draw_struct* info = &rgb->info;

  info->dst = 0;
  info->idScreen = RBG1;
  info->uclipmode = 0;
  info->cor = 0;
  info->cog = 0;
  info->cob = 0;
  info->specialcolorfunction = 0;

  int i;
  info->enable = 0;

  info->cellh = 256 << vdp2_interlace;

// RBG1 mode
  info->enable = ((varVdp2Regs->BGON & 0x20)!=0);
  // RBG1 shall not work without RGB0 but it looks like the HW is able to... MechWarrior 2 - 31st Century Combat - Arcade Combat Edition is using this capability...
  //if (!(varVdp2Regs->BGON & 0x10)) info->enable = 0; //When both R0ON and R1ON are 1, the normal scroll screen can no longer be displayed vdp2 pdf, section 4.1 Screen Display Control

  if (!info->enable) {
   free(rgb);
   return;
  }
  for (int i=info->startLine; i<info->endLine; i++) {
    info->display[i] = info->enable;
    // Color calculation ratio
    rgb->alpha[i] = (~Vdp2Lines[i].CCRNA & 0x1F)<<3;
    info->alpha_per_line[i] = rgb->alpha[i];
  }

    // Read in Parameter B
    Vdp2ReadRotationTable(1, &rgb->paraB, varVdp2Regs, Vdp2Ram);

    if ((info->isbitmap = varVdp2Regs->CHCTLA & 0x2) != 0)
    {
      // Bitmap Mode

      ReadBitmapSize(info, varVdp2Regs->CHCTLA >> 2, 0x3);
      if (vdp2_interlace) info->cellh *= 2;

      info->charaddr = (varVdp2Regs->MPOFR & 0x70) * 0x2000;
      info->paladdr = (varVdp2Regs->BMPNA & 0x7) << 4;
      info->flipfunction = 0;
      info->specialfunction = 0;
    }
    else
    {
      // Tile Mode
      info->mapwh = 4;
      ReadPlaneSize(info, varVdp2Regs->PLSZ >> 12);
      ReadPatternData(info, varVdp2Regs->PNCN0, varVdp2Regs->CHCTLA & 0x1);

      rgb->paraB.ShiftPaneX = 8 + info->planew;
      rgb->paraB.ShiftPaneY = 8 + info->planeh;
      rgb->paraB.MskH = (8 * 64 * info->planew) - 1;
      rgb->paraB.MskV = (8 * 64 * info->planeh) - 1;
      rgb->paraB.MaxH = 8 * 64 * info->planew * 4;
      rgb->paraB.MaxV = 8 * 64 * info->planeh * 4;

    }

    info->rotatenum = 1;
    //rgb->paraB.PlaneAddr = (void FASTCALL(*)(void *, int, Vdp2*))&Vdp2ParameterBPlaneAddr;
    rgb->paraB.coefenab = varVdp2Regs->KTCTL & 0x100;
    rgb->paraB.charaddr = (varVdp2Regs->MPOFR & 0x70) * 0x2000;
    ReadPlaneSizeR(&rgb->paraB, varVdp2Regs->PLSZ >> 12);
    for (i = 0; i < 16; i++)
    {
      Vdp2ParameterBPlaneAddr(info, i, varVdp2Regs);
      rgb->paraB.PlaneAddrv[i] = info->addr;
    }

  ReadMosaicData(info, 0x1, varVdp2Regs);

  info->transparencyenable = !(varVdp2Regs->BGON & 0x100);
  info->specialprimode = varVdp2Regs->SFPRMD & 0x3;
  info->specialcolormode = varVdp2Regs->SFCCMD & 0x3;
  if (varVdp2Regs->SFSEL & 0x1)
    info->specialcode = varVdp2Regs->SFCODE >> 8;
  else
    info->specialcode = varVdp2Regs->SFCODE & 0xFF;

  info->colornumber = (varVdp2Regs->CHCTLA & 0x70) >> 4;
  int dest_alpha = ((varVdp2Regs->CCCTL >> 9) & 0x01);

  info->coloroffset = (varVdp2Regs->CRAOFA & 0x7) << 8;

  info->linecheck_mask = 0x01;
  info->priority = varVdp2Regs->PRINA & 0x7;

  LOG_AREA("RGB1 prio = %d\n", info->priority);

  if (info->priority == 0) {
    free(rgb);
    return;
  }

  ReadLineScrollData(info, varVdp2Regs->SCRCTL & 0xFF, varVdp2Regs->LSTA0.all);
  info->lineinfo = lineNBG0;
  Vdp2GenLineinfo(info);
  if (varVdp2Regs->SCRCTL & 1)
  {
    info->isverticalscroll = 1;
    info->verticalscrolltbl = (varVdp2Regs->VCSTA.all & 0x7FFFE) << 1;
    if (varVdp2Regs->SCRCTL & 0x100)
      info->verticalscrollinc = 8;
    else
      info->verticalscrollinc = 4;
  }
  else
    info->isverticalscroll = 0;

  // RBG1 draw
  Vdp2DrawRotation(rgb, varVdp2Regs);
  free(rgb);
}

static int sameVDP2RegRBG0(Vdp2 *a, Vdp2 *b)
{
  if ((a->BGON & 0x1010) != (b->BGON & 0x1010)) return 0;
  if ((a->PRIR & 0x7) != (b->PRIR & 0x7)) return 0;
//  if ((a->CCCTL & 0xFF10) != (b->CCCTL & 0xFF10)) return 0;
//  if ((a->SFPRMD & 0x300) != (b->SFPRMD & 0x300)) return 0;
//  if ((a->CHCTLB & 0x7700) != (b->CHCTLB & 0x7700)) return 0;
//  if ((a->WCTLC & 0xFF) != (b->WCTLC & 0xFF)) return 0;
  if ((a->RPTA.all) != (b->RPTA.all)) return 0;
//  if ((a->VRSIZE & 0x8000) != (b->VRSIZE & 0x8000)) return 0;
//  if ((a->RAMCTL & 0x80FF) != (b->RAMCTL & 0x80FF)) return 0;
//  if ((a->KTCTL & 0xFFFF) != (b->KTCTL & 0xFFFF)) return 0;
//  if ((a->PLSZ & 0xFF00) != (b->PLSZ & 0xFF00)) return 0;
//  if ((a->KTAOF & 0x707) != (b->KTAOF & 0x707)) return 0;
//  if ((a->MPOFR & 0x77) != (b->MPOFR & 0x77)) return 0;
  if ((a->RPMD & 0x3) != (b->RPMD & 0x3)) return 0;
//  if ((a->WCTLD & 0xF) != (b->WCTLD & 0xF)) return 0;
//  if ((a->BMPNB & 0x7) != (b->BMPNB & 0x7)) return 0;
//  if ((a->PNCR & 0xFFFF) != (b->PNCR & 0xFFFF)) return 0;
//  if ((a->MZCTL & 0xFF10) != (b->MZCTL & 0xFF10)) return 0;
//  if ((a->SFCCMD &0x300) != (b->SFCCMD &0x300)) return 0;
//  if ((a->SFSEL & 0x10) != (b->SFSEL & 0x10)) return 0;
//  if ((a->SFCODE & 0xFFFF) != (b->SFCODE & 0xFFFF)) return 0;
//  if ((a->LNCLEN & 0x10) != (b->LNCLEN & 0x10)) return 0;
//  if ((a->LCTA.all) != (b->LCTA.all)) return 0;
//  if ((a->CRAOFB & 0x7) != (b->CRAOFB & 0x7)) return 0;
//  if ((a->CLOFSL & 0x10) != (b->CLOFSL & 0x10)) return 0;
//  if ((a->COBR & 0x1FF) != (b->COBR & 0x1FF)) return 0;
//  if ((a->COBG & 0x1FF) != (b->COBG & 0x1FF)) return 0;
//  if ((a->COBB & 0x1FF) != (b->COBB & 0x1FF)) return 0;
//  if ((a->COAR & 0x1FF) != (b->COAR & 0x1FF)) return 0;
//  if ((a->COAG & 0x1FF) != (b->COAG & 0x1FF)) return 0;
//  if ((a->COAB & 0x1FF) != (b->COAB & 0x1FF)) return 0;
  return 1;
}

static int sameVDP2RegRBG1(Vdp2 *a, Vdp2 *b)
{
  if ((a->BGON & 0x130) != (b->BGON & 0x130)) return 0;
  if ((a->PRINA & 0x7) != (b->PRINA & 0x7)) return 0;
//  if ((a->CCCTL & 0xFF01) != (b->CCCTL & 0xFF01)) return 0;
//  if ((a->BMPNA & 0x7) != (b->BMPNA & 0x7)) return 0;
//  if ((a->MPOFR & 0x77) != (b->MPOFR & 0x77)) return 0;
//  if ((a->VRSIZE & 0x8000) != (b->VRSIZE & 0x8000)) return 0;
//  if ((a->RAMCTL & 0x80FF) != (b->RAMCTL & 0x80FF)) return 0;
//  if ((a->KTCTL & 0xFFFF) != (b->KTCTL & 0xFFFF)) return 0;
//  if ((a->PLSZ & 0xFF00) != (b->PLSZ & 0xFF00)) return 0;
//  if ((a->SFPRMD & 0x3) != (b->SFPRMD & 0x3)) return 0;
//  if ((a->CHCTLA & 0x7F) != (b->CHCTLA & 0x7F)) return 0;
//  if ((a->MZCTL & 0xFF01) != (b->MZCTL & 0xFF01)) return 0;
 if ((a->CCRNA &0x1F) != (b->CCRNA &0x1F)) return 0;
 if ((a->SFCCMD &0x3) != (b->SFCCMD &0x3)) return 0;
//  if ((a->SFSEL & 0x1) != (b->SFSEL & 0x1)) return 0;
//  if ((a->SFCODE & 0xFFFF) != (b->SFCODE & 0xFFFF)) return 0;
//  if ((a->LNCLEN & 0x1) != (b->LNCLEN & 0x1)) return 0;
//  if ((a->CRAOFA & 0x7) != (b->CRAOFA & 0x7)) return 0;
//  if ((a->CLOFSL & 0x1) != (b->CLOFSL & 0x1)) return 0;
//  if ((a->WCTLA & 0xFF) != (b->WCTLA & 0xFF)) return 0;
//  if ((a->PNCN0 & 0xFFFF) != (b->PNCN0 & 0xFFFF)) return 0;
//  if ((a->SCRCTL & 0x3F) != (b->SCRCTL & 0x3F)) return 0;
//  if ((a->LSTA0.all) != (b->LSTA0.all)) return 0;
//  if ((a->VCSTA.all) != (b->VCSTA.all)) return 0;
  if ((a->RPTA.all) != (b->RPTA.all)) return 0;
//  if ((a->WCTLD & 0xF) != (b->WCTLD & 0xF)) return 0;
//  if ((a->LCTA.all) != (b->LCTA.all)) return 0;
//  if ((a->COBR & 0x1FF) != (b->COBR & 0x1FF)) return 0;
//  if ((a->COBG & 0x1FF) != (b->COBG & 0x1FF)) return 0;
//  if ((a->COBB & 0x1FF) != (b->COBB & 0x1FF)) return 0;
//  if ((a->COAR & 0x1FF) != (b->COAR & 0x1FF)) return 0;
//  if ((a->COAG & 0x1FF) != (b->COAG & 0x1FF)) return 0;
//  if ((a->COAB & 0x1FF) != (b->COAB & 0x1FF)) return 0;
  return 1;
}

static int sameVDP2Reg(int id, Vdp2 *a, Vdp2 *b)
{
  switch (id) {
    case RBG0: return sameVDP2RegRBG0(a, b);
    case RBG1: return sameVDP2RegRBG1(a, b);
    default:
    break;
  }
  return 1;
}

static void Vdp2DrawRBG1(Vdp2 *varVdp2Regs)
{
  int nbZone = 1;
  int lastLine = 0;
  int line;
  int max = (yabsys.VBlankLineCount >= 270)?270:yabsys.VBlankLineCount;
  RBGDrawInfo *rgb;
  for (line = 2; line<max; line++) {
    if (!sameVDP2Reg(RBG1, &Vdp2Lines[line-1], &Vdp2Lines[line])) {
      rgb = (RBGDrawInfo *)calloc(1, sizeof(RBGDrawInfo));
      rgb->rgb_type = 0x04;
      rgb->info.startLine = lastLine;
      rgb->info.endLine = line;
      lastLine = line;
      LOG_AREA("RBG1 Draw from %d to %d %x\n", rgb->info.startLine, rgb->info.endLine, varVdp2Regs->BGON);
      Vdp2DrawRBG1_part(rgb, &Vdp2Lines[rgb->info.startLine]);
    }
  }
  rgb = (RBGDrawInfo *)calloc(1, sizeof(RBGDrawInfo));
  rgb->rgb_type = 0x04;
  rgb->info.startLine = lastLine;
  rgb->info.endLine = line;
  LOG_AREA("RBG1 Draw from %d to %d %x\n", rgb->info.startLine, rgb->info.endLine, varVdp2Regs->BGON);
  Vdp2DrawRBG1_part(rgb, &Vdp2Lines[rgb->info.startLine]);
}

static int isEnabled(int id, Vdp2* varVdp2Regs) {
  int display = 1;
  switch(id) {
    case NBG0:
      display = ((varVdp2Regs->BGON & 0x1)!=0);
      if ((varVdp2Regs->BGON & 0x20) && (varVdp2Regs->BGON & 0x10)) display = 0; //When both R0ON and R1ON are 1, the normal scroll screen can no longer be displayed vdp2 pdf, section 4.1 Screen Display Control
      break;
    case NBG1:
      display = ((varVdp2Regs->BGON & 0x2)!=0);
      if ((varVdp2Regs->BGON & 0x20) && (varVdp2Regs->BGON & 0x10)) display = 0; //When both R0ON and R1ON are 1, the normal scroll screen can no longer be displayed vdp2 pdf, section 4.1 Screen Display Control
      break;
    case NBG2:
      display = ((varVdp2Regs->BGON & 0x4)!=0);
      if ((varVdp2Regs->BGON & 0x20) && (varVdp2Regs->BGON & 0x10)) display = 0; //When both R0ON and R1ON are 1, the normal scroll screen can no longer be displayed vdp2 pdf, section 4.1 Screen Display Control
      break;
    case NBG3:
      display = ((varVdp2Regs->BGON & 0x8)!=0);
      if ((varVdp2Regs->BGON & 0x20) && (varVdp2Regs->BGON & 0x10)) display = 0; //When both R0ON and R1ON are 1, the normal scroll screen can no longer be displayed vdp2 pdf, section 4.1 Screen Display Control
      break;
    case RBG0:
      display = ((varVdp2Regs->BGON & 0x10)!=0);
      break;
    case RBG1:
      display = ((varVdp2Regs->BGON & 0x20)!=0);
      break;
    default:
      display = 1;
  }
  return display;
}

#endif


