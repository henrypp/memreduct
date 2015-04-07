// Mem Reduct
// Copyright © 2011-2015 Henry++

#include <windows.h>

#include "main.h"

#include "resource.h"
#include "routine.h"

NOTIFYICONDATA nid = {0};
STATIC_DATA data = {0};

CONST UINT WM_TASKBARCREATED = RegisterWindowMessage(L"TaskbarCreated");

VOID BresenhamCircle(HDC dc, LONG radius, LPPOINT pt, COLORREF clr)
{
	LONG cx = 0, cy = radius, d = 2 - 2 * radius;

	// let's start drawing the circle:
	SetPixel(dc, cx + pt->x, cy + pt->y, clr);		// point (0, R);
	SetPixel(dc, cx + pt->x, -cy + pt->y, clr);		// point (0, -R);
	SetPixel(dc, cy + pt->x, cx + pt->y, clr);		// point (R, 0);
	SetPixel(dc, -cy + pt->x, cx + pt->y, clr);		// point (-R, 0);

	while(1)
	{
		if(d > -cy)
		{
			--cy;
			d += 1 - 2 * cy;
		}

		if(d <= cx)
		{
			++cx;
			d += 1 + 2 * cx;
		}

		if(!cy) break; // cy is 0, but these points are already drawn;

		// the actual drawing:
		SetPixel(dc, cx + pt->x, cy + pt->y, clr);		// 0-90		degrees
		SetPixel(dc, -cx + pt->x, cy + pt->y, clr);		// 90-180	degrees
		SetPixel(dc, -cx + pt->x, -cy + pt->y, clr);	// 180-270	degrees
		SetPixel(dc, cx + pt->x, -cy + pt->y, clr);		// 270-360	degrees
	}
}

VOID BresenhamLine(HDC dc, INT x0, INT y0, INT x1, INT y1, COLORREF clr)
{
	INT dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
	INT dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
	INT err = (dx > dy ? dx : -dy) / 2, e2;

	while(1)
	{
		SetPixel(dc, x0, y0, clr);

		if (x0 == x1 && y0 == y1) break;

		e2 = err;

		if(e2 >-dx) { err -= dy; x0 += sx;}
		if(e2 < dy) { err += dx; y0 += sy;}
	}
}

BOOL _Application_ShowNotification(DWORD icon, LPCWSTR title, LPCWSTR text)
{
	nid.uFlags = NIF_INFO;
	nid.dwInfoFlags = NIIF_LARGE_ICON | icon;

	StringCchCopy(nid.szInfoTitle, _countof(nid.szInfoTitle), title);
	StringCchCopy(nid.szInfo, _countof(nid.szInfo), text);

	Shell_NotifyIcon(NIM_MODIFY, &nid);

	nid.szInfo[0] = nid.szInfoTitle[0] = NULL; // clear

	return TRUE;
}

DWORD _Application_GetMemoryStatus(MEMORYINFO* m)
{
	MEMORYSTATUSEX msex = {0};
	SYSTEM_CACHE_INFORMATION sci = {0};

	msex.dwLength = sizeof(msex);

	if(GlobalMemoryStatusEx(&msex) && m) // WARNING!!! don't touch "m"!
	{
		m->percent_phys = msex.dwMemoryLoad;

		m->free_phys = msex.ullAvailPhys;
		m->total_phys = msex.ullTotalPhys;

		//msex.ullTotalPageFile -= msex.ullTotalPhys;
		//msex.ullAvailPageFile -= msex.ullAvailPhys;

		m->percent_page = (DWORD)ROUTINE_PERCENT_OF(msex.ullTotalPageFile - msex.ullAvailPageFile, msex.ullTotalPageFile);

		m->free_page = msex.ullAvailPageFile;
		m->total_page = msex.ullTotalPageFile;
	}

	if(m && NtQuerySystemInformation(SystemFileCacheInformation, &sci, sizeof(sci), NULL) >= 0)
	{
		m->percent_ws = (DWORD)ROUTINE_PERCENT_OF(sci.CurrentSize, sci.PeakSize);

		m->free_ws = (sci.PeakSize - sci.CurrentSize);
		m->total_ws = sci.PeakSize;
	}

	return msex.dwMemoryLoad;
}

DWORD _Application_Reduct(HWND hwnd, DWORD mask)
{
	if(!data.is_admin || (hwnd && _r_msg(MB_YESNO | MB_ICONQUESTION, _r_locale(IDS_QUESTION_1)) == IDNO))
	{
		return FALSE;
	}

	data.reduct_before = _Application_GetMemoryStatus(NULL);

	SYSTEM_MEMORY_LIST_COMMAND smlc;

	if(!mask)
	{
		mask = data.reduct_mask;
	}

	// Working set
	if(data.is_supported_os && ((mask & REDUCT_WORKING_SET) != 0))
	{
		smlc = MemoryEmptyWorkingSets;
		NtSetSystemInformation(SystemMemoryListInformation, &smlc, sizeof(smlc));
	}

	// System working set
	if(((mask & REDUCT_SYSTEM_WORKING_SET) != 0))
	{
		SYSTEM_CACHE_INFORMATION sci = {0};

		sci.MinimumWorkingSet = (ULONG)-1;
		sci.MaximumWorkingSet = (ULONG)-1;

		NtSetSystemInformation(SystemFileCacheInformation, &sci, sizeof(sci));
	}

	// Standby priority-0 list
	if(data.is_supported_os && ((mask & REDUCT_STANDBY_PRIORITY0_LIST) != 0))
	{
		smlc = MemoryPurgeLowPriorityStandbyList;
		NtSetSystemInformation(SystemMemoryListInformation, &smlc, sizeof(smlc));
	}

	// Standby list
	if(data.is_supported_os && ((mask & REDUCT_STANDBY_LIST) != 0))
	{
		smlc = MemoryPurgeStandbyList;
		NtSetSystemInformation(SystemMemoryListInformation, &smlc, sizeof(smlc));
	}

	// Modified list
	if(data.is_supported_os && ((mask & REDUCT_MODIFIED_LIST) != 0))
	{
		smlc = MemoryFlushModifiedList;
		NtSetSystemInformation(SystemMemoryListInformation, &smlc, sizeof(smlc));
	}

	data.reduct_after = _Application_GetMemoryStatus(NULL);

	// Statistic
	data.statistic_last_reduct = _r_helper_unixtime64();
	_r_cfg_write(L"StatisticLastReduct", (DWORD)data.statistic_last_reduct); // time of last cleaning

	// Show result
	_Application_ShowNotification(NIIF_INFO, APP_NAME, _r_helper_format(_r_locale(IDS_BALLOON_REDUCT), _r_helper_formatsize64((DWORDLONG)ROUTINE_PERCENT_VAL(data.reduct_before - data.reduct_after, data.ms.total_phys))));

	return data.reduct_after;
}

HICON _Application_DrawIcon()
{
	COLORREF clrText = data.tray_color_text;

	HBRUSH bg_brush = data.border_brush;

	if(data.ms.percent_phys >= data.autoreduct_threshold_value)
	{
		if(data.is_transparent)
		{
			clrText =  data.tray_color_warning;
		}
		else
		{
			bg_brush = data.border_brush_warning;
		}
	}

	// select bitmap
	HBITMAP old_bitmap = (HBITMAP)SelectObject(data.cdc1, data.bitmap);

	// draw transparent mask
	COLORREF clr_prev = SetBkColor(data.cdc1, TRAY_COLOR_MASK);
	ExtTextOut(data.cdc1, 0, 0, ETO_OPAQUE, &data.rc, NULL, 0, NULL);
	SetBkColor(data.cdc1, clr_prev);

	// draw background
	if(!data.is_transparent)
	{
		HGDIOBJ prev_pen = SelectObject(data.cdc1, GetStockObject(NULL_PEN));
		HGDIOBJ prev_brush = SelectObject(data.cdc1, bg_brush);

		RoundRect(data.cdc1, 0, 0, data.rc.right, data.rc.bottom, data.is_round ? (data.rc.right / 2) : 0, data.is_round ? (data.rc.bottom / 2) : 0);

		SelectObject(data.cdc1, prev_pen);
		SelectObject(data.cdc1, prev_brush);
	}

	// draw border
	if(data.is_border)
	{
		if(data.is_round)
		{
			POINT pt = {0};

			pt.x = (data.rc.right / 2) - 1;
			pt.y = (data.rc.bottom / 2) - 1;

			for(UINT i = 1; i < SCALE + 1; i++)
			{
				BresenhamCircle(data.cdc1, (data.rc.right / 2) - i, &pt, clrText);
			}
		}
		else
		{
			for(UINT i = 0; i < SCALE; i++)
			{
				BresenhamLine(data.cdc1, i, 0, i, data.rc.bottom, clrText); // left
				BresenhamLine(data.cdc1, i, i, data.rc.right, i, clrText); // top
				BresenhamLine(data.cdc1, (data.rc.right - 1) - i, 0, (data.rc.right - 1) - i, data.rc.bottom, clrText); // right
				BresenhamLine(data.cdc1, 0, (data.rc.bottom - 1) - i, data.rc.right, (data.rc.bottom - 1) - i, clrText); // bottom
			}
		}
	}

	// draw text
	SetTextColor(data.cdc1, clrText);
	SetBkMode(data.cdc1, TRANSPARENT);

	CString buffer = _r_helper_format(L"%d", data.ms.percent_phys);

	SelectObject(data.cdc1, data.font);
	DrawTextEx(data.cdc1, buffer.GetBuffer(), buffer.GetLength(), &data.rc, DT_VCENTER | DT_CENTER | DT_SINGLELINE | DT_NOCLIP, NULL);

	// draw transparent mask
	HGDIOBJ old_mask = SelectObject(data.cdc2, data.bitmap_mask);

	SetBkColor(data.cdc1, TRAY_COLOR_MASK);
	BitBlt(data.cdc2, 0, 0, data.rc.right, data.rc.bottom, data.cdc1, 0, 0, SRCCOPY);

	SelectObject(data.cdc1, old_bitmap);
	SelectObject(data.cdc2, old_mask);

	ICONINFO ii = {0};

	ii.fIcon = TRUE;
	ii.hbmColor = data.bitmap;
	ii.hbmMask = data.bitmap_mask;

	return CreateIconIndirect(&ii);
}

VOID CALLBACK _Application_TimerCallback(HWND hwnd, UINT, UINT_PTR, DWORD)
{
	_Application_GetMemoryStatus(&data.ms);

	if(nid.hIcon)
	{
		DestroyIcon(nid.hIcon);
	}

	nid.uFlags = NIF_ICON | NIF_TIP;

	StringCchPrintf(nid.szTip, _countof(nid.szTip), _r_locale(IDS_TRAY_TOOLTIP), data.ms.percent_phys, data.ms.percent_page, data.ms.percent_ws);
	nid.hIcon = _Application_DrawIcon();

	Shell_NotifyIcon(NIM_MODIFY, &nid);

	// Autoreduct
	if(data.is_admin)
	{
		if((data.autoreduct_threshold && data.ms.percent_phys >= data.autoreduct_threshold_value) || (data.autoreduct_timeout && data.ms.percent_phys >= data.autoreduct_timeout_threshold_value && ((_r_helper_unixtime64() - data.statistic_last_reduct) >= data.autoreduct_timeout_value * 60)))
		{
			_Application_Reduct(NULL, 0);
		}
	}

	if(IsWindowVisible(hwnd))
	{
		// Physical
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_format(L"%d%%", data.ms.percent_phys), 0, 1, -1, -1, data.ms.percent_phys);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(data.ms.free_phys), 1, 1, -1, -1, data.ms.percent_phys);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(data.ms.total_phys), 2, 1, -1, -1, data.ms.percent_phys);
		
		// Page file
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_format(L"%d%%", data.ms.percent_page), 3, 1, -1, -1, data.ms.percent_page);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(data.ms.free_page), 4, 1, -1, -1, data.ms.percent_page);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(data.ms.total_page), 5, 1, -1, -1, data.ms.percent_page);

		// System working set
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_format(L"%d%%", data.ms.percent_ws), 6, 1, -1, -1, data.ms.percent_ws);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(data.ms.free_ws), 7, 1, -1, -1, data.ms.percent_ws);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(data.ms.total_ws), 8, 1, -1, -1, data.ms.percent_ws);

		// Redraw listview
		SendDlgItemMessage(hwnd, IDC_LISTVIEW, LVM_REDRAWITEMS, 0, 8);
	}
}

VOID _Application_Unitialize(BOOL deletetrayicon)
{
	if(nid.hIcon)
	{
		DestroyIcon(nid.hIcon);
	}

	if(deletetrayicon)
	{
		Shell_NotifyIcon(NIM_DELETE, &nid);
	}

	UnregisterHotKey(_r_hwnd, UID);
	KillTimer(_r_hwnd, UID);

	DeleteObject(data.font);

	DeleteObject(data.border_brush);
	DeleteObject(data.border_brush_warning);

	DeleteObject(data.bitmap);
	DeleteObject(data.bitmap_mask);

	DeleteDC(data.cdc1);
	DeleteDC(data.cdc2);
	DeleteDC(data.dc);
}

VOID _Application_Initialize(BOOL createtrayicon)
{
	_Application_Unitialize(createtrayicon);

	// init configuration
	data.statistic_last_reduct = _r_cfg_read(L"StatisticLastReduct", 0);

	data.autoreduct_threshold = _r_cfg_read(L"AutoreductThreshold", 1);
	data.autoreduct_threshold_value = _r_cfg_read(L"AutoreductThresholdValue", 90);
	
	data.autoreduct_timeout = _r_cfg_read(L"AutoreductTimeout", 0);
	data.autoreduct_timeout_value = _r_cfg_read(L"AutoreductTimeoutValue", 10);

	data.autoreduct_timeout_threshold_value = _r_cfg_read(L"AutoreductTimeoutThresholdValue", 60);

	data.reduct_mask = _r_cfg_read(L"ReductMask", REDUCT_WORKING_SET | REDUCT_SYSTEM_WORKING_SET | REDUCT_STANDBY_PRIORITY0_LIST);

	data.rc.right = GetSystemMetrics(SM_CXSMICON) * SCALE;
	data.rc.bottom = GetSystemMetrics(SM_CYSMICON) * SCALE;

	// create device context
	data.dc = GetDC(NULL);

	data.cdc1 = CreateCompatibleDC(data.dc);
	data.cdc2 = CreateCompatibleDC(data.dc);

	ReleaseDC(NULL, data.dc);

	// create bitmap
	BITMAPINFO bmi = {0};

	bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
	bmi.bmiHeader.biWidth = data.rc.right;
	bmi.bmiHeader.biHeight = data.rc.bottom;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	data.bitmap = CreateDIBSection(data.dc, &bmi, DIB_RGB_COLORS, NULL, NULL, 0);
	data.bitmap_mask = CreateBitmap(data.rc.right, data.rc.bottom, 1, 1, NULL);

	data.is_transparent = _r_cfg_read(L"TrayUseTransparency", 1);
	data.is_border = _r_cfg_read(L"TrayShowBorder", 0);
	data.is_round = _r_cfg_read(L"TrayRoundCorners", 0);

	data.tray_color_bg = _r_cfg_read(L"TrayColorBg", TRAY_COLOR_BG);
	data.tray_color_text = _r_cfg_read(L"TrayColorText", TRAY_COLOR_TEXT);
	data.tray_color_warning = _r_cfg_read(L"TrayColorWarning", TRAY_COLOR_WARNING);

	// brush
	data.border_brush = CreateSolidBrush(data.tray_color_bg);
	data.border_brush_warning = CreateSolidBrush(data.tray_color_warning);

	// load font
	SecureZeroMemory(&data.lf, sizeof(data.lf));

	data.lf.lfQuality = NONANTIALIASED_QUALITY;
	data.lf.lfCharSet = DEFAULT_CHARSET;
	data.lf.lfPitchAndFamily = FF_DONTCARE;
	data.lf.lfWeight = _r_cfg_read(L"TrayFontWeight", FW_NORMAL);
	data.lf.lfHeight = -MulDiv(_r_cfg_read(L"TrayFontSize", FONT_SIZE), GetDeviceCaps(data.cdc1, LOGPIXELSY), 72) * SCALE;

	StringCchCopy(data.lf.lfFaceName, LF_FACESIZE, _r_cfg_read(L"TrayFontName", FONT_NAME));

	data.font = CreateFontIndirect(&data.lf);

	// hotkey
	UINT hk = _r_cfg_read(L"Hotkey", MAKEWORD(VK_F1, HOTKEYF_CONTROL));

	if(data.is_admin && hk && _r_cfg_read(L"HotkeyEnable", 1))
	{
		RegisterHotKey(_r_hwnd, UID, (HIBYTE(hk) & 2) | ((HIBYTE(hk) & 4) >> 2) | ((HIBYTE(hk) & 1) << 2), LOBYTE(hk));
	}

	// always on top
	_r_windowtotop(_r_hwnd, _r_cfg_read(L"AlwaysOnTop", 0));

	// init tray icon
	if(createtrayicon)
	{
		nid.cbSize = data.is_supported_os ? sizeof(nid) : NOTIFYICONDATA_V3_SIZE;
		nid.uVersion = data.is_supported_os ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION;
		nid.hWnd = _r_hwnd;
		nid.uID = UID;
		nid.uFlags = NIF_MESSAGE | NIF_ICON;
		nid.uCallbackMessage = WM_TRAYICON;
		nid.hIcon = _Application_DrawIcon();

		Shell_NotifyIcon(NIM_ADD, &nid);
	}

	// create timer
	_Application_TimerCallback(_r_hwnd, 0, NULL, 0);
	SetTimer(_r_hwnd, UID, TIMER, _Application_TimerCallback);
}

INT_PTR WINAPI PagesDlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG:
		{
			SetProp(hwnd, L"id", (HANDLE)lparam);

			if((INT)lparam == IDD_SETTINGS_1)
			{
				if(!data.is_supported_os || !data.is_admin)
				{
					EnableWindow(GetDlgItem(hwnd, IDC_SKIPUACWARNING_CHK), FALSE);
				}

				CheckDlgButton(hwnd, IDC_ALWAYSONTOP_CHK, _r_cfg_read(L"AlwaysOnTop", 0) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_STARTMINIMIZED_CHK, _r_cfg_read(L"StartMinimized", 1) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_LOADONSTARTUP_CHK, _r_autorun_is_present(APP_NAME) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_SKIPUACWARNING_CHK, _r_skipuac_is_present(FALSE) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_CHECKUPDATES_CHK, _r_cfg_read(L"CheckUpdates", 1) ? BST_CHECKED : BST_UNCHECKED);

				SendDlgItemMessage(hwnd, IDC_LANGUAGE, CB_INSERTSTRING, 0, (LPARAM)L"System default");
				SendDlgItemMessage(hwnd, IDC_LANGUAGE, CB_SETCURSEL, 0, NULL);

				EnumResourceLanguages(NULL, RT_STRING, MAKEINTRESOURCE(63), _r_locale_enum, (LONG_PTR)GetDlgItem(hwnd, IDC_LANGUAGE));
			}
			else if((INT)lparam == IDD_SETTINGS_2)
			{
				if(!data.is_supported_os || !data.is_admin)
				{
					EnableWindow(GetDlgItem(hwnd, IDC_WORKINGSET_CHK), FALSE);
					EnableWindow(GetDlgItem(hwnd, IDC_STANDBYLISTPRIORITY0_CHK), FALSE);
					EnableWindow(GetDlgItem(hwnd, IDC_STANDBYLIST_CHK), FALSE);
					EnableWindow(GetDlgItem(hwnd, IDC_MODIFIEDLIST_CHK), FALSE);

					if(!data.is_admin)
					{
						EnableWindow(GetDlgItem(hwnd, IDC_SYSTEMWORKINGSET_CHK), FALSE);
					}
				}

				CheckDlgButton(hwnd, IDC_WORKINGSET_CHK, ((data.reduct_mask & REDUCT_WORKING_SET) != 0) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_SYSTEMWORKINGSET_CHK, ((data.reduct_mask & REDUCT_SYSTEM_WORKING_SET) != 0) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_STANDBYLISTPRIORITY0_CHK, ((data.reduct_mask & REDUCT_STANDBY_PRIORITY0_LIST) != 0) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_STANDBYLIST_CHK, ((data.reduct_mask & REDUCT_STANDBY_LIST) != 0) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_MODIFIEDLIST_CHK, ((data.reduct_mask & REDUCT_MODIFIED_LIST) != 0) ? BST_CHECKED : BST_UNCHECKED);
			}
			else if((INT)lparam == IDD_SETTINGS_3)
			{
				if(!data.is_admin)
				{
					EnableWindow(GetDlgItem(hwnd, IDC_AUTOREDUCTTHRESHOLD_CHK), FALSE);
					EnableWindow(GetDlgItem(hwnd, IDC_AUTOREDUCTTIMEOUT_CHK), FALSE);
					EnableWindow(GetDlgItem(hwnd, IDC_HOTKEYENABLE_CHK), FALSE);
				}

				CheckDlgButton(hwnd, IDC_AUTOREDUCTTHRESHOLD_CHK, _r_cfg_read(L"AutoreductThreshold", 1) ? BST_CHECKED : BST_UNCHECKED);

				SendDlgItemMessage(hwnd, IDC_AUTOREDUCTTHRESHOLD_VALUE, UDM_SETRANGE32, 5, 95);
				SendDlgItemMessage(hwnd, IDC_AUTOREDUCTTHRESHOLD_VALUE, UDM_SETPOS32, 0, _r_cfg_read(L"AutoreductThresholdValue", 90));

				CheckDlgButton(hwnd, IDC_AUTOREDUCTTIMEOUT_CHK, _r_cfg_read(L"AutoreductTimeout", 0) ? BST_CHECKED : BST_UNCHECKED);

				SendDlgItemMessage(hwnd, IDC_AUTOREDUCTTIMEOUT_VALUE, UDM_SETRANGE32, 5, 1440); // min: 5 minutes, max: 1 day
				SendDlgItemMessage(hwnd, IDC_AUTOREDUCTTIMEOUT_VALUE, UDM_SETPOS32, 0, _r_cfg_read(L"AutoreductTimeoutValue", 10));

				SendDlgItemMessage(hwnd, IDC_AUTOREDUCTTIMEOUTTHRESHOLD_VALUE, UDM_SETRANGE32, 0, 99);
				SendDlgItemMessage(hwnd, IDC_AUTOREDUCTTIMEOUTTHRESHOLD_VALUE, UDM_SETPOS32, 0, _r_cfg_read(L"AutoreductTimeoutThresholdValue", 60));

				CheckDlgButton(hwnd, IDC_HOTKEYENABLE_CHK, _r_cfg_read(L"HotkeyEnable", 1) ? BST_CHECKED : BST_UNCHECKED);
				SendDlgItemMessage(hwnd, IDC_HOTKEY, HKM_SETHOTKEY, _r_cfg_read(L"Hotkey", MAKEWORD(VK_F1, HOTKEYF_CONTROL)), NULL);

				SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_AUTOREDUCTTHRESHOLD_CHK, 0), NULL);
				SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_AUTOREDUCTTIMEOUT_CHK, 0), NULL);
				SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_AUTOREDUCTTIMEOUTTHRESHOLD_VALUE, 0), NULL);
				SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_HOTKEYENABLE_CHK, 0), NULL);
			}
			else if((INT)lparam == IDD_SETTINGS_4)
			{
				CheckDlgButton(hwnd, IDC_TRAYUSETRANSPARENCY_CHK, _r_cfg_read(L"TrayUseTransparency", 1) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_TRAYSHOWBORDER_CHK, _r_cfg_read(L"TrayShowBorder", 0) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_TRAYROUNDCORNERS_CHK, _r_cfg_read(L"TrayRoundCorners", 0) ? BST_CHECKED : BST_UNCHECKED);
				//CheckDlgButton(hwnd, IDC_TRAYROUNDCORNERS_CHK, _r_cfg_read(L"TrayRoundCorners", 0) ? BST_CHECKED : BST_UNCHECKED);

				SetDlgItemText(hwnd, IDC_FONT, _r_helper_format(L"<a href=\"#\">%ls, %dpx</a>", _r_cfg_read(L"TrayFontName", FONT_NAME), _r_cfg_read(L"TrayFontSize", FONT_SIZE)));

				SetProp(GetDlgItem(hwnd, IDC_COLOR_1), L"color", (HANDLE)_r_cfg_read(L"TrayColorBg", TRAY_COLOR_BG));
				SetProp(GetDlgItem(hwnd, IDC_COLOR_2), L"color", (HANDLE)_r_cfg_read(L"TrayColorText", TRAY_COLOR_TEXT));
				SetProp(GetDlgItem(hwnd, IDC_COLOR_3), L"color", (HANDLE)_r_cfg_read(L"TrayColorWarning", TRAY_COLOR_WARNING));
				SetProp(GetDlgItem(hwnd, IDC_COLOR_4), L"color", (HANDLE)_r_cfg_read(L"TrayColorDanger", TRAY_COLOR_DANGER));
			}

			break;
		}

		case WM_DESTROY:
		{
			if(GetProp(GetParent(hwnd), L"is_save"))
			{
				if(INT(GetProp(hwnd, L"id")) == IDD_SETTINGS_1)
				{
					_r_cfg_write(L"AlwaysOnTop", INT((IsDlgButtonChecked(hwnd, IDC_ALWAYSONTOP_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"StartMinimized", INT((IsDlgButtonChecked(hwnd, IDC_STARTMINIMIZED_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_autorun_cancer(APP_NAME, IsDlgButtonChecked(hwnd, IDC_LOADONSTARTUP_CHK) == BST_UNCHECKED);

					if(!_r_system_uacstate())
					{
						_r_skipuac_cancer(IsDlgButtonChecked(hwnd, IDC_SKIPUACWARNING_CHK) == BST_UNCHECKED);
					}

					_r_cfg_write(L"CheckUpdates", INT((IsDlgButtonChecked(hwnd, IDC_CHECKUPDATES_CHK) == BST_CHECKED) ? TRUE : FALSE));

					LCID lang = (LCID)SendDlgItemMessage(hwnd, IDC_LANGUAGE, CB_GETITEMDATA, SendDlgItemMessage(hwnd, IDC_LANGUAGE, CB_GETCURSEL, 0, NULL), NULL);

					if(lang <= 0)
					{
						lang = NULL;
					}

					if((lang != _r_lcid))
					{
						SetProp(_r_hwnd, L"is_restart", (HANDLE)TRUE);
					}

					_r_locale_set(lang);
				}
				else if(INT(GetProp(hwnd, L"id")) == IDD_SETTINGS_2)
				{
					DWORD mask = 0;
					
					if(IsDlgButtonChecked(hwnd, IDC_WORKINGSET_CHK) == BST_CHECKED)
					{
						mask |= REDUCT_WORKING_SET;
					}
					
					if(IsDlgButtonChecked(hwnd, IDC_SYSTEMWORKINGSET_CHK) == BST_CHECKED)
					{
						mask |= REDUCT_SYSTEM_WORKING_SET;
					}
					
					if(IsDlgButtonChecked(hwnd, IDC_STANDBYLISTPRIORITY0_CHK) == BST_CHECKED)
					{
						mask |= REDUCT_STANDBY_PRIORITY0_LIST;
					}
					
					if(IsDlgButtonChecked(hwnd, IDC_STANDBYLIST_CHK) == BST_CHECKED)
					{
						mask |= REDUCT_STANDBY_LIST;
					}

					if(IsDlgButtonChecked(hwnd, IDC_MODIFIEDLIST_CHK) == BST_CHECKED)
					{
						mask |= REDUCT_MODIFIED_LIST;
					}

					_r_cfg_write(L"ReductMask", mask);
				}
				else if(INT(GetProp(hwnd, L"id")) == IDD_SETTINGS_3)
				{
					_r_cfg_write(L"AutoreductThreshold", (IsDlgButtonChecked(hwnd, IDC_AUTOREDUCTTHRESHOLD_CHK) == BST_CHECKED) ? TRUE : FALSE);
					_r_cfg_write(L"AutoreductThresholdValue", (DWORD)SendDlgItemMessage(hwnd, IDC_AUTOREDUCTTHRESHOLD_VALUE, UDM_GETPOS32, 0, NULL));
				
					_r_cfg_write(L"AutoreductTimeout", (IsDlgButtonChecked(hwnd, IDC_AUTOREDUCTTIMEOUT_CHK) == BST_CHECKED) ? TRUE : FALSE);
					_r_cfg_write(L"AutoreductTimeoutValue", (DWORD)SendDlgItemMessage(hwnd, IDC_AUTOREDUCTTIMEOUT_VALUE, UDM_GETPOS32, 0, NULL));

					_r_cfg_write(L"AutoreductTimeoutThresholdValue", (DWORD)SendDlgItemMessage(hwnd, IDC_AUTOREDUCTTIMEOUTTHRESHOLD_VALUE, UDM_GETPOS32, 0, NULL));

					_r_cfg_write(L"HotkeyEnable", (IsDlgButtonChecked(hwnd, IDC_HOTKEYENABLE_CHK) == BST_CHECKED) ? TRUE : FALSE);
					_r_cfg_write(L"Hotkey", (DWORD)SendDlgItemMessage(hwnd, IDC_HOTKEY, HKM_GETHOTKEY, 0, NULL));
				}
				else if(INT(GetProp(hwnd, L"id")) == IDD_SETTINGS_4)
				{
					_r_cfg_write(L"TrayUseTransparency", (IsDlgButtonChecked(hwnd, IDC_TRAYUSETRANSPARENCY_CHK) == BST_CHECKED) ? TRUE : FALSE);
					_r_cfg_write(L"TrayShowBorder", (IsDlgButtonChecked(hwnd, IDC_TRAYSHOWBORDER_CHK) == BST_CHECKED) ? TRUE : FALSE);
					_r_cfg_write(L"TrayRoundCorners", (IsDlgButtonChecked(hwnd, IDC_TRAYROUNDCORNERS_CHK) == BST_CHECKED) ? TRUE : FALSE);

					_r_cfg_write(L"TrayColorBg", (DWORD)GetProp(GetDlgItem(hwnd, IDC_COLOR_1), L"color"));
					_r_cfg_write(L"TrayColorText", (DWORD)GetProp(GetDlgItem(hwnd, IDC_COLOR_2), L"color"));
					_r_cfg_write(L"TrayColorWarning", (DWORD)GetProp(GetDlgItem(hwnd, IDC_COLOR_3), L"color"));
					_r_cfg_write(L"TrayColorDanger", (DWORD)GetProp(GetDlgItem(hwnd, IDC_COLOR_4), L"color"));
				}
			}

			break;
		}


		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch(nmlp->code)
			{
				case NM_CLICK:
				case NM_RETURN:
				{
					PNMLINK nmlp = (PNMLINK)lparam;

					if(nmlp->hdr.idFrom == IDC_FONT)
					{
						CHOOSEFONT cf = {0};

						cf.lStructSize = sizeof(cf);
						cf.hwndOwner = hwnd;
						cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL;

						data.lf.lfHeight /= SCALE; // font size fix
						cf.lpLogFont = &data.lf;

						if(ChooseFont(&cf))
						{
							data.lf.lfHeight = MulDiv(-data.lf.lfHeight, 72, GetDeviceCaps(data.cdc1, LOGPIXELSY));
							SetDlgItemText(hwnd, IDC_FONT, _r_helper_format(L"<a href=\"#\">%ls, %dpx</a>", data.lf.lfFaceName, data.lf.lfHeight));

							_r_cfg_write(L"TrayFontName", data.lf.lfFaceName);
							_r_cfg_write(L"TrayFontSize", data.lf.lfHeight);
							_r_cfg_write(L"TrayFontWeight", data.lf.lfWeight);

							_Application_Initialize(FALSE);
						}
					}

					break;
				}

				case NM_CUSTOMDRAW:
				{
					LPNMCUSTOMDRAW lpnmcd = (LPNMCUSTOMDRAW)lparam;

					if(nmlp->idFrom >= IDC_COLOR_1 && nmlp->idFrom <= IDC_COLOR_4)
					{
						lpnmcd->rc.left += 3;
						lpnmcd->rc.top += 3;
						lpnmcd->rc.right -= 3;
						lpnmcd->rc.bottom -= 3;

						COLORREF clr_prev = SetBkColor(lpnmcd->hdc, (COLORREF)GetProp(nmlp->hwndFrom, L"color"));
						ExtTextOut(lpnmcd->hdc, 0, 0, ETO_OPAQUE, &lpnmcd->rc, NULL, 0, NULL);
						SetBkColor(lpnmcd->hdc, clr_prev);

						return CDRF_DODEFAULT | CDRF_DOERASE;
					}

					break;
				}
			}
			break;
		}

		case WM_COMMAND:
		{
			switch(LOWORD(wparam))
			{
				case IDC_AUTOREDUCTTHRESHOLD_CHK:
				{
					BOOL is_enabled = IsWindowEnabled(GetDlgItem(hwnd, IDC_AUTOREDUCTTHRESHOLD_CHK)) && (IsDlgButtonChecked(hwnd, IDC_AUTOREDUCTTHRESHOLD_CHK) == BST_CHECKED);

					EnableWindow(GetDlgItem(hwnd, IDC_AUTOREDUCTTHRESHOLD_VALUE), is_enabled);
					EnableWindow((HWND)SendDlgItemMessage(hwnd, IDC_AUTOREDUCTTHRESHOLD_VALUE, UDM_GETBUDDY, 0, NULL), is_enabled);

					break;
				}

				case IDC_AUTOREDUCTTIMEOUT_CHK:
				{
					BOOL is_enabled = IsWindowEnabled(GetDlgItem(hwnd, IDC_AUTOREDUCTTIMEOUT_CHK)) && (IsDlgButtonChecked(hwnd, IDC_AUTOREDUCTTIMEOUT_CHK) == BST_CHECKED);
		
					EnableWindow(GetDlgItem(hwnd, IDC_AUTOREDUCTTIMEOUT_VALUE), is_enabled);
					EnableWindow((HWND)SendDlgItemMessage(hwnd, IDC_AUTOREDUCTTIMEOUT_VALUE, UDM_GETBUDDY, 0, NULL), is_enabled);

					EnableWindow(GetDlgItem(hwnd, IDC_AUTOREDUCTTIMEOUTTHRESHOLD), is_enabled);
					EnableWindow(GetDlgItem(hwnd, IDC_AUTOREDUCTTIMEOUTTHRESHOLD_VALUE), is_enabled);
					EnableWindow((HWND)SendDlgItemMessage(hwnd, IDC_AUTOREDUCTTIMEOUTTHRESHOLD_VALUE, UDM_GETBUDDY, 0, NULL), is_enabled);

					break;
				}

				case IDC_HOTKEYENABLE_CHK:
				{
					BOOL is_enabled = IsWindowEnabled(GetDlgItem(hwnd, IDC_HOTKEYENABLE_CHK)) && (IsDlgButtonChecked(hwnd, IDC_HOTKEYENABLE_CHK) == BST_CHECKED);

					EnableWindow(GetDlgItem(hwnd, IDC_HOTKEY), is_enabled);

					break;
				}

				case IDC_FONT:
				{

					break;
				}

				case IDC_COLOR_1:
				case IDC_COLOR_2:
				case IDC_COLOR_3:
				case IDC_COLOR_4:
				{
					CHOOSECOLOR cc = {0};
					COLORREF cust[16] = {TRAY_COLOR_BG, TRAY_COLOR_TEXT, TRAY_COLOR_WARNING, TRAY_COLOR_DANGER};

					cc.lStructSize = sizeof(cc);
					cc.Flags = CC_RGBINIT | CC_FULLOPEN;
					cc.hwndOwner = hwnd;
					cc.lpCustColors = cust;
					cc.rgbResult = (COLORREF)GetProp(GetDlgItem(hwnd, LOWORD(wparam)), L"color");

					if(ChooseColor(&cc))
					{
						SetProp(GetDlgItem(hwnd, LOWORD(wparam)), L"color", (HANDLE)cc.rgbResult);
					}

					break;
				}

			}

			break;
		}
	}

	return FALSE;
}

INT_PTR CALLBACK SettingsDlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG:
		{
			_r_windowcenter(hwnd);

			_r_treeview_setstyle(hwnd, IDC_NAV, TVS_EX_DOUBLEBUFFER, GetSystemMetrics(SM_CYSMICON));

			for(INT i = 0; i < APP_SETTINGS_COUNT; i++)
			{
				_r_treeview_additem(hwnd, IDC_NAV, _r_locale(IDS_SETTINGS_1 + i), -1, (LPARAM)CreateDialogParam(NULL, MAKEINTRESOURCE(IDD_SETTINGS_1 + i), hwnd, PagesDlgProc, IDD_SETTINGS_1 + i));
			}

			SendDlgItemMessage(hwnd, IDC_NAV, TVM_SELECTITEM, TVGN_CARET, SendDlgItemMessage(hwnd, IDC_NAV, TVM_GETNEXTITEM, TVGN_FIRSTVISIBLE, NULL)); // select 1-st item

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR lphdr = (LPNMHDR)lparam;

			if(lphdr->idFrom == IDC_NAV)
			{
				switch(lphdr->code)
				{
					case TVN_SELCHANGED:
					{
						LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lparam;

						ShowWindow((HWND)GetProp(hwnd, L"hwnd"), SW_HIDE);

						SetProp(hwnd, L"hwnd", (HANDLE)pnmtv->itemNew.lParam);

						ShowWindow((HWND)pnmtv->itemNew.lParam, SW_SHOW);

						break;
					}
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			switch(LOWORD(wparam))
			{
				case IDOK: // process Enter key
				case IDC_OK:
				{
					SetProp(hwnd, L"is_save", (HANDLE)TRUE); // save settings indicator

					// without break;
				}

				case IDCANCEL: // process Esc key
				case IDC_CANCEL:
				{
					EndDialog(hwnd, NULL);
					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

LRESULT CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if(msg == WM_TASKBARCREATED)
	{
		_Application_Initialize(TRUE);
		return FALSE;
	}

	switch(msg)
	{
		case WM_INITDIALOG:
		{
			// static initializer
			_r_hwnd = hwnd;

			data.is_admin = _r_system_adminstate();
			data.is_supported_os = _r_system_validversion(6, 0);

			// set privileges
			if(data.is_admin)
			{
				_r_system_setprivilege(SE_INCREASE_QUOTA_NAME, TRUE);
				_r_system_setprivilege(SE_PROF_SINGLE_PROCESS_NAME, TRUE);
			}

			// set priority
			SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

			// dynamic initializer
			_Application_Initialize(TRUE);

			// enable taskbarcreated message bypass uipi
			if(data.is_supported_os)
			{
				CWMFEX _cwmfex = (CWMFEX)GetProcAddress(GetModuleHandle(L"user32.dll"), "ChangeWindowMessageFilterEx");

				if(_cwmfex)
				{
					_cwmfex(hwnd, WM_TASKBARCREATED, MSGFLT_ALLOW, NULL); // windows 7
				}
				else
				{
					CWMF _cwmf = (CWMF)GetProcAddress(GetModuleHandle(L"user32.dll"), "ChangeWindowMessageFilter");

					if(_cwmf)
					{
						_cwmf(WM_TASKBARCREATED, MSGFLT_ALLOW); // windows vista
					}
				}
			}

			// uac indicator (windows vista and above)
			if(_r_system_uacstate())
			{
				RECT rc = {0};

				SendDlgItemMessage(hwnd, IDC_OK, BCM_SETSHIELD, 0, TRUE);

				SendDlgItemMessage(hwnd, IDC_OK, BCM_GETTEXTMARGIN, 0, (LPARAM)&rc);
				rc.left += GetSystemMetrics(SM_CXSMICON) / 2;
				SendDlgItemMessage(hwnd, IDC_OK, BCM_SETTEXTMARGIN, 0, (LPARAM)&rc);
			}

			// configure listview
			_r_listview_setstyle(hwnd, IDC_LISTVIEW, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP);

			_r_listview_addcolumn(hwnd, IDC_LISTVIEW, NULL, 50, 1, LVCFMT_RIGHT);
			_r_listview_addcolumn(hwnd, IDC_LISTVIEW, NULL, 50, 2, LVCFMT_LEFT);

			for(INT i = 0; i < 3; i++)
			{
				_r_listview_addgroup(hwnd, IDC_LISTVIEW, i, _r_locale(IDS_GROUP_1 + i));

				for(INT j = 0; j < 3; j++)
				{
					_r_listview_additem(hwnd, IDC_LISTVIEW, _r_locale(IDS_ITEM_1 + j), -1, 0, -1, i);
				}
			}

			if(!wcsstr(GetCommandLine(), L"/minimized") && !_r_cfg_read(L"StartMinimized", 1))
			{
				_r_windowtoggle(hwnd, TRUE);
			}

			//return TRUE;
			break;
		}

		case WM_DESTROY:
		{
			_Application_Unitialize(TRUE);

			PostQuitMessage(0);

			break;
		}

		case WM_QUERYENDSESSION:
		{
			return TRUE;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			HDC dc = BeginPaint(hwnd, &ps);

			RECT rc = {0};
			GetClientRect(hwnd, &rc);

			static INT height = GetSystemMetrics(SM_CYSIZE) * 2;

			rc.top = rc.bottom - height;
			rc.bottom = rc.top + height;

			COLORREF clr_prev = SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
			ExtTextOut(dc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
			SetBkColor(dc, clr_prev);

			for(INT i = 0; i < rc.right; i++)
			{
				SetPixel(dc, i, rc.top, RGB(223, 223, 223));
			}

			EndPaint(hwnd, &ps);

			break;
		}

		case WM_HOTKEY:
		{
			if(wparam == UID)
			{
				_Application_Reduct(NULL, 0);
			}

			break;
		}

		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORDLG:
		{
			return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			if(nmlp->idFrom == IDC_LISTVIEW)
			{
				switch(nmlp->code)
				{
					case NM_CUSTOMDRAW:
					{
						LONG result = CDRF_DODEFAULT;
						LPNMLVCUSTOMDRAW lpnmlv = (LPNMLVCUSTOMDRAW)lparam;

						switch(lpnmlv->nmcd.dwDrawStage)
						{
							case CDDS_PREPAINT:
							{
								result = (CDRF_NOTIFYPOSTPAINT | CDRF_NOTIFYITEMDRAW);
								break;
							}

							case CDDS_ITEMPREPAINT:
							{
								if((UINT)lpnmlv->nmcd.lItemlParam >= data.autoreduct_threshold_value)
								{
									lpnmlv->clrText = TRAY_COLOR_WARNING;
									result = (CDRF_NOTIFYPOSTPAINT | CDRF_NEWFONT);

								}

								break;
							}
						}

						SetWindowLongPtr(hwnd, DWLP_MSGRESULT, result);
						return TRUE;
					}
				}
			}

			break;
		}

		case WM_SIZE:
		{
			if(wparam == SIZE_MINIMIZED)
			{
				_r_windowtoggle(hwnd, FALSE);
			}

			break;
		}

		case WM_SYSCOMMAND:
		{
			if(wparam == SC_CLOSE)
			{
				_r_windowtoggle(hwnd, FALSE);
				return TRUE;
			}

			break;
		}

		case WM_TRAYICON:
		{
			switch(LOWORD(lparam))
			{
				case WM_LBUTTONDBLCLK:
				{
					SendMessage(hwnd, WM_COMMAND, MAKELPARAM(IDM_TRAY_SHOW, 0), NULL);
					break;
				}

				case WM_RBUTTONUP:
				case WM_CONTEXTMENU:
				{
					HMENU menu = LoadMenu(NULL, MAKEINTRESOURCE(IDM_TRAY)), submenu = GetSubMenu(menu, 0);

					SetForegroundWindow(hwnd); // don't touch

					POINT pt = {0};
					GetCursorPos(&pt);

					TrackPopupMenuEx(submenu, TPM_RIGHTBUTTON | TPM_LEFTBUTTON, pt.x, pt.y, hwnd, NULL);

					DestroyMenu(menu);
					DestroyMenu(submenu);

					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			switch(LOWORD(wparam))
			{
				case IDM_SETTINGS:
				case IDM_TRAY_SETTINGS:
				{
					if(!GetProp(hwnd, L"is_settings_opened"))
					{
						SetProp(hwnd, L"is_settings_opened", (HANDLE)TRUE);

						DialogBox(NULL, MAKEINTRESOURCE(IDD_SETTINGS), hwnd, SettingsDlgProc);

						if(GetProp(hwnd, L"is_restart"))
						{
							_r_uninitialize(TRUE);
						}

						SetProp(hwnd, L"is_settings_opened", FALSE);

						_Application_Initialize(FALSE);
					}

					break;
				}

				case IDM_EXIT:
				case IDM_TRAY_EXIT:
				{
					DestroyWindow(hwnd);
					break;
				}

				case IDCANCEL: // process Esc key
				case IDM_TRAY_SHOW:
				{
					_r_windowtoggle(hwnd, FALSE);
					break;
				}

				case IDC_OK:
				case IDM_TRAY_REDUCT:
				{
					if(_r_system_uacstate())
					{
						if(_r_skipuac_run())
						{
							DestroyWindow(hwnd);
						}
						else
						{
							_Application_ShowNotification(NIIF_ERROR, APP_NAME, _r_locale(IDS_BALLOON_NOPRIVILEGES));
						}
					}
					else
					{
						if(!GetProp(hwnd, L"is_reduct_opened"))
						{
							SetProp(hwnd, L"is_reduct_opened", (HANDLE)TRUE);

							_Application_Reduct(hwnd, 0);

							SetProp(hwnd, L"is_reduct_opened", FALSE);
						}
					}

					break;
				}

				case IDM_WEBSITE:
				case IDM_TRAY_WEBSITE:
				{
					ShellExecute(hwnd, NULL, APP_WEBSITE L"/product/" APP_NAME_SHORT, NULL, NULL, 0);
					break;
				}
					
				case IDM_CHECKUPDATES:
				{
					_r_updatecheck(FALSE);
					break;
				}

				case IDM_ABOUT:
				case IDM_TRAY_ABOUT:
				{
					if(!GetProp(hwnd, L"is_about_opened"))
					{
						SetProp(hwnd, L"is_about_opened", (HANDLE)TRUE);

						_r_aboutbox(hwnd);

						SetProp(hwnd, L"is_about_opened", FALSE);
					}

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

INT APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, INT)
{
	if(_r_initialize((DLGPROC)DlgProc))
	{
		MSG msg = {0};

		while(GetMessage(&msg, NULL, 0, 0))
		{
			if(!IsDialogMessage(_r_hwnd, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	_r_uninitialize(FALSE);

	return ERROR_SUCCESS;
}
