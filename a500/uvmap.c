#include "blitter.h"
#include "coplist.h"
#include "memory.h"
#include "tga.h"
#include "print.h"
#include "file.h"
#include "interrupts.h"

#include "startup.h"

#define WIDTH 160
#define HEIGHT 100
#define DEPTH 5

static PixmapT *chunky[2];
static PixmapT *textureHi, *textureLo;
static BitmapT *screen[2];
static UWORD *uvmap;
static PaletteT *palette;
static UWORD active = 0;
static CopListT *cp;
static CopInsT *bplptr[DEPTH];

extern APTR UVMapRenderTemplate[5];
#define UVMapRenderSize \
  (WIDTH * HEIGHT / 2 * sizeof(UVMapRenderTemplate) + 2)
void (*UVMapRender)(UBYTE *chunky asm("a0"),
                    UBYTE *textureHi asm("a1"),
                    UBYTE *textureLo asm("a2"));

static void PixmapScramble(PixmapT *image, PixmapT *imageHi, PixmapT *imageLo)
{
  UBYTE *data = image->pixels;
  UBYTE *hi = imageHi->pixels;
  UBYTE *lo = imageLo->pixels;
  LONG n = image->width * image->height;

  do {
    BYTE c = *data++;
    /* [0 0 0 0 a0 a1 a2 a3] => [a2 a3 0 0 a0 a1 0 0] */
    *hi++ = (c & 0x0c) | ((c & 0x03) << 6);
    /* [0 0 0 0 a0 a1 a2 a3] => [ 0 0 a2 a3 0 0 a0 a1] */
    *lo++ = ((c & 0x0c) >> 2) | ((c & 0x03) << 4);
  } while (--n);
}

static void Load() {
  cp = NewCopList(1024);
  screen[0] = NewBitmap(WIDTH * 2, HEIGHT * 2, DEPTH, FALSE);
  screen[1] = NewBitmap(WIDTH * 2, HEIGHT * 2, DEPTH, FALSE);

  {
    PixmapT *texture = LoadTGA("data/texture-16.tga", PM_CMAP, MEMF_PUBLIC);
    LONG size = texture->width * texture->height;

    palette = texture->palette;

    textureHi = NewPixmap(texture->width, texture->height * 2,
                          PM_CMAP, MEMF_PUBLIC|MEMF_CLEAR);
    textureLo = NewPixmap(texture->width, texture->height * 2,
                          PM_CMAP, MEMF_PUBLIC|MEMF_CLEAR);
    PixmapScramble(texture, textureHi, textureLo);

    /* Extra halves for cheap texture motion. */
    memcpy(textureHi->pixels + size, textureHi->pixels, size);
    memcpy(textureLo->pixels + size, textureLo->pixels, size);

    DeletePixmap(texture);
  }

  chunky[0] = NewPixmap(WIDTH, HEIGHT, PM_GRAY4, MEMF_CHIP);
  chunky[1] = NewPixmap(WIDTH, HEIGHT, PM_GRAY4, MEMF_CHIP);

  uvmap = ReadFile("data/uvmap.bin", MEMF_PUBLIC);
  UVMapRender = AllocMemSafe(UVMapRenderSize, MEMF_PUBLIC);
}

static void UnLoad() {
  FreeAutoMem(uvmap);
  FreeMem(UVMapRender, UVMapRenderSize);
  DeletePixmap(textureHi);
  DeletePixmap(textureLo);
  DeletePixmap(chunky[0]);
  DeletePixmap(chunky[1]);
  DeleteBitmap(screen[0]);
  DeleteBitmap(screen[1]);
  DeletePalette(palette);
  DeleteCopList(cp);
}

static struct {
  WORD phase;
  WORD active;
} c2p = { 5, 0 };

static void ChunkyToPlanar() {
  BitmapT *dst = screen[c2p.active];
  PixmapT *src = chunky[c2p.active];

  switch (c2p.phase) {
    case 0:
      /* Swap 4x2, pass 1. */
      custom->bltapt = src->pixels;
      custom->bltbpt = src->pixels + 2;
      custom->bltdpt = dst->planes[0];

      custom->bltcon0 = (SRCA | SRCB | DEST) | (ABC | ABNC | ANBC | NABNC);
      custom->bltcon1 = 4 << BSHIFTSHIFT;
      custom->bltsize = 1;
      break;

    case 1:
      custom->bltsize = 1 | (976 << 6);
      break;

    case 2:
      /* Swap 4x2, pass 2. */
      // custom->bltapt = src->pixels + WIDTH * HEIGHT / 2;
      // custom->bltbpt = src->pixels + WIDTH * HEIGHT / 2 + 2;
      custom->bltdpt = dst->planes[2] + WIDTH * HEIGHT / 4;

      custom->bltcon0 = (SRCA | SRCB | DEST) | (ABC | ABNC | ANBC | NABNC) | (4 << ASHIFTSHIFT);
      custom->bltcon1 = BLITREVERSE;
      custom->bltsize = 1;
      break;

    case 3:
      custom->bltsize = 1 | (977 << 6);
      break;

    case 4:
      CopInsSet32(bplptr[0], dst->planes[0]);
      CopInsSet32(bplptr[1], dst->planes[0]);
      CopInsSet32(bplptr[2], dst->planes[2]);
      CopInsSet32(bplptr[3], dst->planes[2]);
      break;

    default:
      return;
  }

  c2p.phase++;
}

static __interrupt_handler void IntLevel3Handler() {
  if (custom->intreqr & INTF_BLIT) {
    asm volatile("" ::: "d0", "d1", "a0", "a1");
    ChunkyToPlanar();
  }

  custom->intreq = INTF_LEVEL3;
  custom->intreq = INTF_LEVEL3;
}

static void MakeCopperList(CopListT *cp) {
  WORD i;

  CopInit(cp);
  CopMakeDispWin(cp, X(0), Y(28), WIDTH * 2, HEIGHT * 2);
  CopMakePlayfield(cp, bplptr, screen[active], DEPTH);
  for (i = 0; i < 16; i++)
    CopSetRGB(cp, i, 0);
  CopLoadPal(cp, palette, 16);
  for (i = 0; i < HEIGHT * 2; i++) {
    CopWaitMask(cp, Y(i + 28), 0, 0xff, 0);
    CopMove16(cp, bplcon1, (i & 1) ? 0x0021 : 0x0010);
    CopMove16(cp, bpl1mod, (i & 1) ? -40 : 0);
    CopMove16(cp, bpl2mod, (i & 1) ? -40 : 0);
  }
  CopEnd(cp);
}

static void MakeUVMapRenderCode() {
  UWORD *code = (APTR)UVMapRender;
  UWORD *tmpl = (APTR)UVMapRenderTemplate;
  UWORD *data = uvmap;
  WORD n = WIDTH * HEIGHT / 2;

  /* UVMap is pre-scrambled. */
  while (n--) {
    *code++ = tmpl[0];
    *code++ = *data++;
    *code++ = tmpl[2];
    *code++ = *data++;
    *code++ = tmpl[4];
  }

  *code++ = 0x4e75; /* return from subroutine instruction */
}

static void Init() {
  WORD i;

  MakeUVMapRenderCode();

  custom->dmacon = DMAF_SETCLR | DMAF_BLITTER;

  for (i = 0; i < 5; i++) {
    BlitterClearSync(screen[0], i);
    BlitterClearSync(screen[1], i);
  }

  memset(screen[0]->planes[4], 0x55, WIDTH * HEIGHT * 4 / 8);
  memset(screen[1]->planes[4], 0x55, WIDTH * HEIGHT * 4 / 8);

  MakeCopperList(cp);
  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER;

  /* Initialize chunky to planar. */
  custom->bltamod = 2;
  custom->bltbmod = 2;
  custom->bltdmod = 0;
  custom->bltcdat = 0xf0f0;
  custom->bltafwm = -1;
  custom->bltalwm = -1;

  InterruptVector->IntLevel3 = IntLevel3Handler;
  custom->intena = INTF_SETCLR | INTF_BLIT;
}

static void Kill() {
  custom->dmacon = DMAF_RASTER | DMAF_BLITTER;
  custom->intena = INTF_BLIT;
}

static void Render() {
  UBYTE *txtHi = textureHi->pixels + (frameCount & 16383);
  UBYTE *txtLo = textureLo->pixels + (frameCount & 16383);

  {
    // LONG lines = ReadLineCounter();
    (*UVMapRender)(chunky[active]->pixels, txtHi, txtLo);
    // Log("uvmap: %ld\n", ReadLineCounter() - lines);
  }

  c2p.phase = 0;
  c2p.active = active;
  ChunkyToPlanar();

  active ^= 1;
}

EffectT Effect = { Load, UnLoad, Init, Kill, Render };
