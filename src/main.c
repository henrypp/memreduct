// Mem Reduct
// Copyright (c) 2011-2021 Henry++

#include "routine.h"

#include "main.h"
#include "rapp.h"

#include "resource.h"

STATIC_DATA config;

RECT icon_rc;
MEMORYINFO mem_info;

UINT limits[10];
UINT intervals[10];

VOID generate_menu_array (PUINT integers, SIZE_T size, UINT val)
{
	// 10
	for (UINT i = 0; i < size; i++)
	{
		integers[i] = (i + 1) * 10;

		for (UINT j = 0; j < size; j++)
		{
		}
	}

	for (UINT i = 0; i < size; i++)
	{
		if (integers[i] <= val && integers[i + 1] >= val)
			integers[i] = val;
	}

	//for (UINT i = 0; i < size; i++)
	//{
	//	integers[i] = (i + 1) * 10;
	//}

	//for (UINT i = val - 2; i <= (val + 2); i++)
	//{
	//	if (i >= 5)
	//		integers[i] = i;
	//}
}

ULONG _app_memorystatus (PMEMORYINFO pinfo)
{
	MEMORYSTATUSEX msex = {0};
	msex.dwLength = sizeof (msex);

	if (!GlobalMemoryStatusEx (&msex))
		return 0;

	if (pinfo)
	{
		// physical memory information
		pinfo->percent_phys = msex.dwMemoryLoad;

		pinfo->free_phys = msex.ullAvailPhys;
		pinfo->total_phys = msex.ullTotalPhys;

		// virtual memory information
		pinfo->percent_page = (ULONG)_r_calc_percentof64 (msex.ullTotalPageFile - msex.ullAvailPageFile, msex.ullTotalPageFile);

		pinfo->free_page = msex.ullAvailPageFile;
		pinfo->total_page = msex.ullTotalPageFile;

		// system cache information
		SYSTEM_CACHE_INFORMATION sci = {0};

		if (NT_SUCCESS (NtQuerySystemInformation (SystemFileCacheInformation, &sci, sizeof (sci), NULL)))
		{
			pinfo->percent_ws = (ULONG)_r_calc_percentof64 (sci.CurrentSize, sci.PeakSize);

			pinfo->free_ws = (sci.PeakSize - sci.CurrentSize);
			pinfo->total_ws = sci.PeakSize;
		}
	}

	return msex.dwMemoryLoad;
}

ULONG64 _app_memoryclean (HWND hwnd, BOOLEAN is_preventfreezes)
{
	MEMORYINFO info;
	MEMORY_COMBINE_INFORMATION_EX combine_info;
	SYSTEM_CACHE_INFORMATION sci;
	SYSTEM_MEMORY_LIST_COMMAND command;
	WCHAR result_string[128];
	HCURSOR hprev_cursor;
	ULONG64 reduct_before;
	ULONG64 reduct_after;
	ULONG mask;

	if (!_r_sys_iselevated ())
	{
		_r_tray_popup (_r_app_gethwnd (), UID, NIIF_ERROR | (_r_config_getboolean (L"IsNotificationsSound", TRUE) ? 0 : NIIF_NOSOUND), APP_NAME, _r_locale_getstring (IDS_STATUS_NOPRIVILEGES));
		return 0;
	}

	mask = _r_config_getulong (L"ReductMask2", REDUCT_MASK_DEFAULT);

	if (is_preventfreezes)
		mask &= ~REDUCT_MASK_FREEZES; // exclude freezes for autoclean feature ;)

	if (hwnd && !_r_show_confirmmessage (hwnd, NULL, _r_locale_getstring (IDS_QUESTION), L"IsShowReductConfirmation"))
		return 0;


	hprev_cursor = SetCursor (LoadCursor (NULL, IDC_WAIT));

	// difference (before)
	memset (&info, 0, sizeof (info));
	_app_memorystatus (&info);

	reduct_before = (info.total_phys - info.free_phys);

	// Combine memory lists (win81+)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_8_1))
	{
		if ((mask & REDUCT_COMBINE_MEMORY_LISTS) != 0)
		{
			memset (&combine_info, 0, sizeof (combine_info));

			NtSetSystemInformation (SystemCombinePhysicalMemoryInformation, &combine_info, sizeof (combine_info));
		}
	}

	// System working set
	if ((mask & REDUCT_SYSTEM_WORKING_SET) != 0)
	{
		memset (&sci, 0, sizeof (sci));

		sci.MinimumWorkingSet = (ULONG_PTR)-1;
		sci.MaximumWorkingSet = (ULONG_PTR)-1;

		NtSetSystemInformation (SystemFileCacheInformation, &sci, sizeof (sci));
	}

	if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
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
	}

	if (hprev_cursor)
		SetCursor (hprev_cursor);

	// difference (after)
	memset (&info, 0, sizeof (info));
	_app_memorystatus (&info);

	reduct_after = (info.total_phys - info.free_phys);

	if (reduct_after < reduct_before)
	{
		reduct_after = (reduct_before - reduct_after);
	}
	else
	{
		reduct_after = 0;
	}

	_r_config_setlong64 (L"StatisticLastReduct", _r_unixtime_now ()); // time of last cleaning

	_r_format_bytesize64 (result_string, RTL_NUMBER_OF (result_string), reduct_after);

	if (_r_config_getboolean (L"BalloonCleanResults", TRUE))
	{
		_r_tray_popupformat (_r_app_gethwnd (), UID, NIIF_INFO | (_r_config_getboolean (L"IsNotificationsSound", TRUE) ? 0 : NIIF_NOSOUND), APP_NAME, _r_locale_getstring (IDS_STATUS_CLEANED), result_string);
	}

	if (_r_config_getboolean (L"LogCleanResults", FALSE))
	{
		_r_log_v (LOG_LEVEL_INFO, 0, L"Memory cleaning result", 0, result_string);
	}

	return reduct_after;
}

VOID _app_fontinit (HWND hwnd, LPLOGFONT plf, INT scale)
{
	_r_config_getfont (L"TrayFont", hwnd, plf, NULL);

	if (scale > 1)
		plf->lfHeight *= scale;
}

VOID _app_drawbackground (HDC hdc, COLORREF pen_clr, COLORREF brush_clr, PRECT rect, BOOLEAN is_round)
{
	INT prev_mode = SetBkMode (hdc, TRANSPARENT);
	COLORREF prev_clr = SetBkColor (hdc, brush_clr);

	HGDIOBJ prev_brush = SelectObject (hdc, GetStockObject (DC_PEN));
	HGDIOBJ prev_pen = SelectObject (hdc, GetStockObject (DC_BRUSH));

	SetDCPenColor (hdc, pen_clr);
	SetDCBrushColor (hdc, brush_clr);

	if (is_round)
	{
		Ellipse (hdc, rect->left, rect->top, rect->right, rect->bottom);
	}
	else
	{
		Rectangle (hdc, rect->left, rect->top, rect->right, rect->bottom);
	}

	SelectObject (hdc, prev_brush);
	SelectObject (hdc, prev_pen);

	SetBkColor (hdc, prev_clr);
	SetBkMode (hdc, prev_mode);
}

VOID _app_drawtext (HDC hdc, LPWSTR text, PRECT lprect, COLORREF clr, BOOLEAN is_aa)
{
	COLORREF prev_clr = SetTextColor (hdc, clr);
	INT prev_mode = SetBkMode (hdc, TRANSPARENT);

	INT length = (INT)(INT_PTR)_r_str_length (text);

	DrawTextEx (hdc, text, length, lprect, DT_VCENTER | DT_CENTER | DT_SINGLELINE | DT_NOCLIP | DT_NOPREFIX, NULL);

	if (is_aa)
		DrawTextEx (hdc, text, length, lprect, DT_VCENTER | DT_CENTER | DT_SINGLELINE | DT_NOCLIP | DT_NOPREFIX, NULL);

	SetBkMode (hdc, prev_mode);
	SetTextColor (hdc, prev_clr);
}

HICON _app_iconcreate ()
{
	COLORREF text_color = _r_config_getulong (L"TrayColorText", TRAY_COLOR_TEXT);
	COLORREF bg_color = _r_config_getulong (L"TrayColorBg", TRAY_COLOR_BG);

	BOOLEAN is_transparent = _r_config_getboolean (L"TrayUseTransparency", FALSE);
	BOOLEAN is_border = _r_config_getboolean (L"TrayShowBorder", FALSE);
	BOOLEAN is_round = _r_config_getboolean (L"TrayRoundCorners", FALSE);

	BOOLEAN has_danger = mem_info.percent_phys >= _r_config_getulong (L"TrayLevelDanger", DEFAULT_DANGER_LEVEL);
	BOOLEAN has_warning = has_danger || mem_info.percent_phys >= _r_config_getulong (L"TrayLevelWarning", DEFAULT_WARNING_LEVEL);

	if (has_danger || has_warning)
	{
		if (_r_config_getboolean (L"TrayChangeBg", TRUE))
		{
			bg_color = has_danger ? _r_config_getulong (L"TrayColorDanger", TRAY_COLOR_DANGER) : _r_config_getulong (L"TrayColorWarning", TRAY_COLOR_WARNING);
			is_transparent = FALSE;
		}
		else
		{
			text_color = has_danger ? _r_config_getulong (L"TrayColorDanger", TRAY_COLOR_DANGER) : _r_config_getulong (L"TrayColorWarning", TRAY_COLOR_WARNING);
		}
	}

	// select bitmap
	HGDIOBJ prev_bmp = SelectObject (config.hdc, config.hbitmap);
	HGDIOBJ prev_font = SelectObject (config.hdc, config.hfont);

	HGDIOBJ prev_bmp_mask = SelectObject (config.hdc_mask, config.hbitmap_mask);
	HGDIOBJ prev_font_mask = SelectObject (config.hdc_mask, config.hfont);

	PatBlt (config.hdc, icon_rc.left, icon_rc.top, icon_rc.right, icon_rc.bottom, BLACKNESS);
	PatBlt (config.hdc_mask, icon_rc.left, icon_rc.top, icon_rc.right, icon_rc.bottom, WHITENESS);

	_app_drawbackground (config.hdc, is_border ? text_color : bg_color, is_transparent ? RGB (0, 0, 0) : bg_color, &icon_rc, is_round);

	if (is_transparent)
		_app_drawbackground (config.hdc_mask, is_border ? RGB (0, 0, 0) : RGB (255, 255, 255), text_color, &icon_rc, is_round);

	// draw text
	{
		WCHAR buffer[8] = {0};
		_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%" PR_ULONG, mem_info.percent_phys);

		_app_drawtext (config.hdc, buffer, &icon_rc, text_color, FALSE);

		if (is_transparent)
			_app_drawtext (config.hdc_mask, buffer, &icon_rc, RGB (0, 0, 0), FALSE);
	}

	//if (is_transparent)
	//{
	//	// Fill blend function and blend new text to window
	//	BLENDFUNCTION bf;

	//	bf.BlendOp = AC_SRC_OVER;
	//	bf.BlendFlags = 0;
	//	bf.SourceConstantAlpha = 255;
	//	bf.AlphaFormat = AC_SRC_ALPHA;

	//	AlphaBlend (config.hdc_mask, icon_rc.left, icon_rc.top, icon_rc.right, icon_rc.bottom, config.hdc, icon_rc.left, icon_rc.top, icon_rc.right, icon_rc.bottom, bf);
	//}

	//BitBlt (config.hdc_mask, 0, 0, icon_rc.right, icon_rc.bottom, config.hdc, icon_rc.right, icon_rc.bottom, PATINVERT);

	SelectObject (config.hdc, prev_font);
	SelectObject (config.hdc, prev_bmp);

	SelectObject (config.hdc_mask, prev_font_mask);
	SelectObject (config.hdc_mask, prev_bmp_mask);

	// finalize icon
	ICONINFO ii = {0};

	ii.fIcon = TRUE;
	ii.hbmColor = config.hbitmap;
	ii.hbmMask = config.hbitmap_mask;

	SAFE_DELETE_ICON (config.htrayicon);

	config.htrayicon = CreateIconIndirect (&ii);

	return config.htrayicon;
}

VOID CALLBACK _app_timercallback (HWND hwnd, UINT uMsg, UINT_PTR idEvent, ULONG dwTime)
{
	_app_memorystatus (&mem_info);

	// autoreduct functional
	if (_r_sys_iselevated ())
	{
		if ((_r_config_getboolean (L"AutoreductEnable", FALSE) && mem_info.percent_phys >= _r_config_getuinteger (L"AutoreductValue", DEFAULT_AUTOREDUCT_VAL)) ||
			(_r_config_getboolean (L"AutoreductIntervalEnable", FALSE) && (_r_unixtime_now () - _r_config_getlong64 (L"StatisticLastReduct", 0)) >= (_r_config_getlong64 (L"AutoreductIntervalValue", DEFAULT_AUTOREDUCTINTERVAL_VAL) * 60)))
		{
			_app_memoryclean (NULL, TRUE);
		}
	}

	// refresh tray information
	{
		HICON hicon = NULL;

		// check previous percent to prevent icon redraw
		if (!config.ms_prev || config.ms_prev != mem_info.percent_phys)
		{
			config.ms_prev = mem_info.percent_phys; // store last percentage value (required!)
			hicon = _app_iconcreate ();
		}

		_r_tray_setinfoformat (hwnd, UID, hicon, L"%s: %" PR_ULONG L"%%\r\n%s: %" PR_ULONG L"%%\r\n%s: %" PR_ULONG L"%%",
							   _r_locale_getstring (IDS_GROUP_1),
							   mem_info.percent_phys,
							   _r_locale_getstring (IDS_GROUP_2),
							   mem_info.percent_page,
							   _r_locale_getstring (IDS_GROUP_3),
							   mem_info.percent_ws
		);
	}

	// refresh listview information
	if (IsWindowVisible (hwnd))
	{
		// set item lparam information
		for (INT i = 0; i < _r_listview_getitemcount (hwnd, IDC_LISTVIEW); i++)
		{
			ULONG percent = 0;

			if (i < 3)
				percent = mem_info.percent_phys;

			else if (i < 6)
				percent = mem_info.percent_page;

			else if (i < 9)
				percent = mem_info.percent_ws;

			_r_listview_setitemex (hwnd, IDC_LISTVIEW, i, 0, NULL, I_IMAGENONE, I_GROUPIDNONE, (LPARAM)percent);
		}

		WCHAR format_string[256];

		_r_str_printf (format_string, RTL_NUMBER_OF (format_string), L"%" PR_ULONG L"%%", mem_info.percent_phys);
		_r_listview_setitem (hwnd, IDC_LISTVIEW, 0, 1, format_string);

		_r_format_bytesize64 (format_string, RTL_NUMBER_OF (format_string), mem_info.free_phys);
		_r_listview_setitem (hwnd, IDC_LISTVIEW, 1, 1, format_string);

		_r_format_bytesize64 (format_string, RTL_NUMBER_OF (format_string), mem_info.total_phys);
		_r_listview_setitem (hwnd, IDC_LISTVIEW, 2, 1, format_string);

		_r_str_printf (format_string, RTL_NUMBER_OF (format_string), L"%" PR_ULONG L"%%", mem_info.percent_page);
		_r_listview_setitem (hwnd, IDC_LISTVIEW, 3, 1, format_string);

		_r_format_bytesize64 (format_string, RTL_NUMBER_OF (format_string), mem_info.free_page);
		_r_listview_setitem (hwnd, IDC_LISTVIEW, 4, 1, format_string);

		_r_format_bytesize64 (format_string, RTL_NUMBER_OF (format_string), mem_info.total_page);
		_r_listview_setitem (hwnd, IDC_LISTVIEW, 5, 1, format_string);

		_r_str_printf (format_string, RTL_NUMBER_OF (format_string), L"%" PR_ULONG L"%%", mem_info.percent_ws);
		_r_listview_setitem (hwnd, IDC_LISTVIEW, 6, 1, format_string);

		_r_format_bytesize64 (format_string, RTL_NUMBER_OF (format_string), mem_info.free_ws);
		_r_listview_setitem (hwnd, IDC_LISTVIEW, 7, 1, format_string);

		_r_format_bytesize64 (format_string, RTL_NUMBER_OF (format_string), mem_info.total_ws);
		_r_listview_setitem (hwnd, IDC_LISTVIEW, 8, 1, format_string);

		_r_listview_redraw (hwnd, IDC_LISTVIEW, -1);
	}
}

VOID _app_iconredraw (HWND hwnd)
{
	config.ms_prev = 0;

	if (hwnd)
		_app_timercallback (hwnd, 0, 0, 0);
}

VOID _app_iconinit (HWND hwnd)
{
	SAFE_DELETE_OBJECT (config.hfont);
	SAFE_DELETE_OBJECT (config.hbitmap);
	SAFE_DELETE_OBJECT (config.hbitmap_mask);

	SAFE_DELETE_DC (config.hdc);
	SAFE_DELETE_DC (config.hdc_mask);

	// common init
	config.scale = _r_config_getboolean (L"TrayUseAntialiasing", FALSE) ? 16 : 1;

	// init font
	LOGFONT lf = {0};

	_app_fontinit (hwnd, &lf, config.scale);

	config.hfont = CreateFontIndirect (&lf);

	// init rect
	SetRect (&icon_rc,
			 0,
			 0,
			 _r_dc_getsystemmetrics (hwnd, SM_CXSMICON) * config.scale,
			 _r_dc_getsystemmetrics (hwnd, SM_CYSMICON) * config.scale
	);

	// init dc
	HDC hdc = GetDC (NULL);

	if (hdc)
	{
		config.hdc = CreateCompatibleDC (hdc);
		config.hdc_mask = CreateCompatibleDC (hdc);

		// init bitmap
		BITMAPV5HEADER bi;
		memset (&bi, 0, sizeof (bi));

		bi.bV5Size = sizeof (bi);
		bi.bV5Width = icon_rc.right;
		bi.bV5Height = icon_rc.bottom;
		bi.bV5Planes = 1;
		bi.bV5BitCount = 32;
		bi.bV5Compression = BI_BITFIELDS;

		// The following mask specification specifies a supported 32 BPP
		// alpha format for Windows XP.
		bi.bV5RedMask = 0x00FF0000;
		bi.bV5GreenMask = 0x0000FF00;
		bi.bV5BlueMask = 0x000000FF;
		bi.bV5AlphaMask = 0xFF000000;

		config.hbitmap = CreateDIBSection (config.hdc, (LPBITMAPINFO)&bi, DIB_RGB_COLORS, NULL, NULL, 0);
		config.hbitmap_mask = CreateDIBSection (config.hdc_mask, (LPBITMAPINFO)&bi, DIB_RGB_COLORS, NULL, NULL, 0);
		//config.hbitmap_mask = CreateBitmap (icon_rc.right, icon_rc.bottom, 1, 1, NULL);

		ReleaseDC (NULL, hdc);
	}
}

VOID _app_hotkeyinit (HWND hwnd)
{
	if (!_r_sys_iselevated ())
		return;

	UnregisterHotKey (hwnd, UID);

	if (_r_config_getboolean (L"HotkeyCleanEnable", FALSE))
	{
		UINT hk = _r_config_getuinteger (L"HotkeyClean", MAKEWORD (VK_F1, HOTKEYF_CONTROL));

		if (hk)
			RegisterHotKey (hwnd, UID, (HIBYTE (hk) & 2) | ((HIBYTE (hk) & 4) >> 2) | ((HIBYTE (hk) & 1) << 2), LOBYTE (hk));
	}
}

INT_PTR CALLBACK SettingsProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		case RM_INITIALIZE:
		{
			INT dialog_id = (INT)wparam;

			switch (dialog_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
					CheckDlgButton (hwnd, IDC_ALWAYSONTOP_CHK, _r_config_getboolean (L"AlwaysOnTop", APP_ALWAYSONTOP) ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_LOADONSTARTUP_CHK, _r_autorun_isenabled () ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_STARTMINIMIZED_CHK, _r_config_getboolean (L"IsStartMinimized", FALSE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_REDUCTCONFIRMATION_CHK, _r_config_getboolean (L"IsShowReductConfirmation", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					if (!_r_sys_iselevated () || !_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
						_r_ctrl_enable (hwnd, IDC_SKIPUACWARNING_CHK, FALSE);

					CheckDlgButton (hwnd, IDC_SKIPUACWARNING_CHK, _r_skipuac_isenabled () ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_CHECKUPDATES_CHK, _r_config_getboolean (L"CheckUpdates", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					_r_locale_enum (hwnd, IDC_LANGUAGE, FALSE);

					break;
				}

				case IDD_SETTINGS_MEMORY:
				{
					if (!_r_sys_iselevated () || !_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
					{
						_r_ctrl_enable (hwnd, IDC_WORKINGSET_CHK, FALSE);
						_r_ctrl_enable (hwnd, IDC_STANDBYLISTPRIORITY0_CHK, FALSE);
						_r_ctrl_enable (hwnd, IDC_STANDBYLIST_CHK, FALSE);
						_r_ctrl_enable (hwnd, IDC_MODIFIEDLIST_CHK, FALSE);

						if (!_r_sys_iselevated ())
						{
							_r_ctrl_enable (hwnd, IDC_SYSTEMWORKINGSET_CHK, FALSE);
							_r_ctrl_enable (hwnd, IDC_AUTOREDUCTENABLE_CHK, FALSE);
							_r_ctrl_enable (hwnd, IDC_AUTOREDUCTINTERVALENABLE_CHK, FALSE);
							_r_ctrl_enable (hwnd, IDC_HOTKEY_CLEAN_CHK, FALSE);
						}
					}

					// Combine memory lists (win81+)
					if (!_r_sys_iselevated () || !_r_sys_isosversiongreaterorequal (WINDOWS_8_1))
						_r_ctrl_enable (hwnd, IDC_COMBINEMEMORYLISTS_CHK, FALSE);

					ULONG mask = _r_config_getulong (L"ReductMask2", REDUCT_MASK_DEFAULT);

					CheckDlgButton (hwnd, IDC_WORKINGSET_CHK, ((mask & REDUCT_WORKING_SET) != 0) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_SYSTEMWORKINGSET_CHK, ((mask & REDUCT_SYSTEM_WORKING_SET) != 0) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_STANDBYLISTPRIORITY0_CHK, ((mask & REDUCT_STANDBY_PRIORITY0_LIST) != 0) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_STANDBYLIST_CHK, ((mask & REDUCT_STANDBY_LIST) != 0) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_MODIFIEDLIST_CHK, ((mask & REDUCT_MODIFIED_LIST) != 0) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_COMBINEMEMORYLISTS_CHK, ((mask & REDUCT_COMBINE_MEMORY_LISTS) != 0) ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_AUTOREDUCTENABLE_CHK, _r_config_getboolean (L"AutoreductEnable", FALSE) ? BST_CHECKED : BST_UNCHECKED);

					SendDlgItemMessage (hwnd, IDC_AUTOREDUCTVALUE, UDM_SETRANGE32, 10, 99);
					SendDlgItemMessage (hwnd, IDC_AUTOREDUCTVALUE, UDM_SETPOS32, 0, (WPARAM)_r_config_getuinteger (L"AutoreductValue", DEFAULT_AUTOREDUCT_VAL));

					CheckDlgButton (hwnd, IDC_AUTOREDUCTINTERVALENABLE_CHK, _r_config_getboolean (L"AutoreductIntervalEnable", FALSE) ? BST_CHECKED : BST_UNCHECKED);

					SendDlgItemMessage (hwnd, IDC_AUTOREDUCTINTERVALVALUE, UDM_SETRANGE32, 5, 1440);
					SendDlgItemMessage (hwnd, IDC_AUTOREDUCTINTERVALVALUE, UDM_SETPOS32, 0, (WPARAM)_r_config_getuinteger (L"AutoreductIntervalValue", DEFAULT_AUTOREDUCTINTERVAL_VAL));

					CheckDlgButton (hwnd, IDC_HOTKEY_CLEAN_CHK, _r_config_getboolean (L"HotkeyCleanEnable", FALSE) ? BST_CHECKED : BST_UNCHECKED);

					SendDlgItemMessage (hwnd, IDC_HOTKEY_CLEAN, HKM_SETHOTKEY, (WPARAM)_r_config_getuinteger (L"HotkeyClean", MAKEWORD (VK_F1, HOTKEYF_CONTROL)), 0);

					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_AUTOREDUCTENABLE_CHK, 0), 0);
					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_AUTOREDUCTINTERVALENABLE_CHK, 0), 0);
					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_HOTKEY_CLEAN_CHK, 0), 0);

					break;
				}

				case IDD_SETTINGS_APPEARANCE:
				{
					CheckDlgButton (hwnd, IDC_TRAYUSETRANSPARENCY_CHK, _r_config_getboolean (L"TrayUseTransparency", FALSE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_TRAYSHOWBORDER_CHK, _r_config_getboolean (L"TrayShowBorder", FALSE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_TRAYROUNDCORNERS_CHK, _r_config_getboolean (L"TrayRoundCorners", FALSE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_TRAYCHANGEBG_CHK, _r_config_getboolean (L"TrayChangeBg", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_TRAYUSEANTIALIASING_CHK, _r_config_getboolean (L"TrayUseAntialiasing", FALSE) ? BST_CHECKED : BST_UNCHECKED);

					{
						LOGFONT lf = {0};

						_app_fontinit (hwnd, &lf, 0);

						_r_ctrl_settextformat (hwnd, IDC_FONT, L"%s, %" PRIi32 L"px", lf.lfFaceName, _r_dc_fontheighttosize (hwnd, lf.lfHeight));
					}

					SetWindowLongPtr (GetDlgItem (hwnd, IDC_COLOR_TEXT), GWLP_USERDATA, (LONG_PTR)_r_config_getulong (L"TrayColorText", TRAY_COLOR_TEXT));
					SetWindowLongPtr (GetDlgItem (hwnd, IDC_COLOR_BACKGROUND), GWLP_USERDATA, (LONG_PTR)_r_config_getulong (L"TrayColorBg", TRAY_COLOR_BG));
					SetWindowLongPtr (GetDlgItem (hwnd, IDC_COLOR_WARNING), GWLP_USERDATA, (LONG_PTR)_r_config_getulong (L"TrayColorWarning", TRAY_COLOR_WARNING));
					SetWindowLongPtr (GetDlgItem (hwnd, IDC_COLOR_DANGER), GWLP_USERDATA, (LONG_PTR)_r_config_getulong (L"TrayColorDanger", TRAY_COLOR_DANGER));

					break;
				}

				case IDD_SETTINGS_TRAY:
				{
					SendDlgItemMessage (hwnd, IDC_TRAYLEVELWARNING, UDM_SETRANGE32, 10, 99);
					SendDlgItemMessage (hwnd, IDC_TRAYLEVELWARNING, UDM_SETPOS32, 0, (WPARAM)_r_config_getuinteger (L"TrayLevelWarning", DEFAULT_WARNING_LEVEL));

					SendDlgItemMessage (hwnd, IDC_TRAYLEVELDANGER, UDM_SETRANGE32, 10, 99);
					SendDlgItemMessage (hwnd, IDC_TRAYLEVELDANGER, UDM_SETPOS32, 0, (WPARAM)_r_config_getuinteger (L"TrayLevelDanger", DEFAULT_DANGER_LEVEL));

					SendDlgItemMessage (hwnd, IDC_TRAYACTIONDC, CB_SETCURSEL, (WPARAM)_r_config_getinteger (L"TrayActionDc", 0), 0);
					SendDlgItemMessage (hwnd, IDC_TRAYACTIONMC, CB_SETCURSEL, (WPARAM)_r_config_getinteger (L"TrayActionMc", 1), 0);

					CheckDlgButton (hwnd, IDC_TRAYICONSINGLECLICK_CHK, _r_config_getboolean (L"IsTrayIconSingleClick", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_SHOW_CLEAN_RESULT_CHK, _r_config_getboolean (L"BalloonCleanResults", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					break;
				}
			}

			break;
		}

		case RM_LOCALIZE:
		{
			INT dialog_id = (INT)wparam;

			// localize titles
			_r_ctrl_settextformat (hwnd, IDC_TITLE_1, L"%s:", _r_locale_getstring (IDS_TITLE_1));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_2, L"%s: (Language)", _r_locale_getstring (IDS_TITLE_2));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_3, L"%s:", _r_locale_getstring (IDS_TITLE_3));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_4, L"%s:", _r_locale_getstring (IDS_TITLE_4));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_5, L"%s:", _r_locale_getstring (IDS_TITLE_5));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_6, L"%s:", _r_locale_getstring (IDS_TITLE_6));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_7, L"%s:", _r_locale_getstring (IDS_TITLE_7));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_8, L"%s:", _r_locale_getstring (IDS_TITLE_8));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_9, L"%s:", _r_locale_getstring (IDS_TITLE_9));

			switch (dialog_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
					_r_ctrl_settext (hwnd, IDC_ALWAYSONTOP_CHK, _r_locale_getstring (IDS_ALWAYSONTOP_CHK));
					_r_ctrl_settext (hwnd, IDC_LOADONSTARTUP_CHK, _r_locale_getstring (IDS_LOADONSTARTUP_CHK));
					_r_ctrl_settext (hwnd, IDC_STARTMINIMIZED_CHK, _r_locale_getstring (IDS_STARTMINIMIZED_CHK));
					_r_ctrl_settext (hwnd, IDC_REDUCTCONFIRMATION_CHK, _r_locale_getstring (IDS_REDUCTCONFIRMATION_CHK));
					_r_ctrl_settext (hwnd, IDC_SKIPUACWARNING_CHK, _r_locale_getstring (IDS_SKIPUACWARNING_CHK));
					_r_ctrl_settext (hwnd, IDC_CHECKUPDATES_CHK, _r_locale_getstring (IDS_CHECKUPDATES_CHK));

					_r_ctrl_settext (hwnd, IDC_LANGUAGE_HINT, _r_locale_getstring (IDS_LANGUAGE_HINT));

					break;
				}

				case IDD_SETTINGS_MEMORY:
				{
					_r_ctrl_settextformat (hwnd, IDC_WORKINGSET_CHK, L"%s (vista+)", _r_locale_getstring (IDS_WORKINGSET_CHK));
					_r_ctrl_settext (hwnd, IDC_SYSTEMWORKINGSET_CHK, _r_locale_getstring (IDS_SYSTEMWORKINGSET_CHK));
					_r_ctrl_settextformat (hwnd, IDC_STANDBYLISTPRIORITY0_CHK, L"%s (vista+)", _r_locale_getstring (IDS_STANDBYLISTPRIORITY0_CHK));
					_r_ctrl_settextformat (hwnd, IDC_STANDBYLIST_CHK, L"%s (vista+)", _r_locale_getstring (IDS_STANDBYLIST_CHK));
					_r_ctrl_settextformat (hwnd, IDC_MODIFIEDLIST_CHK, L"%s (vista+)", _r_locale_getstring (IDS_MODIFIEDLIST_CHK));
					_r_ctrl_settextformat (hwnd, IDC_COMBINEMEMORYLISTS_CHK, L"%s (win81+)", _r_locale_getstring (IDS_COMBINEMEMORYLISTS_CHK));

					_r_ctrl_settext (hwnd, IDC_AUTOREDUCTENABLE_CHK, _r_locale_getstring (IDS_AUTOREDUCTENABLE_CHK));
					_r_ctrl_settext (hwnd, IDC_AUTOREDUCTINTERVALENABLE_CHK, _r_locale_getstring (IDS_AUTOREDUCTINTERVALENABLE_CHK));

					_r_ctrl_settext (hwnd, IDC_HOTKEY_CLEAN_CHK, _r_locale_getstring (IDS_HOTKEY_CLEAN_CHK));

					break;
				}

				case IDD_SETTINGS_APPEARANCE:
				{
					_r_ctrl_settext (hwnd, IDC_TRAYUSETRANSPARENCY_CHK, _r_locale_getstring (IDS_TRAYUSETRANSPARENCY_CHK));
					_r_ctrl_settext (hwnd, IDC_TRAYSHOWBORDER_CHK, _r_locale_getstring (IDS_TRAYSHOWBORDER_CHK));
					_r_ctrl_settext (hwnd, IDC_TRAYROUNDCORNERS_CHK, _r_locale_getstring (IDS_TRAYROUNDCORNERS_CHK));
					_r_ctrl_settext (hwnd, IDC_TRAYCHANGEBG_CHK, _r_locale_getstring (IDS_TRAYCHANGEBG_CHK));
					_r_ctrl_settext (hwnd, IDC_TRAYUSEANTIALIASING_CHK, _r_locale_getstring (IDS_TRAYUSEANTIALIASING_CHK));

					_r_ctrl_settext (hwnd, IDC_FONT_HINT, _r_locale_getstring (IDS_FONT_HINT));
					_r_ctrl_settext (hwnd, IDC_COLOR_TEXT_HINT, _r_locale_getstring (IDS_COLOR_TEXT_HINT));
					_r_ctrl_settext (hwnd, IDC_COLOR_BACKGROUND_HINT, _r_locale_getstring (IDS_COLOR_BACKGROUND_HINT));
					_r_ctrl_settext (hwnd, IDC_COLOR_WARNING_HINT, _r_locale_getstring (IDS_COLOR_WARNING_HINT));
					_r_ctrl_settext (hwnd, IDC_COLOR_DANGER_HINT, _r_locale_getstring (IDS_COLOR_DANGER_HINT));

					BOOLEAN is_classic = _r_app_isclassicui ();

					_r_wnd_addstyle (hwnd, IDC_FONT, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

					_r_wnd_addstyle (hwnd, IDC_COLOR_TEXT, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
					_r_wnd_addstyle (hwnd, IDC_COLOR_BACKGROUND, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
					_r_wnd_addstyle (hwnd, IDC_COLOR_WARNING, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
					_r_wnd_addstyle (hwnd, IDC_COLOR_DANGER, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

					break;
				}

				case IDD_SETTINGS_TRAY:
				{
					_r_ctrl_settext (hwnd, IDC_TRAYLEVELWARNING_HINT, _r_locale_getstring (IDS_TRAYLEVELWARNING_HINT));
					_r_ctrl_settext (hwnd, IDC_TRAYLEVELDANGER_HINT, _r_locale_getstring (IDS_TRAYLEVELDANGER_HINT));

					_r_ctrl_settext (hwnd, IDC_TRAYACTIONDC_HINT, _r_locale_getstring (IDS_TRAYACTIONDC_HINT));
					_r_ctrl_settext (hwnd, IDC_TRAYACTIONMC_HINT, _r_locale_getstring (IDS_TRAYACTIONMC_HINT));
					_r_ctrl_settext (hwnd, IDC_TRAYICONSINGLECLICK_CHK, _r_locale_getstring (IDS_TRAYICONSINGLECLICK_CHK));

					SendDlgItemMessage (hwnd, IDC_TRAYACTIONDC, CB_RESETCONTENT, 0, 0);
					SendDlgItemMessage (hwnd, IDC_TRAYACTIONMC, CB_RESETCONTENT, 0, 0);

					for (INT i = 0; i < 3; i++)
					{
						LPCWSTR item = _r_locale_getstring (IDS_TRAY_ACTION_1 + i);

						SendDlgItemMessage (hwnd, IDC_TRAYACTIONDC, CB_INSERTSTRING, i, (LPARAM)item);
						SendDlgItemMessage (hwnd, IDC_TRAYACTIONMC, CB_INSERTSTRING, i, (LPARAM)item);
					}

					SendDlgItemMessage (hwnd, IDC_TRAYACTIONDC, CB_SETCURSEL, (WPARAM)_r_config_getinteger (L"TrayActionDc", 0), 0);
					SendDlgItemMessage (hwnd, IDC_TRAYACTIONMC, CB_SETCURSEL, (WPARAM)_r_config_getinteger (L"TrayActionMc", 1), 0);

					_r_ctrl_settext (hwnd, IDC_SHOW_CLEAN_RESULT_CHK, _r_locale_getstring (IDS_SHOW_CLEAN_RESULT_CHK));

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

					INT ctrl_id = (INT)(INT_PTR)nmlp->idFrom;

					if (ctrl_id == IDC_COLOR_TEXT ||
						ctrl_id == IDC_COLOR_BACKGROUND ||
						ctrl_id == IDC_COLOR_WARNING ||
						ctrl_id == IDC_COLOR_DANGER
						)
					{
						INT padding = _r_dc_getdpi (hwnd, 3);

						lpnmcd->rc.left += padding;
						lpnmcd->rc.top += padding;
						lpnmcd->rc.right -= padding;
						lpnmcd->rc.bottom -= padding;

						_r_dc_fillrect (lpnmcd->hdc, &lpnmcd->rc, (COLORREF)GetWindowLongPtr (nmlp->hwndFrom, GWLP_USERDATA));

						LONG_PTR result = CDRF_DODEFAULT | CDRF_DOERASE;

						SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result);
						return result;
					}

					break;
				}
			}

			break;
		}

		case WM_VSCROLL:
		case WM_HSCROLL:
		{
			INT ctrl_id = GetDlgCtrlID ((HWND)lparam);
			BOOLEAN is_stylechanged = FALSE;

			if (ctrl_id == IDC_AUTOREDUCTVALUE)
			{
				_r_config_setuinteger (L"AutoreductValue", (UINT)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));
			}
			else if (ctrl_id == IDC_AUTOREDUCTINTERVALVALUE)
			{
				_r_config_setuinteger (L"AutoreductIntervalValue", (UINT)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));
			}
			else if (ctrl_id == IDC_TRAYLEVELWARNING)
			{
				is_stylechanged = TRUE;
				_r_config_setuinteger (L"TrayLevelWarning", (UINT)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));
			}
			else if (ctrl_id == IDC_TRAYLEVELDANGER)
			{
				is_stylechanged = TRUE;
				_r_config_setuinteger (L"TrayLevelDanger", (UINT)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));
			}

			if (is_stylechanged)
			{
				_app_iconredraw (_r_app_gethwnd ());
				_r_listview_redraw (_r_app_gethwnd (), IDC_LISTVIEW, -1);
			}

			break;
		}

		case WM_COMMAND:
		{
			INT ctrl_id = LOWORD (wparam);
			INT notify_code = HIWORD (wparam);

			switch (ctrl_id)
			{
				case IDC_AUTOREDUCTVALUE_CTRL:
				{
					if (notify_code == EN_CHANGE)
						_r_config_setuinteger (L"AutoreductValue", (UINT)SendDlgItemMessage (hwnd, IDC_AUTOREDUCTVALUE, UDM_GETPOS32, 0, 0));

					break;
				}

				case IDC_AUTOREDUCTINTERVALVALUE_CTRL:
				{
					if (notify_code == EN_CHANGE)
						_r_config_setuinteger (L"AutoreductIntervalValue", (UINT)SendDlgItemMessage (hwnd, IDC_AUTOREDUCTINTERVALVALUE, UDM_GETPOS32, 0, 0));

					break;
				}

				case IDC_TRAYLEVELWARNING_CTRL:
				case IDC_TRAYLEVELDANGER_CTRL:
				{
					if (notify_code == EN_CHANGE)
					{
						if (ctrl_id == IDC_TRAYLEVELWARNING_CTRL)
							_r_config_setuinteger (L"TrayLevelWarning", (UINT)SendDlgItemMessage (hwnd, IDC_TRAYLEVELWARNING, UDM_GETPOS32, 0, 0));

						else if (ctrl_id == IDC_TRAYLEVELDANGER_CTRL)
							_r_config_setuinteger (L"TrayLevelDanger", (UINT)SendDlgItemMessage (hwnd, IDC_TRAYLEVELDANGER, UDM_GETPOS32, 0, 0));

						_app_iconredraw (_r_app_gethwnd ());
						_r_listview_redraw (_r_app_gethwnd (), IDC_LISTVIEW, -1);
					}

					break;
				}

				case IDC_ALWAYSONTOP_CHK:
				{
					BOOLEAN is_enable = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					_r_config_setboolean (L"AlwaysOnTop", is_enable);
					_r_menu_checkitem (GetMenu (_r_app_gethwnd ()), IDM_ALWAYSONTOP_CHK, 0, MF_BYCOMMAND, is_enable);

					break;
				}

				case IDC_LOADONSTARTUP_CHK:
				{
					BOOLEAN is_enable = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					_r_autorun_enable (hwnd, is_enable);

					_r_menu_checkitem (GetMenu (_r_app_gethwnd ()), IDM_LOADONSTARTUP_CHK, 0, MF_BYCOMMAND, is_enable);
					CheckDlgButton (hwnd, ctrl_id, _r_autorun_isenabled () ? BST_CHECKED : BST_UNCHECKED);

					break;
				}

				case IDC_STARTMINIMIZED_CHK:
				{
					BOOLEAN is_enable = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					_r_config_setboolean (L"IsStartMinimized", is_enable);
					_r_menu_checkitem (GetMenu (_r_app_gethwnd ()), IDM_STARTMINIMIZED_CHK, 0, MF_BYCOMMAND, is_enable);

					break;
				}

				case IDC_REDUCTCONFIRMATION_CHK:
				{
					BOOLEAN is_enable = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					_r_config_setboolean (L"IsShowReductConfirmation", is_enable);
					_r_menu_checkitem (GetMenu (_r_app_gethwnd ()), IDM_REDUCTCONFIRMATION_CHK, 0, MF_BYCOMMAND, is_enable);

					break;
				}

				case IDC_SKIPUACWARNING_CHK:
				{
					_r_skipuac_enable (hwnd, IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					CheckDlgButton (hwnd, ctrl_id, _r_skipuac_isenabled () ? BST_CHECKED : BST_UNCHECKED);

					break;
				}

				case IDC_CHECKUPDATES_CHK:
				{
					BOOLEAN is_enable = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					_r_config_setboolean (L"CheckUpdates", is_enable);
					_r_menu_checkitem (GetMenu (_r_app_gethwnd ()), IDM_CHECKUPDATES_CHK, 0, MF_BYCOMMAND, is_enable);

					break;
				}

				case IDC_LANGUAGE:
				{
					if (notify_code == CBN_SELCHANGE)
						_r_locale_applyfromcontrol (hwnd, ctrl_id);

					break;
				}

				case IDC_WORKINGSET_CHK:
				case IDC_SYSTEMWORKINGSET_CHK:
				case IDC_STANDBYLISTPRIORITY0_CHK:
				case IDC_STANDBYLIST_CHK:
				case IDC_MODIFIEDLIST_CHK:
				case IDC_COMBINEMEMORYLISTS_CHK:
				{
					ULONG mask = 0;

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

					if ((ctrl_id == IDC_STANDBYLIST_CHK || ctrl_id == IDC_MODIFIEDLIST_CHK) && (mask & REDUCT_MASK_FREEZES) != 0)
					{
						if (!_r_show_confirmmessage (hwnd, NULL, _r_locale_getstring (IDS_QUESTION_WARNING), L"IsShowWarningConfirmation"))
						{
							CheckDlgButton (hwnd, ctrl_id, BST_UNCHECKED);
							return FALSE;
						}
					}

					_r_config_setulong (L"ReductMask2", mask);

					break;
				}

				case IDC_AUTOREDUCTENABLE_CHK:
				{
					BOOLEAN is_enabled = _r_ctrl_isenabled (hwnd, ctrl_id);
					HWND hbuddy = (HWND)SendDlgItemMessage (hwnd, IDC_AUTOREDUCTVALUE, UDM_GETBUDDY, 0, 0);

					if (hbuddy)
						EnableWindow (hbuddy, is_enabled);

					if (is_enabled)
						_r_config_setboolean (L"AutoreductEnable", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));

					break;
				}

				case IDC_AUTOREDUCTINTERVALENABLE_CHK:
				{
					BOOLEAN is_enabled = _r_ctrl_isenabled (hwnd, ctrl_id);
					HWND hbuddy = (HWND)SendDlgItemMessage (hwnd, IDC_AUTOREDUCTINTERVALVALUE, UDM_GETBUDDY, 0, 0);

					if (hbuddy)
						EnableWindow (hbuddy, is_enabled);

					if (is_enabled)
						_r_config_setboolean (L"AutoreductIntervalEnable", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));

					break;
				}

				case IDC_HOTKEY_CLEAN_CHK:
				{
					BOOLEAN is_enabled = _r_ctrl_isenabled (hwnd, ctrl_id);

					_r_ctrl_enable (hwnd, IDC_HOTKEY_CLEAN, is_enabled);

					if (is_enabled)
					{
						_r_config_setboolean (L"HotkeyCleanEnable", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
						_app_hotkeyinit (_r_app_gethwnd ());
					}

					break;
				}

				case IDC_HOTKEY_CLEAN:
				{
					if (IsDlgButtonChecked (hwnd, IDC_HOTKEY_CLEAN_CHK) == BST_CHECKED)
					{
						if (notify_code == EN_CHANGE)
						{
							_r_config_setuinteger (L"HotkeyClean", (UINT)SendDlgItemMessage (hwnd, ctrl_id, HKM_GETHOTKEY, 0, 0));
							_app_hotkeyinit (_r_app_gethwnd ());
						}
					}

					break;
				}

				case IDC_TRAYUSETRANSPARENCY_CHK:
				case IDC_TRAYSHOWBORDER_CHK:
				case IDC_TRAYROUNDCORNERS_CHK:
				case IDC_TRAYCHANGEBG_CHK:
				case IDC_TRAYUSEANTIALIASING_CHK:
				{
					BOOLEAN is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					if (ctrl_id == IDC_TRAYUSETRANSPARENCY_CHK)
						_r_config_setboolean (L"TrayUseTransparency", is_enabled);

					else if (ctrl_id == IDC_TRAYSHOWBORDER_CHK)
						_r_config_setboolean (L"TrayShowBorder", is_enabled);

					else if (ctrl_id == IDC_TRAYROUNDCORNERS_CHK)
						_r_config_setboolean (L"TrayRoundCorners", is_enabled);

					else if (ctrl_id == IDC_TRAYCHANGEBG_CHK)
						_r_config_setboolean (L"TrayChangeBg", is_enabled);

					else if (ctrl_id == IDC_TRAYUSEANTIALIASING_CHK)
						_r_config_setboolean (L"TrayUseAntialiasing", is_enabled);

					_app_iconinit (_r_app_gethwnd ());
					_app_iconredraw (_r_app_gethwnd ());

					break;
				}

				case IDC_TRAYACTIONDC:
				{
					if (notify_code == CBN_SELCHANGE)
						_r_config_setinteger (L"TrayActionDc", (INT)SendDlgItemMessage (hwnd, ctrl_id, CB_GETCURSEL, 0, 0));

					break;
				}

				case IDC_TRAYACTIONMC:
				{
					if (notify_code == CBN_SELCHANGE)
						_r_config_setinteger (L"TrayActionMc", (INT)SendDlgItemMessage (hwnd, ctrl_id, CB_GETCURSEL, 0, 0));

					break;
				}

				case IDC_TRAYICONSINGLECLICK_CHK:
				{
					_r_config_setboolean (L"IsTrayIconSingleClick", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_SHOW_CLEAN_RESULT_CHK:
				{
					_r_config_setboolean (L"BalloonCleanResults", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_COLOR_TEXT:
				case IDC_COLOR_BACKGROUND:
				case IDC_COLOR_WARNING:
				case IDC_COLOR_DANGER:
				{
					CHOOSECOLOR cc = {0};
					COLORREF cust[16] = {TRAY_COLOR_DANGER, TRAY_COLOR_WARNING, TRAY_COLOR_BG, TRAY_COLOR_TEXT};

					HWND hctrl = GetDlgItem (hwnd, ctrl_id);

					cc.lStructSize = sizeof (cc);
					cc.Flags = CC_RGBINIT | CC_FULLOPEN;
					cc.hwndOwner = hwnd;
					cc.lpCustColors = cust;
					cc.rgbResult = (COLORREF)GetWindowLongPtr (hctrl, GWLP_USERDATA);

					if (ChooseColor (&cc))
					{
						COLORREF clr = cc.rgbResult;

						if (ctrl_id == IDC_COLOR_TEXT)
							_r_config_setulong (L"TrayColorText", clr);

						else if (ctrl_id == IDC_COLOR_BACKGROUND)
							_r_config_setulong (L"TrayColorBg", clr);

						else if (ctrl_id == IDC_COLOR_WARNING)
							_r_config_setulong (L"TrayColorWarning", clr);

						else if (ctrl_id == IDC_COLOR_DANGER)
							_r_config_setulong (L"TrayColorDanger", clr);

						SetWindowLongPtr (hctrl, GWLP_USERDATA, (LONG_PTR)clr);

						_app_iconinit (_r_app_gethwnd ());
						_app_iconredraw (_r_app_gethwnd ());
					}

					break;
				}

				case IDC_FONT:
				{
					CHOOSEFONT cf = {0};

					cf.lStructSize = sizeof (cf);
					cf.hwndOwner = hwnd;
					cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_FORCEFONTEXIST | CF_SCREENFONTS;

					LOGFONT lf = {0};

					_app_fontinit (hwnd, &lf, 0);

					cf.lpLogFont = &lf;

					if (ChooseFont (&cf))
					{
						INT size = _r_dc_fontheighttosize (_r_app_gethwnd (), lf.lfHeight);

						_r_config_setfont (L"TrayFont", _r_app_gethwnd (), &lf, NULL);

						_r_ctrl_settextformat (hwnd, IDC_FONT, L"%s, %" PRIi32 L"px", lf.lfFaceName, size);

						_app_iconinit (_r_app_gethwnd ());
						_app_iconredraw (_r_app_gethwnd ());
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
			if (_r_sys_iselevated ())
			{
				// set privileges
				ULONG privileges[] = {
					SE_INCREASE_QUOTA_PRIVILEGE,
					SE_PROF_SINGLE_PROCESS_PRIVILEGE,
				};

				_r_sys_setprivilege (privileges, RTL_NUMBER_OF (privileges), TRUE);
			}
			else
			{
				// uac indicator (vista+)
				_r_ctrl_setbuttonmargins (hwnd, IDC_CLEAN);

				SendDlgItemMessage (hwnd, IDC_CLEAN, BCM_SETSHIELD, 0, TRUE);
			}

			// configure listview
			_r_listview_setstyle (hwnd, IDC_LISTVIEW, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP, TRUE);

			_r_listview_addcolumn (hwnd, IDC_LISTVIEW, 1, NULL, -50, LVCFMT_RIGHT);
			_r_listview_addcolumn (hwnd, IDC_LISTVIEW, 2, NULL, -50, LVCFMT_LEFT);

			// configure listview
			UINT state_mask = 0;

			if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
				state_mask = LVGS_COLLAPSIBLE;

			for (INT i = 0, k = 0; i < 3; i++)
			{
				_r_listview_addgroup (hwnd, IDC_LISTVIEW, i, _r_locale_getstring (IDS_GROUP_1 + i), 0, state_mask, state_mask);

				for (INT j = 0; j < 3; j++)
					_r_listview_additemex (hwnd, IDC_LISTVIEW, k++, 0, _r_locale_getstring (IDS_ITEM_1 + j), I_IMAGENONE, i, 0);
			}

			// settings
			_r_settings_addpage (IDD_SETTINGS_GENERAL, IDS_SETTINGS_GENERAL);
			_r_settings_addpage (IDD_SETTINGS_MEMORY, IDS_SETTINGS_MEMORY);
			_r_settings_addpage (IDD_SETTINGS_APPEARANCE, IDS_SETTINGS_APPEARANCE);
			_r_settings_addpage (IDD_SETTINGS_TRAY, IDS_SETTINGS_TRAY);

			break;
		}

		case WM_NCCREATE:
		{
			_r_wnd_enablenonclientscaling (hwnd);
			break;
		}

		case WM_DESTROY:
		{
			_r_tray_destroy (hwnd, UID);

			PostQuitMessage (0);

			break;
		}

		case RM_INITIALIZE:
		{
			_app_hotkeyinit (hwnd);
			_app_iconinit (hwnd);

			_r_tray_create (hwnd, UID, WM_TRAYICON, _app_iconcreate (), APP_NAME, FALSE);

			_app_iconredraw (hwnd);

			SetTimer (hwnd, UID, TIMER, &_app_timercallback);

			// configure menu
			HMENU hmenu = GetMenu (hwnd);

			if (hmenu)
			{
				_r_menu_checkitem (hmenu, IDM_ALWAYSONTOP_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"AlwaysOnTop", APP_ALWAYSONTOP));

				_r_menu_checkitem (hmenu, IDM_LOADONSTARTUP_CHK, 0, MF_BYCOMMAND, _r_autorun_isenabled ());

				_r_menu_checkitem (hmenu, IDM_STARTMINIMIZED_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsStartMinimized", FALSE));
				_r_menu_checkitem (hmenu, IDM_REDUCTCONFIRMATION_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsShowReductConfirmation", TRUE));
				_r_menu_checkitem (hmenu, IDM_CHECKUPDATES_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"CheckUpdates", TRUE));
			}

			break;
		}

		case RM_TASKBARCREATED:
		{
			_app_iconinit (hwnd);

			_r_tray_destroy (hwnd, UID);
			_r_tray_create (hwnd, UID, WM_TRAYICON, _app_iconcreate (), APP_NAME, FALSE);

			_app_iconredraw (hwnd);

			break;
		}

		case RM_LOCALIZE:
		{
			// localize menu
			HMENU hmenu = GetMenu (hwnd);

			if (hmenu)
			{
				_r_menu_setitemtext (hmenu, 0, TRUE, _r_locale_getstring (IDS_FILE));
				_r_menu_setitemtext (hmenu, 1, TRUE, _r_locale_getstring (IDS_SETTINGS));
				_r_menu_setitemtext (hmenu, 2, TRUE, _r_locale_getstring (IDS_HELP));

				_r_menu_setitemtextformat (hmenu, IDM_SETTINGS, FALSE, L"%s...\tF2", _r_locale_getstring (IDS_SETTINGS));
				_r_menu_setitemtext (hmenu, IDM_EXIT, FALSE, _r_locale_getstring (IDS_EXIT));

				_r_menu_setitemtext (hmenu, IDM_ALWAYSONTOP_CHK, FALSE, _r_locale_getstring (IDS_ALWAYSONTOP_CHK));
				_r_menu_setitemtext (hmenu, IDM_LOADONSTARTUP_CHK, FALSE, _r_locale_getstring (IDS_LOADONSTARTUP_CHK));
				_r_menu_setitemtext (hmenu, IDM_STARTMINIMIZED_CHK, FALSE, _r_locale_getstring (IDS_STARTMINIMIZED_CHK));
				_r_menu_setitemtext (hmenu, IDM_REDUCTCONFIRMATION_CHK, FALSE, _r_locale_getstring (IDS_REDUCTCONFIRMATION_CHK));
				_r_menu_setitemtext (hmenu, IDM_CHECKUPDATES_CHK, FALSE, _r_locale_getstring (IDS_CHECKUPDATES_CHK));

				_r_menu_setitemtextformat (GetSubMenu (hmenu, 1), LANG_MENU, TRUE, L"%s (Language)", _r_locale_getstring (IDS_LANGUAGE));

				_r_menu_setitemtext (hmenu, IDM_WEBSITE, FALSE, _r_locale_getstring (IDS_WEBSITE));
				_r_menu_setitemtext (hmenu, IDM_CHECKUPDATES, FALSE, _r_locale_getstring (IDS_CHECKUPDATES));

				_r_menu_setitemtextformat (hmenu, IDM_ABOUT, FALSE, L"%s\tF1", _r_locale_getstring (IDS_ABOUT));
			}

			// configure listview
			for (INT i = 0, k = 0; i < 3; i++)
			{
				_r_listview_setgroup (hwnd, IDC_LISTVIEW, i, _r_locale_getstring (IDS_GROUP_1 + i), 0, 0);

				for (INT j = 0; j < 3; j++)
					_r_listview_setitem (hwnd, IDC_LISTVIEW, k++, 0, _r_locale_getstring (IDS_ITEM_1 + j));
			}

			// configure button
			_r_ctrl_settext (hwnd, IDC_CLEAN, _r_locale_getstring (IDS_CLEAN));

			_r_wnd_addstyle (hwnd, IDC_CLEAN, _r_app_isclassicui () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			if (hmenu)
				_r_locale_enum (GetSubMenu (hmenu, 1), LANG_MENU, IDX_LANGUAGE); // enum localizations

			break;
		}

		case WM_DPICHANGED:
		{
			_app_iconinit (hwnd);
			_app_iconredraw (hwnd);

			_r_listview_setcolumn (hwnd, IDC_LISTVIEW, 0, NULL, -50);
			_r_listview_setcolumn (hwnd, IDC_LISTVIEW, 1, NULL, -50);

			if (!_r_sys_iselevated ())
				_r_ctrl_setbuttonmargins (hwnd, IDC_CLEAN);

			break;
		}

		case RM_UNINITIALIZE:
		{
			KillTimer (hwnd, UID);

			_r_tray_destroy (hwnd, UID);

			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			RECT rect;
			HDC hdc = BeginPaint (hwnd, &ps);

			if (hdc)
			{
				if (GetClientRect (hwnd, &rect))
				{
					INT height = _r_dc_getdpi (hwnd, PR_SIZE_FOOTERHEIGHT);

					rect.top = rect.bottom - height;
					rect.bottom = rect.top + height;

					_r_dc_fillrect (hdc, &rect, GetSysColor (COLOR_3DFACE));

					for (INT i = 0; i < rect.right; i++)
						SetPixelV (hdc, i, rect.top, GetSysColor (COLOR_APPWORKSPACE));
				}

				EndPaint (hwnd, &ps);
			}

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
				_app_memoryclean (NULL, TRUE);

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
					LPNMLVCUSTOMDRAW lpnmlv = (LPNMLVCUSTOMDRAW)lparam;
					LONG_PTR result = CDRF_DODEFAULT;

					switch (lpnmlv->nmcd.dwDrawStage)
					{
						case CDDS_PREPAINT:
						{
							result = (CDRF_NOTIFYPOSTPAINT | CDRF_NOTIFYITEMDRAW);
							break;
						}

						case CDDS_ITEMPREPAINT:
						{
							if ((ULONG)lpnmlv->nmcd.lItemlParam >= _r_config_getulong (L"TrayLevelDanger", DEFAULT_DANGER_LEVEL))
							{
								lpnmlv->clrText = _r_config_getulong (L"TrayColorDanger", TRAY_COLOR_DANGER);
								result = (CDRF_NOTIFYPOSTPAINT | CDRF_NEWFONT);
							}
							else if ((ULONG)lpnmlv->nmcd.lItemlParam >= _r_config_getulong (L"TrayLevelWarning", DEFAULT_WARNING_LEVEL))
							{
								lpnmlv->clrText = _r_config_getulong (L"TrayColorWarning", TRAY_COLOR_WARNING);
								result = (CDRF_NOTIFYPOSTPAINT | CDRF_NEWFONT);
							}

							break;
						}
					}

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result);
					return result;
				}
			}

			break;
		}

		case WM_TRAYICON:
		{
			switch (LOWORD (lparam))
			{
				case WM_LBUTTONUP:
				case WM_LBUTTONDBLCLK:
				case WM_MBUTTONUP:
				{
					INT action;
					BOOLEAN is_singleclick;

					is_singleclick = _r_config_getboolean (L"IsTrayIconSingleClick", TRUE);

					if (LOWORD (lparam) == WM_LBUTTONUP && !is_singleclick)
					{
						SetForegroundWindow (hwnd);
						break;
					}

					if (LOWORD (lparam) == WM_LBUTTONDBLCLK && is_singleclick)
						break;

					action = (LOWORD (lparam) == WM_MBUTTONUP) ? _r_config_getinteger (L"TrayActionMc", 1) : _r_config_getinteger (L"TrayActionDc", 0);

					switch (action)
					{
						case 1:
						{
							_app_memoryclean (NULL, FALSE);
							break;
						}

						case 2:
						{
							_r_sys_createprocess (NULL, L"taskmgr.exe", NULL);
							break;
						}

						default:
						{
							_r_wnd_toggle (hwnd, FALSE);
							break;
						}
					}

					SetForegroundWindow (hwnd);

					break;
				}

				case WM_CONTEXTMENU:
				{
					SetForegroundWindow (hwnd); // don't touch

#define SUBMENU1 4
#define SUBMENU2 5
#define SUBMENU3 6

					HMENU hmenu = LoadMenu (NULL, MAKEINTRESOURCE (IDM_TRAY));

					if (hmenu)
					{
						HMENU hsubmenu = GetSubMenu (hmenu, 0);

						HMENU hsubmenu1 = GetSubMenu (hsubmenu, SUBMENU1);
						HMENU hsubmenu2 = GetSubMenu (hsubmenu, SUBMENU2);
						HMENU hsubmenu3 = GetSubMenu (hsubmenu, SUBMENU3);

						// localize
						_r_menu_setitemtext (hsubmenu, IDM_TRAY_SHOW, FALSE, _r_locale_getstring (IDS_TRAY_SHOW));

						_r_menu_setitemtextformat (hsubmenu, IDM_TRAY_CLEAN, FALSE, L"%s...", _r_locale_getstring (IDS_CLEAN));

						_r_menu_setitemtext (hsubmenu, SUBMENU1, TRUE, _r_locale_getstring (IDS_TRAY_POPUP_1));
						_r_menu_setitemtext (hsubmenu, SUBMENU2, TRUE, _r_locale_getstring (IDS_TRAY_POPUP_2));
						_r_menu_setitemtext (hsubmenu, SUBMENU3, TRUE, _r_locale_getstring (IDS_TRAY_POPUP_3));

						_r_menu_setitemtextformat (hsubmenu, IDM_TRAY_SETTINGS, FALSE, L"%s...", _r_locale_getstring (IDS_SETTINGS));

						_r_menu_setitemtext (hsubmenu, IDM_TRAY_WEBSITE, FALSE, _r_locale_getstring (IDS_WEBSITE));
						_r_menu_setitemtext (hsubmenu, IDM_TRAY_ABOUT, FALSE, _r_locale_getstring (IDS_ABOUT));
						_r_menu_setitemtext (hsubmenu, IDM_TRAY_EXIT, FALSE, _r_locale_getstring (IDS_EXIT));

						_r_menu_setitemtextformat (hsubmenu1, IDM_WORKINGSET_CHK, FALSE, L"%s (vista+)", _r_locale_getstring (IDS_WORKINGSET_CHK));
						_r_menu_setitemtext (hsubmenu1, IDM_SYSTEMWORKINGSET_CHK, FALSE, _r_locale_getstring (IDS_SYSTEMWORKINGSET_CHK));
						_r_menu_setitemtextformat (hsubmenu1, IDM_STANDBYLISTPRIORITY0_CHK, FALSE, L"%s (vista+)", _r_locale_getstring (IDS_STANDBYLISTPRIORITY0_CHK));
						_r_menu_setitemtextformat (hsubmenu1, IDM_STANDBYLIST_CHK, FALSE, L"%s* (vista+)", _r_locale_getstring (IDS_STANDBYLIST_CHK));
						_r_menu_setitemtextformat (hsubmenu1, IDM_MODIFIEDLIST_CHK, FALSE, L"%s* (vista+)", _r_locale_getstring (IDS_MODIFIEDLIST_CHK));
						_r_menu_setitemtextformat (hsubmenu1, IDM_COMBINEMEMORYLISTS_CHK, FALSE, L"%s (win81+)", _r_locale_getstring (IDS_COMBINEMEMORYLISTS_CHK));

						_r_menu_setitemtext (hsubmenu2, IDM_TRAY_DISABLE_1, FALSE, _r_locale_getstring (IDS_TRAY_DISABLE));
						_r_menu_setitemtext (hsubmenu3, IDM_TRAY_DISABLE_2, FALSE, _r_locale_getstring (IDS_TRAY_DISABLE));

						// configure submenu #1
						ULONG mask = _r_config_getulong (L"ReductMask2", REDUCT_MASK_DEFAULT);

						if ((mask & REDUCT_WORKING_SET) != 0)
							CheckMenuItem (hsubmenu1, IDM_WORKINGSET_CHK, MF_BYCOMMAND | MF_CHECKED);

						if ((mask & REDUCT_SYSTEM_WORKING_SET) != 0)
							CheckMenuItem (hsubmenu1, IDM_SYSTEMWORKINGSET_CHK, MF_BYCOMMAND | MF_CHECKED);

						if ((mask & REDUCT_STANDBY_PRIORITY0_LIST) != 0)
							CheckMenuItem (hsubmenu1, IDM_STANDBYLISTPRIORITY0_CHK, MF_BYCOMMAND | MF_CHECKED);

						if ((mask & REDUCT_STANDBY_LIST) != 0)
							CheckMenuItem (hsubmenu1, IDM_STANDBYLIST_CHK, MF_BYCOMMAND | MF_CHECKED);

						if ((mask & REDUCT_MODIFIED_LIST) != 0)
							CheckMenuItem (hsubmenu1, IDM_MODIFIEDLIST_CHK, MF_BYCOMMAND | MF_CHECKED);

						if ((mask & REDUCT_COMBINE_MEMORY_LISTS) != 0)
							CheckMenuItem (hsubmenu1, IDM_COMBINEMEMORYLISTS_CHK, MF_BYCOMMAND | MF_CHECKED);

						if (!_r_sys_iselevated () || !_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
						{
							EnableMenuItem (hsubmenu1, IDM_WORKINGSET_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
							EnableMenuItem (hsubmenu1, IDM_STANDBYLISTPRIORITY0_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
							EnableMenuItem (hsubmenu1, IDM_STANDBYLIST_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
							EnableMenuItem (hsubmenu1, IDM_MODIFIEDLIST_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

							if (!_r_sys_iselevated ())
								EnableMenuItem (hsubmenu1, IDM_SYSTEMWORKINGSET_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
						}

						// Combine memory lists (win81+)
						if (!_r_sys_iselevated () || !_r_sys_isosversiongreaterorequal (WINDOWS_8_1))
							EnableMenuItem (hsubmenu1, IDM_COMBINEMEMORYLISTS_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

						WCHAR number_string[128];
						UINT menu_id;
						UINT val;

						// configure submenu #2
						{
							val = _r_config_getuinteger (L"AutoreductValue", DEFAULT_AUTOREDUCT_VAL);
							generate_menu_array (limits, RTL_NUMBER_OF (limits), val);

							for (SIZE_T i = 0; i < RTL_NUMBER_OF (limits); i++)
							{
								menu_id = IDX_TRAY_POPUP_1 + (UINT)i;

								_r_str_printf (number_string, RTL_NUMBER_OF (number_string), L"%" PRIu32 L"%%", limits[i]);

								AppendMenu (hsubmenu2, MF_STRING, menu_id, number_string);

								if (!_r_sys_iselevated ())
									_r_menu_enableitem (hsubmenu2, menu_id, MF_BYCOMMAND, FALSE);

								if (val == limits[i])
									_r_menu_checkitem (hsubmenu2, 0, (UINT)RTL_NUMBER_OF (limits), MF_BYPOSITION, (UINT)i + 2);
							}

							if (!_r_config_getboolean (L"AutoreductEnable", FALSE))
								CheckMenuRadioItem (hsubmenu2, 0, (UINT)RTL_NUMBER_OF (limits), 0, MF_BYPOSITION);
						}

						// configure submenu #3
						{
							val = _r_config_getuinteger (L"AutoreductIntervalValue", DEFAULT_AUTOREDUCTINTERVAL_VAL);
							generate_menu_array (intervals, RTL_NUMBER_OF (intervals), val);

							for (SIZE_T i = 0; i < RTL_NUMBER_OF (intervals); i++)
							{
								menu_id = IDX_TRAY_POPUP_2 + (UINT)i;

								_r_str_printf (number_string, RTL_NUMBER_OF (number_string), L"%" PRIu32 L" min.", intervals[i]);

								AppendMenu (hsubmenu3, MF_STRING, menu_id, number_string);

								if (!_r_sys_iselevated ())
									_r_menu_enableitem (hsubmenu3, menu_id, MF_BYCOMMAND, FALSE);

								if (val == intervals[i])
									_r_menu_checkitem (hsubmenu3, 0, (UINT)RTL_NUMBER_OF (intervals), MF_BYPOSITION, (UINT)i + 2);
							}

							if (!_r_config_getboolean (L"AutoreductIntervalEnable", FALSE))
								CheckMenuRadioItem (hsubmenu3, 0, (UINT)RTL_NUMBER_OF (intervals), 0, MF_BYPOSITION);
						}

						_r_menu_popup (hsubmenu, hwnd, NULL, TRUE);

						DestroyMenu (hmenu);
					}

					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			INT ctrl_id = LOWORD (wparam);
			INT notify_code = HIWORD (wparam);

			if (notify_code == 0 && ctrl_id >= IDX_LANGUAGE && ctrl_id <= IDX_LANGUAGE + (INT)_r_locale_getcount ())
			{
				_r_locale_applyfrommenu (GetSubMenu (GetSubMenu (GetMenu (hwnd), 1), LANG_MENU), ctrl_id);
				return FALSE;
			}
			else if ((ctrl_id >= IDX_TRAY_POPUP_1 && ctrl_id <= IDX_TRAY_POPUP_1 + (INT)RTL_NUMBER_OF (limits)))
			{
				SIZE_T idx = (SIZE_T)ctrl_id - IDX_TRAY_POPUP_1;

				_r_config_setboolean (L"AutoreductEnable", TRUE);
				_r_config_setuinteger (L"AutoreductValue", limits[idx]);

				return FALSE;
			}
			else if ((ctrl_id >= IDX_TRAY_POPUP_2 && ctrl_id <= IDX_TRAY_POPUP_2 + (INT)RTL_NUMBER_OF (intervals)))
			{
				SIZE_T idx = (SIZE_T)ctrl_id - IDX_TRAY_POPUP_2;

				_r_config_setboolean (L"AutoreductIntervalEnable", TRUE);
				_r_config_setuinteger (L"AutoreductIntervalValue", intervals[idx]);

				return FALSE;
			}

			switch (ctrl_id)
			{
				case IDM_ALWAYSONTOP_CHK:
				{
					BOOLEAN new_val = !_r_config_getboolean (L"AlwaysOnTop", APP_ALWAYSONTOP);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					_r_config_setboolean (L"AlwaysOnTop", new_val);

					_r_wnd_top (hwnd, new_val);

					break;
				}

				case IDM_STARTMINIMIZED_CHK:
				{
					BOOLEAN new_val = !_r_config_getboolean (L"IsStartMinimized", FALSE);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					_r_config_setboolean (L"IsStartMinimized", new_val);

					break;
				}

				case IDM_REDUCTCONFIRMATION_CHK:
				{
					BOOLEAN new_val = !_r_config_getboolean (L"IsShowReductConfirmation", TRUE);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					_r_config_setboolean (L"IsShowReductConfirmation", new_val);

					break;
				}

				case IDM_LOADONSTARTUP_CHK:
				{
					BOOLEAN new_val = !_r_autorun_isenabled ();

					_r_autorun_enable (hwnd, new_val);
					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, _r_autorun_isenabled ());

					break;
				}

				case IDM_CHECKUPDATES_CHK:
				{
					BOOLEAN new_val = !_r_config_getboolean (L"CheckUpdates", TRUE);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					_r_config_setboolean (L"CheckUpdates", new_val);

					break;
				}

				case IDM_WORKINGSET_CHK:
				case IDM_SYSTEMWORKINGSET_CHK:
				case IDM_STANDBYLISTPRIORITY0_CHK:
				case IDM_STANDBYLIST_CHK:
				case IDM_MODIFIEDLIST_CHK:
				case IDM_COMBINEMEMORYLISTS_CHK:
				{
					ULONG mask = _r_config_getulong (L"ReductMask2", REDUCT_MASK_DEFAULT);
					ULONG new_mask = 0;

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
						if (!_r_show_confirmmessage (hwnd, NULL, _r_locale_getstring (IDS_QUESTION_WARNING), L"IsShowWarningConfirmation"))
							return FALSE;
					}

					_r_config_setulong (L"ReductMask2", (mask & new_mask) != 0 ? (mask & ~new_mask) : (mask | new_mask));

					break;
				}

				case IDM_TRAY_DISABLE_1:
				{
					_r_config_setboolean (L"AutoreductEnable", !_r_config_getboolean (L"AutoreductEnable", FALSE));
					break;
				}

				case IDM_TRAY_DISABLE_2:
				{
					_r_config_setboolean (L"AutoreductIntervalEnable", !_r_config_getboolean (L"AutoreductIntervalEnable", FALSE));
					break;
				}

				case IDM_SETTINGS:
				case IDM_TRAY_SETTINGS:
				{
					_r_settings_createwindow (hwnd, &SettingsProc, 0);
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
					_r_wnd_toggle (hwnd, FALSE);
					break;
				}

				case IDOK: // process Enter key
				case IDC_CLEAN:
				case IDM_TRAY_CLEAN:
				{
					static BOOLEAN is_opened = FALSE;

					if (!is_opened)
					{
						is_opened = TRUE;

						if (!_r_sys_iselevated ())
						{
							if (_r_app_runasadmin ())
							{
								DestroyWindow (hwnd);
							}
							else
							{
								_r_tray_popup (hwnd, UID, NIIF_ERROR | (_r_config_getboolean (L"IsNotificationsSound", TRUE) ? 0 : NIIF_NOSOUND), APP_NAME, _r_locale_getstring (IDS_STATUS_NOPRIVILEGES));
							}
						}
						else
						{
							_app_memoryclean (hwnd, FALSE);
						}

						is_opened = FALSE;
					}

					break;
				}

				case IDM_WEBSITE:
				case IDM_TRAY_WEBSITE:
				{
					_r_shell_opendefault (_r_app_getwebsite_url ());
					break;
				}

				case IDM_CHECKUPDATES:
				{
					_r_update_check (hwnd);
					break;
				}

				case IDM_ABOUT:
				case IDM_TRAY_ABOUT:
				{
					_r_show_aboutmessage (hwnd);
					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

INT APIENTRY wWinMain (_In_ HINSTANCE hinst, _In_opt_ HINSTANCE prev_hinst, _In_ LPWSTR cmdline, _In_ INT show_cmd)
{
	MSG msg;

	RtlSecureZeroMemory (&config, sizeof (config));

	if (_r_app_initialize ())
	{
		if (_r_app_createwindow (IDD_MAIN, IDI_MAIN, &DlgProc))
		{
			HACCEL haccel = LoadAccelerators (_r_sys_getimagebase (), MAKEINTRESOURCE (IDA_MAIN));

			if (haccel)
			{
				while (GetMessage (&msg, NULL, 0, 0) > 0)
				{
					HWND hwnd = GetActiveWindow ();

					if (!TranslateAccelerator (hwnd, haccel, &msg) && !IsDialogMessage (hwnd, &msg))
					{
						TranslateMessage (&msg);
						DispatchMessage (&msg);
					}
				}

				DestroyAcceleratorTable (haccel);
			}
		}
	}

	return ERROR_SUCCESS;
}
