/*==============================================================================
Project:	SPD

File:	drv_mac.c

Description:
Mac-specific code for initialization & displaying.
------------------------------------------------------------------------------
Author:
	Alexander Enzmann, [70323,2461]
	Eduard Schwan, [71513,2161]
------------------------------------------------------------------------------
Change History:
	9212??	[ae]	Created.
	921214	[esp]	Updated to compile under MPW C as well as Think C.
	930117	[esp]	Added MacInit, MacMultiTask, MacShutDown, GetOptions code
	930317	[esp]	Added MoveToMaxDevice, environs check, collapsed multiple
					dialogs to 1, changed display_init to accept bg_color
	930418	[esp]	Added driver checks.
	930803	[esp]	Changed argv parameters to match new 3.1a format.
	930805	[esp]	Added radios for the -t/-c output_format switches.
	930817	[esp]	Added ColorQD check in Display_Init.
	930918	[esp]	Fixed up logic for input parm prompting.
	930918	[esp]	Added grody quick fix to redraw please-wait dlg on switchin.
	940725	[esp]	Removed special palette, use System palette to avoid Finder color snatching
==============================================================================*/

#include <stdio.h>		/* sprintf, etc */
#include <stdlib.h>		/* atoi, etc */

/*==== Macintosh-specific headers ====*/
#include <Controls.h>
#include <Desk.h>
#include <Dialogs.h>
#include <Files.h>
#include <Memory.h>			/* BlockMove */
#include <Menus.h>
#include <OSEvents.h>
#include <OSUtils.h>
#include <Packages.h>
#include <QuickDraw.h>
#include <Palettes.h>
#include <Resources.h>
#include <Types.h>
#include <Windows.h>
#include <sound.h>
#include <AppleEvents.h>
#include <GestaltEqu.h>
#include <Folders.h>
#include <errors.h>			/* dupFNErr, etc */
#include <Fonts.h>			/* checkMark */
#include <segload.h>		/* UnloadSeg */
#include <traps.h>			/* _Unimplemented */
#include <StandardFile.h>	/* SFPutFile */
#include <string.h>			/* strcpy/cat */
#include <toolutils.h>		/* BitTst, GetCursor, etc */

#ifdef applec
#include <strings.h>		/* p2cstr */
#endif

#include "def.h"
#include "drv.h"
#include "lib.h"		/* for output types */

/*------------------------------------------------------------*/
/* These #defines map MPW C names into their Think C equivalent */
#ifdef THINK_C
// <esp>
// #define	c2pstr	CtoPstr
// #define	p2cstr	PtoCstr
#endif THINK_C

#define	kOSEvent				app4Evt	/* event used by MultiFinder */
#define	kSuspendResumeMessage	1		/* high byte of suspend/resume event message */

#define kMouseCursorID 1000	// mouse cursor resource ID

/* #define	USE_CUSTOM_PALETTE		1		/* turn on to attach special palette */

/*------------------------------------------------------------*/
// constants for positioning the default popup item within its box
#define	SLOP_LEFT	13		// leave this much space on left of title
#define	SLOP_RIGHT	12		// this much on right
#define	SLOP_BOTTOM	 5		// this much below baseline

// general dialog constants
#define	DlogID_BADENV		1000
#define	DlogID_WAIT			1001
#define	DlogID_NOCOLORQD	1002
#define	DlogID_GETOPTS		2000

// # of lines or pixels to draw between multitask calls
#define	MAX_TASK_WAIT	50

typedef struct
{
	MenuHandle	fMenuHandle;	// our popUp menu
	short		fMenuID;		// our popUp menu ID
	DialogPtr	fParentDialog;	// our dialog pointer
	short		fPopupItemID;	// our popUp dialog user item ID
	Rect		fPopupBounds;	// boundsrect of popup menu
	short		fTitleItemID;	// our popUp title user item ID
	Rect		fTitleBounds;	// boundsrect of our popUp's title box
	short		fLastChoice;	// the last-chosen item from the pop-up menu
} popupMenuRec_t, *popupMenuPtr_t, **popupMenuHdl_t;

/*------------------------------------------------------------*/
short		gMacRayTracerKind = OUTPUT_RT_DEFAULT;
short		gMacParmSize = 2;
Boolean		gMacDoBuiltIn = true;	/* TRUE= OUTPUT_CURVES, FALSE = OUTPUT_PATCHES */
char		gInFileName[64];

/*------------------------------------------------------------*/
static WindowPtr	myWindow;
#if defined(USE_CUSTOM_PALETTE)
static PaletteHandle PolyPalette;
#endif
static int			maxx = 460;
static int			maxy = 300;
static int			gMultiTaskCount = 0;

static Boolean		gHasSizePrompt;
static Boolean		gHasInFilePrompt;
static Boolean		gHasPatchPrompt;

static COORD3		bkgnd_color;
static int			display_active_flag = 0;
static double		X_Display_Scale = 1.0;
static double		Y_Display_Scale = 1.0;
static SysEnvRec	gSysEnvirons;
static DialogPtr	gDialogPtr = NULL;

static char			*macArgv[] =	{NULL, NULL, NULL, NULL, NULL,
									NULL, NULL, NULL, NULL, NULL};
static char			macArgvBuffer[200];		// fake argc/argv
static char			*argvCurrent;
static popupMenuRec_t	gPopupRec;

/*------------------------------------------------------------*/
void MacMultiTask();


static void DrawDownTriangle(register short h, register short v)
{
	register short i;

	for (i = 0; i < 6; ++i)	{
		MoveTo(h + 5 - i, v - i);
		Line(2*i, 0);
	}
}


/*------------------------------------------------------------*/
// Draw the popup menu (called by Dialog Mgr for updates, and by our filterproc)
static pascal void DrawPopupMenu(DialogPtr pDialogPtr, short pItem)
{
#pragma unused (pItem, pDialogPtr)
	short		newWid, newLen, wid;
	PenState	savePen;
	Rect		aRect;
	Str255		theItemText;
	FontInfo	fontInfo;

	SetPort(pDialogPtr);

	GetPenState(&savePen);
	GetFontInfo(&fontInfo);

	// get currently-selected item text
	GetItem(gPopupRec.fMenuHandle, gPopupRec.fLastChoice, theItemText);

	// get the menu bounds
	aRect = gPopupRec.fPopupBounds;
	InsetRect(&aRect, -1, -1); // make it a little bigger

	// Insure that the item fits. Truncate it and add an ellipses (".") if it doesn't
	wid = (aRect.right-aRect.left) - (CharWidth(checkMark)+SLOP_RIGHT+4); // available string area
	newWid = StringWidth(theItemText); // get current width
	if (newWid > wid)
	{	// doesn't fit - truncate it
		newLen = theItemText[0]; // current length in characters
		wid = wid - CharWidth('É'); // subtract width of ellipses

		do	{ // until it fits (or we run out of characters)
			// drop the last character and its width
			newWid -= CharWidth(theItemText[newLen]);
			newLen--;
		} while ((newWid > wid) && (newLen > 0));

		// add the ellipses character
		newLen++; // add room for elipsis character
		theItemText[newLen] = 'É';
		theItemText[0] = newLen; // set the new true length
	}

	// draw the box
	PenSize(1, 1);
	FrameRect(&aRect);
	// and its drop shadow
	MoveTo(aRect.right, aRect.top+2);
	LineTo(aRect.right, aRect.bottom);
	LineTo(aRect.left+2, aRect.bottom);

	// draw the string
	MoveTo(aRect.left+CharWidth(checkMark)+2, aRect.top+fontInfo.ascent);
	DrawString(theItemText);

	DrawDownTriangle(aRect.right-SLOP_RIGHT-1, aRect.top+fontInfo.ascent);

	SetPenState(&savePen);
} // DrawPopupMenu


/*------------------------------------------------------------*/
// Initialize the popup menu/dialog record, returns true if successful
Boolean IPopupMenu(DialogPtr pDialogPtr, short pMenuID, short pTitleItemID, short pPopupItemID,
				short pCurrentChoice)
{
	Handle		theItem;
	short		theItemType;
	Boolean		returnValue;

	returnValue = false;
	gPopupRec.fMenuHandle	= GetMenu(pMenuID);		// our popUp menu
	if ((gPopupRec.fMenuHandle != NULL) && (pDialogPtr != NULL))
	{
		gPopupRec.fMenuID		= pMenuID;			// our popUp menu ID
		gPopupRec.fParentDialog	= pDialogPtr;		// our dialog pointer
		gPopupRec.fPopupItemID	= pPopupItemID;		// our popUp dialog user item ID
		gPopupRec.fTitleItemID	= pTitleItemID;		// our popUp title user item ID

		gPopupRec.fLastChoice	= pCurrentChoice;	// the last-chosen item from the pop-up menu
		SetItemMark(gPopupRec.fMenuHandle, pCurrentChoice, checkMark); // check this item

		GetDItem(pDialogPtr, pTitleItemID, &theItemType, &theItem, &gPopupRec.fTitleBounds); // boundsrect of our popUp's title box
		GetDItem(pDialogPtr, pPopupItemID, &theItemType, &theItem, &gPopupRec.fPopupBounds); // boundsrect of its prompt
		CalcMenuSize(gPopupRec.fMenuHandle);
		// install our Draw proc (DrawPopupMenu)
		SetDItem(pDialogPtr, pPopupItemID, theItemType, (Handle)DrawPopupMenu, &gPopupRec.fPopupBounds); // boundsrect of its prompt
		returnValue = true;
	}
	return returnValue;
} // IPopupMenu


/*------------------------------------------------------------*/
// Filterproc for popup userItem hits on mouse down (call from your dialog filter proc)
pascal Boolean PopupMouseDnDialogFilterProc(DialogPtr pDialogPtr, EventRecord *pEventPtr, short *pItemHitPtr)
{
Point	mouseLoc, popLoc;
short	newChoice, theItem;
long	chosen;
Boolean	myFilter;

	// pre-initialize return values
	*pItemHitPtr = 0;
	myFilter = false;

	if (pEventPtr->what == mouseDown)
	{
		SetPort(pDialogPtr);

		mouseLoc = pEventPtr->where; // copy the mouse position
		GlobalToLocal(&mouseLoc); // convert it to local dialog coordinates

		// Was the click in a popup item?  NOTE: FindDItem is zero-based!
		theItem = FindDItem(pDialogPtr, mouseLoc);
		if ((theItem + 1) == gPopupRec.fPopupItemID)
		{
			// We're going to pop up our menu. Insert our menu into the menu list,
			// then call CalcMenuSize (to work around a bug in the Menu Manager),
			// then call PopUpMenuSelect and let the user drag around. Note that the
			// (top,left) parameters to PopUpMenuSelect are our item's, converted to
			// global coordinates.
			InvertRect(&gPopupRec.fTitleBounds); // hilight the title
			InsertMenu(gPopupRec.fMenuHandle, -1); // insert our menu in the menu list
			popLoc = *(Point*)(&gPopupRec.fPopupBounds.top); // copy our item's topleft
			LocalToGlobal(&popLoc); // convert back to global coords
			CalcMenuSize(gPopupRec.fMenuHandle); // Work around Menu Mgr bug
			chosen = PopUpMenuSelect(gPopupRec.fMenuHandle,
							popLoc.v, popLoc.h, gPopupRec.fLastChoice);
			DeleteMenu(gPopupRec.fMenuID); // Remove our menu from the menu list
			InvertRect(&gPopupRec.fTitleBounds); // unhilight the title

			// Was something chosen?
			if (chosen)
			{
				// get the chosen item number
				newChoice = chosen & 0x0000ffff;
				if (newChoice != gPopupRec.fLastChoice)
				{
					// the user chose an item other than the current one
					SetItemMark(gPopupRec.fMenuHandle, gPopupRec.fLastChoice, noMark); // unmark the old choice
					SetItemMark(gPopupRec.fMenuHandle, newChoice, checkMark); // mark the new choice
					gPopupRec.fLastChoice = newChoice; // update the current choice

					// Draw the new title
					EraseRect(&gPopupRec.fPopupBounds);
					DrawPopupMenu(pDialogPtr, gPopupRec.fPopupItemID);

					myFilter = true; // dialog is over
					// have ModalDialog return that the user changed items
					*pItemHitPtr = gPopupRec.fPopupItemID;
				} // if new choice
			} // if chosen
		} // if our popup Item
	} // if mousedown

	return myFilter;

} // PopupMouseDnDialogFilterProc


/*------------------------------------------------------------*/
/* Returns the short version string in the application's version resource */
static void GetAppVersionPString(short versID, Str31 versionString)
{
    VersRecHndl	versHandle;		// VersRecHndl declared in MPW's <files.h>

	/* Get the resource string from app, 'vers' versID (1 or 2) resource! */
	versionString[0] = '\0';
	versHandle = (VersRecHndl)GetResource('vers',versID);
	if (versHandle)
	{
		HLock((Handle)versHandle);
		BlockMove((**versHandle).shortVersion, versionString,
					(**versHandle).shortVersion[0]+1);
		ReleaseResource((Handle)versHandle);
	}
} // GetAppVersionPString


/*------------------------------------------------------------*/
static void MoveWindowToMaxDevice(WindowPtr theWindow)
{
	Point		upperCorner;
	Rect		mainRect;
	Rect		deviceRect;
	Rect		windRect;
	Rect		maxDragBounds;
	GDHandle	theMainGDevice, theMaxGDevice;

	if (theWindow == NULL)
		return;

	// Set up bounds for all devices
	SetRect(&maxDragBounds, -32000, -32000, 32000, 32000);

	// Find main screen bounds
	theMainGDevice = GetMainDevice();
	mainRect = (**theMainGDevice).gdRect;

	// Find deepest screen bounds
	theMaxGDevice = GetMaxDevice(&maxDragBounds);
	deviceRect = (**theMaxGDevice).gdRect;

	// if Max is the same as Main, we need do nothing! Already in place!
	if (EqualRect(&mainRect, &deviceRect))
		return;

	// where's the window, relative to main screen
	windRect = theWindow->portRect;
	// yah, but where is it really?
	SetPort(theWindow);
	LocalToGlobal((Point*)&windRect.top);
	LocalToGlobal((Point*)&windRect.bottom);

	// find relative spot on new device
	upperCorner.h = windRect.left;
	upperCorner.v = windRect.top;
	// now relative to new screen
	upperCorner.h = upperCorner.h + deviceRect.left;
	upperCorner.v = upperCorner.v + deviceRect.top;

	// now move it there
	MoveWindow(theWindow, upperCorner.h, upperCorner.v, true);

} // MoveWindowToMaxDevice



/*------------------------------------------------------------*/
// This is a user item proc for drawing default dialog button outlines */
static pascal void outlineDefaultButton(DialogPtr theDialog, short theItem)
{
#pragma unused (theItem)
	PenState	SavePen;
	short		itemType;
	Handle		itemHandle;
	Rect		dispRect;

	GetPenState(&SavePen);
	/* use 'ok' (#1) item's rectangle */
	GetDItem(theDialog, ok, &itemType, &itemHandle, &dispRect);
	SetPort(theDialog);
	PenSize(3, 3);
	InsetRect(&dispRect, -4, -4);
	FrameRoundRect(&dispRect, 16, 16);
	SetPenState(&SavePen);
} // outlineDefaultButton


/*------------------------------------------------------------*/
/* Sets dialog #3 item's display proc to draw outline around item #1 */
static void SetupDefaultButton(DialogPtr theDialog)
{
    short	itemtype;
    Rect	itemrect;
    Handle	tempHandle;

	/* Set up User item (always #3) to display a border around OK button (#1) */
	GetDItem(theDialog, 3, &itemtype, &tempHandle, &itemrect);
    SetDItem(theDialog, 3, itemtype, (Handle)&outlineDefaultButton, &itemrect);
} // SetupDefaultButton


/*------------------------------------------------------------*/
// Move a dialog item (whatever type it is) to absolute position h,v
static void MoveDItem(DialogPtr theDialog, short theItemID, short h, short v)
{
    short	itemtype;
    Rect	itemrect;
    Handle	tempHandle;

	// NOTE: We should check for CtrlItem here and call MoveControl instead, just lazy!
	// get item
	GetDItem(theDialog, theItemID, &itemtype, &tempHandle, &itemrect);
	// move its view rect (to absolute pos)
	OffsetRect(&itemrect, h-itemrect.left, v-itemrect.top);
	// set new rect value back into item
    SetDItem(theDialog, theItemID, itemtype, tempHandle, &itemrect);
} // MoveDItem


/*------------------------------------------------------------*/
static DialogPtr SetupNewDialog(short theDialogID, Boolean doOutlineDefault)
{
	DialogPtr	aDialog;

	aDialog = GetNewDialog(theDialogID, NULL, (WindowPtr)-1);

	/* "default" the OK button */
	if ((aDialog != NULL) && doOutlineDefault)
		SetupDefaultButton(aDialog);

	return aDialog;

} // SetupNewDialog


/*------------------------------------------------------------*/
static void FatalErrDialog(short DlogID)
{
	short		itemHit;

	SysBeep(4);
	gDialogPtr = SetupNewDialog(DlogID, true);
	if (gDialogPtr)
	{
		ShowWindow(gDialogPtr);
		ModalDialog(NULL, &itemHit);
	}
	else
	{
		/* two beeps means something is very wrong! */
		SysBeep(4);
	}
	ExitToShell();
} // FatalErrDialog


/*------------------------------------------------------------*/
static Boolean GetInputFile(char * theInFileName)
{
	SFReply		reply;
	Point		where;
	SFTypeList	theTypes;

	where.h = 30;
	where.v = 50;

	/* prompt */
	theTypes[0] = 'TEXT';
	SFGetFile(where, "\p", (FileFilterProcPtr)NULL, 1, theTypes, (DlgHookProcPtr)NULL, &reply);
	if (reply.good)
	{
		/* return it */
		BlockMove(reply.fName, theInFileName, 1+reply.fName[0]);
		p2cstr((StringPtr)theInFileName);
		SetVol(NULL, reply.vRefNum); /* set current folder */
		return true;
	}
	else
		return false;
} // GetInputFile


/*------------------------------------------------------------*/
#ifdef FUTURE_USE
static Boolean GetOutputFile(char * theOutFileName)
{
	SFReply		reply;
	Point		where;
	char		pOutFileName[64];

	GetBestDlgPoint(&where);
	/* pre-fill output filename with old one */
	strcpy(pOutFileName, theOutFileName);
	c2pstr(pOutFileName);
	/* prompt */
	SFPutFile(where, "\pCreate output Coil file.", pOutFileName,
				(DlgHookProcPtr)NULL, &reply);
	if (reply.good)
	{
		/* return it */
		BlockMove(reply.fName, theOutFileName, 1+reply.fName[0]);
		p2cstr(theOutFileName);
		SetVol(NULL, reply.vRefNum); /* set current folder */
		return true;
	}
	else
		return false;
} // GetOutputFile

#endif FUTURE_USE


/*------------------------------------------------------------*/
static void
ToolBoxInit()
{
	int			k;
	EventRecord	anEvent;

	InitGraf(&qd.thePort);
	InitFonts();
	InitWindows();
	InitMenus();
	TEInit();
	InitDialogs(0L);
	InitCursor();

	/* create master pointer blocks for heap o' mallocs */
	for (k=0; k<10; k++)
		MoreMasters();

	// DTS Hocus Pocus to bring our app to the front
	for (k = 1; k <= 3; k++)
		EventAvail(everyEvent, &anEvent);

	// what kind of environment are we in, anyway?
	SysEnvirons(1, &gSysEnvirons);
	if	(
		(gSysEnvirons.machineType < envMachUnknown)
	||	(gSysEnvirons.systemVersion < 0x0604)
	||	(gSysEnvirons.processor < env68020)
		)
	{
	    FatalErrDialog(DlogID_BADENV);
	}

}


/*------------------------------------------------------------*/
#if defined(USE_CUSTOM_PALETTE)
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
} // determine_color_index
#endif


/*------------------------------------------------------------*/
Coord3ToRGBColor(COORD3 c3color, RGBColor	* rgbc)
{
   rgbc->red    = c3color[R_COLOR]*65535.0;
   rgbc->green  = c3color[G_COLOR]*65535.0;
   rgbc->blue   = c3color[B_COLOR]*65535.0;
} // Coord3ToRGBColor


/*------------------------------------------------------------*/
void
display_clear()
{
   Rect re;

#if defined(USE_CUSTOM_PALETTE)
   PmForeColor(determine_color_index(bkgnd_color));
#else
   RGBColor	rgbc;

   Coord3ToRGBColor(bkgnd_color, &rgbc);
   RGBForeColor(&rgbc);
#endif
   re.top = 0;
   re.left = 0;
   re.right = maxx;
   re.bottom = maxy;
   PaintRect(&re);
}


/*------------------------------------------------------------*/
void
display_init(xres, yres, bk_color)
   int xres, yres;
   COORD3 bk_color;
{
   RGBColor c;
   int i;
   int r, g, b;
   Rect re;
   COORD3 cColor;

	// die if on a machine that has no Color QD
   if (!gSysEnvirons.hasColorQD)
   {
	    FatalErrDialog(DlogID_NOCOLORQD);
   }

   // remember the background color
   COPY_COORD3(bkgnd_color, bk_color);

   // if already displaying, just clear the screen and return
   if (display_active_flag) {
      display_clear();
      return;
      }

   display_active_flag = 1;

   re.top = 24;
   re.left = 4;
   re.right = re.left + maxx;
   re.bottom = re.top + maxy;
   myWindow = NewCWindow(0L, &re, "\pObject Display",
			FALSE, plainDBox,
			(WindowPtr)-1L, FALSE, 0L);

#if defined(USE_CUSTOM_PALETTE)
   PolyPalette = NewPalette(256, 0L, pmTolerant, 0x0000);
   c.red = c.green = c.blue = 0;
   SetEntryColor(PolyPalette, 0, &c);

   i = 0;
   for (r=0;r<8;r++)
      for (g=0;g<8;g++)
	 for (b=0;b<4;b++) {
	    c.red   = r << 13;
	    c.green = g << 13;
	    c.blue  = b << 14;
	    SetEntryColor(PolyPalette, i, &c);
		i++;
	    }

   /* Make sure the last entry is true white */
   c.red   = 0xFFFF;
   c.green = 0xFFFF;
   c.blue  = 0xFFFF;
   SetEntryColor(PolyPalette, i-1, &c);
   SetEntryColor(PolyPalette, 0xFF, &c);

   SetPalette(myWindow, PolyPalette, TRUE);
#endif

   /* Now open the window. */
   MoveWindowToMaxDevice(myWindow);
   ShowWindow(myWindow);
   SetPort(myWindow);

   display_clear();

   if (xres > maxx)
      X_Display_Scale = (double)maxx / (double)xres;
   else
      X_Display_Scale = 1.0;
   if (yres > maxy)
      Y_Display_Scale = (double)maxy / (double)yres;
   else
      Y_Display_Scale = 1.0;

   if (X_Display_Scale < Y_Display_Scale)
      Y_Display_Scale = X_Display_Scale;
   else if (Y_Display_Scale < X_Display_Scale)
      X_Display_Scale = Y_Display_Scale;

   /* Outline the actual "visible" display area in the window */
   SET_COORD3(cColor, 0.5, 0.5, 0.5); // grey
   if (X_Display_Scale == 1.0) {
      display_line(-xres/2, -yres/2, -xres/2, yres/2, cColor);
      display_line( xres/2, -yres/2,  xres/2, yres/2, cColor);
      }
   if (Y_Display_Scale == 1.0) {
      display_line(-xres/2,  yres/2,  xres/2,  yres/2, cColor);
      display_line(-xres/2, -yres/2,  xres/2, -yres/2, cColor);
      }

   return;
}

/*------------------------------------------------------------*/
void
display_close(wait_flag)
   int wait_flag;
{
	EventRecord	anEvent;
	CursHandle	mouseCursorH;

	InitCursor(); // set it back to arrow
	if (gDialogPtr)
		DisposeDialog(gDialogPtr);

	mouseCursorH = GetCursor(kMouseCursorID);
	if (mouseCursorH)
		SetCursor(*mouseCursorH);

	/* hang around until the user is finished (this is grody, but works) */
	if (wait_flag)
	{
		SysBeep(4);
		while (!Button())
			EventAvail(everyEvent, &anEvent);
	}

	/* when all done, close the window on exit */
	if (myWindow)
		CloseWindow(myWindow);

	InitCursor(); // set it back to arrow
}

/*------------------------------------------------------------*/
static void
putpixel(x, y, color)
   int x, y;
   COORD3 color;
{
   Rect r;
#if defined(USE_CUSTOM_PALETTE)
   int color_index;
#else
   RGBColor	rgbc;
#endif

   r.top = y;
   r.left = x;
   r.bottom = y+1;
   r.right = x+1;
#if defined(USE_CUSTOM_PALETTE)
   color_index = determine_color_index(color);
   PmForeColor(color_index);
#else
   Coord3ToRGBColor(color, &rgbc);
   RGBForeColor(&rgbc);
#endif
   PaintRect(&r);
   return;
}

/*------------------------------------------------------------*/
void
display_plot(x, y, color)
   int x, y;
   COORD3 color;
{
   double xt, yt;

   if (gMultiTaskCount++ > MAX_TASK_WAIT)
   {
   	   gMultiTaskCount = 0;
	   MacMultiTask();
   }
   yt = maxy/2 - Y_Display_Scale * y;
   xt = maxx/2 + X_Display_Scale * x;

   if (xt < 0.0) x = 0;
   else if (xt > 320.0) x = 320;
   else x = (int)xt;
   if (yt < 0.0) y = 0;
   else if (yt > 240.0) y = 240;
   else y = (int)yt;

   putpixel(x, y, color);
}

/*------------------------------------------------------------*/
void
display_line(x0, y0, x1, y1, color)
   int x0, y0, x1, y1;
   COORD3 color;
{
   double xt, yt;
   int color_index;

   if (gMultiTaskCount++ > MAX_TASK_WAIT)
   {
   	   gMultiTaskCount = 0;
	   MacMultiTask();
   }

   /* Scale from image size to actual screen pixel size */
   yt = maxy/2 - Y_Display_Scale * y0;
   xt = maxx/2 + X_Display_Scale * x0;

   /* Clip the line to the viewport */
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

#if defined(USE_CUSTOM_PALETTE)
   color_index = determine_color_index(color);
   PmForeColor(color_index);
#else
   {
   RGBColor	rgbc;

   Coord3ToRGBColor(color, &rgbc);
   RGBForeColor(&rgbc);
   }
#endif
   MoveTo(x0, y0);
   LineTo(x1, y1);
}


/*------------------------------------------------------------*/
/* general dialog constants */
#define	Ditem_ET_SizePrompt		4
#define	Ditem_BT_GetInFile		5
#define	Ditem_ST_InFileName		6
#define	Ditem_RB_RTO_BuiltIn	7
#define	Ditem_RB_RTO_TriMesh	8
#define	Ditem_ST_OutTitle		9
#define	Ditem_PU_OutFormat		10
#define	Ditem_ST_SizeText1		11
#define	Ditem_ST_SizeText2		12
#define	Ditem_ST_RTO_Title		13
#define	Ditem_MAX				Ditem_ST_RTO_Title

// STR# resource IDs for this dialog
#define	StrID_APP_NAME			2000
#define	StrID_SIZE_PROMPT		2001
#define	StrID_INFILE_PROMPT		2002
#define	StrID_PATCH_PROMPT		2003

// popup menu ID
#define	MenuID_OutFormat		1000

/*------------------------------------------------------------*/
/* Put up dialog of options, and return TRUE if user clicks ok, else FALSE if cancel */
static Boolean GetUserOptions(int spdType)
{
	short			itemHit;
	short			k, dummy;
	DialogPtr		myDialog;
	Rect			displayRect;
	ControlHandle	theDItems[Ditem_MAX+1];
	char			aString[64];
	Str15			thePatchPrompt;
	Str31			appVers;
	Str63			theInFilePrompt;
	Str63			theAppName;
	Str255			theSizePrompt;

	// load the dialog
	myDialog = SetupNewDialog(DlogID_GETOPTS, true);
	if (!myDialog)
	{
		SysBeep(4);
		ExitToShell();
	}

	// preload all the dialog items we care about
	for (k = 1; k<=Ditem_MAX; k++)
		GetDItem(myDialog, k, &dummy, (Handle *) &theDItems[k], &displayRect);

	// Get app-specific strings from resources
	GetIndString(theAppName,		StrID_APP_NAME,		spdType);
	GetIndString(theSizePrompt,		StrID_SIZE_PROMPT,	spdType);
	GetIndString(theInFilePrompt,	StrID_INFILE_PROMPT,spdType);
	GetIndString(thePatchPrompt,	StrID_PATCH_PROMPT, spdType);

	// SPD Version
//	strcpy(aString, lib_get_version_str());
//	c2pstr(aString);
	// get Mac version from resource instead
		GetAppVersionPString(1, appVers);

	// fill in app-specific strings in dialog
	ParamText(theAppName, theSizePrompt, theInFilePrompt, appVers);

	// see which dialog items to remove.  Some of these items are specific
	// to certain apps only.  If the Size prompt resource string is empty,
	// don't display the size prompt stuff.
	gHasSizePrompt		= (theSizePrompt[0] != '\0');

	// If infile prompt is empty, don't display it.
	gHasInFilePrompt	= (theInFilePrompt[0] != '\0');

	// If infile prompt is empty, don't display it.
	gHasPatchPrompt	= (thePatchPrompt[0] != '\0');

	// Hide any items that we don't want now
	if (!gHasSizePrompt)
	{
		MoveDItem(myDialog, Ditem_ET_SizePrompt, -1000, -1000);
		MoveDItem(myDialog, Ditem_ST_SizeText1, -1000, -1000);
		MoveDItem(myDialog, Ditem_ST_SizeText2, -1000, -1000);
	}
	if (!gHasInFilePrompt)
	{
		MoveControl(theDItems[Ditem_BT_GetInFile], -1000, -1000);
		MoveDItem(myDialog, Ditem_ST_InFileName, -1000, -1000);
	}
	if (!gHasPatchPrompt)
	{
		MoveDItem(myDialog, Ditem_ST_RTO_Title, -1000, -1000);
		MoveControl(theDItems[Ditem_RB_RTO_BuiltIn], -1000, -1000);
		MoveControl(theDItems[Ditem_RB_RTO_TriMesh], -1000, -1000);
	}

	// fill initial size value into dialog
	sprintf(aString, "%d", gMacParmSize);
	c2pstr(aString);
	SetIText((Handle) theDItems[Ditem_ET_SizePrompt], (StringPtr)aString);

	// init the popup
	if (!IPopupMenu(myDialog, MenuID_OutFormat, Ditem_ST_OutTitle, Ditem_PU_OutFormat, gMacRayTracerKind+1))
	{
		SysBeep(4);
		return false;
	}
	// select something..
	SelIText(myDialog, Ditem_ET_SizePrompt, 0, -1);

	// finally show the user our dialog
	MoveWindowToMaxDevice(myDialog);
	ShowWindow(myDialog);

	// prompt until user clicks ok or cancel
	do
	{
		// Set the radio buttons
		SetCtlValue(theDItems[Ditem_RB_RTO_BuiltIn], gMacDoBuiltIn);
		SetCtlValue(theDItems[Ditem_RB_RTO_TriMesh], !gMacDoBuiltIn);

		// get user input
		ModalDialog(PopupMouseDnDialogFilterProc, &itemHit);

		// process some user interface elements
		switch (itemHit)
		{
			case Ditem_RB_RTO_BuiltIn:
				gMacDoBuiltIn = true;
				break;

			case Ditem_RB_RTO_TriMesh:
				gMacDoBuiltIn = false;
				break;

			case Ditem_BT_GetInFile:
				GetInputFile(gInFileName);
				strcpy(aString, gInFileName);
				c2pstr(aString);
				SetIText((Handle) theDItems[Ditem_ST_InFileName], (StringPtr)aString);
				break;
		}
	} while ((itemHit != ok) && (itemHit != cancel));

	if (itemHit == ok)
	{

		// Size
		if (gHasSizePrompt)
		{
			GetIText((Handle) theDItems[Ditem_ET_SizePrompt], (StringPtr)aString);
			p2cstr((StringPtr)aString);
			gMacParmSize = atoi(aString);
			if ((gMacParmSize < 1) || (gMacParmSize > 9))
				gMacParmSize = 2;
		}

		// output format from popup
		gMacRayTracerKind = gPopupRec.fLastChoice - 1;
	}

	DisposeDialog(myDialog);

	// return TRUE if they hit OK
	return (itemHit == ok);

} /* GetUserOptions */


/*------------------------------------------------------------*/
static void AddArgvOpt(int *argcp, char ***argvp, char * optionStr)
{
#pragma unused (argvp)
	strcpy(argvCurrent, optionStr);
	macArgv[*argcp] = argvCurrent;
	argvCurrent += strlen(argvCurrent)+1; /* skip over string and null */
	(*argcp)++;
}


/*------------------------------------------------------------*/
void RedrawMyDialog(DialogPtr pDialogPtr)
{
	if (pDialogPtr)
	{
		SelectWindow(pDialogPtr);
		DrawDialog(pDialogPtr);
	}
} // RedrawDialog


/*------------------------------------------------------------*/
void MacInit(int *argcp, char ***argvp, int spdType)
{
	char	strTemp[10];

	// give us another 80k of stack space, 'cause we're so recursive
	SetApplLimit(GetApplLimit() - 80000);
	MaxApplZone();

	ToolBoxInit();
	if (!GetUserOptions(spdType))
		ExitToShell();
	else
	{
		SetCursor(*GetCursor(watchCursor));
		if (gMacRayTracerKind != OUTPUT_VIDEO)
		{
			/* put up "please wait" dialog */
			gDialogPtr = SetupNewDialog(DlogID_WAIT, false);
			if (gDialogPtr)
			{
				ShowWindow(gDialogPtr);
				RedrawMyDialog(gDialogPtr);
			}
			else
			{
				SysBeep(4);
				ExitToShell();
			}
		}

		*argcp = 0;			/* Set argc to 1 parm initially */
		*argvp = (char **)&macArgv;	/* point argv to our buffer */
		argvCurrent = (char *)&macArgvBuffer;	/* start at beginning of buffer */

		/*==== Program name is always first ====*/
		AddArgvOpt(argcp, argvp, "MacSPD");

		/*==== Raytracer Format ====*/
		AddArgvOpt(argcp, argvp, "-r");
		sprintf(strTemp, "%d", gMacRayTracerKind);
		AddArgvOpt(argcp, argvp, strTemp);

		/*==== Output Format (OUTPUT_CURVES,OUTPUT_PATCHES) ====*/
		if (gHasPatchPrompt)
		{
			if (gMacDoBuiltIn)
				AddArgvOpt(argcp, argvp, "-c"); /*OUTPUT_CURVES*/
			else
				AddArgvOpt(argcp, argvp, "-t"); /*OUTPUT_PATCHES*/
		}

		/*==== Size ====*/
		if (gHasSizePrompt)
		{
			AddArgvOpt(argcp, argvp, "-s");
			sprintf(strTemp, "%d", gMacParmSize);
			AddArgvOpt(argcp, argvp, strTemp);
		}

		/*==== Input File ====*/
		if (gHasInFilePrompt)
		{
			/* Input file name */
			AddArgvOpt(argcp, argvp, "-f");
			AddArgvOpt(argcp, argvp, gInFileName);
		}
	}
} /* MacInit */


/*------------------------------------------------------------*/
void MacMultiTask(void)
{
	EventRecord	anEvent;
	WaitNextEvent(everyEvent, &anEvent, 1, NULL);

	/* grody hack to redraw dialog on suspend/resume */
	if (anEvent.what == kOSEvent)
		if (((anEvent.message >> 24) & 0x0FF) == kSuspendResumeMessage)
		{	/* high byte of message */
			/* suspend/resume is also an activate/deactivate */
			RedrawMyDialog(gDialogPtr);
		}

} /* MacMultiTask */


/*------------------------------------------------------------*/
void MacShutDown(void)
{
	/* nothing needed for now */
} /* MacShutDown */
