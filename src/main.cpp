// Mem Reduct
// Copyright (c) 2011-2017 Henry++

#include <windows.h>
#include <subauth.h>
#include <algorithm>

#include "main.hpp"
#include "rapp.hpp"
#include "routine.hpp"

#include "resource.hpp"

STATIC_DATA config = {0};

rapp app (APP_NAME, APP_NAME_SHORT, APP_VERSION, APP_COPYRIGHT);

std::vector<size_t> limit_vec;
std::vector<size_t> interval_vec;

VOID generate_menu_array (UINT val, std::vector<size_t>& pvc)
{
	pvc.clear ();

	for (UINT i = 1; i < 10; i++)
	{
		pvc.push_back (i * 10);
	}

	for (UINT i = val - 2; i <= (val + 2); i++)
	{
		if (i >= 5)
			pvc.push_back (i);
	}

	std::sort (pvc.begin (), pvc.end ()); // sort
	pvc.erase (std::unique (pvc.begin (), pvc.end ()), pvc.end ()); // remove duplicates
}

VOID BresenhamCircle (HDC dc, LONG radius, LPPOINT pt, COLORREF clr)
{
	LONG cx = 0, cy = radius, d = 2 - 2 * radius;

	// let's start drawing the circle:
	SetPixel (dc, cx + pt->x, cy + pt->y, clr); // point (0, R);
	SetPixel (dc, cx + pt->x, -cy + pt->y, clr); // point (0, -R);
	SetPixel (dc, cy + pt->x, cx + pt->y, clr); // point (R, 0);
	SetPixel (dc, -cy + pt->x, cx + pt->y, clr); // point (-R, 0);

	while (1)
	{
		if (d > -cy)
		{
			--cy;
			d += 1 - 2 * cy;
		}

		if (d <= cx)
		{
			++cx;
			d += 1 + 2 * cx;
		}

		if (!cy)
		{
			break;
		} // cy is 0, but these points are already drawn;

		// the actual drawing:
		SetPixel (dc, cx + pt->x, cy + pt->y, clr); // 0-90 degrees
		SetPixel (dc, -cx + pt->x, cy + pt->y, clr); // 90-180 degrees
		SetPixel (dc, -cx + pt->x, -cy + pt->y, clr); // 180-270 degrees
		SetPixel (dc, cx + pt->x, -cy + pt->y, clr); // 270-360 degrees
	}
}

VOID BresenhamLine (HDC dc, INT x0, INT y0, INT x1, INT y1, COLORREF clr)
{
	INT dx = abs (x1 - x0), sx = x0 < x1 ? 1 : -1;
	INT dy = abs (y1 - y0), sy = y0 < y1 ? 1 : -1;
	INT err = (dx > dy ? dx : -dy) / 2;

	while (1)
	{
		SetPixel (dc, x0, y0, clr);

		if (x0 == x1 && y0 == y1)
		{
			break;
		}

		INT e2 = err;

		if (e2 > -dx)
		{
			err -= dy; x0 += sx;
		}
		if (e2 < dy)
		{
			err += dx; y0 += sy;
		}
	}
}

DWORD _app_getstatus (MEMORYINFO* m)
{
	MEMORYSTATUSEX msex = {0};
	SYSTEM_CACHE_INFORMATION sci = {0};

	msex.dwLength = sizeof (msex);

	GlobalMemoryStatusEx (&msex);

	if (m)
	{
		m->percent_phys = msex.dwMemoryLoad;

		m->free_phys = msex.ullAvailPhys;
		m->total_phys = msex.ullTotalPhys;

		m->percent_page = _R_PERCENT_OF (msex.ullTotalPageFile - msex.ullAvailPageFile, msex.ullTotalPageFile);

		m->free_page = msex.ullAvailPageFile;
		m->total_page = msex.ullTotalPageFile;

		if (NT_SUCCESS (NtQuerySystemInformation (SystemFileCacheInformation, &sci, sizeof (sci), nullptr)))
		{
			m->percent_ws = _R_PERCENT_OF (sci.CurrentSize, sci.PeakSize);

			m->free_ws = (sci.PeakSize - sci.CurrentSize);
			m->total_ws = sci.PeakSize;
		}
	}

	return msex.dwMemoryLoad;
}

DWORD _app_clean (HWND hwnd)
{
	MEMORYINFO mem = {0};
	SYSTEM_MEMORY_LIST_COMMAND smlc;
	const DWORD mask = app.ConfigGet (L"ReductMask", MASK_DEFAULT).AsUlong ();

	if (!mask || !app.IsAdmin () || (hwnd && app.ConfigGet (L"ReductConfirmation", true).AsBool () && _r_msg (hwnd, MB_YESNO | MB_ICONQUESTION, APP_NAME, nullptr, I18N (&app, IDS_QUESTION, 0)) != IDYES))
		return 0;

	// difference (before)
	_app_getstatus (&mem);
	const DWORD reduct_before = DWORD (mem.total_phys - mem.free_phys);

	// Working set
	if (app.IsVistaOrLater () && (mask & REDUCT_WORKING_SET) != 0)
	{
		smlc = MemoryEmptyWorkingSets;
		NtSetSystemInformation (SystemMemoryListInformation, &smlc, sizeof (smlc));
	}

	// System working set
	if ((mask & REDUCT_SYSTEM_WORKING_SET) != 0)
	{
		SYSTEM_CACHE_INFORMATION sci = {0};

		sci.MinimumWorkingSet = (ULONG)-1;
		sci.MaximumWorkingSet = (ULONG)-1;

		NtSetSystemInformation (SystemFileCacheInformation, &sci, sizeof (sci));
	}

	// Standby priority-0 list
	if (app.IsVistaOrLater () && (mask & REDUCT_STANDBY_PRIORITY0_LIST) != 0)
	{
		smlc = MemoryPurgeLowPriorityStandbyList;
		NtSetSystemInformation (SystemMemoryListInformation, &smlc, sizeof (smlc));
	}

	// Standby list
	if (app.IsVistaOrLater () && (mask & REDUCT_STANDBY_LIST) != 0)
	{
		smlc = MemoryPurgeStandbyList;
		NtSetSystemInformation (SystemMemoryListInformation, &smlc, sizeof (smlc));
	}

	// Modified list
	if (app.IsVistaOrLater () && (mask & REDUCT_MODIFIED_LIST) != 0)
	{
		smlc = MemoryFlushModifiedList;
		NtSetSystemInformation (SystemMemoryListInformation, &smlc, sizeof (smlc));
	}

	// difference (after)
	_app_getstatus (&mem);
	const DWORD reduct_result = reduct_before - DWORD (mem.total_phys - mem.free_phys);

	app.ConfigSet (L"StatisticLastReduct", _r_unixtime_now ()); // time of last cleaning

	if (app.ConfigGet (L"BalloonCleanResults", true).AsBool ())
		app.TrayPopup (NIIF_INFO, APP_NAME, _r_fmt (I18N (&app, IDS_STATUS_CLEANED, 0), _r_fmt_size64 ((DWORDLONG)reduct_result)));

	return reduct_result;
}

HICON _app_drawicon ()
{
	COLORREF color = app.ConfigGet (L"TrayColorText", TRAY_COLOR_TEXT).AsUlong ();
	HBRUSH bg_brush = config.bg_brush;
	bool is_transparent = app.ConfigGet (L"TrayUseTransparency", false).AsBool ();
	const bool is_round = app.ConfigGet (L"TrayRoundCorners", false).AsBool ();

	const bool has_danger = config.ms.percent_phys >= app.ConfigGet (L"TrayLevelDanger", 90).AsUlong ();
	const bool has_warning = has_danger || config.ms.percent_phys >= app.ConfigGet (L"TrayLevelWarning", 60).AsUlong ();

	if (has_danger || has_warning)
	{
		if (app.ConfigGet (L"TrayChangeBg", true).AsBool ())
		{
			bg_brush = has_danger ? config.bg_brush_danger : config.bg_brush_warning;
			is_transparent = false;
		}
		else
		{
			if (has_danger)
			{
				color = app.ConfigGet (L"TrayColorDanger", TRAY_COLOR_DANGER).AsUlong ();
			}
			else
			{
				color = app.ConfigGet (L"TrayColorWarning", TRAY_COLOR_WARNING).AsUlong ();
			}
		}
	}

	// select bitmap
	const HBITMAP old_bitmap = (HBITMAP)SelectObject (config.cdc1, config.bitmap);

	// draw transparent mask
	COLORREF clr_prev = SetBkColor (config.cdc1, TRAY_COLOR_MASK);
	ExtTextOut (config.cdc1, 0, 0, ETO_OPAQUE, &config.rc, nullptr, 0, nullptr);
	SetBkColor (config.cdc1, clr_prev);

	// draw background
	if (!is_transparent)
	{
		HGDIOBJ prev_pen = SelectObject (config.cdc1, GetStockObject (NULL_PEN));
		HGDIOBJ prev_brush = SelectObject (config.cdc1, bg_brush);

		RoundRect (config.cdc1, 0, 0, config.rc.right, config.rc.bottom, is_round ? ((config.rc.right - 2)) : 0, is_round ? ((config.rc.right) / 2) : 0);

		SelectObject (config.cdc1, prev_pen);
		SelectObject (config.cdc1, prev_brush);
	}

	// draw border
	if (app.ConfigGet (L"TrayShowBorder", false).AsBool ())
	{
		if (is_round)
		{
			POINT pt = {0};

			pt.x = ((config.rc.left + config.rc.right) / 2) - 1;
			pt.y = ((config.rc.top + config.rc.bottom) / 2) - 1;

			INT half = pt.x + 1;

			for (LONG i = 1; i < config.scale + 1; i++)
			{
				BresenhamCircle (config.cdc1, half - (i), &pt, color);
			}
		}
		else
		{
			for (LONG i = 0; i < config.scale; i++)
			{
				BresenhamLine (config.cdc1, i, 0, i, config.rc.bottom, color); // left
				BresenhamLine (config.cdc1, i, i, config.rc.right, i, color); // top
				BresenhamLine (config.cdc1, (config.rc.right - 1) - i, 0, (config.rc.right - 1) - i, config.rc.bottom, color); // right
				BresenhamLine (config.cdc1, 0, (config.rc.bottom - 1) - i, config.rc.right, (config.rc.bottom - 1) - i, color); // bottom
			}
		}
	}

	// draw text
	SetTextColor (config.cdc1, color);
	SetBkMode (config.cdc1, TRANSPARENT);

	rstring buffer;
	buffer.Format (L"%d", config.ms.percent_phys);

	SelectObject (config.cdc1, config.font);
	DrawTextEx (config.cdc1, buffer.GetBuffer (), static_cast<int>(buffer.GetLength ()), &config.rc, DT_VCENTER | DT_CENTER | DT_SINGLELINE | DT_NOCLIP, nullptr);

	// draw transparent mask
	HGDIOBJ old_mask = SelectObject (config.cdc2, config.bitmap_mask);

	SetBkColor (config.cdc1, TRAY_COLOR_MASK);
	BitBlt (config.cdc2, 0, 0, config.rc.right, config.rc.bottom, config.cdc1, 0, 0, SRCCOPY);

	SelectObject (config.cdc2, old_mask);
	SelectObject (config.cdc1, old_bitmap);

	// finalize icon
	ICONINFO ii = {0};

	ii.fIcon = TRUE;
	ii.hbmColor = config.bitmap;
	ii.hbmMask = config.bitmap_mask;

	return CreateIconIndirect (&ii);
}

VOID CALLBACK _app_timercallback (HWND hwnd, UINT, UINT_PTR, DWORD)
{
	_app_getstatus (&config.ms);

	// autoreduct
	if (app.IsAdmin ())
	{
		if ((app.ConfigGet (L"AutoreductEnable", true).AsBool () && config.ms.percent_phys >= app.ConfigGet (L"AutoreductValue", 90).AsUlong ()) ||
			(app.ConfigGet (L"AutoreductIntervalEnable", false).AsBool () && (_r_unixtime_now () - app.ConfigGet (L"StatisticLastReduct", 0).AsLonglong ()) >= (app.ConfigGet (L"AutoreductIntervalValue", 30).AsInt () * 60)))
		{
			_app_clean (nullptr);
		}
	}

	if (config.ms_prev != config.ms.percent_phys)
	{
		app.TraySetInfo (_app_drawicon (), _r_fmt (I18N (&app, IDS_TOOLTIP, 0), config.ms.percent_phys, config.ms.percent_page, config.ms.percent_ws));

		config.ms_prev = config.ms.percent_phys; // store last percentage value (required!)
	}

	if (IsWindowVisible (hwnd))
	{
		// Physical memory
		_r_listview_additem (hwnd, IDC_LISTVIEW, _r_fmt (L"%d%%", config.ms.percent_phys), 0, 1, LAST_VALUE, LAST_VALUE, config.ms.percent_phys);
		_r_listview_additem (hwnd, IDC_LISTVIEW, _r_fmt_size64 (config.ms.free_phys), 1, 1, LAST_VALUE, LAST_VALUE, config.ms.percent_phys);
		_r_listview_additem (hwnd, IDC_LISTVIEW, _r_fmt_size64 (config.ms.total_phys), 2, 1, LAST_VALUE, LAST_VALUE, config.ms.percent_phys);

		// Page file
		_r_listview_additem (hwnd, IDC_LISTVIEW, _r_fmt (L"%d%%", config.ms.percent_page), 3, 1, LAST_VALUE, LAST_VALUE, config.ms.percent_page);
		_r_listview_additem (hwnd, IDC_LISTVIEW, _r_fmt_size64 (config.ms.free_page), 4, 1, LAST_VALUE, LAST_VALUE, config.ms.percent_page);
		_r_listview_additem (hwnd, IDC_LISTVIEW, _r_fmt_size64 (config.ms.total_page), 5, 1, LAST_VALUE, LAST_VALUE, config.ms.percent_page);

		// System working set
		_r_listview_additem (hwnd, IDC_LISTVIEW, _r_fmt (L"%d%%", config.ms.percent_ws), 6, 1, LAST_VALUE, LAST_VALUE, config.ms.percent_ws);
		_r_listview_additem (hwnd, IDC_LISTVIEW, _r_fmt_size64 (config.ms.free_ws), 7, 1, LAST_VALUE, LAST_VALUE, config.ms.percent_ws);
		_r_listview_additem (hwnd, IDC_LISTVIEW, _r_fmt_size64 (config.ms.total_ws), 8, 1, LAST_VALUE, LAST_VALUE, config.ms.percent_ws);

		SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_REDRAWITEMS, 0, _r_listview_getitemcount (hwnd, IDC_LISTVIEW)); // redraw (required!)
	}
}

BOOL initializer_callback (HWND hwnd, DWORD msg, LPVOID, LPVOID)
{
	switch (msg)
	{
		case _RM_INITIALIZE:
		{
			// clear params
			config.ms_prev = 0;

			config.scale = app.ConfigGet (L"TrayUseAntialiasing", false).AsBool () ? 16 : 1;

			// init resolution
			config.rc.right = GetSystemMetrics (SM_CXSMICON) * config.scale;
			config.rc.bottom = GetSystemMetrics (SM_CYSMICON) * config.scale;

			// init device context
			config.dc = GetDC (nullptr);

			config.cdc1 = CreateCompatibleDC (config.dc);
			config.cdc2 = CreateCompatibleDC (config.dc);

			ReleaseDC (nullptr, config.dc);

			// init bitmap
			BITMAPINFO bmi = {0};

			bmi.bmiHeader.biSize = sizeof (bmi.bmiHeader);
			bmi.bmiHeader.biWidth = config.rc.right;
			bmi.bmiHeader.biHeight = config.rc.bottom;
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = 32;
			bmi.bmiHeader.biCompression = BI_RGB;

			config.bitmap = CreateDIBSection (config.dc, &bmi, DIB_RGB_COLORS, 0, nullptr, 0);
			config.bitmap_mask = CreateBitmap (config.rc.right, config.rc.bottom, 1, 1, nullptr);

			// init brush
			config.bg_brush = CreateSolidBrush (app.ConfigGet (L"TrayColorBg", TRAY_COLOR_BG).AsUlong ());
			config.bg_brush_warning = CreateSolidBrush (app.ConfigGet (L"TrayColorWarning", TRAY_COLOR_WARNING).AsUlong ());
			config.bg_brush_danger = CreateSolidBrush (app.ConfigGet (L"TrayColorDanger", TRAY_COLOR_DANGER).AsUlong ());

			// init font
			SecureZeroMemory (&config.lf, sizeof (config.lf));

			config.lf.lfQuality = (app.ConfigGet (L"TrayUseTransparency", false).AsBool () || app.ConfigGet (L"TrayUseAntialiasing", false).AsBool ()) ? NONANTIALIASED_QUALITY : CLEARTYPE_QUALITY;
			config.lf.lfCharSet = DEFAULT_CHARSET;
			config.lf.lfPitchAndFamily = FF_DONTCARE;
			config.lf.lfWeight = app.ConfigGet (L"TrayFontWeight", FW_NORMAL).AsLong ();
			config.lf.lfHeight = -MulDiv (app.ConfigGet (L"TrayFontSize", FONT_SIZE).AsInt (), GetDeviceCaps (config.cdc1, LOGPIXELSY), 72) * config.scale;

			StringCchCopy (config.lf.lfFaceName, LF_FACESIZE, app.ConfigGet (L"TrayFontName", FONT_NAME));

			config.font = CreateFontIndirect (&config.lf);

			// init hotkey
			UINT hk = app.ConfigGet (L"HotkeyClean", MAKEWORD (VK_F1, HOTKEYF_CONTROL)).AsUint ();

			if (app.IsAdmin () && hk && app.ConfigGet (L"HotkeyCleanEnable", true).AsBool ())
			{
				RegisterHotKey (hwnd, UID, (HIBYTE (hk) & 2) | ((HIBYTE (hk) & 4) >> 2) | ((HIBYTE (hk) & 1) << 2), LOBYTE (hk));
			}

			// init tray icon
			app.TrayCreate (hwnd, UID, WM_TRAYICON, _app_drawicon (), false);

			// init timer
			_app_timercallback (hwnd, 0, 0, 0);
			SetTimer (hwnd, UID, TIMER, &_app_timercallback);

			break;
		}

		case _RM_LOCALIZE:
		{
			_r_listview_deleteallitems (hwnd, IDC_LISTVIEW);
			_r_listview_deleteallgroups (hwnd, IDC_LISTVIEW);

			// configure listview
			for (INT i = 0; i < 3; i++)
			{
				_r_listview_addgroup (hwnd, IDC_LISTVIEW, I18N (&app, IDS_GROUP_1 + i, _r_fmt (L"IDS_GROUP_%d", i + 1)), i);

				for (INT j = 0; j < 3; j++)
					_r_listview_additem (hwnd, IDC_LISTVIEW, I18N (&app, IDS_ITEM_1 + j, _r_fmt (L"IDS_ITEM_%d", j + 1)), LAST_VALUE, 0, LAST_VALUE, i);
			}

			// configure menu
			HMENU menu = GetMenu (hwnd);

			app.LocaleMenu (menu, I18N (&app, IDS_FILE, 0), 0, true);
			app.LocaleMenu (menu, I18N (&app, IDS_SETTINGS, 0), IDM_SETTINGS, false);
			app.LocaleMenu (menu, I18N (&app, IDS_EXIT, 0), IDM_EXIT, false);
			app.LocaleMenu (menu, I18N (&app, IDS_HELP, 0), 1, true);
			app.LocaleMenu (menu, I18N (&app, IDS_WEBSITE, 0), IDM_WEBSITE, false);
			app.LocaleMenu (menu, I18N (&app, IDS_DONATE, 0), IDM_DONATE, false);
			app.LocaleMenu (menu, I18N (&app, IDS_CHECKUPDATES, 0), IDM_CHECKUPDATES, false);
			app.LocaleMenu (menu, I18N (&app, IDS_ABOUT, 0), IDM_ABOUT, false);

			// configure button
			SetDlgItemText (hwnd, IDC_CLEAN, I18N (&app, IDS_CLEAN, 0));
			_r_wnd_addstyle (hwnd, IDC_CLEAN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			break;
		}

		case _RM_UNINITIALIZE:
		{
			app.TrayDestroy (UID);

			UnregisterHotKey (hwnd, UID);
			KillTimer (hwnd, UID);

			DeleteObject (config.font);

			DeleteObject (config.bg_brush);
			DeleteObject (config.bg_brush_warning);
			DeleteObject (config.bg_brush_danger);

			DeleteObject (config.bitmap);
			DeleteObject (config.bitmap_mask);

			DeleteDC (config.cdc1);
			DeleteDC (config.cdc2);
			DeleteDC (config.dc);

			break;
		}
	}

	return FALSE;
}

BOOL settings_callback (HWND hwnd, DWORD msg, LPVOID lpdata1, LPVOID lpdata2)
{
	PAPP_SETTINGS_PAGE page = (PAPP_SETTINGS_PAGE)lpdata2;

	switch (msg)
	{
		case _RM_INITIALIZE:
		{
			switch (page->dlg_id)
			{
				case IDD_SETTINGS_1:
				{
					// localize
					SetDlgItemText (hwnd, IDC_TITLE_1, I18N (&app, IDS_TITLE_1, 0));
					SetDlgItemText (hwnd, IDC_TITLE_2, I18N (&app, IDS_TITLE_2, 0));

					SetDlgItemText (hwnd, IDC_ALWAYSONTOP_CHK, I18N (&app, IDS_ALWAYSONTOP_CHK, 0));
					SetDlgItemText (hwnd, IDC_LOADONSTARTUP_CHK, I18N (&app, IDS_LOADONSTARTUP_CHK, 0));
					SetDlgItemText (hwnd, IDC_REDUCTCONFIRMATION_CHK, I18N (&app, IDS_REDUCTCONFIRMATION_CHK, 0));
					SetDlgItemText (hwnd, IDC_SKIPUACWARNING_CHK, I18N (&app, IDS_SKIPUACWARNING_CHK, 0));
					SetDlgItemText (hwnd, IDC_CHECKUPDATES_CHK, I18N (&app, IDS_CHECKUPDATES_CHK, 0));

					SetDlgItemText (hwnd, IDC_LANGUAGE_HINT, I18N (&app, IDS_LANGUAGE_HINT, 0));

					// set checks
					if (!app.IsVistaOrLater () || !app.IsAdmin ())
						_r_ctrl_enable (hwnd, IDC_SKIPUACWARNING_CHK, false);

					CheckDlgButton (hwnd, IDC_ALWAYSONTOP_CHK, app.ConfigGet (L"AlwaysOnTop", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

#ifdef _APP_HAVE_AUTORUN
					CheckDlgButton (hwnd, IDC_LOADONSTARTUP_CHK, app.AutorunIsEnabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // _APP_HAVE_AUTORUN

					CheckDlgButton (hwnd, IDC_REDUCTCONFIRMATION_CHK, app.ConfigGet (L"ReductConfirmation", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

#ifdef _APP_HAVE_SKIPUAC
					CheckDlgButton (hwnd, IDC_SKIPUACWARNING_CHK, app.SkipUacIsEnabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // _APP_HAVE_SKIPUAC

					CheckDlgButton (hwnd, IDC_CHECKUPDATES_CHK, app.ConfigGet (L"CheckUpdates", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					app.LocaleEnum (hwnd, IDC_LANGUAGE, false, 0);

					SetWindowLongPtr (hwnd, GWLP_USERDATA, (LONG_PTR)SendDlgItemMessage (hwnd, IDC_LANGUAGE, CB_GETCURSEL, 0, 0)); // check on save

					break;
				}

				case IDD_SETTINGS_2:
				{
					// localize
					SetDlgItemText (hwnd, IDC_TITLE_3, I18N (&app, IDS_TITLE_3, 0));
					SetDlgItemText (hwnd, IDC_TITLE_4, I18N (&app, IDS_TITLE_4, 0));
					SetDlgItemText (hwnd, IDC_TITLE_5, I18N (&app, IDS_TITLE_5, 0));

					SetDlgItemText (hwnd, IDC_WORKINGSET_CHK, I18N (&app, IDS_WORKINGSET_CHK, 0));
					SetDlgItemText (hwnd, IDC_SYSTEMWORKINGSET_CHK, I18N (&app, IDS_SYSTEMWORKINGSET_CHK, 0));
					SetDlgItemText (hwnd, IDC_STANDBYLISTPRIORITY0_CHK, I18N (&app, IDS_STANDBYLISTPRIORITY0_CHK, 0));
					SetDlgItemText (hwnd, IDC_STANDBYLIST_CHK, I18N (&app, IDS_STANDBYLIST_CHK, 0));
					SetDlgItemText (hwnd, IDC_MODIFIEDLIST_CHK, I18N (&app, IDS_MODIFIEDLIST_CHK, 0));

					SetDlgItemText (hwnd, IDC_AUTOREDUCTENABLE_CHK, I18N (&app, IDS_AUTOREDUCTENABLE_CHK, 0));
					SetDlgItemText (hwnd, IDC_AUTOREDUCTINTERVALENABLE_CHK, I18N (&app, IDS_AUTOREDUCTINTERVALENABLE_CHK, 0));

					SetDlgItemText (hwnd, IDC_HOTKEY_CLEAN_CHK, I18N (&app, IDS_HOTKEY_CLEAN_CHK, 0));

					// set checks
					if (!app.IsVistaOrLater () || !app.IsAdmin ())
					{
						_r_ctrl_enable (hwnd, IDC_WORKINGSET_CHK, false);
						_r_ctrl_enable (hwnd, IDC_STANDBYLISTPRIORITY0_CHK, false);
						_r_ctrl_enable (hwnd, IDC_STANDBYLIST_CHK, false);
						_r_ctrl_enable (hwnd, IDC_MODIFIEDLIST_CHK, false);

						if (!app.IsAdmin ())
						{
							_r_ctrl_enable (hwnd, IDC_SYSTEMWORKINGSET_CHK, false);
							_r_ctrl_enable (hwnd, IDC_AUTOREDUCTENABLE_CHK, false);
							_r_ctrl_enable (hwnd, IDC_AUTOREDUCTINTERVALENABLE_CHK, false);
							_r_ctrl_enable (hwnd, IDC_HOTKEY_CLEAN_CHK, false);
						}
					}

					DWORD mask = app.ConfigGet (L"ReductMask", MASK_DEFAULT).AsUlong ();

					CheckDlgButton (hwnd, IDC_WORKINGSET_CHK, ((mask & REDUCT_WORKING_SET) != 0) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_SYSTEMWORKINGSET_CHK, ((mask & REDUCT_SYSTEM_WORKING_SET) != 0) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_STANDBYLISTPRIORITY0_CHK, ((mask & REDUCT_STANDBY_PRIORITY0_LIST) != 0) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_STANDBYLIST_CHK, ((mask & REDUCT_STANDBY_LIST) != 0) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_MODIFIEDLIST_CHK, ((mask & REDUCT_MODIFIED_LIST) != 0) ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_AUTOREDUCTENABLE_CHK, app.ConfigGet (L"AutoreductEnable", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					SendDlgItemMessage (hwnd, IDC_AUTOREDUCTVALUE, UDM_SETRANGE32, 10, 99);
					SendDlgItemMessage (hwnd, IDC_AUTOREDUCTVALUE, UDM_SETPOS32, 0, app.ConfigGet (L"AutoreductValue", 90).AsUint ());

					CheckDlgButton (hwnd, IDC_AUTOREDUCTINTERVALENABLE_CHK, app.ConfigGet (L"AutoreductIntervalEnable", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					SendDlgItemMessage (hwnd, IDC_AUTOREDUCTINTERVALVALUE, UDM_SETRANGE32, 5, 1440);
					SendDlgItemMessage (hwnd, IDC_AUTOREDUCTINTERVALVALUE, UDM_SETPOS32, 0, app.ConfigGet (L"AutoreductIntervalValue", 30).AsUint ());

					CheckDlgButton (hwnd, IDC_HOTKEY_CLEAN_CHK, app.ConfigGet (L"HotkeyCleanEnable", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					SendDlgItemMessage (hwnd, IDC_HOTKEY_CLEAN, HKM_SETHOTKEY, app.ConfigGet (L"HotkeyClean", MAKEWORD (VK_F1, HOTKEYF_CONTROL)).AsUint (), 0);

					SendMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_AUTOREDUCTENABLE_CHK, 0), 0);
					SendMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_AUTOREDUCTINTERVALENABLE_CHK, 0), 0);
					SendMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_HOTKEY_CLEAN_CHK, 0), 0);

					break;
				}

				case IDD_SETTINGS_3:
				{
					// localize
					SetDlgItemText (hwnd, IDC_TITLE_1, I18N (&app, IDS_TITLE_1, 0));
					SetDlgItemText (hwnd, IDC_TITLE_6, I18N (&app, IDS_TITLE_6, 0));

					SetDlgItemText (hwnd, IDC_TRAYUSETRANSPARENCY_CHK, I18N (&app, IDS_TRAYUSETRANSPARENCY_CHK, 0));
					SetDlgItemText (hwnd, IDC_TRAYSHOWBORDER_CHK, I18N (&app, IDS_TRAYSHOWBORDER_CHK, 0));
					SetDlgItemText (hwnd, IDC_TRAYROUNDCORNERS_CHK, I18N (&app, IDS_TRAYROUNDCORNERS_CHK, 0));
					SetDlgItemText (hwnd, IDC_TRAYCHANGEBG_CHK, I18N (&app, IDS_TRAYCHANGEBG_CHK, 0));
					SetDlgItemText (hwnd, IDC_TRAYUSEANTIALIASING_CHK, I18N (&app, IDS_TRAYUSEANTIALIASING_CHK, 0));

					SetDlgItemText (hwnd, IDC_FONT_HINT, I18N (&app, IDS_FONT_HINT, 0));
					SetDlgItemText (hwnd, IDC_COLOR_TEXT_HINT, I18N (&app, IDS_COLOR_TEXT_HINT, 0));
					SetDlgItemText (hwnd, IDC_COLOR_BACKGROUND_HINT, I18N (&app, IDS_COLOR_BACKGROUND_HINT, 0));
					SetDlgItemText (hwnd, IDC_COLOR_WARNING_HINT, I18N (&app, IDS_COLOR_WARNING_HINT, 0));
					SetDlgItemText (hwnd, IDC_COLOR_DANGER_HINT, I18N (&app, IDS_COLOR_DANGER_HINT, 0));

					// set checks
					CheckDlgButton (hwnd, IDC_TRAYUSETRANSPARENCY_CHK, app.ConfigGet (L"TrayUseTransparency", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_TRAYSHOWBORDER_CHK, app.ConfigGet (L"TrayShowBorder", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_TRAYROUNDCORNERS_CHK, app.ConfigGet (L"TrayRoundCorners", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_TRAYCHANGEBG_CHK, app.ConfigGet (L"TrayChangeBg", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_TRAYUSEANTIALIASING_CHK, app.ConfigGet (L"TrayUseAntialiasing", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					_r_ctrl_settext (hwnd, IDC_FONT, L"%s, %dpx", app.ConfigGet (L"TrayFontName", FONT_NAME), app.ConfigGet (L"TrayFontSize", FONT_SIZE).AsInt ());

					SetWindowLongPtr (GetDlgItem (hwnd, IDC_COLOR_TEXT), GWLP_USERDATA, app.ConfigGet (L"TrayColorText", TRAY_COLOR_TEXT).AsUlong ());
					SetWindowLongPtr (GetDlgItem (hwnd, IDC_COLOR_BACKGROUND), GWLP_USERDATA, app.ConfigGet (L"TrayColorBg", TRAY_COLOR_BG).AsUlong ());
					SetWindowLongPtr (GetDlgItem (hwnd, IDC_COLOR_WARNING), GWLP_USERDATA, app.ConfigGet (L"TrayColorWarning", TRAY_COLOR_WARNING).AsUlong ());
					SetWindowLongPtr (GetDlgItem (hwnd, IDC_COLOR_DANGER), GWLP_USERDATA, app.ConfigGet (L"TrayColorDanger", TRAY_COLOR_DANGER).AsUlong ());

					_r_wnd_addstyle (hwnd, IDC_FONT, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

					_r_wnd_addstyle (hwnd, IDC_COLOR_TEXT, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
					_r_wnd_addstyle (hwnd, IDC_COLOR_BACKGROUND, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
					_r_wnd_addstyle (hwnd, IDC_COLOR_WARNING, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
					_r_wnd_addstyle (hwnd, IDC_COLOR_DANGER, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

					break;
				}

				case IDD_SETTINGS_4:
				{
					// localize
					SetDlgItemText (hwnd, IDC_TITLE_7, I18N (&app, IDS_TITLE_7, 0));
					SetDlgItemText (hwnd, IDC_TITLE_8, I18N (&app, IDS_TITLE_8, 0));
					SetDlgItemText (hwnd, IDC_TITLE_9, I18N (&app, IDS_TITLE_9, 0));

					SetDlgItemText (hwnd, IDC_TRAYLEVELWARNING_HINT, I18N (&app, IDS_TRAYLEVELWARNING_HINT, 0));
					SetDlgItemText (hwnd, IDC_TRAYLEVELDANGER_HINT, I18N (&app, IDS_TRAYLEVELDANGER_HINT, 0));

					SetDlgItemText (hwnd, IDC_TRAYACTIONDC_HINT, I18N (&app, IDS_TRAYACTIONDC_HINT, 0));
					SetDlgItemText (hwnd, IDC_TRAYACTIONMC_HINT, I18N (&app, IDS_TRAYACTIONMC_HINT, 0));

					SetDlgItemText (hwnd, IDC_SHOW_CLEAN_RESULT_CHK, I18N (&app, IDS_SHOW_CLEAN_RESULT_CHK, 0));

					// set checks
					SendDlgItemMessage (hwnd, IDC_TRAYLEVELWARNING, UDM_SETRANGE32, 10, 99);
					SendDlgItemMessage (hwnd, IDC_TRAYLEVELWARNING, UDM_SETPOS32, 0, app.ConfigGet (L"TrayLevelWarning", 60).AsUint ());

					SendDlgItemMessage (hwnd, IDC_TRAYLEVELDANGER, UDM_SETRANGE32, 10, 99);
					SendDlgItemMessage (hwnd, IDC_TRAYLEVELDANGER, UDM_SETPOS32, 0, app.ConfigGet (L"TrayLevelDanger", 90).AsUint ());

					SendDlgItemMessage (hwnd, IDC_TRAYACTIONMC, CB_RESETCONTENT, 0, 0);
					SendDlgItemMessage (hwnd, IDC_TRAYACTIONDC, CB_RESETCONTENT, 0, 0);

					for (INT i = 0; i < 3; i++)
					{
						rstring item = I18N (&app, IDS_TRAY_ACTION_1 + i, _r_fmt (L"IDS_TRAY_ACTION_%d", i + 1));

						SendDlgItemMessage (hwnd, IDC_TRAYACTIONMC, CB_INSERTSTRING, i, (LPARAM)item.GetString ());
						SendDlgItemMessage (hwnd, IDC_TRAYACTIONDC, CB_INSERTSTRING, i, (LPARAM)item.GetString ());
					}

					SendDlgItemMessage (hwnd, IDC_TRAYACTIONDC, CB_SETCURSEL, app.ConfigGet (L"TrayActionDc", 0).AsUint (), 0);
					SendDlgItemMessage (hwnd, IDC_TRAYACTIONMC, CB_SETCURSEL, app.ConfigGet (L"TrayActionMc", 1).AsUint (), 0);

					CheckDlgButton (hwnd, IDC_SHOW_CLEAN_RESULT_CHK, app.ConfigGet (L"BalloonCleanResults", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					break;
				}
			}

			break;
		}

		case _RM_SAVE:
		{
			switch (page->dlg_id)
			{
				case IDD_SETTINGS_1:
				{
					app.ConfigSet (L"AlwaysOnTop", (IsDlgButtonChecked (hwnd, IDC_ALWAYSONTOP_CHK) == BST_CHECKED) ? true : false);

#ifdef _APP_HAVE_AUTORUN
					app.AutorunEnable (IsDlgButtonChecked (hwnd, IDC_LOADONSTARTUP_CHK) == BST_CHECKED);
#endif // _APP_HAVE_AUTORUN

					app.ConfigSet (L"ReductConfirmation", (IsDlgButtonChecked (hwnd, IDC_REDUCTCONFIRMATION_CHK) == BST_CHECKED) ? true : false);

#ifdef _APP_HAVE_SKIPUAC
					if (!_r_sys_uacstate ())
					{
						app.SkipUacEnable (IsDlgButtonChecked (hwnd, IDC_SKIPUACWARNING_CHK) == BST_CHECKED);
					}
#endif // _APP_HAVE_SKIPUAC

					app.ConfigSet (L"CheckUpdates", (IsDlgButtonChecked (hwnd, IDC_CHECKUPDATES_CHK) == BST_CHECKED) ? true : false);

					// set language
					rstring buffer;

					if (SendDlgItemMessage (hwnd, IDC_LANGUAGE, CB_GETCURSEL, 0, 0) >= 1)
					{
						buffer = _r_ctrl_gettext (hwnd, IDC_LANGUAGE);
					}

					app.ConfigSet (L"Language", buffer);

					if (GetWindowLongPtr (hwnd, GWLP_USERDATA) != (INT)SendDlgItemMessage (hwnd, IDC_LANGUAGE, CB_GETCURSEL, 0, 0))
					{
						return TRUE; // for restart
					}

					break;
				}

				case IDD_SETTINGS_2:
				{
					DWORD mask = 0;

					if (IsDlgButtonChecked (hwnd, IDC_WORKINGSET_CHK) == BST_CHECKED)
						mask |= REDUCT_WORKING_SET;

					if (IsDlgButtonChecked (hwnd, IDC_SYSTEMWORKINGSET_CHK) == BST_CHECKED)
						mask |= REDUCT_SYSTEM_WORKING_SET;

					if (IsDlgButtonChecked (hwnd, IDC_STANDBYLISTPRIORITY0_CHK) == BST_CHECKED)
						mask |= REDUCT_STANDBY_PRIORITY0_LIST;

					if (IsDlgButtonChecked (hwnd, IDC_STANDBYLIST_CHK) == BST_CHECKED)
						mask |= REDUCT_STANDBY_LIST;

					if (IsDlgButtonChecked (hwnd, IDC_MODIFIEDLIST_CHK) == BST_CHECKED)
						mask |= REDUCT_MODIFIED_LIST;

					app.ConfigSet (L"ReductMask", mask);

					app.ConfigSet (L"AutoreductEnable", (IsDlgButtonChecked (hwnd, IDC_AUTOREDUCTENABLE_CHK) == BST_CHECKED) ? true : false);
					app.ConfigSet (L"AutoreductValue", (DWORD)SendDlgItemMessage (hwnd, IDC_AUTOREDUCTVALUE, UDM_GETPOS32, 0, 0));

					app.ConfigSet (L"AutoreductIntervalEnable", (IsDlgButtonChecked (hwnd, IDC_AUTOREDUCTINTERVALENABLE_CHK) == BST_CHECKED) ? true : false);
					app.ConfigSet (L"AutoreductIntervalValue", (DWORD)SendDlgItemMessage (hwnd, IDC_AUTOREDUCTINTERVALVALUE, UDM_GETPOS32, 0, 0));

					app.ConfigSet (L"HotkeyCleanEnable", (IsDlgButtonChecked (hwnd, IDC_HOTKEY_CLEAN_CHK) == BST_CHECKED) ? true : false);
					app.ConfigSet (L"HotkeyClean", (DWORD)SendDlgItemMessage (hwnd, IDC_HOTKEY_CLEAN, HKM_GETHOTKEY, 0, 0));

					break;
				}

				case IDD_SETTINGS_3:
				{
					app.ConfigSet (L"TrayUseTransparency", (IsDlgButtonChecked (hwnd, IDC_TRAYUSETRANSPARENCY_CHK) == BST_CHECKED) ? true : false);
					app.ConfigSet (L"TrayShowBorder", (IsDlgButtonChecked (hwnd, IDC_TRAYSHOWBORDER_CHK) == BST_CHECKED) ? true : false);
					app.ConfigSet (L"TrayRoundCorners", (IsDlgButtonChecked (hwnd, IDC_TRAYROUNDCORNERS_CHK) == BST_CHECKED) ? true : false);
					app.ConfigSet (L"TrayChangeBg", (IsDlgButtonChecked (hwnd, IDC_TRAYCHANGEBG_CHK) == BST_CHECKED) ? true : false);
					app.ConfigSet (L"TrayUseAntialiasing", (IsDlgButtonChecked (hwnd, IDC_TRAYUSEANTIALIASING_CHK) == BST_CHECKED) ? true : false);

					app.ConfigSet (L"TrayColorText", (DWORD)GetWindowLongPtr (GetDlgItem (hwnd, IDC_COLOR_TEXT), GWLP_USERDATA));
					app.ConfigSet (L"TrayColorBg", (DWORD)GetWindowLongPtr (GetDlgItem (hwnd, IDC_COLOR_BACKGROUND), GWLP_USERDATA));
					app.ConfigSet (L"TrayColorWarning", (DWORD)GetWindowLongPtr (GetDlgItem (hwnd, IDC_COLOR_WARNING), GWLP_USERDATA));
					app.ConfigSet (L"TrayColorDanger", (DWORD)GetWindowLongPtr (GetDlgItem (hwnd, IDC_COLOR_DANGER), GWLP_USERDATA));

					break;
				}

				case IDD_SETTINGS_4:
				{
					app.ConfigSet (L"TrayActionDc", (DWORD)SendDlgItemMessage (hwnd, IDC_TRAYACTIONDC, CB_GETCURSEL, 0, 0));
					app.ConfigSet (L"TrayActionMc", (DWORD)SendDlgItemMessage (hwnd, IDC_TRAYACTIONMC, CB_GETCURSEL, 0, 0));

					app.ConfigSet (L"TrayLevelWarning", (DWORD)SendDlgItemMessage (hwnd, IDC_TRAYLEVELWARNING, UDM_GETPOS32, 0, 0));
					app.ConfigSet (L"TrayLevelDanger", (DWORD)SendDlgItemMessage (hwnd, IDC_TRAYLEVELDANGER, UDM_GETPOS32, 0, 0));

					app.ConfigSet (L"BalloonCleanResults", (IsDlgButtonChecked (hwnd, IDC_SHOW_CLEAN_RESULT_CHK) == BST_CHECKED) ? true : false);

					break;
				}
			}

			break;
		}

		case _RM_MESSAGE:
		{
			LPMSG pmsg = (LPMSG)lpdata1;

			switch (pmsg->message)
			{
				case WM_NOTIFY:
				{
					LPNMHDR nmlp = (LPNMHDR)pmsg->lParam;

					switch (nmlp->code)
					{
						case NM_CUSTOMDRAW:
						{
							LPNMCUSTOMDRAW lpnmcd = (LPNMCUSTOMDRAW)pmsg->lParam;

							if (nmlp->idFrom == IDC_COLOR_TEXT ||
								nmlp->idFrom == IDC_COLOR_BACKGROUND ||
								nmlp->idFrom == IDC_COLOR_WARNING ||
								nmlp->idFrom == IDC_COLOR_DANGER
								)
							{
								lpnmcd->rc.left += 3;
								lpnmcd->rc.top += 3;
								lpnmcd->rc.right -= 3;
								lpnmcd->rc.bottom -= 3;

								COLORREF clr_prev = SetBkColor (lpnmcd->hdc, static_cast<COLORREF>(GetWindowLongPtr (nmlp->hwndFrom, GWLP_USERDATA)));
								ExtTextOut (lpnmcd->hdc, 0, 0, ETO_OPAQUE, &lpnmcd->rc, nullptr, 0, nullptr);
								SetBkColor (lpnmcd->hdc, clr_prev);

								SetWindowLongPtr (hwnd, DWLP_MSGRESULT, CDRF_DODEFAULT | CDRF_DOERASE);
								return CDRF_DODEFAULT | CDRF_DOERASE;
							}

							break;
						}
					}

					break;
				}

				case WM_COMMAND:
				{
					switch (LOWORD (pmsg->wParam))
					{
						case IDC_AUTOREDUCTENABLE_CHK:
						case IDC_AUTOREDUCTINTERVALENABLE_CHK:
						{
							const UINT ctrl1 = LOWORD (pmsg->wParam);
							const UINT ctrl2 = LOWORD (pmsg->wParam) + 1;

							const bool is_enabled = IsWindowEnabled (GetDlgItem (hwnd, ctrl1)) && (IsDlgButtonChecked (hwnd, ctrl1) == BST_CHECKED);

							_r_ctrl_enable (hwnd, ctrl2, is_enabled);
							EnableWindow ((HWND)SendDlgItemMessage (hwnd, ctrl2, UDM_GETBUDDY, 0, 0), is_enabled);

							break;
						}

						case IDC_HOTKEY_CLEAN_CHK:
						{
							const bool is_enabled = IsWindowEnabled (GetDlgItem (hwnd, IDC_HOTKEY_CLEAN_CHK)) && (IsDlgButtonChecked (hwnd, IDC_HOTKEY_CLEAN_CHK) == BST_CHECKED);

							_r_ctrl_enable (hwnd, IDC_HOTKEY_CLEAN, is_enabled);

							break;
						}

						case IDC_COLOR_TEXT:
						case IDC_COLOR_BACKGROUND:
						case IDC_COLOR_WARNING:
						case IDC_COLOR_DANGER:
						{
							CHOOSECOLOR cc = {0};
							COLORREF cust[16] = {TRAY_COLOR_TEXT, TRAY_COLOR_BG, TRAY_COLOR_WARNING, TRAY_COLOR_DANGER};

							HWND hctrl = GetDlgItem (hwnd, LOWORD (pmsg->wParam));

							cc.lStructSize = sizeof (cc);
							cc.Flags = CC_RGBINIT | CC_FULLOPEN;
							cc.hwndOwner = hwnd;
							cc.lpCustColors = cust;
							cc.rgbResult = static_cast<COLORREF>(GetWindowLongPtr (hctrl, GWLP_USERDATA));

							if (ChooseColor (&cc))
								SetWindowLongPtr (hctrl, GWLP_USERDATA, cc.rgbResult);

							break;
						}

						case IDC_FONT:
						{
							CHOOSEFONT cf = {0};

							cf.lStructSize = sizeof (cf);
							cf.hwndOwner = hwnd;
							cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL;

							config.lf.lfHeight /= config.scale; // font size fix
							cf.lpLogFont = &config.lf;

							if (ChooseFont (&cf))
							{
								app.ConfigSet (L"TrayFontName", config.lf.lfFaceName);
								app.ConfigSet (L"TrayFontSize", (DWORD)MulDiv (-config.lf.lfHeight, 72, GetDeviceCaps (config.cdc1, LOGPIXELSY)));
								app.ConfigSet (L"TrayFontWeight", (DWORD)config.lf.lfWeight);

								_r_ctrl_settext (hwnd, IDC_FONT, L"%s, %dpx", app.ConfigGet (L"TrayFontName", FONT_NAME), app.ConfigGet (L"TrayFontSize", FONT_SIZE).AsInt ());

								initializer_callback (app.GetHWND (), _RM_INITIALIZE, nullptr, nullptr);
							}

							break;
						}
					}

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

INT_PTR CALLBACK DlgProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			// set privileges
			if (app.IsAdmin ())
			{
				_r_sys_setprivilege (SE_INCREASE_QUOTA_NAME, true);
				_r_sys_setprivilege (SE_PROF_SINGLE_PROCESS_NAME, true);
			}

			// uac indicator (windows vista and above)
			if (_r_sys_uacstate ())
			{
				RECT rc = {0};

				SendDlgItemMessage (hwnd, IDC_CLEAN, BCM_SETSHIELD, 0, TRUE);

				SendDlgItemMessage (hwnd, IDC_CLEAN, BCM_GETTEXTMARGIN, 0, (LPARAM)&rc);
				rc.left += GetSystemMetrics (SM_CXSMICON) / 2;
				SendDlgItemMessage (hwnd, IDC_CLEAN, BCM_SETTEXTMARGIN, 0, (LPARAM)&rc);
			}

			// configure listview
			_r_listview_setstyle (hwnd, IDC_LISTVIEW, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP);

			_r_listview_addcolumn (hwnd, IDC_LISTVIEW, nullptr, 50, 1, LVCFMT_RIGHT);
			_r_listview_addcolumn (hwnd, IDC_LISTVIEW, nullptr, 50, 2, LVCFMT_LEFT);

			// settings
			app.AddSettingsPage (nullptr, IDD_SETTINGS_1, IDS_SETTINGS_1, L"IDS_SETTINGS_1", &settings_callback);
			app.AddSettingsPage (nullptr, IDD_SETTINGS_2, IDS_SETTINGS_2, L"IDS_SETTINGS_2", &settings_callback);
			app.AddSettingsPage (nullptr, IDD_SETTINGS_3, IDS_SETTINGS_3, L"IDS_SETTINGS_3", &settings_callback);
			app.AddSettingsPage (nullptr, IDD_SETTINGS_4, IDS_SETTINGS_4, L"IDS_SETTINGS_4", &settings_callback);

			break;
		}

		case WM_DESTROY:
		{
			PostQuitMessage (0);
			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			HDC dc = BeginPaint (hwnd, &ps);

			RECT rc = {0};
			GetClientRect (hwnd, &rc);

			static INT height = app.GetDPI (48);

			rc.top = rc.bottom - height;
			rc.bottom = rc.top + height;

			COLORREF clr_prev = SetBkColor (dc, GetSysColor (COLOR_3DFACE));
			ExtTextOut (dc, 0, 0, ETO_OPAQUE, &rc, nullptr, 0, nullptr);
			SetBkColor (dc, clr_prev);

			for (INT i = 0; i < rc.right; i++)
			{
				SetPixel (dc, i, rc.top, RGB (223, 223, 223));
			}

			EndPaint (hwnd, &ps);

			break;
		}

		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORDLG:
		{
			return (INT_PTR)GetSysColorBrush (COLOR_WINDOW);
		}

		case WM_HOTKEY:
		{
			if (wparam == UID)
				_app_clean (nullptr);

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			if (nmlp->idFrom == IDC_LISTVIEW)
			{
				switch (nmlp->code)
				{
					case NM_CUSTOMDRAW:
					{
						LONG result = CDRF_DODEFAULT;
						LPNMLVCUSTOMDRAW lpnmlv = (LPNMLVCUSTOMDRAW)lparam;

						switch (lpnmlv->nmcd.dwDrawStage)
						{
							case CDDS_PREPAINT:
							{
								result = (CDRF_NOTIFYPOSTPAINT | CDRF_NOTIFYITEMDRAW);
								break;
							}

							case CDDS_ITEMPREPAINT:
							{
								if ((UINT)lpnmlv->nmcd.lItemlParam >= app.ConfigGet (L"TrayLevelDanger", 90).AsUint ())
								{
									lpnmlv->clrText = app.ConfigGet (L"TrayColorDanger", TRAY_COLOR_DANGER).AsUlong ();
									result = (CDRF_NOTIFYPOSTPAINT | CDRF_NEWFONT);
								}
								else if ((UINT)lpnmlv->nmcd.lItemlParam >= app.ConfigGet (L"TrayLevelWarning", 60).AsUint ())
								{
									lpnmlv->clrText = app.ConfigGet (L"TrayColorWarning", TRAY_COLOR_WARNING).AsUlong ();
									result = (CDRF_NOTIFYPOSTPAINT | CDRF_NEWFONT);
								}

								break;
							}
						}

						SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result);
						return TRUE;
					}
				}
			}

			break;
		}

		case WM_TRAYICON:
		{
			switch (LOWORD (lparam))
			{
				case WM_LBUTTONUP:
				{
					SetForegroundWindow (hwnd);
					break;
				}

				case WM_LBUTTONDBLCLK:
				case WM_MBUTTONDOWN:
				{
					const INT action = (LOWORD (lparam) == WM_LBUTTONDBLCLK) ? app.ConfigGet (L"TrayActionDc", 0).AsInt () : app.ConfigGet (L"TrayActionMc", 1).AsInt ();

					switch (action)
					{
						case 1:
						{
							_app_clean (nullptr);
							break;
						}

						case 2:
						{
							_r_run (nullptr, L"taskmgr.exe");
							break;
						}

						default:
						{
							_r_wnd_toggle (hwnd, false);
							break;
						}
					}

					SetForegroundWindow (hwnd);

					break;
				}

				case WM_RBUTTONUP:
				{
					SetForegroundWindow (hwnd); // don't touch

#define SUBMENU1 3
#define SUBMENU2 4
#define SUBMENU3 5

					HMENU menu = LoadMenu (nullptr, MAKEINTRESOURCE (IDM_TRAY)), submenu = GetSubMenu (menu, 0);

					HMENU submenu1 = GetSubMenu (submenu, SUBMENU1);
					HMENU submenu2 = GetSubMenu (submenu, SUBMENU2);
					HMENU submenu3 = GetSubMenu (submenu, SUBMENU3);

					// localize
					app.LocaleMenu (submenu, I18N (&app, IDS_TRAY_SHOW, 0), IDM_TRAY_SHOW, false);
					app.LocaleMenu (submenu, I18N (&app, IDS_CLEAN, 0), IDM_TRAY_CLEAN, false);
					app.LocaleMenu (submenu, I18N (&app, IDS_TRAY_POPUP_1, 0), SUBMENU1, true);
					app.LocaleMenu (submenu, I18N (&app, IDS_TRAY_POPUP_2, 0), SUBMENU2, true);
					app.LocaleMenu (submenu, I18N (&app, IDS_TRAY_POPUP_3, 0), SUBMENU3, true);
					app.LocaleMenu (submenu, I18N (&app, IDS_SETTINGS, 0), IDM_TRAY_SETTINGS, false);
					app.LocaleMenu (submenu, I18N (&app, IDS_WEBSITE, 0), IDM_TRAY_WEBSITE, false);
					app.LocaleMenu (submenu, I18N (&app, IDS_ABOUT, 0), IDM_TRAY_ABOUT, false);
					app.LocaleMenu (submenu, I18N (&app, IDS_EXIT, 0), IDM_TRAY_EXIT, false);

					app.LocaleMenu (submenu1, I18N (&app, IDS_WORKINGSET_CHK, 0), IDM_WORKINGSET_CHK, false);
					app.LocaleMenu (submenu1, I18N (&app, IDS_SYSTEMWORKINGSET_CHK, 0), IDM_SYSTEMWORKINGSET_CHK, false);
					app.LocaleMenu (submenu1, I18N (&app, IDS_STANDBYLISTPRIORITY0_CHK, 0), IDM_STANDBYLISTPRIORITY0_CHK, false);
					app.LocaleMenu (submenu1, I18N (&app, IDS_STANDBYLIST_CHK, 0), IDM_STANDBYLIST_CHK, false);
					app.LocaleMenu (submenu1, I18N (&app, IDS_MODIFIEDLIST_CHK, 0), IDM_MODIFIEDLIST_CHK, false);

					app.LocaleMenu (submenu2, I18N (&app, IDS_TRAY_DISABLE, 0), IDM_TRAY_DISABLE_1, false);
					app.LocaleMenu (submenu3, I18N (&app, IDS_TRAY_DISABLE, 0), IDM_TRAY_DISABLE_2, false);

					// configure submenu #1
					DWORD mask = app.ConfigGet (L"ReductMask", MASK_DEFAULT).AsUlong ();

					if ((mask & REDUCT_WORKING_SET) != 0)
					{
						CheckMenuItem (submenu1, IDM_WORKINGSET_CHK, MF_BYCOMMAND | MF_CHECKED);
					}
					if ((mask & REDUCT_SYSTEM_WORKING_SET) != 0)
					{
						CheckMenuItem (submenu1, IDM_SYSTEMWORKINGSET_CHK, MF_BYCOMMAND | MF_CHECKED);
					}
					if ((mask & REDUCT_STANDBY_PRIORITY0_LIST) != 0)
					{
						CheckMenuItem (submenu1, IDM_STANDBYLISTPRIORITY0_CHK, MF_BYCOMMAND | MF_CHECKED);
					}
					if ((mask & REDUCT_STANDBY_LIST) != 0)
					{
						CheckMenuItem (submenu1, IDM_STANDBYLIST_CHK, MF_BYCOMMAND | MF_CHECKED);
					}
					if ((mask & REDUCT_MODIFIED_LIST) != 0)
					{
						CheckMenuItem (submenu1, IDM_MODIFIEDLIST_CHK, MF_BYCOMMAND | MF_CHECKED);
					}

					if (!app.IsVistaOrLater () || !app.IsAdmin ())
					{
						EnableMenuItem (submenu1, IDM_WORKINGSET_CHK, MF_BYCOMMAND | MF_DISABLED);
						EnableMenuItem (submenu1, IDM_STANDBYLISTPRIORITY0_CHK, MF_BYCOMMAND | MF_DISABLED);
						EnableMenuItem (submenu1, IDM_STANDBYLIST_CHK, MF_BYCOMMAND | MF_DISABLED);
						EnableMenuItem (submenu1, IDM_MODIFIEDLIST_CHK, MF_BYCOMMAND | MF_DISABLED);

						if (!app.IsAdmin ())
						{
							EnableMenuItem (submenu1, IDM_SYSTEMWORKINGSET_CHK, MF_BYCOMMAND | MF_DISABLED);
						}
					}

					// configure submenu #2
					generate_menu_array (app.ConfigGet (L"AutoreductValue", 90).AsUint (), limit_vec);

					for (size_t i = 0; i < limit_vec.size (); i++)
					{
						AppendMenu (submenu2, MF_STRING, IDM_TRAY_POPUP_1 + i, _r_fmt (L"%d%%", limit_vec.at (i)));

						if (!app.IsAdmin ())
						{
							EnableMenuItem (submenu2, static_cast<UINT>(IDM_TRAY_POPUP_1 + i), MF_BYCOMMAND | MF_DISABLED);
						}

						if (app.ConfigGet (L"AutoreductValue", 90).AsSizeT () == limit_vec.at (i))
						{
							CheckMenuRadioItem (submenu2, 0, static_cast<UINT>(limit_vec.size ()), static_cast<UINT>(i) + 2, MF_BYPOSITION);
						}
					}

					if (!app.ConfigGet (L"AutoreductEnable", true).AsBool ())
					{
						CheckMenuRadioItem (submenu2, 0, static_cast<UINT>(limit_vec.size ()), 0, MF_BYPOSITION);
					}

					// configure submenu #3
					generate_menu_array (app.ConfigGet (L"AutoreductIntervalValue", 30).AsUint (), interval_vec);

					for (size_t i = 0; i < interval_vec.size (); i++)
					{
						AppendMenu (submenu3, MF_STRING, IDM_TRAY_POPUP_2 + i, _r_fmt (L"%d min.", interval_vec.at (i)));

						if (!app.IsAdmin ())
						{
							EnableMenuItem (submenu3, static_cast<UINT>(IDM_TRAY_POPUP_2 + i), MF_BYCOMMAND | MF_DISABLED);
						}

						if (app.ConfigGet (L"AutoreductIntervalValue", 30).AsSizeT () == interval_vec.at (i))
						{
							CheckMenuRadioItem (submenu3, 0, static_cast<UINT>(interval_vec.size ()), static_cast<UINT>(i) + 2, MF_BYPOSITION);
						}
					}

					if (!app.ConfigGet (L"AutoreductIntervalEnable", false).AsBool ())
					{
						CheckMenuRadioItem (submenu3, 0, static_cast<UINT>(interval_vec.size ()), 0, MF_BYPOSITION);
					}

					POINT pt = {0};
					GetCursorPos (&pt);

					TrackPopupMenuEx (submenu, TPM_RIGHTBUTTON | TPM_LEFTBUTTON, pt.x, pt.y, hwnd, nullptr);

					DestroyMenu (submenu1);
					DestroyMenu (submenu2);
					DestroyMenu (submenu3);

					DestroyMenu (menu);
					DestroyMenu (submenu);

					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			if ((LOWORD (wparam) >= IDM_TRAY_POPUP_1 && LOWORD (wparam) <= IDM_TRAY_POPUP_1 + limit_vec.size ()))
			{
				app.ConfigSet (L"AutoreductEnable", true);
				app.ConfigSet (L"AutoreductValue", (DWORD)limit_vec.at (LOWORD (wparam) - IDM_TRAY_POPUP_1));

				break;
			}
			else if ((LOWORD (wparam) >= IDM_TRAY_POPUP_2 && LOWORD (wparam) <= IDM_TRAY_POPUP_2 + interval_vec.size ()))
			{
				app.ConfigSet (L"AutoreductIntervalEnable", true);
				app.ConfigSet (L"AutoreductIntervalValue", (DWORD)interval_vec.at (LOWORD (wparam) - IDM_TRAY_POPUP_2));

				break;
			}

			switch (LOWORD (wparam))
			{
				case IDM_WORKINGSET_CHK:
				case IDM_SYSTEMWORKINGSET_CHK:
				case IDM_STANDBYLISTPRIORITY0_CHK:
				case IDM_STANDBYLIST_CHK:
				case IDM_MODIFIEDLIST_CHK:
				{
					const DWORD mask = app.ConfigGet (L"ReductMask", MASK_DEFAULT).AsUlong ();
					DWORD new_flag = 0;

					if ((LOWORD (wparam)) == IDM_WORKINGSET_CHK)
					{
						new_flag = REDUCT_WORKING_SET;
					}
					else if ((LOWORD (wparam)) == IDM_SYSTEMWORKINGSET_CHK)
					{
						new_flag = REDUCT_SYSTEM_WORKING_SET;
					}
					else if ((LOWORD (wparam)) == IDM_STANDBYLISTPRIORITY0_CHK)
					{
						new_flag = REDUCT_STANDBY_PRIORITY0_LIST;
					}
					else if ((LOWORD (wparam)) == IDM_STANDBYLIST_CHK)
					{
						new_flag = REDUCT_STANDBY_LIST;
					}
					else if ((LOWORD (wparam)) == IDM_MODIFIEDLIST_CHK)
					{
						new_flag = REDUCT_MODIFIED_LIST;
					}

					app.ConfigSet (L"ReductMask", (mask & new_flag) != 0 ? (mask & ~new_flag) : (mask | new_flag));

					break;
				}

				case IDM_TRAY_DISABLE_1:
				{
					app.ConfigSet (L"AutoreductEnable", !app.ConfigGet (L"AutoreductEnable", true).AsBool ());
					break;
				}

				case IDM_TRAY_DISABLE_2:
				{
					app.ConfigSet (L"AutoreductIntervalEnable", !app.ConfigGet (L"AutoreductIntervalEnable", false).AsBool ());
					break;
				}

				case IDM_SETTINGS:
				case IDM_TRAY_SETTINGS:
				{
					app.CreateSettingsWindow ();
					break;
				}

				case IDM_EXIT:
				case IDM_TRAY_EXIT:
				{
					DestroyWindow (hwnd);
					break;
				}

				case IDCANCEL: // process Esc key
				case IDM_TRAY_SHOW:
				{
					_r_wnd_toggle (hwnd, false);
					break;
				}

				case IDOK: // process Enter key
				case IDC_CLEAN:
				case IDM_TRAY_CLEAN:
				{
					SetProp (hwnd, L"is_reduct_opened", (HANDLE)TRUE);

					if (!app.IsAdmin ())
					{
						if (app.RunAsAdmin ())
							DestroyWindow (hwnd);

						app.TrayPopup (NIIF_ERROR, APP_NAME, I18N (&app, IDS_STATUS_NOPRIVILEGES, 0));
					}
					else
					{
						_app_clean (hwnd);
					}

					SetProp (hwnd, L"is_reduct_opened", FALSE);

					break;
				}

				case IDM_WEBSITE:
				case IDM_TRAY_WEBSITE:
				{
					ShellExecute (hwnd, nullptr, _APP_WEBSITE_URL, nullptr, nullptr, SW_SHOWDEFAULT);
					break;
				}

				case IDM_DONATE:
				{
					app.CreateDonateWindow ();
					break;
				}

				case IDM_CHECKUPDATES:
				{
					app.CheckForUpdates (false);
					break;
				}

				case IDM_ABOUT:
				case IDM_TRAY_ABOUT:
				{
					app.CreateAboutWindow ();
					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

INT APIENTRY wWinMain (HINSTANCE, HINSTANCE, LPWSTR, INT)
{
	if (app.CreateMainWindow (&DlgProc, &initializer_callback))
	{
		MSG msg = {0};

		while (GetMessage (&msg, nullptr, 0, 0) > 0)
		{
			if (!IsDialogMessage (app.GetHWND (), &msg))
			{
				TranslateMessage (&msg);
				DispatchMessage (&msg);
			}
		}
	}

	return ERROR_SUCCESS;
}
