/*
 * Modified: 7 Sep 1993
 *           Antonio [acc] Costa
 *           Added support to GRX graphics
 *             If less than 256 colors, use only black & white.
 *             Define GRX to have GRX graphics
 *
 * Modified: 17 Mar 1993
 *           Eduard [esp] Schwan
 *           Passed bg_color to display_init
 *             (unfortunate side-effect is you should now call
 *             lib_output_background_color BEFORE lib_output_viewpoint.
 *             This may not be the best approach - please review!)
 */

#include <dos.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
/* Note that if you use Borland/Turbo C++, you need "<mem.h>" in addition to
   <stdlib.h>.  Why they put their allocation routines there I don't know. */
#include "drv.h"

#if defined( __GNUC__ )
#include <pc.h>

#ifdef GRX
#include "grx.h"
#else
#define MK_FP(seg, ofs) ((void *)(0xE0000000 + ((seg)<<4) + ofs))
#define FP_OFF(ptr) (unsigned short)(ptr)
#define FP_SEG(ptr) (unsigned short)(((unsigned long)ptr >> 16) & 0x0FFF)
#define segread(x) (void)(x)
#endif
#define getch() getkey()
#define _far
#else
#include <conio.h>
#endif

/* Scaling factors to make an image fit onto the screen */
static float X_Display_Scale = 1.0;
static float Y_Display_Scale = 1.0;

int maxx = 320;
int maxy = 200;

static COORD3 bkgnd_color;

#ifndef GRX
typedef unsigned char pallette_array[256][3];
pallette_array *pallette = NULL;

/* Use the PC's BIOS to set a video mode. */
static void
setvideomode(int mode)
{
    union REGS reg;
    reg.h.ah = 0;
    reg.h.al = mode;
    int86(0x10, &reg, &reg);
}

/* Use the PC's BIOS to set a pixel on the screen */
static void
biospixel(unsigned x, unsigned y, unsigned char val)
{
    union REGS reg;
    reg.h.ah = 0x0c;
    reg.h.al = val;
    reg.x.cx = x;
    reg.x.dx = y;
    int86(0x10, &reg, &reg);
}

/* Set a 256 entry pallette with the values given in "palbuf". */
static void
setmany(unsigned char palbuf[256][3], int start, int count)
{
    unsigned char _far *fp;
    union REGS regs;
    struct SREGS sregs;
    unsigned i;

#if defined( __GNUC__ )
    /* This should work for most compilers - only tested with GNU C++, so
       it isn't turned on for anything else */
    for (i=start;i<start+count;i++) {
	regs.x.ax = 0x1010;
	regs.x.bx = i;
	regs.h.ch = palbuf[i][1];
	regs.h.cl = palbuf[i][2];
	regs.h.dh = palbuf[i][0];
	int86x(0x10, &regs, &regs, &sregs);
	return;
    }
#elif defined( DOS386 )
    /* Put the contents of the new palette into real memory, the only
       place I know that won't impact things is the video RAM. */
    fp = MK_FP(_x386_zero_base_selector, 0xA0000);
#else
    /* If you aren't using GNU C++ or Zortech C++ for 386 protected mode,
       then you need to use a compiler that supports access to all of the
       registers, including ES, to be able to set the pallette all at once. */
    fp = MK_FP(0xA000, 0);
#endif

    /* If you got here then you are using Zortech C++ */
    for (i=start;i<start+count;i++) {
	fp[3*(i-start)  ] = palbuf[i][0];
	fp[3*(i-start)+1] = palbuf[i][1];
	fp[3*(i-start)+2] = palbuf[i][2];
    }

    regs.x.ax = 0x1012;
    regs.x.bx = start;
    regs.x.cx = count;
    regs.x.dx = 0;
    segread(&sregs);
    sregs.es  = 0xA000;
#if defined( DOS386 )
    int86x_real(0x10, &regs, &regs, &sregs);
#else
    int86x(0x10, &regs, &regs, &sregs);
#endif
    /* Now clear off the values that got dumped into the video RAM */
    for (i=0;i<3*count; i++)
	*fp++ = 0;
}
#endif

/* Make space for a pallette & make a 332 color map, if possible */
static void
pallette_init()
{
    unsigned i, r, g, b;

#ifdef GRX
    if (GrNumColors() != 256) {
 	GrSetColor(0, 0, 0, 0);
 	GrSetColor(1, 255, 255, 255);
 	return;
    }
#else
    if (pallette == NULL) {
	pallette = malloc(sizeof(pallette_array));
	if (pallette == NULL) {
	    fprintf(stderr, "Failed to allocate pallette array\n");
	    exit(1);
	}
    }
#endif

    i = 0;
    for (r=0;r<8;r++)
	for (g=0;g<8;g++)
	    for (b=0;b<4;b++) {
#ifdef GRX
		GrSetColor(i, r << 5, g << 5, b << 6);
#else
		(*pallette)[i][0] = r << 3;
		(*pallette)[i][1] = g << 3;
		(*pallette)[i][2] = b << 4;
#endif
		i++;
	    }
#ifndef GRX
    setmany(*pallette, 0, 256);
#endif
}

/* Turn a RGB color defined as <0, 0, 0> -> <1, 1, 1> into a color that
   is in a 332 pallette, if possible */
static int
determine_color_index(color)
    COORD3 color;
{
    int i;
    unsigned char r, g, b;

#ifdef GRX
    if (GrNumColors() != 256)
	return 1;
#endif

    i = 255.0 * color[Z];
    if (i<0) i=0;
    else if (i>=256) i = 255;
    b = (unsigned char)i;

    i = 255.0 * color[Y];
    if (i<0) i=0;
    else if (i>=256) i = 255;
    g = (unsigned char)i;

    i = 255.0 * color[X];
    if (i<0) i=0;
    else if (i>=256) i = 255;
    r = (unsigned char)i;

    return (r & 0xE0) | ((g & 0xE0) >> 3) | (b >> 6);
}

static void
plotpoint(int x, int y, unsigned char color)
{
#ifdef GRX
    GrPlotNC(x, y, color);
#else
    unsigned char _far *fp;

#if defined( _WINDOWS )
#elif defined( DOS386 )
    fp = MK_FP(_x386_zero_base_selector, 0xA0000 + 320 * y + x);
#else
    fp = MK_FP(0xA000, 320 * y + x);
#endif
    *fp = color;
#endif
}

/* Draw a line between two points - if you have a better one available
   from your compiler, then replace this thing - it's not fast. */
static void
line2d(int x1, int y1, int x2, int y2, int color_index)
{
#ifdef GRX
    if (x1 == x2)
	GrVLineNC(x1, y1, y2, color_index);
    else if (y1 == y2)
	GrHLineNC(x1, x2, y1, color_index);
    else
	GrLineNC(x1, y1, x2, y2, color_index);
#else
    int d1, x, y;
    int ax, ay;
    int sx, sy;
    int dx, dy;

    dx = x2 - x1;
    ax = ABSOLUTE(dx) << 1;
    sx = SGN(dx);
    dy = y2 - y1;
    ay = ABSOLUTE(dy) << 1;
    sy = SGN(dy);

    x = x1;
    y = y1;

    plotpoint(x, y, color_index);
    if (ax > ay) {
	/* x dominant */
	d1 = ay - (ax >> 1);
	for (;;) {
	    if (x==x2) return;
	    if (d1>=0) {
		y += sy;
		d1 -= ax;
	    }
	    x += sx;
	    d1 += ay;
	    plotpoint(x, y, color_index);
	}
    }
    else {
	/* y dominant */
	d1 = ax - (ay >> 1);
	for (;;) {
	    if (y == y2) return;
	    if (d1 >= 0) {
		x += sx;
		d1 -= ay;
	    }
	    y += sy;
	    d1 += ax;
	    plotpoint(x, y, color_index);
	}
    }
#endif
}

void
display_clear(void)
{
#ifdef GRX
    GrClearScreen(0);
#else
    unsigned _far *fp;
    unsigned i;

    /* Proposal: Use "bkgnd_color" to clear screen? [esp] */

#if defined( _WINDOWS )
    /* For windows you need to make a region and do a clear, currently
       there isn't anything written for that. */
#elif defined( DOS386 )
    /* An unsigned is 4 bytes, so we can clear the screen twice as fast */
    fp = MK_FP(_x386_zero_base_selector, 0xA0000);
    for (i=0;i<16000; i++)
	*fp++ = 0;
#elif defined( __GNUC__ )
    fp = MK_FP(0xA000, 0);
    for (i=0;i<16000; i++)
	*fp++ = 0;
#else
    /* An unsigned is only 2 bytes */
    fp = MK_FP(0xA000, 0);
    for (i=0;i<32000; i++)
	*fp++ = 0;
#endif
#endif
}

void
display_init(xres, yres, bk_color)
    int xres, yres;
    COORD3 bk_color;
{
    static int init_flag = 0;
    COORD3 white;

    /* remember the background color */
    COPY_COORD3(bkgnd_color, bk_color);

    if (init_flag) {
	display_clear();
	return;
    }
    else
       init_flag = 1;

#ifdef GRX
    GrSetMode(GR_default_graphics);
    maxx = GrSizeX();
    maxy = GrSizeY();
#else
    /* Turn on the 320x200 VGA mode & set the pallette */
    setvideomode(19);
    maxx = 320;
    maxy = 200;
#endif

    pallette_init();

    if (xres > maxx)
	X_Display_Scale = (float)maxx / (float)xres;
    else
	X_Display_Scale = 1.0;
    if (yres > maxy)
	Y_Display_Scale = (float)maxy / (float)yres;
    else
	Y_Display_Scale = 1.0;
    if (X_Display_Scale < Y_Display_Scale)
	Y_Display_Scale = X_Display_Scale;
    else if (Y_Display_Scale < X_Display_Scale)
	X_Display_Scale = Y_Display_Scale;

    /* Outline the actual "visible" display area in the window */
    SET_COORD3(white, 1, 1, 1);
    if (X_Display_Scale == 1.0) {
	display_line(-xres/2, -yres/2, -xres/2, yres/2, white);
	display_line( xres/2, -yres/2,  xres/2, yres/2, white);
    }
    if (Y_Display_Scale == 1.0) {
	display_line(-xres/2,  yres/2,  xres/2,  yres/2, white);
	display_line(-xres/2, -yres/2,  xres/2, -yres/2, white);
    }
}

void
display_close(int wait_flag)
{
#ifndef GRX
    union REGS regs;
#endif

    if (wait_flag) {
#if !defined( _WINDOWS )
	while (!kbhit()) ;
#endif
	if (!getch()) getch();
    }
#ifndef GRX
    if (pallette != NULL)
	free(pallette);
#endif

    /* Go back to standard text mode */
#ifdef GRX
    GrSetMode(GR_default_text);
#else
    regs.x.ax = 0x0003;
    int86(0x10, &regs, &regs);
#endif

    return;
}

void
display_plot(x, y, color)
    int x, y;
    COORD3 color;
{
    double xt, yt;

    yt = maxy/2 - Y_Display_Scale * y;
    xt = maxx/2 + X_Display_Scale * x;

    /* Make sure the point is in the display */
    if (xt < 0.0) x = 0;
    else if (xt > maxx) x = maxx;
    else x = (int)xt;
    if (yt < 0.0) y = 0;
    else if (yt > maxy) y = maxy;
    else y = (int)yt;

    plotpoint(x, y, determine_color_index(color));
}

void
display_line(x0, y0, x1, y1, color)
    int x0, y0, x1, y1;
    COORD3 color;
{
    float xt, yt;

#if !defined( _WINDOWS )
    if (kbhit())
    {
      if (!getch())
	getch();
      display_close(0);
      exit(1);
    }
#endif

    /* Scale from image size to actual screen pixel size */
    yt = maxy/2 - Y_Display_Scale * y0;
    xt = maxx/2 + X_Display_Scale * x0;

    if (xt < 0.0)
	x0 = 0;
    else if (xt > maxx) {
	x0 = maxx - 1;
    }
    else x0 = (int)xt;
    if (yt < 0.0)
	y0 = 0;
    else if (yt > maxy) {
	y0 = maxy;
    }
    else
	y0 = (int)yt;

    /* Scale from image size to actual screen pixel size */
    yt = maxy/2 - Y_Display_Scale * y1;
    xt = maxx/2 + X_Display_Scale * x1;

    if (xt < 0.0)
	x1 = 0;
    else if (xt > maxx) {
	x1 = maxx - 1;
    }
    else x1 = (int)xt;
    if (yt < 0.0)
	y1 = 0;
    else if (yt > maxy) {
	y1 = maxy;
    }
    else
	y1 = (int)yt;

    line2d(x0, y0, x1, y1, determine_color_index(color));
}
