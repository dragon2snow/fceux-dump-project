/* ---------------------------------------------------------------------------------
Implementation file of PIANO_ROLL class
Copyright (c) 2011-2012 AnS

(The MIT License)
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------------
Piano Roll - Piano Roll interface
[Singleton]

* implements the working of Piano Roll List: creating, redrawing, scrolling, mouseover, clicks, drag
* regularly updates the size of the List according to current movie input
* on demand: scrolls visible area of the List to any given item: to Playback Cursor, to Selection Cursor, to "undo pointer", to a Marker
* saves and loads current position of vertical scrolling from a project file. On error: scrolls the List to the beginning
* implements the working of Piano Roll List Header: creating, redrawing, animating, mouseover, clicks
* regularly updates lights in the Header according to button presses data from Recorder and Alt key state
* on demand: launches flashes in the Header
* implements the working of mouse wheel: List scrolling, Playback cursor movement, Selection cursor movement, scrolling across gaps in Input/markers
* implements context menu on Right-click
* stores resources: save id, ids of columns, widths of columns, tables of colors, gradient of Hot Changes, gradient of Header flashings, timings of flashes, all fonts used in TAS Editor, images
------------------------------------------------------------------------------------ */

#include "taseditor_project.h"
#include "utils/xstring.h"
#include "uxtheme.h"
#include <math.h>

#pragma comment(lib, "UxTheme.lib")

extern int joysticks_per_frame[NUM_SUPPORTED_INPUT_TYPES];
extern char buttonNames[NUM_JOYPAD_BUTTONS][2];

extern TASEDITOR_CONFIG taseditor_config;
extern TASEDITOR_WINDOW taseditor_window;
extern BOOKMARKS bookmarks;
extern PLAYBACK playback;
extern RECORDER recorder;
extern GREENZONE greenzone;
extern HISTORY history;
extern MARKERS_MANAGER markers_manager;
extern SELECTION selection;
extern EDITOR editor;

extern int GetInputType(MovieData& md);

LRESULT APIENTRY HeaderWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT APIENTRY ListWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
WNDPROC hwndList_oldWndProc = 0, hwndHeader_oldWndproc = 0;

LRESULT APIENTRY MarkerDragBoxWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

// resources
char piano_roll_save_id[PIANO_ROLL_ID_LEN] = "PIANO_ROLL";
char piano_roll_skipsave_id[PIANO_ROLL_ID_LEN] = "PIANO_ROLX";
COLORREF hot_changes_colors[16] = { 0x0, 0x5c4c44, 0x854604, 0xab2500, 0xc20006, 0xd6006f, 0xd40091, 0xba00a4, 0x9500ba, 0x7a00cc, 0x5800d4, 0x0045e2, 0x0063ea, 0x0079f4, 0x0092fa, 0x00aaff };
//COLORREF hot_changes_colors[16] = { 0x0, 0x661212, 0x842B4E, 0x652C73, 0x48247D, 0x383596, 0x2947AE, 0x1E53C1, 0x135DD2, 0x116EDA, 0x107EE3, 0x0F8EEB, 0x209FF4, 0x3DB1FD, 0x51C2FF, 0x4DCDFF };
COLORREF header_lights_colors[11] = { 0x0, 0x007313, 0x009100, 0x1daf00, 0x42c700, 0x65d900, 0x91e500, 0xb0f000, 0xdaf700, 0xf0fc7c, 0xfcffba };

char markerDragBoxClassName[] = "MarkerDragBox";

PIANO_ROLL::PIANO_ROLL()
{
	hwndMarkerDragBox = 0;
	// register MARKER_DRAG_BOX window class
	wincl.hInstance = fceu_hInstance;
	wincl.lpszClassName = markerDragBoxClassName;
	wincl.lpfnWndProc = MarkerDragBoxWndProc;
	wincl.style = CS_DBLCLKS;
	wincl.cbSize = sizeof(WNDCLASSEX);
	wincl.hIcon = 0;
	wincl.hIconSm = 0;
	wincl.hCursor = 0;
	wincl.lpszMenuName = 0;
	wincl.cbClsExtra = 0;
	wincl.cbWndExtra = 0;
	wincl.hbrBackground = 0;
	if (!RegisterClassEx(&wincl))
		FCEU_printf("Error registering MARKER_DRAG_BOX window class\n");

	// create blendfunction
	blend.BlendOp = AC_SRC_OVER;
	blend.BlendFlags = 0;
	blend.AlphaFormat = 0;
	blend.SourceConstantAlpha = 255;

}

void PIANO_ROLL::init()
{
	free();
	// create fonts for main listview
	hMainListFont = CreateFont(14, 7,				/*Height,Width*/
		0, 0,										/*escapement,orientation*/
		FW_BOLD, FALSE, FALSE, FALSE,				/*weight, italic, underline, strikeout*/
		ANSI_CHARSET, OUT_DEVICE_PRECIS, CLIP_MASK,	/*charset, precision, clipping*/
		DEFAULT_QUALITY, DEFAULT_PITCH,				/*quality, and pitch*/
		"Arial");									/*font name*/
	hMainListSelectFont = CreateFont(15, 10,		/*Height,Width*/
		0, 0,										/*escapement,orientation*/
		FW_BOLD, FALSE, FALSE, FALSE,				/*weight, italic, underline, strikeout*/
		ANSI_CHARSET, OUT_DEVICE_PRECIS, CLIP_MASK,	/*charset, precision, clipping*/
		DEFAULT_QUALITY, DEFAULT_PITCH,				/*quality, and pitch*/
		"Courier New");								/*font name*/
	// create fonts for Marker notes fields
	hMarkersFont = CreateFont(16, 8,				/*Height,Width*/
		0, 0,										/*escapement,orientation*/
		FW_BOLD, FALSE, FALSE, FALSE,				/*weight, italic, underline, strikeout*/
		ANSI_CHARSET, OUT_DEVICE_PRECIS, CLIP_MASK,	/*charset, precision, clipping*/
		DEFAULT_QUALITY, DEFAULT_PITCH,				/*quality, and pitch*/
		"Arial");									/*font name*/
	hMarkersEditFont = CreateFont(16, 7,			/*Height,Width*/
		0, 0,										/*escapement,orientation*/
		FW_NORMAL, FALSE, FALSE, FALSE,				/*weight, italic, underline, strikeout*/
		ANSI_CHARSET, OUT_DEVICE_PRECIS, CLIP_MASK,	/*charset, precision, clipping*/
		DEFAULT_QUALITY, DEFAULT_PITCH,				/*quality, and pitch*/
		"Arial");									/*font name*/
	hTaseditorAboutFont = CreateFont(24, 10,		/*Height,Width*/
		0, 0,										/*escapement,orientation*/
		FW_NORMAL, FALSE, FALSE, FALSE,				/*weight, italic, underline, strikeout*/
		ANSI_CHARSET, OUT_DEVICE_PRECIS, CLIP_MASK,	/*charset, precision, clipping*/
		DEFAULT_QUALITY, DEFAULT_PITCH,				/*quality, and pitch*/
		"Arial");								/*font name*/

	bg_brush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
	marker_drag_box_brush = CreateSolidBrush(MARKED_FRAMENUM_COLOR);
	marker_drag_box_brush_bind = CreateSolidBrush(BINDMARKED_FRAMENUM_COLOR);

	hwndList = GetDlgItem(taseditor_window.hwndTasEditor, IDC_LIST1);
	// prepare the main listview
	ListView_SetExtendedListViewStyleEx(hwndList, LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES, LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
	// subclass the header
	hwndHeader = ListView_GetHeader(hwndList);
	hwndHeader_oldWndproc = (WNDPROC)SetWindowLong(hwndHeader, GWL_WNDPROC, (LONG)HeaderWndProc);
	// subclass the whole listview
	hwndList_oldWndProc = (WNDPROC)SetWindowLong(hwndList, GWL_WNDPROC, (LONG)ListWndProc);
	// disable Visual Themes for header
	SetWindowTheme(hwndHeader, L"", L"");
	// setup images for the listview
	himglist = ImageList_Create(13, 13, ILC_COLOR8 | ILC_MASK, 1, 1);
	HBITMAP bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_0));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_1));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_2));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_3));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_4));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_5));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_6));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_7));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_8));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_9));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_10));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_11));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_12));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_13));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_14));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_15));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_16));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_17));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_18));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_19));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_0));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_1));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_2));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_3));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_4));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_5));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_6));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_7));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_8));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_9));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_10));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_11));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_12));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_13));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_14));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_15));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_16));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_17));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_18));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_PLAYBACK_19));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_0));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_1));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_2));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_3));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_4));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_5));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_6));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_7));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_8));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_9));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_10));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_11));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_12));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_13));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_14));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_15));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_16));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_17));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_18));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_PIANO_LOSTPOS_19));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_TE_ARROW));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_TE_GREEN_ARROW));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	bmp = LoadBitmap(fceu_hInstance, MAKEINTRESOURCE(IDB_TE_GREEN_BLUE_ARROW));
	ImageList_AddMasked(himglist, bmp, 0xFFFFFF);
	DeleteObject(bmp);
	ListView_SetImageList(hwndList, himglist, LVSIL_SMALL);
	// setup 0th column
	LVCOLUMN lvc;
	// icons column
	lvc.mask = LVCF_WIDTH;
	lvc.cx = COLUMN_ICONS_WIDTH;
	ListView_InsertColumn(hwndList, 0, &lvc);
	// find rows top/height (for mouseover hittest calculations)
	ListView_SetItemCountEx(hwndList, 1, LVSICF_NOSCROLL|LVSICF_NOINVALIDATEALL);
	RECT temp_rect;
	if (ListView_GetSubItemRect(hwndList, 0, 0, LVIR_BOUNDS, &temp_rect) && temp_rect.bottom != temp_rect.top)
	{
		list_row_top = temp_rect.top;
		list_row_height = temp_rect.bottom - temp_rect.top;
	} else
	{
		// couldn't get rect, set default values
		list_row_top = 20;
		list_row_height = 14;
	}
	ListView_SetItemCountEx(hwndList, 0, LVSICF_NOSCROLL|LVSICF_NOINVALIDATEALL);
	// find header height
	RECT wrect;
	if (GetWindowRect(hwndHeader, &wrect))
		list_header_height = wrect.bottom - wrect.top;
	else
		list_header_height = 20;

	hrmenu = LoadMenu(fceu_hInstance,"TASEDITORCONTEXTMENUS");
	header_colors.resize(TOTAL_COLUMNS);
	// fill TrackMouseEvent struct
	tme.cbSize = sizeof(tme);
	tme.dwFlags = TME_LEAVE;
	tme.hwndTrack = hwndHeader;
	drag_mode = DRAG_MODE_NONE;
	rbutton_drag_mode = false;
}
void PIANO_ROLL::free()
{
	if (hMainListFont)
	{
		DeleteObject(hMainListFont);
		hMainListFont = 0;
	}
	if (hMainListSelectFont)
	{
		DeleteObject(hMainListSelectFont);
		hMainListSelectFont = 0;
	}
	if (hMarkersFont)
	{
		DeleteObject(hMarkersFont);
		hMarkersFont = 0;
	}
	if (hMarkersEditFont)
	{
		DeleteObject(hMarkersEditFont);
		hMarkersEditFont = 0;
	}
	if (hTaseditorAboutFont)
	{
		DeleteObject(hTaseditorAboutFont);
		hTaseditorAboutFont = 0;
	}
	if (bg_brush)
	{
		DeleteObject(bg_brush);
		bg_brush = 0;
	}
	if (marker_drag_box_brush)
	{
		DeleteObject(marker_drag_box_brush);
		marker_drag_box_brush = 0;
	}
	if (marker_drag_box_brush_bind)
	{
		DeleteObject(marker_drag_box_brush_bind);
		marker_drag_box_brush_bind = 0;
	}
	if (himglist)
	{
		ImageList_Destroy(himglist);
		himglist = 0;
	}
	header_colors.resize(0);
}
void PIANO_ROLL::reset()
{
	must_redraw_list = must_check_item_under_mouse = true;
	row_last_clicked = 0;
	shift_held = ctrl_held = alt_held = false;
	shift_timer = ctrl_timer = shift_count = ctrl_count = 0;
	next_header_update_time = header_item_under_mouse = 0;
	// delete all columns except 0th
	while (ListView_DeleteColumn(hwndList, 1)) {}
	// setup columns
	num_columns = 1;
	LVCOLUMN lvc;
	// frame number column
	lvc.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_FMT;
	lvc.fmt = LVCFMT_CENTER;
	lvc.cx = COLUMN_FRAMENUM_WIDTH;
	lvc.pszText = "Frame#";
	ListView_InsertColumn(hwndList, num_columns++, &lvc);
	// pads columns
	lvc.cx = COLUMN_BUTTON_WIDTH;
	int num_joysticks = joysticks_per_frame[GetInputType(currMovieData)];
	for (int joy = 0; joy < num_joysticks; ++joy)
	{
		for (int btn = 0; btn < NUM_JOYPAD_BUTTONS; ++btn)
		{
			lvc.pszText = buttonNames[btn];
			ListView_InsertColumn(hwndList, num_columns++, &lvc);
		}
	}
	// add 2nd frame number column if needed
	if (num_columns >= NUM_COLUMNS_NEED_2ND_FRAMENUM)
	{
		
		lvc.cx = COLUMN_FRAMENUM_WIDTH;
		lvc.pszText = "Frame#";
		ListView_InsertColumn(hwndList, num_columns++, &lvc);
	}
}
void PIANO_ROLL::update()
{
	UpdateItemCount();

	if (must_check_item_under_mouse)
	{
		// find row and column
		POINT p;
		if (GetCursorPos(&p))
		{
			ScreenToClient(hwndList, &p);
			// perform hit test
			LVHITTESTINFO info;
			info.pt.x = p.x;
			info.pt.y = p.y;
			ListView_SubItemHitTest(hwndList, &info);
			row_under_mouse = info.iItem;
			real_row_under_mouse = row_under_mouse;
			if (real_row_under_mouse < 0)
			{
				real_row_under_mouse = ListView_GetTopIndex(hwndList) + (p.y - list_row_top) / list_row_height;
				if (real_row_under_mouse < 0) real_row_under_mouse--;
			}
			column_under_mouse = info.iSubItem;
		}
		// and don't check until mouse moves or Piano Roll scrolls
		must_check_item_under_mouse = false;
		taseditor_window.must_update_mouse_cursor = true;
	}
	
	// update state of Shift/Ctrl/Alt holding
	bool last_shift_held = shift_held, last_ctrl_held = ctrl_held, last_alt_held = alt_held;
	shift_held = (GetAsyncKeyState(VK_SHIFT) < 0);
	ctrl_held = (GetAsyncKeyState(VK_CONTROL) < 0);
	alt_held = (GetAsyncKeyState(VK_MENU) < 0);
	// check doubletap of Shift/Ctrl
	if (last_shift_held != shift_held)
	{
		if ((int)(shift_timer + GetDoubleClickTime()) > clock())
		{
			shift_count++;
			if (shift_count >= DOUBLETAP_COUNT)
			{
				if (taseditor_window.TASEditor_focus)
					FollowPlayback();
				shift_count = ctrl_count = 0;
			}
		} else
		{
			shift_count = 0;
		}
		shift_timer = clock();
	}
	if (last_ctrl_held != ctrl_held)
	{
		if ((int)(ctrl_timer + GetDoubleClickTime()) > clock())
		{
			ctrl_count++;
			if (ctrl_count >= DOUBLETAP_COUNT)
			{
				if (taseditor_window.TASEditor_focus)
					FollowSelection();
				ctrl_count = shift_count = 0;
			}
		} else
		{
			ctrl_count = 0;
		}
		ctrl_timer = clock();
	}
	// show/hide "focused" rectangle on row_last_clicked
	if (shift_held || alt_held)
	{
		if ((shift_held && !last_shift_held) || (alt_held && !last_alt_held))
			row_last_clicked_blinking_phase_shift = ROW_LAST_CLICKED_BLINKING_PERIOD + (clock() % (2 * ROW_LAST_CLICKED_BLINKING_PERIOD));
		bool show_row_last_clicked = (((clock() - row_last_clicked_blinking_phase_shift) / ROW_LAST_CLICKED_BLINKING_PERIOD) & 1) != 0;
		if (show_row_last_clicked)
		{
			ListView_SetItemState(hwndList, row_last_clicked, LVIS_FOCUSED, LVIS_FOCUSED);
		} else
		{
			ListView_SetItemState(hwndList, row_last_clicked, 0, LVIS_FOCUSED);
		}
	} else if (last_shift_held || last_alt_held)
	{
		ListView_SetItemState(hwndList, row_last_clicked, 0, LVIS_FOCUSED);
	}

	// update dragging
	if (drag_mode != DRAG_MODE_NONE)
	{
		// check if user released left button
		if (GetAsyncKeyState(GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON) >= 0)
			FinishDrag();
	}
	if (rbutton_drag_mode)
	{
		// check if user released right button
		if (GetAsyncKeyState(GetSystemMetrics(SM_SWAPBUTTON) ? VK_LBUTTON : VK_RBUTTON) >= 0)
			rbutton_drag_mode = false;
	}
	// perform drag
	switch (drag_mode)
	{
		case DRAG_MODE_PLAYBACK:
		{
			if (!playback.pause_frame || can_drag_when_seeking)
			{
				DragPlaybackCursor();
				// after first seeking is finished (if there was any seeking), it now becomes possible to drag when seeking
				can_drag_when_seeking = true;
			}
			break;
		}
		case DRAG_MODE_MARKER:
		{
			// if suddenly source frame lost its Marker, abort drag
			if (!markers_manager.GetMarker(marker_drag_framenum))
			{
				if (hwndMarkerDragBox)
				{
					DestroyWindow(hwndMarkerDragBox);
					hwndMarkerDragBox = 0;
				}
				drag_mode = DRAG_MODE_NONE;
				break;
			}
			// when dragging, always show semi-transparent yellow rectangle under mouse
			POINT p = {0, 0};
			GetCursorPos(&p);
			marker_drag_box_x = p.x - marker_drag_box_dx;
			marker_drag_box_y = p.y - marker_drag_box_dy;
			if (!hwndMarkerDragBox)
			{
				hwndMarkerDragBox = CreateWindowEx(WS_EX_LAYERED | WS_EX_TRANSPARENT, markerDragBoxClassName, markerDragBoxClassName, WS_POPUP, marker_drag_box_x, marker_drag_box_y, COLUMN_FRAMENUM_WIDTH, list_row_height, taseditor_window.hwndTasEditor, NULL, fceu_hInstance, NULL);
				ShowWindow(hwndMarkerDragBox, SW_SHOWNA);
			} else
			{
				SetWindowPos(hwndMarkerDragBox, 0, marker_drag_box_x, marker_drag_box_y, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
			}
			SetLayeredWindowAttributes(hwndMarkerDragBox, 0, MARKER_DRAG_BOX_ALPHA, LWA_ALPHA);
			UpdateLayeredWindow(hwndMarkerDragBox, 0, 0, 0, 0, 0, 0, &blend, ULW_ALPHA);
			break;
		}
		case DRAG_MODE_SET:
		case DRAG_MODE_UNSET:
		{
			POINT p;
			if (GetCursorPos(&p))
			{
				ScreenToClient(hwndList, &p);
				int drawing_current_x = p.x + GetScrollPos(hwndList, SB_HORZ);
				int drawing_current_y = p.y + GetScrollPos(hwndList, SB_VERT) * list_row_height;
				// draw (or erase) line from [drawing_current_x, drawing_current_y] to (drawing_last_x, drawing_last_y)
				int total_dx = drawing_last_x - drawing_current_x, total_dy = drawing_last_y - drawing_current_y;
				if (!shift_held)
				{
					// when user is not holding Shift, draw only vertical lines
					total_dx = 0;
					drawing_current_x = drawing_last_x;
					p.x = drawing_current_x - GetScrollPos(hwndList, SB_HORZ);
				}
				double total_len = sqrt((double)(total_dx * total_dx + total_dy * total_dy));
				LVHITTESTINFO info;
				int row_index, column_index, joy, bit;
				int min_row_index = currMovieData.getNumRecords(), max_row_index = -1;
				bool changes_made = false;
				int drawing_min_line_len = list_row_height;		// = min(list_row_width, list_row_height) in pixels
				for (double len = 0; len < total_len; len += drawing_min_line_len)
				{
					// perform hit test
					info.pt.x = p.x + (len / total_len) * total_dx;
					info.pt.y = p.y + (len / total_len) * total_dy;
					ListView_SubItemHitTest(hwndList, &info);
					row_index = info.iItem;
					if (row_index < 0)
						row_index = ListView_GetTopIndex(hwndList) + (info.pt.y - list_row_top) / list_row_height;
					// pad movie size if user tries to draw below Piano Roll limit
					if (row_index >= currMovieData.getNumRecords())
						currMovieData.insertEmpty(-1, row_index + 1 - currMovieData.getNumRecords());
					column_index = info.iSubItem;
					if (row_index >= 0 && column_index >= COLUMN_JOYPAD1_A && column_index <= COLUMN_JOYPAD4_R)
					{
						joy = (column_index - COLUMN_JOYPAD1_A) / NUM_JOYPAD_BUTTONS;
						bit = (column_index - COLUMN_JOYPAD1_A) % NUM_JOYPAD_BUTTONS;
						if (drag_mode == DRAG_MODE_SET && !currMovieData.records[row_index].checkBit(joy, bit))
						{
							currMovieData.records[row_index].setBit(joy, bit);
							changes_made = true;
							if (min_row_index > row_index) min_row_index = row_index;
							if (max_row_index < row_index) max_row_index = row_index;
						} else if (drag_mode == DRAG_MODE_UNSET && currMovieData.records[row_index].checkBit(joy, bit))
						{
							currMovieData.records[row_index].clearBit(joy, bit);
							changes_made = true;
							if (min_row_index > row_index) min_row_index = row_index;
							if (max_row_index < row_index) max_row_index = row_index;
						}
					}
				}
				if (changes_made)
				{
					if (drag_mode == DRAG_MODE_SET)
						greenzone.InvalidateAndCheck(history.RegisterChanges(MODTYPE_SET, min_row_index, max_row_index, NULL, drawing_start_time));
					else
						greenzone.InvalidateAndCheck(history.RegisterChanges(MODTYPE_UNSET, min_row_index, max_row_index, NULL, drawing_start_time));
				}
				drawing_last_x = drawing_current_x;
				drawing_last_y = drawing_current_y;
			}
			break;
		}
		case DRAG_MODE_SELECTION:
		{
			int new_drag_selection_ending_frame = real_row_under_mouse;
			// if trying to select above Piano Roll, select from frame 0
			if (new_drag_selection_ending_frame < 0)
				new_drag_selection_ending_frame = 0;
			else if (new_drag_selection_ending_frame >= currMovieData.getNumRecords())
				new_drag_selection_ending_frame = currMovieData.getNumRecords() - 1;
			if (new_drag_selection_ending_frame >= 0 && new_drag_selection_ending_frame != drag_selection_ending_frame)
			{
				// change selection shape
				if (new_drag_selection_ending_frame >= drag_selection_starting_frame)
				{
					// selecting from upper to lower
					if (drag_selection_ending_frame < drag_selection_starting_frame)
					{
						selection.ClearRegionSelection(drag_selection_ending_frame, drag_selection_starting_frame);
						selection.SetRegionSelection(drag_selection_starting_frame, new_drag_selection_ending_frame + 1);
					} else	// both ending_frame and new_ending_frame are >= starting_frame
					{
						if (drag_selection_ending_frame > new_drag_selection_ending_frame)
							selection.ClearRegionSelection(new_drag_selection_ending_frame + 1, drag_selection_ending_frame + 1);
						else
							selection.SetRegionSelection(drag_selection_ending_frame + 1, new_drag_selection_ending_frame + 1);
					}
				} else
				{
					// selecting from lower to upper
					if (drag_selection_ending_frame > drag_selection_starting_frame)
					{
						selection.ClearRegionSelection(drag_selection_starting_frame + 1, drag_selection_ending_frame + 1);
						selection.SetRegionSelection(new_drag_selection_ending_frame, drag_selection_starting_frame);
					} else	// both ending_frame and new_ending_frame are <= starting_frame
					{
						if (drag_selection_ending_frame < new_drag_selection_ending_frame)
							selection.ClearRegionSelection(drag_selection_ending_frame, new_drag_selection_ending_frame);
						else
							selection.SetRegionSelection(new_drag_selection_ending_frame, drag_selection_ending_frame);
					}
				}
				drag_selection_ending_frame = new_drag_selection_ending_frame;
			}
			break;
		}
	}
	// update MarkerDragBox when it's flying away
	if (hwndMarkerDragBox && drag_mode != DRAG_MODE_MARKER)
	{
		marker_drag_countdown--;
		if (marker_drag_countdown > 0)
		{
			marker_drag_box_dy += MARKER_DRAG_GRAVITY;
			marker_drag_box_x += marker_drag_box_dx;
			marker_drag_box_y += marker_drag_box_dy;
			SetWindowPos(hwndMarkerDragBox, 0, marker_drag_box_x, marker_drag_box_y, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
			SetLayeredWindowAttributes(hwndMarkerDragBox, 0, marker_drag_countdown * MARKER_DRAG_ALPHA_PER_TICK, LWA_ALPHA);
			UpdateLayeredWindow(hwndMarkerDragBox, 0, 0, 0, 0, 0, 0, &blend, ULW_ALPHA);
		} else
		{
			DestroyWindow(hwndMarkerDragBox);
			hwndMarkerDragBox = 0;
		}
	}
	// scroll Piano Roll if user is dragging cursor outside
	if (drag_mode != DRAG_MODE_NONE || rbutton_drag_mode)
	{
		POINT p;
		if (GetCursorPos(&p))
		{
			ScreenToClient(hwndList, &p);
			RECT wrect;
			GetClientRect(hwndList, &wrect);
			int scroll_dx = 0, scroll_dy = 0;
			if (drag_mode != DRAG_MODE_MARKER)		// in DRAG_MODE_MARKER user can't scroll Piano Roll horizontally
			{
				if (p.x < DRAG_SCROLLING_BORDER_SIZE)
					scroll_dx = p.x - DRAG_SCROLLING_BORDER_SIZE;
				else if (p.x > (wrect.right - wrect.left - DRAG_SCROLLING_BORDER_SIZE))
					scroll_dx = p.x - (wrect.right - wrect.left - DRAG_SCROLLING_BORDER_SIZE);
			}
			if (p.y < (list_header_height + DRAG_SCROLLING_BORDER_SIZE))
				scroll_dy = p.y - (list_header_height + DRAG_SCROLLING_BORDER_SIZE);
			else if (p.y > (wrect.bottom - wrect.top - DRAG_SCROLLING_BORDER_SIZE))
				scroll_dy = p.y - (wrect.bottom - wrect.top - DRAG_SCROLLING_BORDER_SIZE);
			if (scroll_dx || scroll_dy)
				ListView_Scroll(hwndList, scroll_dx, scroll_dy);
		}
	}
	// redraw list if needed
	if (must_redraw_list)
	{
		InvalidateRect(hwndList, 0, false);
		must_redraw_list = false;
	}

	// once per 40 milliseconds update colors alpha in the Header
	if (clock() > next_header_update_time)
	{
		next_header_update_time = clock() + HEADER_LIGHT_UPDATE_TICK;
		bool changes_made = false;
		int light_value = 0;
		// 1 - update Frame# columns' heads
		//if (GetAsyncKeyState(VK_MENU) & 0x8000) light_value = HEADER_LIGHT_HOLD; else
		if (drag_mode == DRAG_MODE_NONE && (header_item_under_mouse == COLUMN_FRAMENUM || header_item_under_mouse == COLUMN_FRAMENUM2))
			light_value = (selection.GetCurrentSelectionSize() > 0) ? HEADER_LIGHT_MOUSEOVER_SEL : HEADER_LIGHT_MOUSEOVER;
		if (header_colors[COLUMN_FRAMENUM] < light_value)
		{
			header_colors[COLUMN_FRAMENUM]++;
			changes_made = true;
		} else if (header_colors[COLUMN_FRAMENUM] > light_value)
		{
			header_colors[COLUMN_FRAMENUM]--;
			changes_made = true;
		}
		header_colors[COLUMN_FRAMENUM2] = header_colors[COLUMN_FRAMENUM];
		// 2 - update input columns' heads
		int i = num_columns-1;
		if (i == COLUMN_FRAMENUM2) i--;
		for (; i >= COLUMN_JOYPAD1_A; i--)
		{
			light_value = 0;
			if (recorder.current_joy[(i - COLUMN_JOYPAD1_A) / NUM_JOYPAD_BUTTONS] & (1 << ((i - COLUMN_JOYPAD1_A) % NUM_JOYPAD_BUTTONS)))
				light_value = HEADER_LIGHT_HOLD;
			else if (drag_mode == DRAG_MODE_NONE && header_item_under_mouse == i)
				light_value = (selection.GetCurrentSelectionSize() > 0) ? HEADER_LIGHT_MOUSEOVER_SEL : HEADER_LIGHT_MOUSEOVER;
			if (header_colors[i] < light_value)
			{
				header_colors[i]++;
				changes_made = true;
			} else if (header_colors[i] > light_value)
			{
				header_colors[i]--;
				changes_made = true;
			}
		}
		// 3 - redraw
		if (changes_made)
			RedrawHeader();
	}

}

void PIANO_ROLL::save(EMUFILE *os, bool really_save)
{
	if (really_save)
	{
		update();
		// write "PIANO_ROLL" string
		os->fwrite(piano_roll_save_id, PIANO_ROLL_ID_LEN);
		// write current top item
		int top_item = ListView_GetTopIndex(hwndList);
		write32le(top_item, os);
	} else
	{
		// write "PIANO_ROLX" string
		os->fwrite(piano_roll_skipsave_id, PIANO_ROLL_ID_LEN);
	}
}
// returns true if couldn't load
bool PIANO_ROLL::load(EMUFILE *is, bool really_load)
{
	reset();
	update();
	if (!really_load)
	{
		// scroll to the beginning
		ListView_EnsureVisible(hwndList, 0, FALSE);
		return false;
	}
	// read "PIANO_ROLL" string
	char save_id[PIANO_ROLL_ID_LEN];
	if ((int)is->fread(save_id, PIANO_ROLL_ID_LEN) < PIANO_ROLL_ID_LEN) goto error;
	if (!strcmp(piano_roll_skipsave_id, save_id))
	{
		// string says to skip loading Piano Roll
		FCEU_printf("No Piano Roll data in the file\n");
		// scroll to the beginning
		ListView_EnsureVisible(hwndList, 0, FALSE);
		return false;
	}
	if (strcmp(piano_roll_save_id, save_id)) goto error;		// string is not valid
	// read current top item and scroll Piano Roll there
	int top_item;
	if (!read32le(&top_item, is)) goto error;
	ListView_EnsureVisible(hwndList, currMovieData.getNumRecords() - 1, FALSE);
	ListView_EnsureVisible(hwndList, top_item, FALSE);
	return false;
error:
	FCEU_printf("Error loading Piano Roll data\n");
	// scroll to the beginning
	ListView_EnsureVisible(hwndList, 0, FALSE);
	return true;
}
// ----------------------------------------------------------------------
void PIANO_ROLL::RedrawList()
{
	must_redraw_list = true;
	must_check_item_under_mouse = true;
}
void PIANO_ROLL::RedrawRow(int index)
{
	ListView_RedrawItems(hwndList, index, index);
}
void PIANO_ROLL::RedrawHeader()
{
	InvalidateRect(hwndHeader, 0, FALSE);
}

// -------------------------------------------------------------------------
void PIANO_ROLL::UpdateItemCount()
{
	// update the number of items in the list
	int currLVItemCount = ListView_GetItemCount(hwndList);
	int movie_size = currMovieData.getNumRecords();
	if (currLVItemCount != movie_size)
		ListView_SetItemCountEx(hwndList, movie_size, LVSICF_NOSCROLL|LVSICF_NOINVALIDATEALL);
}
bool PIANO_ROLL::CheckItemVisible(int frame)
{
	int top = ListView_GetTopIndex(hwndList);
	// in fourscore there's horizontal scrollbar which takes one row for itself
	if (frame >= top && frame < top + ListView_GetCountPerPage(hwndList))
		return true;
	return false;
}

void PIANO_ROLL::CenterListAt(int frame)
{
	int list_items = ListView_GetCountPerPage(hwndList);
	int lower_border = (list_items - 1) / 2;
	int upper_border = (list_items - 1) - lower_border;
	int index = frame + lower_border;
	if (index >= currMovieData.getNumRecords())
		index = currMovieData.getNumRecords()-1;
	ListView_EnsureVisible(hwndList, index, false);
	index = frame - upper_border;
	if (index < 0)
		index = 0;
	ListView_EnsureVisible(hwndList, index, false);
}

void PIANO_ROLL::FollowPlayback()
{
	CenterListAt(currFrameCounter);
}
void PIANO_ROLL::FollowPlaybackIfNeeded()
{
	if (taseditor_config.follow_playback) ListView_EnsureVisible(hwndList,currFrameCounter,FALSE);
}
void PIANO_ROLL::FollowUndo()
{
	int jump_frame = history.GetUndoHint();
	if (taseditor_config.jump_to_undo && jump_frame >= 0)
	{
		if (!CheckItemVisible(jump_frame))
			CenterListAt(jump_frame);
	}
}
void PIANO_ROLL::FollowSelection()
{
	SelectionFrames* current_selection = selection.MakeStrobe();
	if (current_selection->size() == 0) return;

	int list_items = ListView_GetCountPerPage(hwndList);
	int selection_start = *current_selection->begin();
	int selection_end = *current_selection->rbegin();
	int selection_items = 1 + selection_end - selection_start;
	
	if (selection_items <= list_items)
	{
		// selected region can fit in screen
		int lower_border = (list_items - selection_items) / 2;
		int upper_border = (list_items - selection_items) - lower_border;
		int index = selection_end + lower_border;
		if (index >= currMovieData.getNumRecords())
			index = currMovieData.getNumRecords()-1;
		ListView_EnsureVisible(hwndList, index, false);
		index = selection_start - upper_border;
		if (index < 0)
			index = 0;
		ListView_EnsureVisible(hwndList, index, false);
	} else
	{
		// selected region is too big to fit in screen
		// oh well, just center at selection_start
		CenterListAt(selection_start);
	}
}
void PIANO_ROLL::FollowPauseframe()
{
	if (playback.pause_frame > 0)
		CenterListAt(playback.pause_frame - 1);
}
void PIANO_ROLL::FollowMarker(int marker_id)
{
	if (marker_id > 0)
	{
		int frame = markers_manager.GetMarkerFrame(marker_id);
		if (frame >= 0)
			CenterListAt(frame);
	} else
	{
		ListView_EnsureVisible(hwndList, 0, false);
	}
}
void PIANO_ROLL::EnsureVisible(int row_index)
{
	ListView_EnsureVisible(hwndList, row_index, false);
}

void PIANO_ROLL::ColumnSet(int column, bool alt_pressed)
{
	if (column == COLUMN_FRAMENUM || column == COLUMN_FRAMENUM2)
	{
		// user clicked on "Frame#" - apply ColumnSet to Markers
		if (alt_pressed)
		{
			if (editor.FrameColumnSetPattern())
				SetHeaderColumnLight(COLUMN_FRAMENUM, HEADER_LIGHT_MAX);
		} else
		{
			if (editor.FrameColumnSet())
				SetHeaderColumnLight(COLUMN_FRAMENUM, HEADER_LIGHT_MAX);
		}
	} else
	{
		// user clicked on Input column - apply ColumnSet to Input
		int joy = (column - COLUMN_JOYPAD1_A) / NUM_JOYPAD_BUTTONS;
		int button = (column - COLUMN_JOYPAD1_A) % NUM_JOYPAD_BUTTONS;
		if (alt_pressed)
		{
			if (editor.InputColumnSetPattern(joy, button))
				SetHeaderColumnLight(column, HEADER_LIGHT_MAX);
		} else
		{
			if (editor.InputColumnSet(joy, button))
				SetHeaderColumnLight(column, HEADER_LIGHT_MAX);
		}
	}
}

void PIANO_ROLL::SetHeaderColumnLight(int column, int level)
{
	if (column < COLUMN_FRAMENUM || column >= num_columns || level < 0 || level > HEADER_LIGHT_MAX)
		return;

	if (header_colors[column] != level)
	{
		header_colors[column] = level;
		RedrawHeader();
		next_header_update_time = clock() + HEADER_LIGHT_UPDATE_TICK;
	}
}

void PIANO_ROLL::StartDraggingPlaybackCursor()
{
	if (drag_mode == DRAG_MODE_NONE)
	{
		drag_mode = DRAG_MODE_PLAYBACK;
		can_drag_when_seeking = false;
		// call it once
		DragPlaybackCursor();
	}
}
void PIANO_ROLL::StartDraggingMarker(int mouse_x, int mouse_y, int row_index, int column_index)
{
	if (drag_mode == DRAG_MODE_NONE)
	{
		// start dragging the Marker
		drag_mode = DRAG_MODE_MARKER;
		marker_drag_framenum = row_index;
		marker_drag_countdown = MARKER_DRAG_COUNTDOWN_MAX;
		RECT temp_rect;
		if (ListView_GetSubItemRect(hwndList, row_index, column_index, LVIR_BOUNDS, &temp_rect))
		{
			marker_drag_box_dx = mouse_x - temp_rect.left;
			marker_drag_box_dy = mouse_y - temp_rect.top;
		} else
		{
			marker_drag_box_dx = 0;
			marker_drag_box_dy = 0;
		}
		// redraw the row to show that Marker was lifted
		RedrawRow(row_index);
	}
}
void PIANO_ROLL::StartSelectingDrag(int start_frame)
{
	if (drag_mode == DRAG_MODE_NONE)
	{
		drag_mode = DRAG_MODE_SELECTION;
		drag_selection_starting_frame = start_frame;
		drag_selection_ending_frame = drag_selection_starting_frame;	// assuming that start_frame is already selected
	}
}

void PIANO_ROLL::DragPlaybackCursor()
{
	int target_frame = real_row_under_mouse;
	if (target_frame < 0)
		target_frame = 0;
	if (currFrameCounter != target_frame)
	{
		int lastCursor = currFrameCounter;
		playback.jump(target_frame);
		if (lastCursor != currFrameCounter)
		{
			// redraw row where Playback cursor was (in case there's two or more drags before playback.update())
			RedrawRow(lastCursor);
			bookmarks.RedrawChangedBookmarks(lastCursor);
		}
	}
}

void PIANO_ROLL::FinishDrag()
{
	switch (drag_mode)
	{
		case DRAG_MODE_MARKER:
		{
			// place Marker here
			if (markers_manager.GetMarker(marker_drag_framenum))
			{
				POINT p = {0, 0};
				GetCursorPos(&p);
				int mouse_x = p.x, mouse_y = p.y;
				ScreenToClient(hwndList, &p);
				RECT wrect;
				GetClientRect(hwndList, &wrect);
				if (p.x < 0 || p.x > (wrect.right - wrect.left) || p.y < 0 || p.y > (wrect.bottom - wrect.top))
				{
					// user threw the Marker away
					markers_manager.ClearMarker(marker_drag_framenum);
					RedrawRow(marker_drag_framenum);
					history.RegisterMarkersChange(MODTYPE_MARKER_REMOVE, marker_drag_framenum);
					selection.must_find_current_marker = playback.must_find_current_marker = true;
					// calculate vector of movement
					POINT p = {0, 0};
					GetCursorPos(&p);
					marker_drag_box_dx = (mouse_x - marker_drag_box_dx) - marker_drag_box_x;
					marker_drag_box_dy = (mouse_y - marker_drag_box_dy) - marker_drag_box_y;
					if (marker_drag_box_dx || marker_drag_box_dy)
					{
						// limit max speed
						double marker_drag_box_speed = sqrt((double)(marker_drag_box_dx * marker_drag_box_dx + marker_drag_box_dy * marker_drag_box_dy));
						if (marker_drag_box_speed > MARKER_DRAG_MAX_SPEED)
						{
							marker_drag_box_dx *= MARKER_DRAG_MAX_SPEED / marker_drag_box_speed;
							marker_drag_box_dy *= MARKER_DRAG_MAX_SPEED / marker_drag_box_speed;
						}
					}
					marker_drag_countdown = MARKER_DRAG_COUNTDOWN_MAX;
				} else
				{
					if (row_under_mouse >= 0 && row_under_mouse != marker_drag_framenum && (column_under_mouse <= COLUMN_FRAMENUM || column_under_mouse >= COLUMN_FRAMENUM2))
					{
						if (markers_manager.GetMarker(row_under_mouse))
						{
							int dragged_marker_id = markers_manager.GetMarker(marker_drag_framenum);
							int destination_marker_id = markers_manager.GetMarker(row_under_mouse);
							// swap Notes of these Markers
							char dragged_marker_note[MAX_NOTE_LEN];
							strcpy(dragged_marker_note, markers_manager.GetNote(dragged_marker_id).c_str());
							if (strcmp(markers_manager.GetNote(destination_marker_id).c_str(), dragged_marker_note))
							{
								// notes are different, swap them
								markers_manager.SetNote(dragged_marker_id, markers_manager.GetNote(destination_marker_id).c_str());
								markers_manager.SetNote(destination_marker_id, dragged_marker_note);
								history.RegisterMarkersChange(MODTYPE_MARKER_SWAP, marker_drag_framenum, row_under_mouse);
								selection.must_find_current_marker = playback.must_find_current_marker = true;
								SetHeaderColumnLight(COLUMN_FRAMENUM, HEADER_LIGHT_MAX);
							}
						} else
						{
							// move Marker
							int new_marker_id = markers_manager.SetMarker(row_under_mouse);
							if (new_marker_id)
							{
								markers_manager.SetNote(new_marker_id, markers_manager.GetNote(markers_manager.GetMarker(marker_drag_framenum)).c_str());
								// and delete it from old frame
								markers_manager.ClearMarker(marker_drag_framenum);
								history.RegisterMarkersChange(MODTYPE_MARKER_DRAG, marker_drag_framenum, row_under_mouse, markers_manager.GetNote(markers_manager.GetMarker(row_under_mouse)).c_str());
								selection.must_find_current_marker = playback.must_find_current_marker = true;
								SetHeaderColumnLight(COLUMN_FRAMENUM, HEADER_LIGHT_MAX);
								RedrawRow(row_under_mouse);
							}
						}
					}
					RedrawRow(marker_drag_framenum);
					if (hwndMarkerDragBox)
					{
						DestroyWindow(hwndMarkerDragBox);
						hwndMarkerDragBox = 0;
					}
				}
			} else
			{
				// abort drag
				if (hwndMarkerDragBox)
				{
					DestroyWindow(hwndMarkerDragBox);
					hwndMarkerDragBox = 0;
				}
			}
			break;
		}
	}
	drag_mode = DRAG_MODE_NONE;
	must_check_item_under_mouse = true;
}

void PIANO_ROLL::GetDispInfo(NMLVDISPINFO* nmlvDispInfo)
{
	LVITEM& item = nmlvDispInfo->item;
	if (item.mask & LVIF_TEXT)
	{
		switch(item.iSubItem)
		{
			case COLUMN_ICONS:
			{
				item.iImage = bookmarks.FindBookmarkAtFrame(item.iItem);
				if (item.iImage < 0)
				{
					// no bookmark at this frame
					if (item.iItem == playback.GetLostPosition())
					{
						if (item.iItem == currFrameCounter)
							item.iImage = GREEN_BLUE_ARROW_IMAGE_ID;
						else
							item.iImage = GREEN_ARROW_IMAGE_ID;
					} else if (item.iItem == currFrameCounter)
					{
						item.iImage = BLUE_ARROW_IMAGE_ID;
					}
				} else
				{
					// bookmark at this frame
					if (item.iItem == playback.GetLostPosition())
						item.iImage += BOOKMARKS_WITH_GREEN_ARROW;
					else if (item.iItem == currFrameCounter)
						item.iImage += BOOKMARKS_WITH_BLUE_ARROW;
				}
				break;
			}
			case COLUMN_FRAMENUM:
			case COLUMN_FRAMENUM2:
			{
				U32ToDecStr(item.pszText, item.iItem, DIGITS_IN_FRAMENUM);
				break;
			}
			case COLUMN_JOYPAD1_A: case COLUMN_JOYPAD1_B: case COLUMN_JOYPAD1_S: case COLUMN_JOYPAD1_T:
			case COLUMN_JOYPAD1_U: case COLUMN_JOYPAD1_D: case COLUMN_JOYPAD1_L: case COLUMN_JOYPAD1_R:
			case COLUMN_JOYPAD2_A: case COLUMN_JOYPAD2_B: case COLUMN_JOYPAD2_S: case COLUMN_JOYPAD2_T:
			case COLUMN_JOYPAD2_U: case COLUMN_JOYPAD2_D: case COLUMN_JOYPAD2_L: case COLUMN_JOYPAD2_R:
			case COLUMN_JOYPAD3_A: case COLUMN_JOYPAD3_B: case COLUMN_JOYPAD3_S: case COLUMN_JOYPAD3_T:
			case COLUMN_JOYPAD3_U: case COLUMN_JOYPAD3_D: case COLUMN_JOYPAD3_L: case COLUMN_JOYPAD3_R:
			case COLUMN_JOYPAD4_A: case COLUMN_JOYPAD4_B: case COLUMN_JOYPAD4_S: case COLUMN_JOYPAD4_T:
			case COLUMN_JOYPAD4_U: case COLUMN_JOYPAD4_D: case COLUMN_JOYPAD4_L: case COLUMN_JOYPAD4_R:
			{
				int joy = (item.iSubItem - COLUMN_JOYPAD1_A) / NUM_JOYPAD_BUTTONS;
				int bit = (item.iSubItem - COLUMN_JOYPAD1_A) % NUM_JOYPAD_BUTTONS;
				uint8 data = ((int)currMovieData.records.size() > item.iItem) ? currMovieData.records[item.iItem].joysticks[joy] : 0;
				if (data & (1<<bit))
				{
					item.pszText[0] = buttonNames[bit][0];
					item.pszText[2] = 0;
				} else
				{
					if (taseditor_config.enable_hot_changes && history.GetCurrentSnapshot().GetHotChangeInfo(item.iItem, item.iSubItem - COLUMN_JOYPAD1_A))
					{
						item.pszText[0] = 45;	// "-"
						item.pszText[1] = 0;
					} else item.pszText[0] = 0;
				}
			}
			break;
		}
	}
}

LONG PIANO_ROLL::CustomDraw(NMLVCUSTOMDRAW* msg)
{
	int cell_x, cell_y;
	switch(msg->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		return CDRF_NOTIFYITEMDRAW;
	case CDDS_ITEMPREPAINT:
		return CDRF_NOTIFYSUBITEMDRAW;
	case CDDS_SUBITEMPREPAINT:
		cell_x = msg->iSubItem;
		cell_y = msg->nmcd.dwItemSpec;
		if (cell_x > COLUMN_ICONS)
		{
			// text color
			if (taseditor_config.enable_hot_changes && cell_x >= COLUMN_JOYPAD1_A && cell_x <= COLUMN_JOYPAD4_R)
				msg->clrText = hot_changes_colors[history.GetCurrentSnapshot().GetHotChangeInfo(cell_y, cell_x - COLUMN_JOYPAD1_A)];
			else
				msg->clrText = NORMAL_TEXT_COLOR;
			// bg color and text font
			if (cell_x == COLUMN_FRAMENUM || cell_x == COLUMN_FRAMENUM2)
			{
				// font
				if (markers_manager.GetMarker(cell_y))
					SelectObject(msg->nmcd.hdc, hMainListSelectFont);
				else
					SelectObject(msg->nmcd.hdc, hMainListFont);
				// bg
				// frame number
				if (cell_y == history.GetUndoHint())
				{
					// undo hint here
					if (markers_manager.GetMarker(cell_y) && (drag_mode != DRAG_MODE_MARKER || marker_drag_framenum != cell_y))
					{
						msg->clrTextBk = (taseditor_config.bind_markers) ? BINDMARKED_UNDOHINT_FRAMENUM_COLOR : MARKED_UNDOHINT_FRAMENUM_COLOR;
					} else
					{
						msg->clrTextBk = UNDOHINT_FRAMENUM_COLOR;
					}
				} else if (cell_y == currFrameCounter || cell_y == (playback.GetFlashingPauseFrame() - 1))
				{
					// this is current frame
					if (markers_manager.GetMarker(cell_y) && (drag_mode != DRAG_MODE_MARKER || marker_drag_framenum != cell_y))
					{
						msg->clrTextBk = (taseditor_config.bind_markers) ? CUR_BINDMARKED_FRAMENUM_COLOR : CUR_MARKED_FRAMENUM_COLOR;
					} else
					{
						msg->clrTextBk = CUR_FRAMENUM_COLOR;
					}
				} else if (markers_manager.GetMarker(cell_y) && (drag_mode != DRAG_MODE_MARKER || marker_drag_framenum != cell_y))
				{
					// this is marked frame
					msg->clrTextBk = (taseditor_config.bind_markers) ? BINDMARKED_FRAMENUM_COLOR : MARKED_FRAMENUM_COLOR;
				} else if (cell_y < greenzone.GetSize())
				{
					if (!greenzone.SavestateIsEmpty(cell_y))
					{
						// the frame is normal Greenzone frame
						if (greenzone.GetLagHistoryAtFrame(cell_y))
							msg->clrTextBk = LAG_FRAMENUM_COLOR;
						else
							msg->clrTextBk = GREENZONE_FRAMENUM_COLOR;
					} else if ((!greenzone.SavestateIsEmpty(cell_y & EVERY16TH) && !greenzone.SavestateIsEmpty((cell_y & EVERY16TH) + 16))
						|| (!greenzone.SavestateIsEmpty(cell_y & EVERY8TH) && !greenzone.SavestateIsEmpty((cell_y & EVERY8TH) + 8))
						|| (!greenzone.SavestateIsEmpty(cell_y & EVERY4TH) && !greenzone.SavestateIsEmpty((cell_y & EVERY4TH) + 4))
						|| (!greenzone.SavestateIsEmpty(cell_y & EVERY2ND) && !greenzone.SavestateIsEmpty((cell_y & EVERY2ND) + 2)))
					{
						// the frame is in a gap (in Greenzone tail)
						if (greenzone.GetLagHistoryAtFrame(cell_y))
							msg->clrTextBk = PALE_LAG_FRAMENUM_COLOR;
						else
							msg->clrTextBk = PALE_GREENZONE_FRAMENUM_COLOR;
					} else 
					{
						// the frame is above Greenzone tail
						if (greenzone.GetLagHistoryAtFrame(cell_y))
							msg->clrTextBk = VERY_PALE_LAG_FRAMENUM_COLOR;
						else
							msg->clrTextBk = VERY_PALE_GREENZONE_FRAMENUM_COLOR;
					}
				} else
				{
					// the frame is below Greenzone head
					if (greenzone.GetLagHistoryAtFrame(cell_y))
						msg->clrTextBk = VERY_PALE_LAG_FRAMENUM_COLOR;
					else
						msg->clrTextBk = VERY_PALE_GREENZONE_FRAMENUM_COLOR;
				}
			} else if ((cell_x - COLUMN_JOYPAD1_A) / NUM_JOYPAD_BUTTONS == 0 || (cell_x - COLUMN_JOYPAD1_A) / NUM_JOYPAD_BUTTONS == 2)
			{
				// pad 1 or 3
				// font: empty cells have "SelectFont", non-empty have normal font
				int joy = (cell_x - COLUMN_JOYPAD1_A) / NUM_JOYPAD_BUTTONS;
				int bit = (cell_x - COLUMN_JOYPAD1_A) % NUM_JOYPAD_BUTTONS;
				if ((int)currMovieData.records.size() <= cell_y ||
					((currMovieData.records[cell_y].joysticks[joy]) & (1<<bit)) )
					SelectObject(msg->nmcd.hdc, hMainListFont);
				else
					SelectObject(msg->nmcd.hdc, hMainListSelectFont);
				// bg
				if (cell_y == history.GetUndoHint())
				{
					// undo hint here
					msg->clrTextBk = UNDOHINT_INPUT_COLOR1;
				} else if (cell_y == currFrameCounter || cell_y == (playback.GetFlashingPauseFrame() - 1))
				{
					// this is current frame
					msg->clrTextBk = CUR_INPUT_COLOR1;
				} else if (cell_y < greenzone.GetSize())
				{
					if (!greenzone.SavestateIsEmpty(cell_y))
					{
						// the frame is normal Greenzone frame
						if (greenzone.GetLagHistoryAtFrame(cell_y))
							msg->clrTextBk = LAG_INPUT_COLOR1;
						else
							msg->clrTextBk = GREENZONE_INPUT_COLOR1;
					} else if ((!greenzone.SavestateIsEmpty(cell_y & EVERY16TH) && !greenzone.SavestateIsEmpty((cell_y & EVERY16TH) + 16))
						|| (!greenzone.SavestateIsEmpty(cell_y & EVERY8TH) && !greenzone.SavestateIsEmpty((cell_y & EVERY8TH) + 8))
						|| (!greenzone.SavestateIsEmpty(cell_y & EVERY4TH) && !greenzone.SavestateIsEmpty((cell_y & EVERY4TH) + 4))
						|| (!greenzone.SavestateIsEmpty(cell_y & EVERY2ND) && !greenzone.SavestateIsEmpty((cell_y & EVERY2ND) + 2)))
					{
						// the frame is in a gap (in Greenzone tail)
						if (greenzone.GetLagHistoryAtFrame(cell_y))
							msg->clrTextBk = PALE_LAG_INPUT_COLOR1;
						else
							msg->clrTextBk = PALE_GREENZONE_INPUT_COLOR1;
					} else
					{
						// the frame is above Greenzone tail
						if (greenzone.GetLagHistoryAtFrame(cell_y))
							msg->clrTextBk = VERY_PALE_LAG_INPUT_COLOR1;
						else
							msg->clrTextBk = VERY_PALE_GREENZONE_INPUT_COLOR1;
					}
				} else
				{
					// the frame is below Greenzone head
					if (greenzone.GetLagHistoryAtFrame(cell_y))
						msg->clrTextBk = VERY_PALE_LAG_INPUT_COLOR1;
					else
						msg->clrTextBk = VERY_PALE_GREENZONE_INPUT_COLOR1;
				}
			} else if ((cell_x - COLUMN_JOYPAD1_A) / NUM_JOYPAD_BUTTONS == 1 || (cell_x - COLUMN_JOYPAD1_A) / NUM_JOYPAD_BUTTONS == 3)
			{
				// pad 2 or 4
				// font: empty cells have "SelectFont", non-empty have normal font
				int joy = (cell_x - COLUMN_JOYPAD1_A) / NUM_JOYPAD_BUTTONS;
				int bit = (cell_x - COLUMN_JOYPAD1_A) % NUM_JOYPAD_BUTTONS;
				if ((int)currMovieData.records.size() <= cell_y ||
					((currMovieData.records[cell_y].joysticks[joy]) & (1<<bit)) )
					SelectObject(msg->nmcd.hdc, hMainListFont);
				else
					SelectObject(msg->nmcd.hdc, hMainListSelectFont);
				// bg
				if (cell_y == history.GetUndoHint())
				{
					// undo hint here
					msg->clrTextBk = UNDOHINT_INPUT_COLOR2;
				} else if (cell_y == currFrameCounter || cell_y == (playback.GetFlashingPauseFrame() - 1))
				{
					// this is current frame
					msg->clrTextBk = CUR_INPUT_COLOR2;
				} else if (cell_y < greenzone.GetSize())
				{
					if (!greenzone.SavestateIsEmpty(cell_y))
					{
						// the frame is normal Greenzone frame
						if (greenzone.GetLagHistoryAtFrame(cell_y))
							msg->clrTextBk = LAG_INPUT_COLOR2;
						else
							msg->clrTextBk = GREENZONE_INPUT_COLOR2;
					} else if ((!greenzone.SavestateIsEmpty(cell_y & EVERY16TH) && !greenzone.SavestateIsEmpty((cell_y & EVERY16TH) + 16))
						|| (!greenzone.SavestateIsEmpty(cell_y & EVERY8TH) && !greenzone.SavestateIsEmpty((cell_y & EVERY8TH) + 8))
						|| (!greenzone.SavestateIsEmpty(cell_y & EVERY4TH) && !greenzone.SavestateIsEmpty((cell_y & EVERY4TH) + 4))
						|| (!greenzone.SavestateIsEmpty(cell_y & EVERY2ND) && !greenzone.SavestateIsEmpty((cell_y & EVERY2ND) + 2)))
					{
						// the frame is in a gap (in Greenzone tail)
						if (greenzone.GetLagHistoryAtFrame(cell_y))
							msg->clrTextBk = PALE_LAG_INPUT_COLOR2;
						else
							msg->clrTextBk = PALE_GREENZONE_INPUT_COLOR2;
					} else
					{
						// the frame is above Greenzone tail
						if (greenzone.GetLagHistoryAtFrame(cell_y))
							msg->clrTextBk = VERY_PALE_LAG_INPUT_COLOR2;
						else
							msg->clrTextBk = VERY_PALE_GREENZONE_INPUT_COLOR2;
					}
				} else
				{
					// the frame is below Greenzone head
					if (greenzone.GetLagHistoryAtFrame(cell_y))
						msg->clrTextBk = VERY_PALE_LAG_INPUT_COLOR2;
					else
						msg->clrTextBk = VERY_PALE_GREENZONE_INPUT_COLOR2;
				}
			}
		}
	default:
		return CDRF_DODEFAULT;
	}
}

LONG PIANO_ROLL::HeaderCustomDraw(NMLVCUSTOMDRAW* msg)
{
	switch(msg->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		SelectObject(msg->nmcd.hdc, hMainListFont);
		return CDRF_NOTIFYITEMDRAW;
	case CDDS_ITEMPREPAINT:
		{
			int cell_x = msg->nmcd.dwItemSpec;
			if (cell_x < num_columns)
			{
				int cur_color = header_colors[cell_x];
				if (cur_color)
					SetTextColor(msg->nmcd.hdc, header_lights_colors[cur_color]);
			}
		}
	default:
		return CDRF_DODEFAULT;
	}
}

// ----------------------------------------------------
void PIANO_ROLL::RightClick(LVHITTESTINFO& info)
{
	if (selection.CheckFrameSelected(info.iItem))
	{
		SelectionFrames* current_selection = selection.MakeStrobe();
		HMENU sub = GetSubMenu(hrmenu, 0);
		SetMenuDefaultItem(sub, ID_SELECTED_SETMARKERS, false);
		// inspect current selection and disable inappropriate menu items
		SelectionFrames::iterator current_selection_begin(current_selection->begin());
		SelectionFrames::iterator current_selection_end(current_selection->end());
		bool set_found = false, unset_found = false;
		for(SelectionFrames::iterator it(current_selection_begin); it != current_selection_end; it++)
		{
			if (markers_manager.GetMarker(*it))
				set_found = true;
			else 
				unset_found = true;
		}
		if (unset_found)
			EnableMenuItem(sub, ID_SELECTED_SETMARKERS, MF_BYCOMMAND | MF_ENABLED);
		else
			EnableMenuItem(sub, ID_SELECTED_SETMARKERS, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
		if (set_found)
			EnableMenuItem(sub, ID_SELECTED_REMOVEMARKERS, MF_BYCOMMAND | MF_ENABLED);
		else
			EnableMenuItem(sub, ID_SELECTED_REMOVEMARKERS, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
		POINT pt = info.pt;
		ClientToScreen(hwndList, &pt);
		TrackPopupMenu(sub, 0, pt.x, pt.y, 0, taseditor_window.hwndTasEditor, 0);
	}
}

void PIANO_ROLL::CrossGaps(int zDelta)
{
	POINT p;
	if (GetCursorPos(&p))
	{
		ScreenToClient(hwndList, &p);
		RECT wrect;
		GetClientRect(hwndList, &wrect);
		if (p.x >= 0 && p.x < wrect.right - wrect.left && p.y >= list_row_top && p.y < wrect.bottom - wrect.top)
		{
			// perform hit test
			LVHITTESTINFO info;
			info.pt.x = p.x;
			info.pt.y = p.y;
			ListView_SubItemHitTest(hwndList, &info);
			int row_index = info.iItem;
			int column_index = info.iSubItem;
			if (row_index >= 0 && column_index >= COLUMN_FRAMENUM && column_index <= COLUMN_FRAMENUM2)
			{
				if (column_index == COLUMN_FRAMENUM || column_index == COLUMN_FRAMENUM2)
				{
					// cross gaps in Markers
					if (zDelta < 0)
					{
						// search down
						int last_frame = currMovieData.getNumRecords() - 1;
						if (row_index < last_frame)
						{
							int frame = row_index + 1;
							bool result_of_closest_frame = (markers_manager.GetMarker(frame) != 0);
							while ((++frame) <= last_frame)
							{
								if ((markers_manager.GetMarker(frame) != 0) != result_of_closest_frame)
								{
									// found different result, so we crossed the gap
									ListView_Scroll(hwndList, 0, list_row_height * (frame - row_index));
									break;
								}
							}
						}
					} else
					{
						// search up
						int first_frame = 0;
						if (row_index > first_frame)
						{
							int frame = row_index - 1;
							bool result_of_closest_frame = (markers_manager.GetMarker(frame) != 0);
							while ((--frame) >= first_frame)
							{
								if ((markers_manager.GetMarker(frame) != 0) != result_of_closest_frame)
								{
									// found different result, so we crossed the gap
									ListView_Scroll(hwndList, 0, list_row_height * (frame - row_index));
									break;
								}
							}
						}
					}
				} else
				{
					// cross gaps in Input
					int joy = (column_index - COLUMN_JOYPAD1_A) / NUM_JOYPAD_BUTTONS;
					int button = (column_index - COLUMN_JOYPAD1_A) % NUM_JOYPAD_BUTTONS;
					if (zDelta < 0)
					{
						// search down
						int last_frame = currMovieData.getNumRecords() - 1;
						if (row_index < last_frame)
						{
							int frame = row_index + 1;
							bool result_of_closest_frame = currMovieData.records[frame].checkBit(joy, button);
							while ((++frame) <= last_frame)
							{
								if (currMovieData.records[frame].checkBit(joy, button) != result_of_closest_frame)
								{
									// found different result, so we crossed the gap
									ListView_Scroll(hwndList, 0, list_row_height * (frame - row_index));
									break;
								}
							}
						}
					} else
					{
						// search up
						int first_frame = 0;
						if (row_index > first_frame)
						{
							int frame = row_index - 1;
							bool result_of_closest_frame = currMovieData.records[frame].checkBit(joy, button);
							while ((--frame) >= first_frame)
							{
								if (currMovieData.records[frame].checkBit(joy, button) != result_of_closest_frame)
								{
									// found different result, so we crossed the gap
									ListView_Scroll(hwndList, 0, list_row_height * (frame - row_index));
									break;
								}
							}
						}
					}
				}
			}
		}
	}	
}
// -------------------------------------------------------------------------
LRESULT APIENTRY HeaderWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	extern PIANO_ROLL piano_roll;
	switch(msg)
	{
	case WM_SETCURSOR:
		// no column resizing cursor, always show arrow
		SetCursor(LoadCursor(0, IDC_ARROW));
		return true;
	case WM_MOUSEMOVE:
	{
		// perform hit test on header items
		HD_HITTESTINFO info;
		info.pt.x = GET_X_LPARAM(lParam) + HEADER_DX_FIX;
		info.pt.y = GET_Y_LPARAM(lParam);
		SendMessage(hWnd, HDM_HITTEST, 0, (LPARAM)&info);
		piano_roll.header_item_under_mouse = info.iItem;
		// ensure that WM_MOUSELEAVE will be catched
		TrackMouseEvent(&piano_roll.tme);
		break;
	}
	case WM_MOUSELEAVE:
	{
		piano_roll.header_item_under_mouse = -1;
		break;
	}
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		{
			if (selection.GetCurrentSelectionSize())
			{
				// perform hit test on header items
				HD_HITTESTINFO info;
				info.pt.x = GET_X_LPARAM(lParam) + HEADER_DX_FIX;
				info.pt.y = GET_Y_LPARAM(lParam);
				SendMessage(hWnd, HDM_HITTEST, 0, (LPARAM)&info);
				if (info.iItem >= COLUMN_FRAMENUM && info.iItem <= COLUMN_FRAMENUM2)
					piano_roll.ColumnSet(info.iItem, (GetKeyState(VK_MENU) < 0));
			}
		}
		return true;
	}
	return CallWindowProc(hwndHeader_oldWndproc, hWnd, msg, wParam, lParam);
}

// The subclass wndproc for the listview
LRESULT APIENTRY ListWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	extern PIANO_ROLL piano_roll;
	switch(msg)
	{
		case WM_CHAR:
		case WM_KILLFOCUS:
			return 0;
		case WM_SETCURSOR:
			if (LOWORD(lParam) == HTCLIENT)
			{
				piano_roll.must_check_item_under_mouse = true;
				return true;
			}
			break;
		case WM_MOUSEMOVE:
		{
			piano_roll.must_check_item_under_mouse = true;
			return 0;
		}
		case WM_NOTIFY:
		{
			if (((LPNMHDR)lParam)->hwndFrom == piano_roll.hwndHeader)
			{
				switch (((LPNMHDR)lParam)->code)
				{
				case HDN_BEGINTRACKW:
				case HDN_BEGINTRACKA:
				case HDN_TRACK:
					return true;	// no column resizing
				case NM_CUSTOMDRAW:
					return piano_roll.HeaderCustomDraw((NMLVCUSTOMDRAW*)lParam);
				}
			}
			break;
		}
		case WM_KEYDOWN:
			return 0;
		case WM_TIMER:
			// disable timer of entering edit mode (there's no edit mode anyway)
			return 0;
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
		{
			int fwKeys = GET_KEYSTATE_WPARAM(wParam);
			bool alt_pressed = (GetKeyState(VK_MENU) < 0);
			// perform hit test
			LVHITTESTINFO info;
			info.pt.x = GET_X_LPARAM(lParam);
			info.pt.y = GET_Y_LPARAM(lParam);
			ListView_SubItemHitTest(hWnd, &info);
			int row_index = info.iItem;
			int column_index = info.iSubItem;
			if (column_index == COLUMN_ICONS)
			{
				// clicked on the "icons" column
				piano_roll.StartDraggingPlaybackCursor();
			} else if (column_index == COLUMN_FRAMENUM || column_index == COLUMN_FRAMENUM2)
			{
				// clicked on the "Frame#" column
				if (row_index >= 0)
				{
					if (msg == WM_LBUTTONDBLCLK)
					{
						// doubleclick - set Marker and start dragging it
						if (!markers_manager.GetMarker(row_index))
						{
							if (markers_manager.SetMarker(row_index))
							{
								selection.must_find_current_marker = playback.must_find_current_marker = true;
								history.RegisterMarkersChange(MODTYPE_MARKER_SET, row_index);
								piano_roll.RedrawRow(row_index);
							}
						}
						piano_roll.StartDraggingMarker(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), row_index, column_index);
					} else
					{
						if (fwKeys & MK_SHIFT)
						{
							// select region from row_last_clicked to row_index
							if (piano_roll.row_last_clicked < row_index)
								selection.SetRegionSelection(piano_roll.row_last_clicked, row_index + 1);
							else
								selection.SetRegionSelection(row_index, piano_roll.row_last_clicked + 1);
						} else if (alt_pressed)
						{
							// make selection by Pattern
							if (piano_roll.row_last_clicked < row_index)
								selection.SetRegionSelectionPattern(piano_roll.row_last_clicked, row_index);
							else
								selection.SetRegionSelectionPattern(row_index, piano_roll.row_last_clicked);
						} else if (fwKeys & MK_CONTROL)
						{
							if (selection.GetRowSelection(row_index))
								selection.ClearRowSelection(row_index);
							else
								selection.SetRowSelection(row_index);
							piano_roll.row_last_clicked = row_index;
						} else	// just click
						{
							selection.ClearSelection();
							selection.SetRowSelection(row_index);
							piano_roll.row_last_clicked = row_index;
						}
						piano_roll.StartSelectingDrag(row_index);
					}
				}
			} else if (column_index >= COLUMN_JOYPAD1_A && column_index <= COLUMN_JOYPAD4_R)
			{
				// clicked on Input
				if (row_index >= 0)
				{
					if (!alt_pressed && !(fwKeys & MK_SHIFT))
					{
						// clicked without Shift/Alt - set "row_last_clicked" here
						piano_roll.row_last_clicked = row_index;
						if (!(fwKeys & MK_CONTROL))
						{
							// change Selection to this row
							selection.ClearSelection();
							selection.SetRowSelection(row_index);
						}
					}
					// toggle input
					piano_roll.drawing_start_time = clock();
					int joy = (column_index - COLUMN_JOYPAD1_A) / NUM_JOYPAD_BUTTONS;
					int button = (column_index - COLUMN_JOYPAD1_A) % NUM_JOYPAD_BUTTONS;
					if (alt_pressed)
						editor.InputSetPattern(piano_roll.row_last_clicked, row_index, joy, button, piano_roll.drawing_start_time);
					else
						editor.InputToggle(piano_roll.row_last_clicked, row_index, joy, button, piano_roll.drawing_start_time);
					// and start dragging/drawing
					if (piano_roll.drag_mode == DRAG_MODE_NONE)
					{
						if (taseditor_config.draw_input)
						{
							// if clicked this click created buttonpress, then start painting, else start erasing
							if (currMovieData.records[row_index].checkBit(joy, button))
								piano_roll.drag_mode = DRAG_MODE_SET;
							else
								piano_roll.drag_mode = DRAG_MODE_UNSET;
							piano_roll.drawing_last_x = GET_X_LPARAM(lParam) + GetScrollPos(piano_roll.hwndList, SB_HORZ);
							piano_roll.drawing_last_y = GET_Y_LPARAM(lParam) + GetScrollPos(piano_roll.hwndList, SB_VERT) * piano_roll.list_row_height;
						} else
						{
							piano_roll.drag_mode = DRAG_MODE_OBSERVE;
						}
					}
				}
			}
			return 0;
		}
		case WM_MBUTTONDOWN:
		case WM_MBUTTONDBLCLK:
		{
			if (GetFocus() != hWnd)
				SetFocus(hWnd);
			playback.MiddleButtonClick();
			return 0;
		}
		case WM_MOUSEWHEEL:
		{
			bool alt_pressed = (GetKeyState(VK_MENU) < 0);
			int fwKeys = GET_KEYSTATE_WPARAM(wParam);
			int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			if (fwKeys & MK_SHIFT)
			{
				// Shift + wheel = Playback rewind full(speed)/forward full(speed)
				if (zDelta < 0)
					playback.ForwardFull(-zDelta / WHEEL_DELTA);
				else if (zDelta > 0)
					playback.RewindFull(zDelta / WHEEL_DELTA);
			} else if (fwKeys & MK_CONTROL)
			{
				// Ctrl + wheel = Selection rewind full(speed)/forward full(speed)
				if (zDelta < 0)
					selection.JumpNextMarker(-zDelta / WHEEL_DELTA);
				else if (zDelta > 0)
					selection.JumpPrevMarker(zDelta / WHEEL_DELTA);
			} else if (fwKeys & MK_RBUTTON)
			{
				// Right button + wheel = rewind/forward
				int destination_frame = currFrameCounter - (zDelta / WHEEL_DELTA);
				if (destination_frame < 0) destination_frame = 0;
				int lastCursor = currFrameCounter;
				playback.jump(destination_frame);
				if (lastCursor != currFrameCounter)
				{
					// redraw row where Playback cursor was (in case there's two or more WM_MOUSEWHEEL messages before playback.update())
					piano_roll.RedrawRow(lastCursor);
					bookmarks.RedrawChangedBookmarks(lastCursor);
				}
			} else if (history.CursorOverHistoryList())
			{
				return SendMessage(history.hwndHistoryList, WM_MOUSEWHEEL_RESENT, wParam, lParam);
			} else if (alt_pressed)
			{
				// cross gaps in input/Markers
				piano_roll.CrossGaps(zDelta);
			} else
			{
				// normal scrolling - make it 2x faster than usual
				CallWindowProc(hwndList_oldWndProc, hWnd, msg, MAKELONG(fwKeys, zDelta * PIANO_ROLL_SCROLLING_BOOST), lParam);
			}
			return 0;
		}
		case WM_RBUTTONDOWN:
		case WM_RBUTTONDBLCLK:
			piano_roll.rbutton_drag_mode = true;
			if (GetFocus() != hWnd)
				SetFocus(hWnd);
			return 0;
		case WM_RBUTTONUP:
		{
			// perform hit test
			LVHITTESTINFO info;
			info.pt.x = GET_X_LPARAM(lParam);
			info.pt.y = GET_Y_LPARAM(lParam);
			ListView_SubItemHitTest(hWnd, &info);
			// show context menu if user right-clicked on Frame#
			if (info.iSubItem <= COLUMN_FRAMENUM || info.iSubItem >= COLUMN_FRAMENUM2)
				piano_roll.RightClick(info);
			return 0;
		}
		case WM_NCLBUTTONDOWN:
		{
			if (wParam == HTBORDER)
			{
				POINT p;
				p.x = GET_X_LPARAM(lParam);
				p.y = GET_Y_LPARAM(lParam);
				ScreenToClient(piano_roll.hwndList, &p);
				if (p.x <= 0)
				{
					// user clicked on left border of the Piano Roll
					// consider this as a "misclick" on Piano Roll's first column
					piano_roll.StartDraggingPlaybackCursor();
					return 0;
				}
			}
			break;
		}
		case WM_MOUSEACTIVATE:
		{
			if (GetFocus() != hWnd)
				SetFocus(hWnd);
            break;
		}
		case LVM_ENSUREVISIBLE:
		{
			// Piano Roll probably scrolls
			piano_roll.must_check_item_under_mouse = true;
			break;
		}
		case WM_VSCROLL:
		{
			// fix for known WinXP bug
			if (LOWORD(wParam) == SB_LINEUP)
			{
				ListView_Scroll(piano_roll.hwndList, 0, -piano_roll.list_row_height);
				return 0;
			} else if (LOWORD(wParam) == SB_LINEDOWN)
			{
				ListView_Scroll(piano_roll.hwndList, 0, piano_roll.list_row_height);
				return 0;
			}
			break;
		}
		case WM_HSCROLL:
		{
			if (LOWORD(wParam) == SB_LINELEFT)
			{
				ListView_Scroll(piano_roll.hwndList, -COLUMN_BUTTON_WIDTH, 0);
				return 0;
			} else if (LOWORD(wParam) == SB_LINERIGHT)
			{
				ListView_Scroll(piano_roll.hwndList, COLUMN_BUTTON_WIDTH, 0);
				return 0;
			}
			break;
		}

	}
	return CallWindowProc(hwndList_oldWndProc, hWnd, msg, wParam, lParam);
}
// ----------------------------------------------------------------------------------------
LRESULT APIENTRY MarkerDragBoxWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	extern PIANO_ROLL piano_roll;
	switch(message)
	{
		case WM_CREATE:
		{
			// create static bitmap placeholder
			char framenum[DIGITS_IN_FRAMENUM + 1];
			U32ToDecStr(framenum, piano_roll.marker_drag_framenum, DIGITS_IN_FRAMENUM);
			piano_roll.hwndMarkerDragBoxText = CreateWindow(WC_STATIC, framenum, SS_CENTER| WS_CHILD | WS_VISIBLE, 0, 0, COLUMN_FRAMENUM_WIDTH, piano_roll.list_row_height, hwnd, NULL, NULL, NULL);
			SendMessage(piano_roll.hwndMarkerDragBoxText, WM_SETFONT, (WPARAM)piano_roll.hMainListSelectFont, 0);
			return 0;
		}
		case WM_CTLCOLORSTATIC:
		{
			// change color of static text fields
			if ((HWND)lParam == piano_roll.hwndMarkerDragBoxText)
			{
				SetTextColor((HDC)wParam, NORMAL_TEXT_COLOR);
				SetBkMode((HDC)wParam, TRANSPARENT);
				if (taseditor_config.bind_markers)
					return (LRESULT)(piano_roll.marker_drag_box_brush_bind);
				else
					return (LRESULT)(piano_roll.marker_drag_box_brush);
			}
			break;
		}
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}

