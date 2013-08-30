#include "std/debug.h"
#include "std/math.h"
#include "std/memory.h"
#include "std/resource.h"

#include "audio/stream.h"
#include "uvmap/generate.h"
#include "uvmap/render.h"
#include "gfx/blit.h"
#include "gfx/colors.h"
#include "gfx/palette.h"
#include "tools/frame.h"

#include "system/audio.h"
#include "system/c2p.h"
#include "system/display.h"
#include "system/input.h"
#include "system/vblank.h"

#include "config.h"
#include "demo.h"

#include <stdio.h>

const char *ConfigPath = "spy-shs10.json";

const int WIDTH = 320;
const int HEIGHT = 256;
const int DEPTH = 8;

#define BPM 142.18f
#define BPF (BPM / (60.0f * FRAMERATE))
#define BEAT_F(x) ((float)(x) * FRAMERATE * (60.0f / BPM))

static AudioStreamT *TheAudio;
static PixBufT *TheCanvas;
static PaletteT *ThePalette;
static PixBufT *TheImage;

#ifdef GENERATEMAPS
UVMapGenerate(0,
              0.3f / (r + 0.5f * x),
              3.0f * a / M_PI);

UVMapGenerate(1,
              x * cos(2.0f * r) - y * sin(2.0f * r),
              y * cos(2.0f * r) + x * sin(2.0f * r));

UVMapGenerate(2,
              pow(r, 0.33f),
              a / M_PI + r);

UVMapGenerate(3,
              cos(a) / (3 * r),
              sin(a) / (3 * r));

UVMapGenerate(4,
              0.04f * y + 0.06f * cos(a * 3) / r,
              0.04f * x + 0.06f * sin(a * 3) / r);

UVMapGenerate(5,
              0.1f * y / (0.11f + r * 0.15f),
              0.1f * x / (0.11f + r * 0.15f));

UVMapGenerate(6,
              0.5f * a / M_PI + 0.25f * r,
              pow(r, 0.25f));

UVMapGenerate(7,
              0.5f * a / M_PI,
              sin(5.0f * r));

UVMapGenerate(8,
              3.0f * a / M_PI,
              sin(3.0f * r) + 0.5f * cos(5.0f * a));
#endif

/*
 * Set up resources.
 */
void SetupResources() {
  ResAdd("Audio", AudioStreamOpen("data/last-christmas-techno.wav"));

  ResAdd("EffectPal", NewPalette(256));

#ifdef GENERATEMAPS
  ResAdd("Map0", NewUVMap(WIDTH, HEIGHT, UV_OPTIMIZED, 256, 256));
  ResAdd("Map1", NewUVMap(WIDTH, HEIGHT, UV_OPTIMIZED, 256, 256));
  ResAdd("Map2", NewUVMap(WIDTH, HEIGHT, UV_OPTIMIZED, 256, 256));
  ResAdd("Map3", NewUVMap(WIDTH, HEIGHT, UV_OPTIMIZED, 256, 256));
  ResAdd("Map4", NewUVMap(WIDTH, HEIGHT, UV_OPTIMIZED, 256, 256));
  ResAdd("Map5", NewUVMap(WIDTH, HEIGHT, UV_OPTIMIZED, 256, 256));
  ResAdd("Map6", NewUVMap(WIDTH, HEIGHT, UV_OPTIMIZED, 256, 256));
  ResAdd("Map7", NewUVMap(WIDTH, HEIGHT, UV_OPTIMIZED, 256, 256));
  ResAdd("Map8", NewUVMap(WIDTH, HEIGHT, UV_OPTIMIZED, 256, 256));

  LOG("Generating map 0.");
  UVMapGenerate0(R_("Map0"));
  UVMapWriteToFile(R_("Map0"), "map0.bin");
  LOG("Generating map 1.");
  UVMapGenerate1(R_("Map1"));
  UVMapWriteToFile(R_("Map1"), "map1.bin");
  LOG("Generating map 2.");
  UVMapGenerate2(R_("Map2"));
  UVMapWriteToFile(R_("Map2"), "map2.bin");
  LOG("Generating map 3.");
  UVMapGenerate3(R_("Map3"));
  UVMapWriteToFile(R_("Map3"), "map3.bin");
  LOG("Generating map 4.");
  UVMapGenerate4(R_("Map4"));
  UVMapWriteToFile(R_("Map4"), "map4.bin");
  LOG("Generating map 5.");
  UVMapGenerate5(R_("Map5"));
  UVMapWriteToFile(R_("Map5"), "map5.bin");
  LOG("Generating map 6.");
  UVMapGenerate6(R_("Map6"));
  UVMapWriteToFile(R_("Map6"), "map6.bin");
  LOG("Generating map 7.");
  UVMapGenerate7(R_("Map7"));
  UVMapWriteToFile(R_("Map7"), "map7.bin");
  LOG("Generating map 8.");
  UVMapGenerate8(R_("Map8"));
  UVMapWriteToFile(R_("Map8"), "map8.bin");
#else
  ResAdd("Map0", NewUVMapFromFile("data/map0.bin"));
  ResAdd("Map1", NewUVMapFromFile("data/map1.bin"));
  ResAdd("Map2", NewUVMapFromFile("data/map2.bin"));
  ResAdd("Map3", NewUVMapFromFile("data/map3.bin"));
  ResAdd("Map4", NewUVMapFromFile("data/map4.bin"));
  ResAdd("Map5", NewUVMapFromFile("data/map5.bin"));
  ResAdd("Map6", NewUVMapFromFile("data/map6.bin"));
  ResAdd("Map7", NewUVMapFromFile("data/map7.bin"));
  ResAdd("Map8", NewUVMapFromFile("data/map8.bin"));
#endif

  /* Slider & knob */
  LinkPalettes(R_("texture-4.pal"), R_("slider.pal"), NULL);
  PixBufRemap(R_("slider.8"), R_("slider.pal"));
  PixBufRemap(R_("knob.8"), R_("slider.pal"));

  /* And finally audio stream... */
  TheAudio = R_("Audio");
  AudioStreamPlay(TheAudio);
}

/*
 * Set up demo.
 */

bool SetupDemo() {
  ResAdd("BeginImg", NewPixBufFromFile("data/begin.8"));
  ResAdd("BeginPal", NewPaletteFromFile("data/begin.pal"));

  if (InitDisplay(WIDTH, HEIGHT, DEPTH) && InitAudio()) {
    PixBufT *BeginImg = R_("BeginImg");
    PaletteT *BeginPal = R_("BeginPal");

    ResAdd("Canvas", NewPixBuf(PIXBUF_CLUT, WIDTH, HEIGHT));
    TheCanvas = R_("Canvas");

    c2p1x1_8_c5_bm(BeginImg->data, GetCurrentBitMap(), WIDTH, HEIGHT, 0, 0);
    LoadPalette(BeginPal);
    DisplaySwap();

    return true;
  }

  return false;
}

/*
 * Tear down demo.
 */
void KillDemo() {
  UnlinkPalettes(R_("11.pal"));
  UnlinkPalettes(R_("texture-1.pal"));
  UnlinkPalettes(R_("texture-2.pal"));
  UnlinkPalettes(R_("texture-3.pal"));
  UnlinkPalettes(R_("texture-4.pal"));
  UnlinkPalettes(R_("texture-5.pal"));
  AudioStreamStop(TheAudio);
  KillAudio();
}

/*
 * Handle events during the demo.
 */
void HandleEvents(int frameNumber) {
  static InputEventT event; 
#ifdef ENABLEREWIND
  static int counter = 1;
#endif

  while (EventQueuePop(&event)) {
    switch (event.ie_Class) {
      case IECLASS_RAWKEY:
        if (event.ie_Code & IECODE_UP_PREFIX) {
          switch (event.ie_Code & ~IECODE_UP_PREFIX) {
#if ENABLEREWIND
            case KEY_UP:
              ChangeVBlankCounter(-10 * FRAMERATE);
              AudioStreamUpdatePos(TheAudio);
              break;

            case KEY_DOWN:
              ChangeVBlankCounter(10 * FRAMERATE);
              AudioStreamUpdatePos(TheAudio);
              break;

            case KEY_LEFT:
              ChangeVBlankCounter(-FRAMERATE);
              AudioStreamUpdatePos(TheAudio);
              break;

            case KEY_RIGHT:
              ChangeVBlankCounter(FRAMERATE);
              AudioStreamUpdatePos(TheAudio);
              break;

            case KEY_SPACE:
              LOG("Event %d at %.2f (frame %d).",
                  counter++, (float)frameNumber / FRAMERATE, frameNumber);
              break;
#endif

            case KEY_ESCAPE:
              ExitDemo = TRUE;
              break;

            default:
              break;
          }
        }

      default:
        break;
    }
  }
}

/*** Part 1 ******************************************************************/

static PixBufT *TheTexture;
static PaletteT *TheTexturePal;
static UVMapT *TheMap;

void SetupPart1a(FrameT *frame) {
  AudioStreamSetVolume(TheAudio, 0.5f);
  TheMap = R_("Map0");
  TheTexture = R_("texture-3.8");
  TheTexturePal = R_("texture-3.pal");
  LinkPalettes(TheTexturePal, R_("spy.pal"), NULL);
  PixBufRemap(R_("spy.8"), R_("spy.pal"));
  LoadPalette(TheTexturePal);
}

void SetupPart1b(FrameT *frame) {
  TheMap = R_("Map3");
  TheTexture = R_("texture-2.8");
  TheTexturePal = R_("texture-2.pal");
  LinkPalettes(TheTexturePal, R_("shs10.pal"), NULL);
  PixBufRemap(R_("shs10.8"), R_("shs10.pal"));
  LoadPalette(TheTexturePal);
}

void SetupPart1c(FrameT *frame) {
  TheImage = NULL;
  TheMap = R_("Map5");
  TheTexture = R_("texture-4.8");
  TheTexturePal = R_("texture-4.pal");
  LinkPalettes(R_("texture-4.pal"), R_("slider.pal"), NULL);
  LoadPalette(TheTexturePal);
}

void VolumeUp(FrameT *frame) {
  int frames = frame->last - frame->first + 1;
  float volume = 0.5f + 0.5f * ((float)frame->number / frames);
  float dx = 1.75f * (frame->number * 4) / 3;
  float dy = 1.75f * frame->number;

  AudioStreamSetVolume(TheAudio, volume);

  PixBufBlit(TheCanvas, 0, 0, R_("slider.8"), NULL);
  PixBufBlit(TheCanvas, 50 + (int)dx , 110 - (int)dy, R_("knob.8"), NULL);
}

void ShowVolume(FrameT *frame) {
  PixBufBlit(TheCanvas, 0, 0, R_("slider.8"), NULL);
  PixBufBlit(TheCanvas, 50, 110, R_("knob.8"), NULL);
}

void RenderPart1(FrameT *frame) {
  int du = 2 * frame->number;
  int dv = 4 * frame->number;

  UVMapSetOffset(TheMap, du, dv);
  UVMapSetTexture(TheMap, TheTexture);
  UVMapRender(TheMap, TheCanvas);
}

void ShowTitle(FrameT *frame, PixBufT *title) {
  int frames = BEAT_F(8);
  int w = title->width;
  int h = title->height;
  int x, y;

  if (frame->number < frames) {
    w = w * frame->number / frames;
    h = h * frame->number / frames;
  }

  x = (WIDTH - w) / 2;
  y = (HEIGHT - h) / 2;

  PixBufBlitScaled(TheCanvas, x, y, w, h, title);
}

void ShowSpy(FrameT *frame) {
  ShowTitle(frame, R_("spy.8"));
}

void ShowSHS10(FrameT *frame) {
  ShowTitle(frame, R_("shs10.8"));
}

TimeSliceT Part1[] = {
  /* Part 1 */
  DO_ONCE(SetupPart1a, 0, BEAT_F(67)),
  DO_ONCE(SetupPart1b, BEAT_F(32), BEAT_F(67)),
  DO_ONCE(SetupPart1c, BEAT_F(50), BEAT_F(67)),
  EACH_FRAME(RenderPart1, 0, BEAT_F(67)),
  EACH_FRAME(ShowVolume, BEAT_F(50), BEAT_F(62)),
  EACH_FRAME(VolumeUp, BEAT_F(62), BEAT_F(67)),
  EACH_FRAME(ShowSpy, BEAT_F(0), BEAT_F(32)),
  EACH_FRAME(ShowSHS10, BEAT_F(32), BEAT_F(50)),
  THE_END
};

/*** Part 2 ******************************************************************/

#define EPISODE_F BEAT_F(16)
#define EPISODE(a) ((int)((a) * BEAT_F(16)))

static FrameT EpisodeFrame;
static int EpisodeNum = 0;

void SetupEpisode(FrameT *frame, char *imgName, char *palName, int map, int texture) {
  EpisodeNum++;

  AudioStreamSetVolume(TheAudio, 1.0f);

  {
    char name[32];

    if (map >= 0 && map <= 8) {
      snprintf(name, sizeof(name), "Map%d", map);
      TheMap = R_(name);
    }

    if (texture >= 1 && texture <= 5) {
      snprintf(name, sizeof(name), "texture-%d.8", texture);
      TheTexture = R_(name);
      snprintf(name, sizeof(name), "texture-%d.pal", texture);
      TheTexturePal = R_(name);
    }
  }

  TheImage = R_(imgName);

  if (TheImage->uniqueColors <= 128) {
    ThePalette = TheTexturePal;
    LinkPalettes(ThePalette, R_(palName), NULL);
    PixBufRemap(R_(imgName), R_(palName));
  } else if (TheImage->uniqueColors <= 192) {
    ThePalette = R_(palName);

    if (EpisodeNum == 5) {
      LinkPalettes(ThePalette, R_("greets1.pal"), NULL);
      PixBufRemap(R_("greets1.8"), R_("greets1.pal"));
    } else if (EpisodeNum == 6) {
      LinkPalettes(ThePalette, R_("greets2.pal"), NULL);
      PixBufRemap(R_("greets2.8"), R_("greets2.pal"));
    }
  } else {
    ThePalette = R_(palName);
  }

  memcpy(&EpisodeFrame, frame, sizeof(FrameT));
}

void Image01(FrameT *frame) { SetupEpisode(frame, "01.8", "01.pal", 0, 1); }
void Image02(FrameT *frame) { SetupEpisode(frame, "02.8", "02.pal", 1, 2); }
void Image03(FrameT *frame) { SetupEpisode(frame, "03.8", "03.pal", 1, 3); }
void Image04(FrameT *frame) { SetupEpisode(frame, "04.8", "04.pal", 4, 4); }
void Image05(FrameT *frame) { SetupEpisode(frame, "05.8", "05.pal", 2, 1); }
void Image06(FrameT *frame) { SetupEpisode(frame, "06.8", "06.pal", 3, 2); }
void Image07(FrameT *frame) { SetupEpisode(frame, "07.8", "07.pal", 4, 3); }
void Image08(FrameT *frame) { SetupEpisode(frame, "08.8", "08.pal", 6, 4); }
void Image09(FrameT *frame) { SetupEpisode(frame, "09.8", "09.pal", 0, 1); }
void Image10(FrameT *frame) { SetupEpisode(frame, "10.8", "10.pal", 5, 2); }
void Image11(FrameT *frame) { SetupEpisode(frame, "11.8", "11.pal", -1, -1); }
void Image12(FrameT *frame) { SetupEpisode(frame, "12.8", "12.pal", 5, 5); }
void Image13(FrameT *frame) { SetupEpisode(frame, "13.8", "13.pal", 5, 5); }
void Image15(FrameT *frame) { SetupEpisode(frame, "15.8", "15.pal", -1, -1); }
void Image16(FrameT *frame) { SetupEpisode(frame, "16.8", "16.pal", -1, -1); }
void Image17(FrameT *frame) { SetupEpisode(frame, "17.8", "17.pal", 8, 3); }
void Image18(FrameT *frame) { SetupEpisode(frame, "18.8", "18.pal", 7, 4); }
void Image19(FrameT *frame) { SetupEpisode(frame, "19.8", "19.pal", 7, 1); }
void ImageEnd1(FrameT *frame) { SetupEpisode(frame, "end1.8", "end1.pal", -1, -1); }
void ImageEnd2(FrameT *frame) { SetupEpisode(frame, "end2.8", "end2.pal", -1, -1); }
void ImageAudio(FrameT *frame) { SetupEpisode(frame, "audio.8", "audio.pal", -1, -1); }
void ImageCode(FrameT *frame) { SetupEpisode(frame, "code.8", "code.pal", -1, -1); }
void ImageGfx(FrameT *frame) { SetupEpisode(frame, "gfx.8", "gfx.pal", -1, -1); }
void ImagePics(FrameT *frame) { SetupEpisode(frame, "pics.8", "pics.pal", -1, -1); }

void Waiving(FrameT *frame) {
  if ((frame->number / (int)BEAT_F(1)) & 1) {
    SetupEpisode(frame, "14-2.8", "14-2.pal", 8, 5);
  } else {
    SetupEpisode(frame, "14-1.8", "14-1.pal", 8, 5);
  }
}

bool CountBeat(int thisFrame) {
  static int lastFrame = 0;

  float lf = lastFrame * BPF;
  float tf = thisFrame * BPF;
  float li, ti;

  lf = modff(lf, &li);
  tf = modff(tf, &ti);

  lastFrame = thisFrame;

  return li < ti;
}

void PaletteEffect(FrameT *frame, PaletteT *src, PaletteT *dst) {
  bool beatFlash = CountBeat(frame->number);
  int j = 0;

  while (src) {
    int i;

    for (i = 0; i < src->count; i++) {
      RGB col = src->colors[i];

      int r = col.r;
      int g = col.g;
      int b = col.b;

      if (beatFlash) {
        r += 64; g += 64; b += 64;
      }

      {
        int f = frame->number;

        if (f >= 25)
          f = (frame->last - frame->first) - f;

        if (f < 25) {
          r = r * f / 25;
          g = g * f / 25;
          b = b * f / 25;
        }
      }

      dst->colors[j++] = MakeRGB(r, g, b);

      if (j >= 256)
        break;
    }

    src = src->next;
  }
  
  LoadPalette(dst);
}

void RenderPart2(FrameT *frame) {
  if (TheImage->uniqueColors <= 128) {
    int du = 2 * frame->number;
    int dv = 2 * frame->number;

    UVMapSetTexture(TheMap, TheTexture);
    UVMapSetOffset(TheMap, du, dv);
    UVMapRender(TheMap, TheCanvas);
  }

  EpisodeFrame.number = frame->number - EpisodeFrame.first;

  {
    RectT rect = { 0, 0, WIDTH, HEIGHT };

    frame = &EpisodeFrame;

    if (TheImage->height > HEIGHT) {
      int frames = frame->last - frame->first + 1;
      int f = frame->number % frames;
      int dy = (f < frames / 2) ? f : (frames - f);

      rect.y = (int)((float)(TheImage->height - HEIGHT) * 2 * dy / frames);
    }

    PixBufBlit(TheCanvas, 0, 0, TheImage, &rect);

    PaletteEffect(frame, ThePalette, R_("EffectPal"));
  }

  if (EpisodeNum == 5 || EpisodeNum == 6) {
    RectT rect = { 0, 0, WIDTH, HEIGHT };
    int frames = frame->last - frame->first + 1;

    if (EpisodeNum == 5) {
      PixBufT *TheGreets = R_("greets1.8");

      rect.x = (TheGreets->width - WIDTH) * frame->number / frames;

      PixBufBlit(TheCanvas, 0, 54, TheGreets, &rect);
    } else {
      PixBufT *TheGreets = R_("greets2.8");

      rect.x = (TheGreets->width - WIDTH) * (frames - frame->number) / frames;

      PixBufBlit(TheCanvas, 0, 54, TheGreets, &rect);
    }
  }
}

TimeSliceT Part2[] = {
  DO_ONCE(Image01, EPISODE(0), EPISODE(1)),
  DO_ONCE(Image02, EPISODE(1), EPISODE(2)),
  DO_ONCE(Image03, EPISODE(2), EPISODE(3)),
  DO_ONCE(Image04, EPISODE(3), EPISODE(4)),

  /* Greets */
  DO_ONCE(Image11, EPISODE(4), EPISODE(5)),
  DO_ONCE(Image11, EPISODE(5), EPISODE(6)),

  DO_ONCE(Image05, EPISODE(6), EPISODE(7)),
  DO_ONCE(Image06, EPISODE(7), EPISODE(9)), /* long - azzaro 1 */
  DO_ONCE(Image07, EPISODE(9), EPISODE(11)), /* long - azzaro 2 */
  DO_ONCE(Image08, EPISODE(11), EPISODE(12)),
  DO_ONCE(Image10, EPISODE(12), EPISODE(14)), /* long */

  /* Credits */
  DO_ONCE(ImageAudio, EPISODE(14), EPISODE(14.5f)),
  DO_ONCE(ImageCode, EPISODE(14.5f), EPISODE(15)),
  DO_ONCE(ImageGfx, EPISODE(15), EPISODE(15.5f)),
  DO_ONCE(ImagePics, EPISODE(15.5f), EPISODE(16)),

  DO_ONCE(Image09, EPISODE(16), EPISODE(17)),
  DO_ONCE(Image12, EPISODE(17), EPISODE(18)),
  DO_ONCE(Image13, EPISODE(18), EPISODE(19)),
  DO_ONCE(Image15, EPISODE(19), EPISODE(20)),
  DO_ONCE(Image16, EPISODE(20), EPISODE(21)),
  DO_ONCE(Image18, EPISODE(21), EPISODE(22)),
  DO_ONCE(Image17, EPISODE(22), EPISODE(23)),
  DO_ONCE(Image19, EPISODE(23), EPISODE(24)),
  DO_ONCE(ImageEnd1, EPISODE(24), EPISODE(25)),
  DO_ONCE(ImageEnd2, EPISODE(25), EPISODE(26)),
  EACH_FRAME(Waiving, EPISODE(26), TIME_END),
  EACH_FRAME(RenderPart2, EPISODE(0), TIME_END),
  THE_END
};

/*** The demo ****************************************************************/

void Render(FrameT *frame) {
  c2p1x1_8_c5_bm(TheCanvas->data, GetCurrentBitMap(), WIDTH, HEIGHT, 0, 0);

#ifdef SHOWFRAMES
  RenderFrameNumber(frame->number);
  RenderFramesPerSecond(frame->number);
#endif
}

void FeedAudioStream(FrameT *frame) {
  AudioStreamT *TheAudio = R_("Audio");
  AudioStreamFeed(TheAudio);
}

void Quit(FrameT *frame) {
  ExitDemo = TRUE;
}

TimeSliceT TheDemo[] = {
  TIMESLICE(Part1, 0, BEAT_F(67)),
  TIMESLICE(Part2, BEAT_F(67), TIME_END),
  EACH_NTH_FRAME(FeedAudioStream, 0, TIME_END, 5),
  EACH_FRAME(Render, 0, TIME_END),
  DO_ONCE(Quit, 5407, 5407),
  THE_END
};
