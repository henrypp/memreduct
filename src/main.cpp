// Mem Reduct
// Copyright (c) 2011-2018 Henry++

#include <windows.h>
#include <subauth.h>
#include <algorithm>

#include "main.hpp"
#include "rapp.hpp"
#include "routine.hpp"

#include "resource.hpp"

STATIC_DATA config;

RECT icon_rc;
MEMORYINFO meminfo;

rapp app (APP_NAME, APP_NAME_SHORT, APP_VERSION, APP_COPYRIGHT);

std::vector<size_t> limit_vec;
std::vector<size_t> interval_vec;

void generate_menu_array (size_t val, std::vector<size_t>& pvc)
{
	pvc.clear ();

	for (size_t i = 1; i < 10; i++)
		pvc.push_back (i * 10);

	for (size_t i = val - 2; i <= (val + 2); i++)
	{
		if (i >= 5)
			pvc.push_back (i);
	}

	std::sort (pvc.begin (), pvc.end ()); // sort
	pvc.erase (std::unique (pvc.begin (), pvc.end ()), pvc.end ()); // remove duplicates
}

void BresenhamCircle (HDC dc, LONG radius, LPPOINT pt, COLORREF clr)
{
	LONG cx = 0, cy = radius, d = 2 - 2 * radius;

	// let's start drawing the circle:
	SetPixel (dc, cx + pt->x, cy + pt->y, clr); // point (0, R);
	SetPixel (dc, cx + pt->x, -cy + pt->y, clr); // point (0, -R);
	SetPixel (dc, cy + pt->x, cx + pt->y, clr); // point (R, 0);
	SetPixel (dc, -cy + pt->x, cx + pt->y, clr); // point (-R, 0);

	while (true)
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

void BresenhamLine (HDC dc, INT x0, INT y0, INT x1, INT y1, COLORREF clr)
{
	INT dx = abs (x1 - x0), sx = x0 < x1 ? 1 : -1;
	INT dy = abs (y1 - y0), sy = y0 < y1 ? 1 : -1;
	INT err = (dx > dy ? dx : -dy) / 2;

	while (true)
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

DWORD _app_memorystatus (MEMORYINFO* ptr_info)
{
	MEMORYSTATUSEX msex = {0};
	SYSTEM_CACHE_INFORMATION sci = {0};

	msex.dwLength = sizeof (msex);

	GlobalMemoryStatusEx (&msex);

	if (ptr_info)
	{
		ptr_info->percent_phys = msex.dwMemoryLoad;

		ptr_info->free_phys = msex.ullAvailPhys;
		ptr_info->total_phys = msex.ullTotalPhys;

		ptr_info->percent_page = _R_PERCENT_OF (msex.ullTotalPageFile - msex.ullAvailPageFile, msex.ullTotalPageFile);

		ptr_info->free_page = msex.ullAvailPageFile;
		ptr_info->total_page = msex.ullTotalPageFile;

		if (NT_SUCCESS (NtQuerySystemInformation (SystemFileCacheInformation, &sci, sizeof (sci), nullptr)))
		{
			ptr_info->percent_ws = _R_PERCENT_OF (sci.CurrentSize, sci.PeakSize);

			ptr_info->free_ws = (sci.PeakSize - sci.CurrentSize);
			ptr_info->total_ws = sci.PeakSize;
		}
	}

	return msex.dwMemoryLoad;
}

DWORD _app_memoryclean (HWND hwnd, bool is_preventfrezes)
{
	if (!app.IsAdmin ())
		return 0;

	MEMORYINFO info = {0};

	UINT command = 0;
	DWORD mask = app.ConfigGet (L"ReductMask2", REDUCT_MASK_DEFAULT).AsUlong ();

	if (is_preventfrezes)
		mask &= ~REDUCT_MASK_FREEZES; // exclude freezes for autoclean feature ;)

	if (hwnd && !app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION, nullptr), L"IsShowReductConfirmation"))
		return 0;

	SetCursor (LoadCursor (nullptr, IDC_WAIT));

	// difference (before)
	_app_memorystatus (&info);
	const DWORD reduct_before = DWORD (info.total_phys - info.free_phys);

	// System working set
	if ((mask & REDUCT_SYSTEM_WORKING_SET) != 0)
	{
		SYSTEM_CACHE_INFORMATION sci = {0};

		sci.MinimumWorkingSet = (ULONG_PTR)-1;
		sci.MaximumWorkingSet = (ULONG_PTR)-1;

		NtSetSystemInformation (SystemFileCacheInformation, &sci, sizeof (sci));
	}

	if (app.IsVistaOrLater ())
	{
		// Working set (vista+)
		if ((mask & REDUCT_WORKING_SET) != 0)
		{
			command = MemoryEmptyWorkingSets;
			NtSetSystemInformation (SystemMemoryListInformation, &command, sizeof (command));
		}

		// Standby priority-0 list (vista+)
		if ((mask & REDUCT_STANDBY_PRIORITY0_LIST) != 0)
		{
			command = MemoryPurgeLowPriorityStandbyList;
			NtSetSystemInformation (SystemMemoryListInformation, &command, sizeof (command));
		}

		// Standby list (vista+)
		if ((mask & REDUCT_STANDBY_LIST) != 0)
		{
			command = MemoryPurgeStandbyList;
			NtSetSystemInformation (SystemMemoryListInformation, &command, sizeof (command));
		}

		// Modified page list (vista+)
		if ((mask & REDUCT_MODIFIED_LIST) != 0)
		{
			command = MemoryFlushModifiedList;
			NtSetSystemInformation (SystemMemoryListInformation, &command, sizeof (command));
		}

		// Combine memory lists (win10+)
		if (_r_sys_validversion (10, 0))
		{
			if ((mask & REDUCT_COMBINE_MEMORY_LISTS) != 0)
			{
				MEMORY_COMBINE_INFORMATION_EX combineInfo = {0};

				NtSetSystemInformation (SystemCombinePhysicalMemoryInformation, &combineInfo, sizeof (combineInfo));
			}
		}
	}

	SetCursor (LoadCursor (nullptr, IDC_ARROW));

	// difference (after)
	_app_memorystatus (&info);
	const DWORD reduct_result = max (0, reduct_before - DWORD (info.total_phys - info.free_phys));

	app.ConfigSet (L"StatisticLastReduct", _r_unixtime_now ()); // time of last cleaning

	if (app.ConfigGet (L"BalloonCleanResults", true).AsBool ())
		app.TrayPopup (app.GetHWND (), UID, nullptr, NIIF_USER, APP_NAME, _r_fmt (app.LocaleString (IDS_STATUS_CLEANED, nullptr), _r_fmt_size64 ((DWORDLONG)reduct_result).GetString ()));

	return reduct_result;
}

void _app_fontinit (LOGFONT* plf, UINT scale)
{
	if (!plf)
		return;

	const rstring buffer = app.ConfigGet (L"TrayFont", FONT_DEFAULT);

	if (buffer)
	{
		rstring::rvector vc = buffer.AsVector (L";");

		for (size_t i = 0; i < vc.size (); i++)
		{
			const rstring font = vc.at (i).Trim (L" \r\n");

			if (font.IsEmpty ())
				continue;

			if (i == 0)
				StringCchCopy (plf->lfFaceName, LF_FACESIZE, vc.at (i));

			else if (i == 1)
				plf->lfHeight = _r_dc_fontsizetoheight (vc.at (i).AsInt ());

			else if (i == 2)
				plf->lfWeight = vc.at (i).AsInt ();

			else
				break;
		}
	}

	if (!plf->lfFaceName[0])
		StringCchCopy (plf->lfFaceName, LF_FACESIZE, L"Tahoma");

	if (!plf->lfHeight)
		plf->lfHeight = _r_dc_fontsizetoheight (8);

	if (!plf->lfWeight)
		plf->lfWeight = FW_NORMAL;

	plf->lfQuality = app.ConfigGet (L"TrayUseTransparency", false).AsBool () || app.ConfigGet (L"TrayUseAntialiasing", false).AsBool () ? NONANTIALIASED_QUALITY : CLEARTYPE_QUALITY;
	plf->lfCharSet = DEFAULT_CHARSET;

	if (scale)
		plf->lfHeight *= scale;
}

HICON _app_iconcreate ()
{
	COLORREF color = app.ConfigGet (L"TrayColorText", TRAY_COLOR_TEXT).AsUlong ();
	HBRUSH bg_brush = config.bg_brush;
	bool is_transparent = app.ConfigGet (L"TrayUseTransparency", false).AsBool ();
	const bool is_round = app.ConfigGet (L"TrayRoundCorners", false).AsBool ();

	const bool has_danger = meminfo.percent_phys >= app.ConfigGet (L"TrayLevelDanger", 90).AsUlong ();
	const bool has_warning = has_danger || meminfo.percent_phys >= app.ConfigGet (L"TrayLevelWarning", 60).AsUlong ();

	if (has_danger || has_warning)
	{
		if (app.ConfigGet (L"TrayChangeBg", true).AsBool ())
		{
			bg_brush = has_danger ? config.bg_brush_danger : config.bg_brush_warning;
			is_transparent = false;
		}
		else
		{
			color = has_danger ? app.ConfigGet (L"TrayColorDanger", TRAY_COLOR_DANGER).AsUlong () : app.ConfigGet (L"TrayColorWarning", TRAY_COLOR_WARNING).AsUlong ();
		}
	}

	// select bitmap
	const HGDIOBJ prev_bmp = SelectObject (config.hdc, config.hbitmap);

	// draw transparent mask
	_r_dc_fillrect (config.hdc, &icon_rc, TRAY_COLOR_MASK);

	// draw background
	if (!is_transparent)
	{
		const HGDIOBJ prev_pen = SelectObject (config.hdc, GetStockObject (NULL_PEN));
		const HGDIOBJ prev_brush = SelectObject (config.hdc, bg_brush);

		RoundRect (config.hdc, 0, 0, icon_rc.right, icon_rc.bottom, is_round ? ((icon_rc.right - 2)) : 0, is_round ? ((icon_rc.right) / 2) : 0);

		SelectObject (config.hdc, prev_pen);
		SelectObject (config.hdc, prev_brush);
	}

	// draw border
	if (app.ConfigGet (L"TrayShowBorder", false).AsBool ())
	{
		if (is_round)
		{
			POINT pt = {0};

			pt.x = ((icon_rc.left + icon_rc.right) / 2) - 1;
			pt.y = ((icon_rc.top + icon_rc.bottom) / 2) - 1;

			const INT half = pt.x + 1;

			for (LONG i = 1; i < config.scale + 1; i++)
				BresenhamCircle (config.hdc, half - (i), &pt, color);
		}
		else
		{
			for (LONG i = 0; i < config.scale; i++)
			{
				BresenhamLine (config.hdc, i, 0, i, icon_rc.bottom, color); // left
				BresenhamLine (config.hdc, i, i, icon_rc.right, i, color); // top
				BresenhamLine (config.hdc, (icon_rc.right - 1) - i, 0, (icon_rc.right - 1) - i, icon_rc.bottom, color); // right
				BresenhamLine (config.hdc, 0, (icon_rc.bottom - 1) - i, icon_rc.right, (icon_rc.bottom - 1) - i, color); // bottom
			}
		}
	}

	WCHAR buffer[8] = {0};
	StringCchPrintf (buffer, _countof (buffer), L"%" PRIu64, meminfo.percent_phys);

	// draw text
	{
		SetBkMode (config.hdc, TRANSPARENT);
		SetTextColor (config.hdc, color);

		SelectObject (config.hdc, config.font);
		DrawTextEx (config.hdc, buffer, (int)wcslen (buffer), &icon_rc, DT_VCENTER | DT_CENTER | DT_SINGLELINE | DT_NOCLIP, nullptr);
	}

	// draw transparent mask
	{
		const HGDIOBJ old_mask = SelectObject (config.hdc_buffer, config.hbitmap_mask);

		SetBkMode (config.hdc, TRANSPARENT);
		SetTextColor (config.hdc, color);

		DrawTextEx (config.hdc_buffer, buffer, (int)wcslen (buffer), &icon_rc, DT_VCENTER | DT_CENTER | DT_SINGLELINE | DT_NOCLIP, nullptr);

		SetBkColor (config.hdc, TRAY_COLOR_MASK);
		BitBlt (config.hdc_buffer, 0, 0, icon_rc.right, icon_rc.bottom, config.hdc, 0, 0, SRCCOPY);

		SelectObject (config.hdc_buffer, old_mask);
	}

	SelectObject (config.hdc, prev_bmp);

	// finalize icon
	ICONINFO ii = {0};

	ii.fIcon = TRUE;
	ii.hbmColor = config.hbitmap;
	ii.hbmMask = config.hbitmap_mask;

	if (config.htrayicon)
	{
		DestroyIcon (config.htrayicon);
		config.htrayicon = nullptr;
	}

	config.htrayicon = CreateIconIndirect (&ii);

	return config.htrayicon;
}

void CALLBACK _app_timercallback (HWND hwnd, UINT, UINT_PTR, DWORD)
{
	_app_memorystatus (&meminfo);

	// autoreduct functional
	if (app.IsAdmin ())
	{
		if ((app.ConfigGet (L"AutoreductEnable", false).AsBool () && meminfo.percent_phys >= app.ConfigGet (L"AutoreductValue", 90).AsUlong ()) ||
			(app.ConfigGet (L"AutoreductIntervalEnable", false).AsBool () && (_r_unixtime_now () - app.ConfigGet (L"StatisticLastReduct", 0).AsLonglong ()) >= (app.ConfigGet (L"AutoreductIntervalValue", 30).AsInt () * 60)))
		{
			_app_memoryclean (nullptr, true);
		}
	}

	// refresh tray information
	{
		HICON hicon = nullptr;

		// check previous percent to prevent icon redraw
		if (!config.ms_prev || config.ms_prev != meminfo.percent_phys)
		{
			hicon = _app_iconcreate ();
			config.ms_prev = meminfo.percent_phys; // store last percentage value (required!)
		}

		app.TraySetInfo (hwnd, UID, nullptr, hicon, _r_fmt (L"%s: %d%%\r\n%s: %d%%\r\n%s: %d%%", app.LocaleString (IDS_GROUP_1, nullptr).GetString (), meminfo.percent_phys, app.LocaleString (IDS_GROUP_2, nullptr).GetString (), meminfo.percent_page, app.LocaleString (IDS_GROUP_3, nullptr).GetString (), meminfo.percent_ws));
	}

	// refresh listview information
	if (IsWindowVisible (hwnd))
	{
		_r_listview_setitem (hwnd, IDC_LISTVIEW, 0, 1, _r_fmt (L"%d%%", meminfo.percent_phys));
		_r_listview_setitem (hwnd, IDC_LISTVIEW, 1, 1, _r_fmt_size64 (meminfo.free_phys));
		_r_listview_setitem (hwnd, IDC_LISTVIEW, 2, 1, _r_fmt_size64 (meminfo.total_phys));

		_r_listview_setitem (hwnd, IDC_LISTVIEW, 3, 1, _r_fmt (L"%d%%", meminfo.percent_page));
		_r_listview_setitem (hwnd, IDC_LISTVIEW, 4, 1, _r_fmt_size64 (meminfo.free_page));
		_r_listview_setitem (hwnd, IDC_LISTVIEW, 5, 1, _r_fmt_size64 (meminfo.total_page));

		_r_listview_setitem (hwnd, IDC_LISTVIEW, 6, 1, _r_fmt (L"%d%%", meminfo.percent_ws));
		_r_listview_setitem (hwnd, IDC_LISTVIEW, 7, 1, _r_fmt_size64 (meminfo.free_ws));
		_r_listview_setitem (hwnd, IDC_LISTVIEW, 8, 1, _r_fmt_size64 (meminfo.total_ws));

		// set item lparam information
		for (size_t i = 0; i < _r_listview_getitemcount (hwnd, IDC_LISTVIEW); i++)
		{
			DWORD percent = 0;

			if (i < 3)
				percent = meminfo.percent_phys;

			else if (i < 6)
				percent = meminfo.percent_page;

			else if (i < 9)
				percent = meminfo.percent_ws;

			_r_listview_setitem (hwnd, IDC_LISTVIEW, i, 0, nullptr, LAST_VALUE, LAST_VALUE, (LPARAM)percent);
		}

		_r_listview_redraw (hwnd, IDC_LISTVIEW);
	}
}

void _app_iconredraw (HWND hwnd)
{
	config.ms_prev = 0;

	if (hwnd)
		_app_timercallback (hwnd, 0, 0, 0);
}

void _app_iconinit (HWND hwnd)
{
	if (config.font)
	{
		DeleteObject (config.font);
		config.font = nullptr;
	}

	if (config.bg_brush)
	{
		DeleteObject (config.bg_brush);
		config.bg_brush = nullptr;
	}

	if (config.bg_brush_warning)
	{
		DeleteObject (config.bg_brush_warning);
		config.bg_brush_warning = nullptr;
	}

	if (config.bg_brush_danger)
	{
		DeleteObject (config.bg_brush_danger);
		config.bg_brush_danger = nullptr;
	}

	if (config.hbitmap)
	{
		DeleteObject (config.hbitmap);
		config.hbitmap = nullptr;
	}

	if (config.hbitmap_mask)
	{
		DeleteObject (config.hbitmap_mask);
		config.hbitmap_mask = nullptr;
	}

	if (config.hdc)
	{
		DeleteDC (config.hdc);
		config.hdc = nullptr;
	}

	if (config.hdc_buffer)
	{
		DeleteDC (config.hdc_buffer);
		config.hdc_buffer = nullptr;
	}

	// common init
	config.scale = app.ConfigGet (L"TrayUseAntialiasing", false).AsBool () ? 16 : 1;

	// init font
	LOGFONT lf = {0};
	_app_fontinit (&lf, config.scale);

	config.font = CreateFontIndirect (&lf);

	// init rect
	icon_rc.right = GetSystemMetrics (SM_CXSMICON) * config.scale;
	icon_rc.bottom = GetSystemMetrics (SM_CYSMICON) * config.scale;

	// init dc
	const HDC hdc = GetDC (nullptr);

	if (hdc)
	{
		config.hdc = CreateCompatibleDC (hdc);
		config.hdc_buffer = CreateCompatibleDC (hdc);

		// init bitmap
		BITMAPINFO bmi = {0};

		bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = icon_rc.right;
		bmi.bmiHeader.biHeight = icon_rc.bottom;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biCompression = BI_RGB;
		bmi.bmiHeader.biBitCount = 32; // four 8-bit components
		bmi.bmiHeader.biSizeImage = (icon_rc.right * icon_rc.bottom) * 4; // rgba

		config.hbitmap = CreateDIBSection (hdc, &bmi, DIB_RGB_COLORS, nullptr, nullptr, 0);
		config.hbitmap_mask = CreateBitmap (icon_rc.right, icon_rc.bottom, 1, 1, nullptr);

		// init brush
		config.bg_brush = CreateSolidBrush (app.ConfigGet (L"TrayColorBg", TRAY_COLOR_BG).AsUlong ());
		config.bg_brush_warning = CreateSolidBrush (app.ConfigGet (L"TrayColorWarning", TRAY_COLOR_WARNING).AsUlong ());
		config.bg_brush_danger = CreateSolidBrush (app.ConfigGet (L"TrayColorDanger", TRAY_COLOR_DANGER).AsUlong ());

		ReleaseDC (nullptr, hdc);
	}

	_app_iconredraw (hwnd);
}

void _app_hotkeyinit (HWND hwnd)
{
	if (!app.IsAdmin ())
		return;

	UnregisterHotKey (hwnd, UID);

	if (app.ConfigGet (L"HotkeyCleanEnable", true).AsBool ())
	{
		const UINT hk = app.ConfigGet (L"HotkeyClean", MAKEWORD (VK_F1, HOTKEYF_CONTROL)).AsUint ();

		if (hk)
			RegisterHotKey (hwnd, UID, (HIBYTE (hk) & 2) | ((HIBYTE (hk) & 4) >> 2) | ((HIBYTE (hk) & 1) << 2), LOBYTE (hk));
	}
}

INT_PTR CALLBACK SettingsProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static PAPP_SETTINGS_PAGE ptr_page = nullptr;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			ptr_page = (PAPP_SETTINGS_PAGE)lparam;
			break;
		}

		case RM_INITIALIZE:
		{
			const UINT dialog_id = (UINT)wparam;

			switch (dialog_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
#ifdef _APP_HAVE_SKIPUAC
					if (!app.IsVistaOrLater () || !app.IsAdmin ())
						_r_ctrl_enable (hwnd, IDC_SKIPUACWARNING_CHK, false);
#endif // _APP_HAVE_SKIPUAC

					CheckDlgButton (hwnd, IDC_ALWAYSONTOP_CHK, app.ConfigGet (L"AlwaysOnTop", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

#ifdef _APP_HAVE_AUTORUN
					CheckDlgButton (hwnd, IDC_LOADONSTARTUP_CHK, app.AutorunIsEnabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // _APP_HAVE_AUTORUN

					CheckDlgButton (hwnd, IDC_STARTMINIMIZED_CHK, app.ConfigGet (L"IsStartMinimized", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_REDUCTCONFIRMATION_CHK, app.ConfigGet (L"IsShowReductConfirmation", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

#ifdef _APP_HAVE_SKIPUAC
					CheckDlgButton (hwnd, IDC_SKIPUACWARNING_CHK, app.SkipUacIsEnabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // _APP_HAVE_SKIPUAC

					CheckDlgButton (hwnd, IDC_CHECKUPDATES_CHK, app.ConfigGet (L"CheckUpdates", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					app.LocaleEnum (hwnd, IDC_LANGUAGE, false, 0);

					break;
				}

				case IDD_SETTINGS_MEMORY:
				{
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

					// Combine memory lists (win10+)
					if (!app.IsAdmin () || !_r_sys_validversion (10, 0))
						_r_ctrl_enable (hwnd, IDC_COMBINEMEMORYLISTS_CHK, false);

					const DWORD mask = app.ConfigGet (L"ReductMask2", REDUCT_MASK_DEFAULT).AsUlong ();

					CheckDlgButton (hwnd, IDC_WORKINGSET_CHK, ((mask & REDUCT_WORKING_SET) != 0) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_SYSTEMWORKINGSET_CHK, ((mask & REDUCT_SYSTEM_WORKING_SET) != 0) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_STANDBYLISTPRIORITY0_CHK, ((mask & REDUCT_STANDBY_PRIORITY0_LIST) != 0) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_STANDBYLIST_CHK, ((mask & REDUCT_STANDBY_LIST) != 0) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_MODIFIEDLIST_CHK, ((mask & REDUCT_MODIFIED_LIST) != 0) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_COMBINEMEMORYLISTS_CHK, ((mask & REDUCT_COMBINE_MEMORY_LISTS) != 0) ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_AUTOREDUCTENABLE_CHK, app.ConfigGet (L"AutoreductEnable", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					SendDlgItemMessage (hwnd, IDC_AUTOREDUCTVALUE, UDM_SETRANGE32, 10, 99);
					SendDlgItemMessage (hwnd, IDC_AUTOREDUCTVALUE, UDM_SETPOS32, 0, (LPARAM)app.ConfigGet (L"AutoreductValue", 90).AsUint ());

					CheckDlgButton (hwnd, IDC_AUTOREDUCTINTERVALENABLE_CHK, app.ConfigGet (L"AutoreductIntervalEnable", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					SendDlgItemMessage (hwnd, IDC_AUTOREDUCTINTERVALVALUE, UDM_SETRANGE32, 5, 1440);
					SendDlgItemMessage (hwnd, IDC_AUTOREDUCTINTERVALVALUE, UDM_SETPOS32, 0, (LPARAM)app.ConfigGet (L"AutoreductIntervalValue", 30).AsUint ());

					CheckDlgButton (hwnd, IDC_HOTKEY_CLEAN_CHK, app.ConfigGet (L"HotkeyCleanEnable", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					SendDlgItemMessage (hwnd, IDC_HOTKEY_CLEAN, HKM_SETHOTKEY, (LPARAM)app.ConfigGet (L"HotkeyClean", MAKEWORD (VK_F1, HOTKEYF_CONTROL)).AsUint (), 0);

					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_AUTOREDUCTENABLE_CHK, 0), 0);
					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_AUTOREDUCTINTERVALENABLE_CHK, 0), 0);
					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_HOTKEY_CLEAN_CHK, 0), 0);

					break;
				}

				case IDD_SETTINGS_APPEARANCE:
				{
					CheckDlgButton (hwnd, IDC_TRAYUSETRANSPARENCY_CHK, app.ConfigGet (L"TrayUseTransparency", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_TRAYSHOWBORDER_CHK, app.ConfigGet (L"TrayShowBorder", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_TRAYROUNDCORNERS_CHK, app.ConfigGet (L"TrayRoundCorners", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_TRAYCHANGEBG_CHK, app.ConfigGet (L"TrayChangeBg", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_TRAYUSEANTIALIASING_CHK, app.ConfigGet (L"TrayUseAntialiasing", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					{
						LOGFONT lf = {0};
						_app_fontinit (&lf, 0);

						_r_ctrl_settext (hwnd, IDC_FONT, L"%s, %dpx", lf.lfFaceName, _r_dc_fontheighttosize (lf.lfHeight));
					}

					SetWindowLongPtr (GetDlgItem (hwnd, IDC_COLOR_TEXT), GWLP_USERDATA, (LONG_PTR)app.ConfigGet (L"TrayColorText", TRAY_COLOR_TEXT).AsUlong ());
					SetWindowLongPtr (GetDlgItem (hwnd, IDC_COLOR_BACKGROUND), GWLP_USERDATA, (LONG_PTR)app.ConfigGet (L"TrayColorBg", TRAY_COLOR_BG).AsUlong ());
					SetWindowLongPtr (GetDlgItem (hwnd, IDC_COLOR_WARNING), GWLP_USERDATA, (LONG_PTR)app.ConfigGet (L"TrayColorWarning", TRAY_COLOR_WARNING).AsUlong ());
					SetWindowLongPtr (GetDlgItem (hwnd, IDC_COLOR_DANGER), GWLP_USERDATA, (LONG_PTR)app.ConfigGet (L"TrayColorDanger", TRAY_COLOR_DANGER).AsUlong ());

					break;
				}

				case IDD_SETTINGS_TRAY:
				{
					SendDlgItemMessage (hwnd, IDC_TRAYLEVELWARNING, UDM_SETRANGE32, 10, 99);
					SendDlgItemMessage (hwnd, IDC_TRAYLEVELWARNING, UDM_SETPOS32, 0, (LPARAM)app.ConfigGet (L"TrayLevelWarning", 60).AsUint ());

					SendDlgItemMessage (hwnd, IDC_TRAYLEVELDANGER, UDM_SETRANGE32, 10, 99);
					SendDlgItemMessage (hwnd, IDC_TRAYLEVELDANGER, UDM_SETPOS32, 0, (LPARAM)app.ConfigGet (L"TrayLevelDanger", 90).AsUint ());

					SendDlgItemMessage (hwnd, IDC_TRAYACTIONDC, CB_SETCURSEL, (WPARAM)app.ConfigGet (L"TrayActionDc", 0).AsUint (), 0);
					SendDlgItemMessage (hwnd, IDC_TRAYACTIONMC, CB_SETCURSEL, (WPARAM)app.ConfigGet (L"TrayActionMc", 1).AsUint (), 0);

					CheckDlgButton (hwnd, IDC_SHOW_CLEAN_RESULT_CHK, app.ConfigGet (L"BalloonCleanResults", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					break;
				}
			}

			break;
		}

		case RM_LOCALIZE:
		{
			const UINT dialog_id = (UINT)wparam;

			// localize titles
			SetDlgItemText (hwnd, IDC_TITLE_1, app.LocaleString (IDS_TITLE_1, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_2, app.LocaleString (IDS_TITLE_2, L": (Language)"));
			SetDlgItemText (hwnd, IDC_TITLE_3, app.LocaleString (IDS_TITLE_3, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_4, app.LocaleString (IDS_TITLE_4, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_5, app.LocaleString (IDS_TITLE_5, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_6, app.LocaleString (IDS_TITLE_6, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_7, app.LocaleString (IDS_TITLE_7, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_8, app.LocaleString (IDS_TITLE_8, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_9, app.LocaleString (IDS_TITLE_9, L":"));

			switch (dialog_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
					SetDlgItemText (hwnd, IDC_ALWAYSONTOP_CHK, app.LocaleString (IDS_ALWAYSONTOP_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_LOADONSTARTUP_CHK, app.LocaleString (IDS_LOADONSTARTUP_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_STARTMINIMIZED_CHK, app.LocaleString (IDS_STARTMINIMIZED_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_REDUCTCONFIRMATION_CHK, app.LocaleString (IDS_REDUCTCONFIRMATION_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_SKIPUACWARNING_CHK, app.LocaleString (IDS_SKIPUACWARNING_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_CHECKUPDATES_CHK, app.LocaleString (IDS_CHECKUPDATES_CHK, nullptr));

					SetDlgItemText (hwnd, IDC_LANGUAGE_HINT, app.LocaleString (IDS_LANGUAGE_HINT, nullptr));

					break;
				}

				case IDD_SETTINGS_MEMORY:
				{
					SetDlgItemText (hwnd, IDC_WORKINGSET_CHK, app.LocaleString (IDS_WORKINGSET_CHK, L" (vista+)"));
					SetDlgItemText (hwnd, IDC_SYSTEMWORKINGSET_CHK, app.LocaleString (IDS_SYSTEMWORKINGSET_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_STANDBYLISTPRIORITY0_CHK, app.LocaleString (IDS_STANDBYLISTPRIORITY0_CHK, L" (vista+)"));
					_r_ctrl_settext (hwnd, IDC_STANDBYLIST_CHK, app.LocaleString (IDS_STANDBYLIST_CHK, L"* (vista+)"));
					_r_ctrl_settext (hwnd, IDC_MODIFIEDLIST_CHK, app.LocaleString (IDS_MODIFIEDLIST_CHK, L"* (vista+)"));
					SetDlgItemText (hwnd, IDC_COMBINEMEMORYLISTS_CHK, app.LocaleString (IDS_COMBINEMEMORYLISTS_CHK, L" (win10+)"));

					SetDlgItemText (hwnd, IDC_AUTOREDUCTENABLE_CHK, app.LocaleString (IDS_AUTOREDUCTENABLE_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_AUTOREDUCTINTERVALENABLE_CHK, app.LocaleString (IDS_AUTOREDUCTINTERVALENABLE_CHK, nullptr));

					SetDlgItemText (hwnd, IDC_HOTKEY_CLEAN_CHK, app.LocaleString (IDS_HOTKEY_CLEAN_CHK, nullptr));

					break;
				}

				case IDD_SETTINGS_APPEARANCE:
				{
					SetDlgItemText (hwnd, IDC_TRAYUSETRANSPARENCY_CHK, app.LocaleString (IDS_TRAYUSETRANSPARENCY_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_TRAYSHOWBORDER_CHK, app.LocaleString (IDS_TRAYSHOWBORDER_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_TRAYROUNDCORNERS_CHK, app.LocaleString (IDS_TRAYROUNDCORNERS_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_TRAYCHANGEBG_CHK, app.LocaleString (IDS_TRAYCHANGEBG_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_TRAYUSEANTIALIASING_CHK, app.LocaleString (IDS_TRAYUSEANTIALIASING_CHK, L" [BETA]"));

					SetDlgItemText (hwnd, IDC_FONT_HINT, app.LocaleString (IDS_FONT_HINT, nullptr));
					SetDlgItemText (hwnd, IDC_COLOR_TEXT_HINT, app.LocaleString (IDS_COLOR_TEXT_HINT, nullptr));
					SetDlgItemText (hwnd, IDC_COLOR_BACKGROUND_HINT, app.LocaleString (IDS_COLOR_BACKGROUND_HINT, nullptr));
					SetDlgItemText (hwnd, IDC_COLOR_WARNING_HINT, app.LocaleString (IDS_COLOR_WARNING_HINT, nullptr));
					SetDlgItemText (hwnd, IDC_COLOR_DANGER_HINT, app.LocaleString (IDS_COLOR_DANGER_HINT, nullptr));

					_r_wnd_addstyle (hwnd, IDC_FONT, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

					_r_wnd_addstyle (hwnd, IDC_COLOR_TEXT, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
					_r_wnd_addstyle (hwnd, IDC_COLOR_BACKGROUND, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
					_r_wnd_addstyle (hwnd, IDC_COLOR_WARNING, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
					_r_wnd_addstyle (hwnd, IDC_COLOR_DANGER, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

					break;
				}

				case IDD_SETTINGS_TRAY:
				{
					SetDlgItemText (hwnd, IDC_TRAYLEVELWARNING_HINT, app.LocaleString (IDS_TRAYLEVELWARNING_HINT, nullptr));
					SetDlgItemText (hwnd, IDC_TRAYLEVELDANGER_HINT, app.LocaleString (IDS_TRAYLEVELDANGER_HINT, nullptr));

					SetDlgItemText (hwnd, IDC_TRAYACTIONDC_HINT, app.LocaleString (IDS_TRAYACTIONDC_HINT, nullptr));
					SetDlgItemText (hwnd, IDC_TRAYACTIONMC_HINT, app.LocaleString (IDS_TRAYACTIONMC_HINT, nullptr));

					SendDlgItemMessage (hwnd, IDC_TRAYACTIONDC, CB_RESETCONTENT, 0, 0);
					SendDlgItemMessage (hwnd, IDC_TRAYACTIONMC, CB_RESETCONTENT, 0, 0);

					for (INT i = 0; i < 3; i++)
					{
						rstring item = app.LocaleString (IDS_TRAY_ACTION_1 + i, nullptr);

						SendDlgItemMessage (hwnd, IDC_TRAYACTIONDC, CB_INSERTSTRING, i, (LPARAM)item.GetString ());
						SendDlgItemMessage (hwnd, IDC_TRAYACTIONMC, CB_INSERTSTRING, i, (LPARAM)item.GetString ());
					}

					SendDlgItemMessage (hwnd, IDC_TRAYACTIONDC, CB_SETCURSEL, (WPARAM)app.ConfigGet (L"TrayActionDc", 0).AsUint (), 0);
					SendDlgItemMessage (hwnd, IDC_TRAYACTIONMC, CB_SETCURSEL, (WPARAM)app.ConfigGet (L"TrayActionMc", 1).AsUint (), 0);

					SetDlgItemText (hwnd, IDC_SHOW_CLEAN_RESULT_CHK, app.LocaleString (IDS_SHOW_CLEAN_RESULT_CHK, nullptr));

					break;
				}
			}

			break;
		}


		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case NM_CUSTOMDRAW:
				{
					LPNMCUSTOMDRAW lpnmcd = (LPNMCUSTOMDRAW)lparam;

					const UINT ctrl_id = (UINT)nmlp->idFrom;

					if (ctrl_id == IDC_COLOR_TEXT ||
						ctrl_id == IDC_COLOR_BACKGROUND ||
						ctrl_id == IDC_COLOR_WARNING ||
						ctrl_id == IDC_COLOR_DANGER
						)
					{
						lpnmcd->rc.left += app.GetDPI (3);
						lpnmcd->rc.top += app.GetDPI (3);
						lpnmcd->rc.right -= app.GetDPI (3);
						lpnmcd->rc.bottom -= app.GetDPI (3);

						_r_dc_fillrect (lpnmcd->hdc, &lpnmcd->rc, (COLORREF)GetWindowLongPtr (nmlp->hwndFrom, GWLP_USERDATA));

						SetWindowLongPtr (hwnd, DWLP_MSGRESULT, CDRF_DODEFAULT | CDRF_DOERASE);
						return bool (CDRF_DODEFAULT | CDRF_DOERASE);
					}

					break;
				}
			}

			break;
		}

		case WM_VSCROLL:
		case WM_HSCROLL:
		{
			const UINT ctrl_id = GetDlgCtrlID ((HWND)lparam);
			bool is_stylechanged = false;

			if (ctrl_id == IDC_AUTOREDUCTVALUE)
			{
				app.ConfigSet (L"AutoreductValue", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));
			}
			else if (ctrl_id == IDC_AUTOREDUCTINTERVALVALUE)
			{
				app.ConfigSet (L"AutoreductIntervalValue", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));
			}
			else if (ctrl_id == IDC_TRAYLEVELWARNING)
			{
				is_stylechanged = true;
				app.ConfigSet (L"TrayLevelWarning", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));
			}
			else if (ctrl_id == IDC_TRAYLEVELDANGER)
			{
				is_stylechanged = true;
				app.ConfigSet (L"TrayLevelDanger", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));
			}

			if (is_stylechanged)
			{
				_app_iconredraw (app.GetHWND ());
				_r_listview_redraw (app.GetHWND (), IDC_LISTVIEW);
			}

			break;
		}

		case WM_COMMAND:
		{
			const UINT ctrl_id = LOWORD (wparam);
			const UINT notify_code = HIWORD (wparam);

			switch (LOWORD (wparam))
			{
				case IDC_AUTOREDUCTVALUE_CTRL:
				case IDC_AUTOREDUCTINTERVALVALUE_CTRL:
				case IDC_TRAYLEVELWARNING_CTRL:
				case IDC_TRAYLEVELDANGER_CTRL:
				{
					bool is_stylechanged = false;

					if (ctrl_id == IDC_AUTOREDUCTVALUE_CTRL && notify_code == EN_CHANGE)
					{
						app.ConfigSet (L"AutoreductValue", (DWORD)SendDlgItemMessage (hwnd, IDC_AUTOREDUCTVALUE, UDM_GETPOS32, 0, 0));
					}
					else if (ctrl_id == IDC_AUTOREDUCTINTERVALVALUE_CTRL && notify_code == EN_CHANGE)
					{
						app.ConfigSet (L"AutoreductIntervalValue", (DWORD)SendDlgItemMessage (hwnd, IDC_AUTOREDUCTINTERVALVALUE, UDM_GETPOS32, 0, 0));
					}
					else if (ctrl_id == IDC_TRAYLEVELWARNING_CTRL && notify_code == EN_CHANGE)
					{
						is_stylechanged = true;
						app.ConfigSet (L"TrayLevelWarning", (DWORD)SendDlgItemMessage (hwnd, IDC_TRAYLEVELWARNING, UDM_GETPOS32, 0, 0));
					}
					else if (ctrl_id == IDC_TRAYLEVELDANGER_CTRL && notify_code == EN_CHANGE)
					{
						is_stylechanged = true;
						app.ConfigSet (L"TrayLevelDanger", (DWORD)SendDlgItemMessage (hwnd, IDC_TRAYLEVELDANGER, UDM_GETPOS32, 0, 0));
					}

					if (is_stylechanged)
					{
						_app_iconredraw (app.GetHWND ());
						_r_listview_redraw (app.GetHWND (), IDC_LISTVIEW);
					}

					break;
				}

				case IDC_ALWAYSONTOP_CHK:
				case IDC_LOADONSTARTUP_CHK:
				case IDC_STARTMINIMIZED_CHK:
				case IDC_REDUCTCONFIRMATION_CHK:
				case IDC_SKIPUACWARNING_CHK:
				case IDC_CHECKUPDATES_CHK:
				case IDC_LANGUAGE:
				case IDC_WORKINGSET_CHK:
				case IDC_SYSTEMWORKINGSET_CHK:
				case IDC_STANDBYLISTPRIORITY0_CHK:
				case IDC_STANDBYLIST_CHK:
				case IDC_MODIFIEDLIST_CHK:
				case IDC_COMBINEMEMORYLISTS_CHK:
				case IDC_AUTOREDUCTENABLE_CHK:
				case IDC_AUTOREDUCTINTERVALENABLE_CHK:
				case IDC_HOTKEY_CLEAN_CHK:
				case IDC_HOTKEY_CLEAN:
				case IDC_TRAYUSETRANSPARENCY_CHK:
				case IDC_TRAYSHOWBORDER_CHK:
				case IDC_TRAYROUNDCORNERS_CHK:
				case IDC_TRAYCHANGEBG_CHK:
				case IDC_TRAYUSEANTIALIASING_CHK:
				case IDC_TRAYACTIONDC:
				case IDC_TRAYACTIONMC:
				case IDC_SHOW_CLEAN_RESULT_CHK:
				{
					bool is_stylechanged = false;

					if (ctrl_id == IDC_ALWAYSONTOP_CHK && notify_code == BN_CLICKED)
					{
						const bool is_enable = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

						app.ConfigSet (L"AlwaysOnTop", is_enable);
						CheckMenuItem (GetMenu (app.GetHWND ()), IDM_ALWAYSONTOP_CHK, MF_BYCOMMAND | (is_enable ? MF_CHECKED : MF_UNCHECKED));
					}
					else if (ctrl_id == IDC_STARTMINIMIZED_CHK && notify_code == BN_CLICKED)
					{
						const bool is_enable = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

						app.ConfigSet (L"IsStartMinimized", is_enable);
						CheckMenuItem (GetMenu (app.GetHWND ()), IDM_STARTMINIMIZED_CHK, MF_BYCOMMAND | (is_enable ? MF_CHECKED : MF_UNCHECKED));
					}
					else if (ctrl_id == IDC_REDUCTCONFIRMATION_CHK && notify_code == BN_CLICKED)
					{
						const bool is_enable = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

						app.ConfigSet (L"IsShowReductConfirmation", is_enable);
						CheckMenuItem (GetMenu (app.GetHWND ()), IDM_REDUCTCONFIRMATION_CHK, MF_BYCOMMAND | (is_enable ? MF_CHECKED : MF_UNCHECKED));
					}
					else if (ctrl_id == IDC_LOADONSTARTUP_CHK && notify_code == BN_CLICKED)
					{
						const bool is_enable = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

						app.AutorunEnable (is_enable);
						CheckMenuItem (GetMenu (app.GetHWND ()), IDM_LOADONSTARTUP_CHK, MF_BYCOMMAND | (is_enable ? MF_CHECKED : MF_UNCHECKED));
					}
					else if (ctrl_id == IDC_SKIPUACWARNING_CHK && notify_code == BN_CLICKED)
					{
						app.SkipUacEnable (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);
					}
					else if (ctrl_id == IDC_CHECKUPDATES_CHK && notify_code == BN_CLICKED)
					{
						const bool is_enable = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

						app.ConfigSet (L"CheckUpdates", is_enable);
						CheckMenuItem (GetMenu (app.GetHWND ()), IDM_CHECKUPDATES_CHK, MF_BYCOMMAND | (is_enable ? MF_CHECKED : MF_UNCHECKED));
					}
					else if (ctrl_id == IDC_LANGUAGE && notify_code == CBN_SELCHANGE)
					{
						app.LocaleApplyFromControl (hwnd, ctrl_id);
					}
					else if ((
						ctrl_id == IDC_WORKINGSET_CHK ||
						ctrl_id == IDC_SYSTEMWORKINGSET_CHK ||
						ctrl_id == IDC_STANDBYLISTPRIORITY0_CHK ||
						ctrl_id == IDC_STANDBYLIST_CHK ||
						ctrl_id == IDC_MODIFIEDLIST_CHK ||
						ctrl_id == IDC_COMBINEMEMORYLISTS_CHK
						) && notify_code == BN_CLICKED)
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

						if (IsDlgButtonChecked (hwnd, IDC_COMBINEMEMORYLISTS_CHK) == BST_CHECKED)
							mask |= REDUCT_COMBINE_MEMORY_LISTS;

						if (
							(ctrl_id == IDC_STANDBYLIST_CHK && (mask & REDUCT_STANDBY_LIST) != 0) ||
							(ctrl_id == IDC_MODIFIEDLIST_CHK && (mask & REDUCT_MODIFIED_LIST) != 0)
							)
						{
							if (!app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_WARNING, nullptr), L"IsShowWarningConfirmation"))
							{
								const LONG_PTR dialog_id = GetWindowLongPtr (hwnd, GWLP_USERDATA);
								SendMessage (hwnd, RM_INITIALIZE, dialog_id, (LPARAM)ptr_page);
								return FALSE;
							}
						}

						app.ConfigSet (L"ReductMask2", mask);
					}
					else if (ctrl_id == IDC_AUTOREDUCTENABLE_CHK && notify_code == BN_CLICKED)
					{
						const bool is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

						EnableWindow ((HWND)SendDlgItemMessage (hwnd, IDC_AUTOREDUCTVALUE, UDM_GETBUDDY, 0, 0), is_enabled);

						app.ConfigSet (L"AutoreductEnable", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_AUTOREDUCTINTERVALENABLE_CHK && notify_code == BN_CLICKED)
					{
						const bool is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

						EnableWindow ((HWND)SendDlgItemMessage (hwnd, IDC_AUTOREDUCTINTERVALVALUE, UDM_GETBUDDY, 0, 0), is_enabled);

						app.ConfigSet (L"AutoreductIntervalEnable", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_HOTKEY_CLEAN_CHK && notify_code == BN_CLICKED)
					{
						const bool is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

						_r_ctrl_enable (hwnd, IDC_HOTKEY_CLEAN, is_enabled);

						app.ConfigSet (L"HotkeyCleanEnable", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

						_app_hotkeyinit (app.GetHWND ());
					}
					else if (ctrl_id == IDC_HOTKEY_CLEAN && notify_code == EN_CHANGE)
					{
						app.ConfigSet (L"HotkeyClean", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, HKM_GETHOTKEY, 0, 0));

						_app_hotkeyinit (app.GetHWND ());
					}
					else if (ctrl_id == IDC_TRAYUSETRANSPARENCY_CHK && notify_code == BN_CLICKED)
					{
						is_stylechanged = true;
						app.ConfigSet (L"TrayUseTransparency", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_TRAYSHOWBORDER_CHK && notify_code == BN_CLICKED)
					{
						is_stylechanged = true;
						app.ConfigSet (L"TrayShowBorder", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_TRAYROUNDCORNERS_CHK && notify_code == BN_CLICKED)
					{
						is_stylechanged = true;
						app.ConfigSet (L"TrayRoundCorners", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_TRAYCHANGEBG_CHK && notify_code == BN_CLICKED)
					{
						is_stylechanged = true;
						app.ConfigSet (L"TrayChangeBg", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_TRAYUSEANTIALIASING_CHK && notify_code == BN_CLICKED)
					{
						is_stylechanged = true;
						app.ConfigSet (L"TrayUseAntialiasing", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_TRAYACTIONDC && notify_code == CBN_SELCHANGE)
						app.ConfigSet (L"TrayActionDc", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, CB_GETCURSEL, 0, 0));

					else if (ctrl_id == IDC_TRAYACTIONMC && notify_code == CBN_SELCHANGE)
					{
						app.ConfigSet (L"TrayActionMc", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, CB_GETCURSEL, 0, 0));
					}
					else if (ctrl_id == IDC_SHOW_CLEAN_RESULT_CHK && notify_code == BN_CLICKED)
					{
						app.ConfigSet (L"BalloonCleanResults", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}

					if (is_stylechanged)
						_app_iconinit (app.GetHWND ());

					break;
				}

				case IDC_COLOR_TEXT:
				case IDC_COLOR_BACKGROUND:
				case IDC_COLOR_WARNING:
				case IDC_COLOR_DANGER:
				{
					CHOOSECOLOR cc = {0};
					COLORREF cust[16] = {TRAY_COLOR_TEXT, TRAY_COLOR_BG, TRAY_COLOR_WARNING, TRAY_COLOR_DANGER};

					HWND hctrl = GetDlgItem (hwnd, ctrl_id);

					cc.lStructSize = sizeof (cc);
					cc.Flags = CC_RGBINIT | CC_FULLOPEN;
					cc.hwndOwner = hwnd;
					cc.lpCustColors = cust;
					cc.rgbResult = static_cast<COLORREF>(GetWindowLongPtr (hctrl, GWLP_USERDATA));

					if (ChooseColor (&cc))
					{
						const COLORREF clr = cc.rgbResult;

						if (ctrl_id == IDC_COLOR_TEXT)
							app.ConfigSet (L"TrayColorText", clr);

						else if (ctrl_id == IDC_COLOR_BACKGROUND)
							app.ConfigSet (L"TrayColorBg", clr);

						else if (ctrl_id == IDC_COLOR_WARNING)
							app.ConfigSet (L"TrayColorWarning", clr);

						else if (ctrl_id == IDC_COLOR_DANGER)
							app.ConfigSet (L"TrayColorDanger", clr);

						SetWindowLongPtr (hctrl, GWLP_USERDATA, (LONG_PTR)clr);

						_app_iconinit (app.GetHWND ());
					}

					break;
				}

				case IDC_FONT:
				{
					CHOOSEFONT cf = {0};

					cf.lStructSize = sizeof (cf);
					cf.hwndOwner = hwnd;
					cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL;

					LOGFONT lf = {0};
					_app_fontinit (&lf, 0);

					cf.lpLogFont = &lf;

					if (ChooseFont (&cf))
					{
						const DWORD size = _r_dc_fontheighttosize (lf.lfHeight);

						app.ConfigSet (L"TrayFont", _r_fmt (L"%s;%d;%d", lf.lfFaceName, size, lf.lfWeight));

						_r_ctrl_settext (hwnd, IDC_FONT, L"%s, %dpx", lf.lfFaceName, size);

						_app_iconinit (app.GetHWND ());
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
				LPCWSTR privileges[] = {
					SE_INCREASE_QUOTA_NAME,
					SE_PROF_SINGLE_PROCESS_NAME,
				};

				_r_sys_setprivilege (privileges, _countof (privileges), true);
			}

			// uac indicator (vista+)
			if (_r_sys_uacstate ())
			{
				RECT rc = {0};
				rc.left = rc.right = app.GetDPI (4);

				SendDlgItemMessage (hwnd, IDC_CLEAN, BCM_SETTEXTMARGIN, 0, (LPARAM)&rc);
				SendDlgItemMessage (hwnd, IDC_CLEAN, BCM_SETSHIELD, 0, TRUE);
			}

			// configure listview
			_r_listview_setstyle (hwnd, IDC_LISTVIEW, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP);

			_r_listview_addcolumn (hwnd, IDC_LISTVIEW, 1, nullptr, 50, LVCFMT_RIGHT);
			_r_listview_addcolumn (hwnd, IDC_LISTVIEW, 2, nullptr, 50, LVCFMT_LEFT);

			// configure listview
			for (size_t i = 0, k = 0; i < 3; i++)
			{
				_r_listview_addgroup (hwnd, IDC_LISTVIEW, i, app.LocaleString (IDS_GROUP_1 + UINT (i), nullptr), 0, 0);

				for (size_t j = 0; j < 3; j++)
					_r_listview_additem (hwnd, IDC_LISTVIEW, k++, 0, app.LocaleString (IDS_ITEM_1 + UINT (j), nullptr), LAST_VALUE, i);
			}

			// settings
			app.SettingsAddPage (IDD_SETTINGS_GENERAL, IDS_SETTINGS_GENERAL);
			app.SettingsAddPage (IDD_SETTINGS_MEMORY, IDS_SETTINGS_MEMORY);
			app.SettingsAddPage (IDD_SETTINGS_APPEARANCE, IDS_SETTINGS_APPEARANCE);
			app.SettingsAddPage (IDD_SETTINGS_TRAY, IDS_SETTINGS_TRAY);

			break;
		}

		case WM_DESTROY:
		{
			PostQuitMessage (0);
			break;
		}

		case RM_INITIALIZE:
		{
			_app_hotkeyinit (hwnd);
			_app_iconinit (nullptr);

			app.TrayCreate (hwnd, UID, nullptr, WM_TRAYICON, _app_iconcreate (), false);

			_app_iconredraw (hwnd);

			SetTimer (hwnd, UID, TIMER, &_app_timercallback);

			// configure menu
			CheckMenuItem (GetMenu (hwnd), IDM_ALWAYSONTOP_CHK, MF_BYCOMMAND | (app.ConfigGet (L"AlwaysOnTop", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (GetMenu (hwnd), IDM_LOADONSTARTUP_CHK, MF_BYCOMMAND | (app.AutorunIsEnabled () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (GetMenu (hwnd), IDM_STARTMINIMIZED_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsStartMinimized", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (GetMenu (hwnd), IDM_REDUCTCONFIRMATION_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsShowReductConfirmation", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (GetMenu (hwnd), IDM_CHECKUPDATES_CHK, MF_BYCOMMAND | (app.ConfigGet (L"CheckUpdates", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));

			break;
		}

		case RM_LOCALIZE:
		{
			// localize menu
			const HMENU menu = GetMenu (hwnd);

			app.LocaleMenu (menu, IDS_FILE, 0, true, nullptr);
			app.LocaleMenu (menu, IDS_SETTINGS, IDM_SETTINGS, false, L"...\tF2");
			app.LocaleMenu (menu, IDS_EXIT, IDM_EXIT, false, L"...\tAlt+F4");
			app.LocaleMenu (menu, IDS_SETTINGS, 1, true, nullptr);
			app.LocaleMenu (menu, IDS_ALWAYSONTOP_CHK, IDM_ALWAYSONTOP_CHK, false, nullptr);
			app.LocaleMenu (menu, IDS_LOADONSTARTUP_CHK, IDM_LOADONSTARTUP_CHK, false, nullptr);
			app.LocaleMenu (menu, IDS_STARTMINIMIZED_CHK, IDM_STARTMINIMIZED_CHK, false, nullptr);
			app.LocaleMenu (menu, IDS_REDUCTCONFIRMATION_CHK, IDM_REDUCTCONFIRMATION_CHK, false, nullptr);
			app.LocaleMenu (menu, IDS_CHECKUPDATES_CHK, IDM_CHECKUPDATES_CHK, false, nullptr);
			app.LocaleMenu (GetSubMenu (menu, 1), IDS_LANGUAGE, LANG_MENU, true, L" (Language)");
			app.LocaleMenu (menu, IDS_HELP, 2, true, nullptr);
			app.LocaleMenu (menu, IDS_WEBSITE, IDM_WEBSITE, false, nullptr);
			app.LocaleMenu (menu, IDS_CHECKUPDATES, IDM_CHECKUPDATES, false, nullptr);
			app.LocaleMenu (menu, IDS_ABOUT, IDM_ABOUT, false, L"\tF1");

			// configure listview
			for (size_t i = 0, k = 0; i < 3; i++)
			{
				_r_listview_setgroup (hwnd, IDC_LISTVIEW, i, app.LocaleString (IDS_GROUP_1 + UINT (i), nullptr), 0, 0);

				for (size_t j = 0; j < 3; j++)
					_r_listview_setitem (hwnd, IDC_LISTVIEW, k++, 0, app.LocaleString (IDS_ITEM_1 + UINT (j), nullptr));
			}

			// configure button
			SetDlgItemText (hwnd, IDC_CLEAN, app.LocaleString (IDS_CLEAN, nullptr));

			_r_wnd_addstyle (hwnd, IDC_CLEAN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			app.LocaleEnum ((HWND)GetSubMenu (menu, 1), LANG_MENU, true, IDX_LANGUAGE); // enum localizations

			break;
		}

		case RM_UNINITIALIZE:
		{
			app.TrayDestroy (hwnd, UID, nullptr);
			KillTimer (hwnd, UID);

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

			_r_dc_fillrect (dc, &rc, GetSysColor (COLOR_3DFACE));

			for (INT i = 0; i < rc.right; i++)
				SetPixel (dc, i, rc.top, GetSysColor (COLOR_APPWORKSPACE));

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
				_app_memoryclean (nullptr, true);

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			if (nmlp->idFrom != IDC_LISTVIEW)
				break;

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
				case WM_MBUTTONUP:
				{
					const INT action = (LOWORD (lparam) == WM_LBUTTONDBLCLK) ? app.ConfigGet (L"TrayActionDc", 0).AsInt () : app.ConfigGet (L"TrayActionMc", 1).AsInt ();

					switch (action)
					{
						case 1:
						{
							_app_memoryclean (nullptr, false);
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

					HMENU menu = LoadMenu (nullptr, MAKEINTRESOURCE (IDM_TRAY));
					HMENU submenu = GetSubMenu (menu, 0);

					HMENU submenu1 = GetSubMenu (submenu, SUBMENU1);
					HMENU submenu2 = GetSubMenu (submenu, SUBMENU2);
					HMENU submenu3 = GetSubMenu (submenu, SUBMENU3);

					// localize
					app.LocaleMenu (submenu, IDS_TRAY_SHOW, IDM_TRAY_SHOW, false, nullptr);
					app.LocaleMenu (submenu, IDS_CLEAN, IDM_TRAY_CLEAN, false, nullptr);
					app.LocaleMenu (submenu, IDS_TRAY_POPUP_1, SUBMENU1, true, nullptr);
					app.LocaleMenu (submenu, IDS_TRAY_POPUP_2, SUBMENU2, true, nullptr);
					app.LocaleMenu (submenu, IDS_TRAY_POPUP_3, SUBMENU3, true, nullptr);
					app.LocaleMenu (submenu, IDS_SETTINGS, IDM_TRAY_SETTINGS, false, L"...");
					app.LocaleMenu (submenu, IDS_WEBSITE, IDM_TRAY_WEBSITE, false, nullptr);
					app.LocaleMenu (submenu, IDS_ABOUT, IDM_TRAY_ABOUT, false, nullptr);
					app.LocaleMenu (submenu, IDS_EXIT, IDM_TRAY_EXIT, false, nullptr);

					app.LocaleMenu (submenu1, IDS_WORKINGSET_CHK, IDM_WORKINGSET_CHK, false, L" (vista+)");
					app.LocaleMenu (submenu1, IDS_SYSTEMWORKINGSET_CHK, IDM_SYSTEMWORKINGSET_CHK, false, nullptr);
					app.LocaleMenu (submenu1, IDS_STANDBYLISTPRIORITY0_CHK, IDM_STANDBYLISTPRIORITY0_CHK, false, L" (vista+)");
					app.LocaleMenu (submenu1, IDS_STANDBYLIST_CHK, IDM_STANDBYLIST_CHK, false, L"* (vista+)");
					app.LocaleMenu (submenu1, IDS_MODIFIEDLIST_CHK, IDM_MODIFIEDLIST_CHK, false, L"* (vista+)");
					app.LocaleMenu (submenu1, IDS_COMBINEMEMORYLISTS_CHK, IDM_COMBINEMEMORYLISTS_CHK, false, L" (win10+)");

					app.LocaleMenu (submenu2, IDS_TRAY_DISABLE, IDM_TRAY_DISABLE_1, false, nullptr);
					app.LocaleMenu (submenu3, IDS_TRAY_DISABLE, IDM_TRAY_DISABLE_2, false, nullptr);

					// configure submenu #1
					const DWORD mask = app.ConfigGet (L"ReductMask2", REDUCT_MASK_DEFAULT).AsUlong ();

					if ((mask & REDUCT_WORKING_SET) != 0)
						CheckMenuItem (submenu1, IDM_WORKINGSET_CHK, MF_BYCOMMAND | MF_CHECKED);

					if ((mask & REDUCT_SYSTEM_WORKING_SET) != 0)
						CheckMenuItem (submenu1, IDM_SYSTEMWORKINGSET_CHK, MF_BYCOMMAND | MF_CHECKED);

					if ((mask & REDUCT_STANDBY_PRIORITY0_LIST) != 0)
						CheckMenuItem (submenu1, IDM_STANDBYLISTPRIORITY0_CHK, MF_BYCOMMAND | MF_CHECKED);

					if ((mask & REDUCT_STANDBY_LIST) != 0)
						CheckMenuItem (submenu1, IDM_STANDBYLIST_CHK, MF_BYCOMMAND | MF_CHECKED);

					if ((mask & REDUCT_MODIFIED_LIST) != 0)
						CheckMenuItem (submenu1, IDM_MODIFIEDLIST_CHK, MF_BYCOMMAND | MF_CHECKED);

					if ((mask & REDUCT_COMBINE_MEMORY_LISTS) != 0)
						CheckMenuItem (submenu1, IDM_COMBINEMEMORYLISTS_CHK, MF_BYCOMMAND | MF_CHECKED);

					if (!app.IsVistaOrLater () || !app.IsAdmin ())
					{
						EnableMenuItem (submenu1, IDM_WORKINGSET_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
						EnableMenuItem (submenu1, IDM_STANDBYLISTPRIORITY0_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
						EnableMenuItem (submenu1, IDM_STANDBYLIST_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
						EnableMenuItem (submenu1, IDM_MODIFIEDLIST_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

						if (!app.IsAdmin ())
							EnableMenuItem (submenu1, IDM_SYSTEMWORKINGSET_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					}

					// Combine memory lists (win10+)
					if (!app.IsAdmin () || !_r_sys_validversion (10, 0))
						EnableMenuItem (submenu1, IDM_COMBINEMEMORYLISTS_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

					// configure submenu #2
					generate_menu_array (app.ConfigGet (L"AutoreductValue", 90).AsSizeT (), limit_vec);

					for (size_t i = 0; i < limit_vec.size (); i++)
					{
						AppendMenu (submenu2, MF_STRING, IDX_TRAY_POPUP_1 + UINT (i), _r_fmt (L"%zu%%", limit_vec.at (i)));

						if (!app.IsAdmin ())
							EnableMenuItem (submenu2, static_cast<UINT>(IDX_TRAY_POPUP_1 + i), MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

						if (app.ConfigGet (L"AutoreductValue", 90).AsSizeT () == limit_vec.at (i))
							CheckMenuRadioItem (submenu2, 0, static_cast<UINT>(limit_vec.size ()), static_cast<UINT>(i) + 2, MF_BYPOSITION);
					}

					if (!app.ConfigGet (L"AutoreductEnable", false).AsBool ())
						CheckMenuRadioItem (submenu2, 0, static_cast<UINT>(limit_vec.size ()), 0, MF_BYPOSITION);

					// configure submenu #3
					generate_menu_array (app.ConfigGet (L"AutoreductIntervalValue", 30).AsSizeT (), interval_vec);

					for (size_t i = 0; i < interval_vec.size (); i++)
					{
						AppendMenu (submenu3, MF_STRING, IDX_TRAY_POPUP_2 + UINT (i), _r_fmt (L"%zu min.", interval_vec.at (i)));

						if (!app.IsAdmin ())
							EnableMenuItem (submenu3, static_cast<UINT>(IDX_TRAY_POPUP_2 + i), MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

						if (app.ConfigGet (L"AutoreductIntervalValue", 30).AsSizeT () == interval_vec.at (i))
							CheckMenuRadioItem (submenu3, 0, static_cast<UINT>(interval_vec.size ()), static_cast<UINT>(i) + 2, MF_BYPOSITION);
					}

					if (!app.ConfigGet (L"AutoreductIntervalEnable", false).AsBool ())
						CheckMenuRadioItem (submenu3, 0, static_cast<UINT>(interval_vec.size ()), 0, MF_BYPOSITION);

					POINT pt = {0};
					GetCursorPos (&pt);

					TrackPopupMenuEx (submenu, TPM_RIGHTBUTTON | TPM_LEFTBUTTON, pt.x, pt.y, hwnd, nullptr);

					DestroyMenu (menu);

					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			if (HIWORD (wparam) == 0 && LOWORD (wparam) >= IDX_LANGUAGE && LOWORD (wparam) <= IDX_LANGUAGE + app.LocaleGetCount ())
			{
				app.LocaleApplyFromMenu (GetSubMenu (GetSubMenu (GetMenu (hwnd), 1), LANG_MENU), LOWORD (wparam), IDX_LANGUAGE);
				return FALSE;
			}
			else if ((LOWORD (wparam) >= IDX_TRAY_POPUP_1 && LOWORD (wparam) <= IDX_TRAY_POPUP_1 + limit_vec.size ()))
			{
				const size_t idx = (LOWORD (wparam) - IDX_TRAY_POPUP_1);

				app.ConfigSet (L"AutoreductEnable", true);
				app.ConfigSet (L"AutoreductValue", (DWORD)limit_vec.at (idx));

				return FALSE;
			}
			else if ((LOWORD (wparam) >= IDX_TRAY_POPUP_2 && LOWORD (wparam) <= IDX_TRAY_POPUP_2 + interval_vec.size ()))
			{
				const size_t idx = (LOWORD (wparam) - IDX_TRAY_POPUP_2);

				app.ConfigSet (L"AutoreductIntervalEnable", true);
				app.ConfigSet (L"AutoreductIntervalValue", (DWORD)interval_vec.at (idx));

				return FALSE;
			}

			switch (LOWORD (wparam))
			{
				case IDM_ALWAYSONTOP_CHK:
				{
					const bool new_val = !app.ConfigGet (L"AlwaysOnTop", false).AsBool ();

					CheckMenuItem (GetMenu (hwnd), LOWORD (wparam), MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"AlwaysOnTop", new_val);

					_r_wnd_top (hwnd, new_val);

					break;
				}

				case IDM_STARTMINIMIZED_CHK:
				{
					const bool new_val = !app.ConfigGet (L"IsStartMinimized", false).AsBool ();

					CheckMenuItem (GetMenu (hwnd), LOWORD (wparam), MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"IsStartMinimized", new_val);

					break;
				}

				case IDM_REDUCTCONFIRMATION_CHK:
				{
					const bool new_val = !app.ConfigGet (L"IsShowReductConfirmation", true).AsBool ();

					CheckMenuItem (GetMenu (hwnd), LOWORD (wparam), MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"IsShowReductConfirmation", new_val);

					break;
				}

				case IDM_LOADONSTARTUP_CHK:
				{
					const bool new_val = !app.AutorunIsEnabled ();

					app.AutorunEnable (new_val);
					CheckMenuItem (GetMenu (hwnd), LOWORD (wparam), MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));

					break;
				}

				case IDM_CHECKUPDATES_CHK:
				{
					const bool new_val = !app.ConfigGet (L"CheckUpdates", true).AsBool ();

					CheckMenuItem (GetMenu (hwnd), LOWORD (wparam), MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"CheckUpdates", new_val);

					break;
				}

				case IDM_WORKINGSET_CHK:
				case IDM_SYSTEMWORKINGSET_CHK:
				case IDM_STANDBYLISTPRIORITY0_CHK:
				case IDM_STANDBYLIST_CHK:
				case IDM_MODIFIEDLIST_CHK:
				case IDM_COMBINEMEMORYLISTS_CHK:
				{
					const UINT ctrl_id = LOWORD (wparam);
					const DWORD mask = app.ConfigGet (L"ReductMask2", REDUCT_MASK_DEFAULT).AsUlong ();
					DWORD new_mask = 0;

					if (ctrl_id == IDM_WORKINGSET_CHK)
						new_mask = REDUCT_WORKING_SET;

					else if (ctrl_id == IDM_SYSTEMWORKINGSET_CHK)
						new_mask = REDUCT_SYSTEM_WORKING_SET;

					else if (ctrl_id == IDM_STANDBYLISTPRIORITY0_CHK)
						new_mask = REDUCT_STANDBY_PRIORITY0_LIST;

					else if (ctrl_id == IDM_STANDBYLIST_CHK)
						new_mask = REDUCT_STANDBY_LIST;

					else if (ctrl_id == IDM_MODIFIEDLIST_CHK)
						new_mask = REDUCT_MODIFIED_LIST;

					else if (ctrl_id == IDM_COMBINEMEMORYLISTS_CHK)
						new_mask = REDUCT_COMBINE_MEMORY_LISTS;

					if (
						(ctrl_id == IDM_STANDBYLIST_CHK && (mask & REDUCT_STANDBY_LIST) == 0) ||
						(ctrl_id == IDM_MODIFIEDLIST_CHK && (mask & REDUCT_MODIFIED_LIST) == 0)
						)
					{
						if (!app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_WARNING, nullptr), L"IsShowWarningConfirmation"))
							return FALSE;
					}

					app.ConfigSet (L"ReductMask2", (mask & new_mask) != 0 ? (mask & ~new_mask) : (mask | new_mask));

					break;
				}

				case IDM_TRAY_DISABLE_1:
				{
					app.ConfigSet (L"AutoreductEnable", !app.ConfigGet (L"AutoreductEnable", false).AsBool ());
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
					app.CreateSettingsWindow (&SettingsProc);
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

						app.TrayPopup (hwnd, UID, nullptr, NIIF_ERROR, APP_NAME, app.LocaleString (IDS_STATUS_NOPRIVILEGES, nullptr));
					}
					else
					{
						_app_memoryclean (hwnd, false);
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

				case IDM_CHECKUPDATES:
				{
					app.UpdateCheck (true);
					break;
				}

				case IDM_ABOUT:
				case IDM_TRAY_ABOUT:
				{
					app.CreateAboutWindow (hwnd);
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
	MSG msg = {0};

	if (app.CreateMainWindow (IDD_MAIN, IDI_MAIN, &DlgProc))
	{
		const HACCEL haccel = LoadAccelerators (app.GetHINSTANCE (), MAKEINTRESOURCE (IDA_MAIN));

		if (haccel)
		{
			while (GetMessage (&msg, nullptr, 0, 0) > 0)
			{
				TranslateAccelerator (app.GetHWND (), haccel, &msg);

				if (!IsDialogMessage (app.GetHWND (), &msg))
				{
					TranslateMessage (&msg);
					DispatchMessage (&msg);
				}
			}

			DestroyAcceleratorTable (haccel);
		}
	}

	return (INT)msg.wParam;
}
