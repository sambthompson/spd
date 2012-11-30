/* drvr_zrtc.c - Zortech Flash Graphics specific version of display routines. */

/*
 * Modified: 17 Mar 1993
 *           Eduard [esp] Schwan
 *           Passed bg_color to display_init
*/

#include <dos.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <fg.h>
#include "def.h"
#include "drv.h"

#define TEST_ABORT if (kbhit() && getch() == 27) { display_close(0); exit(1); }

static float X_Display_Scale = 1.0;
static float Y_Display_Scale = 1.0;
int maxx = 320;
int maxy = 200;

typedef unsigned char pallette_array[256][3];
pallette_array *pallette = NULL;

static fg_box_t clipbox;
static COORD4 bkgnd_color;

/* Scale from image size to actual screen pixel size */
static void
rescale_coord(x, y)
    int *x, *y;
{
    float xt, yt;

    yt = maxy / 2 + Y_Display_Scale * *y;
    xt = maxx / 2 + X_Display_Scale * *x;

    *x = (xt < 0.0 ? 0 : (xt > maxx ? maxx - 1 : (int)xt));
    *y = (yt < 0.0 ? 0 : (yt > maxy ? maxy - 1 : (int)yt));
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
#elif defined( DOS16RM )
    /* If you aren't using GNU C++ or Zortech C++ for 386 protected mode,
       then you need to be using Zortech C++ for 286 protected mode. */
    fp = MK_FP(0xA000, 0);
#else
    /* This is where you need to add a routine to set the pallette using
       whatever compiler you own. */
    fprintf(stderr, "Unsupported compiler.  Add pallette initialization\n");
    exit(1);
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

/* Make space for a pallette & make a 332 color map */
static void
pallette_init()
{
    unsigned i, r, g, b;

    if (pallette == NULL) {
	pallette = malloc(sizeof(pallette_array));
	if (pallette == NULL) {
	    fprintf(stderr, "Failed to allocate pallette array\n");
	    exit(1);
	}
    }

    i = 0;
    for (r=0;r<8;r++)
	for (g=0;g<8;g++)
	    for (b=0;b<4;b++) {
	       (*pallette)[i][0] = r << 3;
	       (*pallette)[i][1] = g << 3;
	       (*pallette)[i][2] = b << 4;
	       i++;
	    }
    setmany(*pallette, 0, 256);
}

/* Turn a RGB color defined as <0, 0, 0> -> <1, 1, 1> into a color that
   is in a 332 pallette. */
static int
determine_color_index(color)
    COORD3 color;
{
    int i;
    unsigned char r, g, b;

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

void
display_clear(void)
{
    /* Proposal: Use "bkgnd_color" to clear screen? [esp] */

    /* Clear the screen using a box the size of the screen. */
    fg_fillbox(FG_BLACK, FG_MODE_SET, ~0, fg.displaybox);
}

void
display_init(xres, yres, bk_color)
    int xres, yres;
    COORD3 bk_color;
{
    int x, y;
    COORD3 white;

    // remember the background color
    COPY_COORD3(bkgnd_color, bk_color);

    /* Ignore the resolution and make the display 640x480 in 16 colors. */
#if defined( DOS386 )
    fg_init_vesa1();
    maxx = 640;
    maxy = 480;
    pallette_init();
#else
    fg_init_vga12();
    maxx = 640;
    maxy = 480;
#endif

    /* Scale the display to fit onto the screen */
    X_Display_Scale = (xres > maxx ? (float)maxx / (float)xres : 1.0);
    Y_Display_Scale = (yres > maxy ? (float)maxy / (float)yres : 1.0);
    if (X_Display_Scale < Y_Display_Scale)
	Y_Display_Scale = X_Display_Scale;
    else if (Y_Display_Scale < X_Display_Scale)
	X_Display_Scale = Y_Display_Scale;

    /* Make the clipping box for the display */
    x = -xres/2; y = -yres/2;
    rescale_coord(&x, &y);
    clipbox[FG_X1] = x;
    clipbox[FG_Y1] = y;
    x = xres/2; y = yres/2-1;
    rescale_coord(&x, &y);
    clipbox[FG_X2] = x;
    clipbox[FG_Y2] = y;

    /* Outline the actual "visible" display area in the window */
    SET_COORD3(white, 1.0, 1.0, 1.0);
    display_line(-xres/2, -yres/2, -xres/2, yres/2-1, white);
    display_line( xres/2-1, -yres/2,  xres/2-1, yres/2-1, white);
    display_line(-xres/2,  yres/2-1,  xres/2-1,  yres/2-1, white);
    display_line(-xres/2, -yres/2,  xres/2-1, -yres/2, white);
}

void
display_close(int wait_flag)
{
    if (wait_flag) {
#if !defined( _WINDOWS )
	while (!kbhit()) ;
#endif
	if (!getch()) getch();
    }
    if (pallette != NULL)
	free(pallette);

    /* Turn off graphics mode */
    fg_term();
}

void
display_plot(x, y, color)
    int x, y;
    COORD3 color;
{
    int xp, yp;

    xp = x; yp = y;
    rescale_coord(&xp, &yp);

    /* Insert code to plot a single pixel here */
#if defined( DOS386 )
    fg_drawdot(determine_color_index(color), FG_MODE_SET, ~0, xp, yp);
#else
    fg_drawdot(FG_WHITE, FG_MODE_SET, ~0, xp, yp);
#endif
}

void
display_line(x0, y0, x1, y1, color)
    int x0, y0, x1, y1;
    COORD3 color;
{
    float xt, yt;
    int xs, ys, xe, ye;
    fg_line_t line1;

    TEST_ABORT

    xs = x0; ys = y0; xe = x1; ye = y1;
    rescale_coord(&xs, &ys);
    rescale_coord(&xe, &ye);

    /* Make a line out of the end points */
    fg_make_line(line1, xs, ys, xe, ye);

    /* Clip against the view */
#if defined( DOS386 )
    fg_drawlineclip(determine_color_index(color), FG_MODE_SET, ~0,
	    FG_LINE_SOLID, line1, clipbox);
#else
    fg_drawlineclip(FG_WHITE, FG_MODE_SET, ~0, FG_LINE_SOLID,
	    line1, clipbox);
#endif
}
