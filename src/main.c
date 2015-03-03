// Mem Reduct
// Copyright © 2011-2013, 2015 Henry++

#include <windows.h>

#include "main.h"

#include "resource.h"
#include "routine.h"

NOTIFYICONDATA nid = {0};
STATIC_DATA sd = {0};

CONST UINT WM_TASKBARCREATED = RegisterWindowMessage(L"TaskbarCreated");

BOOL _Reduct_ShowNotification(DWORD icon, LPCWSTR title, LPCWSTR text, BOOL important)
{
	if(!important && sd.statistic_last_baloon && (GetTickCount() - sd.statistic_last_baloon) <= 7500)
	{
		return FALSE;
	}

	nid.uFlags = NIF_INFO;
	nid.dwInfoFlags = NIIF_RESPECT_QUIET_TIME | icon;

	StringCchCopy(nid.szInfoTitle, _countof(nid.szInfoTitle), title);
	StringCchCopy(nid.szInfo, _countof(nid.szInfo), text);

	Shell_NotifyIcon(NIM_MODIFY, &nid);

	nid.szInfo[0] = nid.szInfoTitle[0] = NULL; // clean

	sd.statistic_last_baloon = GetTickCount();

	return TRUE;
}

DWORD _Application_GetMemoryStatus(MEMORYINFO* m)
{
	MEMORYSTATUSEX msex = {0};
	SYSTEM_CACHE_INFORMATION sci = {0};

	msex.dwLength = sizeof(msex);

	if(GlobalMemoryStatusEx(&msex) && m) // WARNING!!! don't tounch "m"!
	{
		m->percent_phys = msex.dwMemoryLoad;

		m->free_phys = msex.ullAvailPhys;
		m->total_phys = msex.ullTotalPhys;

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

BOOL _Application_Reduct(HWND hwnd)
{
	if(!sd.is_admin || (hwnd && _r_msg(MB_YESNO | MB_ICONQUESTION, L"Memory reduction", L"Do you want to start memory reduction?") == IDNO))
	{
		return FALSE;
	}

	sd.reduct_before = _Application_GetMemoryStatus(NULL);

	SYSTEM_MEMORY_LIST_COMMAND smlc;

	// Working set
	if(sd.is_supported_os && _r_cfg_read(L"ReductWorkingSet", 1))
	{
		smlc = MemoryEmptyWorkingSets;
		NtSetSystemInformation(SystemMemoryListInformation, &smlc, sizeof(smlc));
	}

	// System working set
	if(_r_cfg_read(L"ReductSystemWorkingSet", 1))
	{
		SYSTEM_CACHE_INFORMATION sci = {0};

		sci.MinimumWorkingSet = (ULONG)-1;
		sci.MaximumWorkingSet = (ULONG)-1;

		NtSetSystemInformation(SystemFileCacheInformation, &sci, sizeof(sci));
	}

	// Standby priority-0 list
	if(sd.is_supported_os && _r_cfg_read(L"ReductStandbyPriority0List", 1))
	{
		smlc = MemoryPurgeLowPriorityStandbyList;
		NtSetSystemInformation(SystemMemoryListInformation, &smlc, sizeof(smlc));
	}

	// Standby list
	if(sd.is_supported_os && _r_cfg_read(L"ReductStandbyList", 0))
	{
		smlc = MemoryPurgeStandbyList;
		NtSetSystemInformation(SystemMemoryListInformation, &smlc, sizeof(smlc));
	}

	// Modified list
	if(sd.is_supported_os && _r_cfg_read(L"ReductModifiedList", 0))
	{
		smlc = MemoryFlushModifiedList;
		NtSetSystemInformation(SystemMemoryListInformation, &smlc, sizeof(smlc));
	}

	sd.reduct_after = _Application_GetMemoryStatus(NULL);

	// Statistic
	_r_cfg_write(L"StatisticLastResult", sd.reduct_before - sd.reduct_after); // last result
	_r_cfg_write(L"StatisticMaxResult", max(_r_cfg_read(L"StatisticMaxResult", 0), sd.reduct_before - sd.reduct_after)); // maximum result

	sd.statistic_last_reduct = _r_helper_unixtime64();
	_r_cfg_write(L"StatisticLastReduct", (DWORD)sd.statistic_last_reduct); // time of last cleaning

	_r_cfg_write(L"StatisticTotalReduct", _r_cfg_read(L"StatisticTotalReduct", 0) + 1); // total count

	// Show result
	_Reduct_ShowNotification(NIIF_INFO, APP_NAME, _r_helper_format(_r_locale(IDS_BALLOON_REDUCT), _r_helper_formatsize64((DWORDLONG)ROUTINE_PERCENT_VAL(sd.reduct_before - sd.reduct_after, sd.ms.total_phys))), TRUE);

	return TRUE;
}

HICON _Application_GenerateIcon()
{
	COLORREF clrText = 0, clrTextBk= COLOR_TRAY_MASK;

	BOOL is_transparent = TRUE;

	if(sd.color_indication_tray)
	{
		if(sd.ms.percent_phys >= sd.level_danger)
		{
			is_transparent = FALSE;

			clrText = 0xFFFFFF;
			clrTextBk = sd.color_danger;
		}
		else if(sd.ms.percent_phys >= sd.level_warning)
		{
			is_transparent = FALSE;

			clrText = 0xFFFFFF;
			clrTextBk = sd.color_warning;
		}
	}

	if(!clrText) clrText = is_transparent ? 0 : 0xFFFFFF;

	HBITMAP old_bitmap = (HBITMAP)SelectObject(sd.cdc, sd.bitmap);

	// Draw background
	COLORREF clr_prev = SetBkColor(sd.cdc, clrTextBk);
	ExtTextOut(sd.cdc, 0, 0, ETO_OPAQUE, &sd.rc, NULL, 0, NULL);
	SetBkColor(sd.cdc, clr_prev);

	// Draw text
	SetTextColor(sd.cdc, clrText);
	SetBkMode(sd.cdc, TRANSPARENT);

	CString buffer = _r_helper_format(L"%d\0", sd.ms.percent_phys);
	DrawTextEx(sd.cdc, buffer.GetBuffer(), buffer.GetLength(), &sd.rc, DT_VCENTER | DT_CENTER | DT_SINGLELINE | DT_NOCLIP, NULL);

	SelectObject(sd.dc, old_bitmap);

	// Transparency
	if(is_transparent)
	{
		DWORD* pixel = (DWORD*)sd.rgb;

		for(INT i = ((sd.rc.right * sd.rc.right) - 1); i >= 0; i--)
		{
			*pixel &= COLOR_TRAY_MASK;
			*pixel |= ((*pixel == COLOR_TRAY_MASK) ? 0 : 0xffffffff);

			pixel++;
		}
	}

	ICONINFO ii = {TRUE, 0, 0, sd.bitmap_mask, sd.bitmap};

	return CreateIconIndirect(&ii);
}

VOID CALLBACK _Application_MonitorCallback(HWND hwnd, UINT, UINT_PTR, DWORD)
{
	_Application_GetMemoryStatus(&sd.ms);

	if(nid.hIcon)
	{
		DestroyIcon(nid.hIcon);
	}

	nid.uFlags = NIF_ICON | NIF_TIP;

	StringCchPrintf(nid.szTip, _countof(nid.szTip), _r_locale(IDS_TRAY_TOOLTIP), sd.ms.percent_phys, sd.ms.percent_page, sd.ms.percent_ws);
	nid.hIcon = _Application_GenerateIcon();

	Shell_NotifyIcon(NIM_MODIFY, &nid);

	BOOL has_danger = (sd.ms.percent_phys >= sd.level_danger);
	BOOL has_warning = (sd.ms.percent_phys >= sd.level_warning);

	// Autoreduct
	if(sd.is_admin)
	{
		if(
			(sd.autoreduct_percent_mode && (sd.ms.percent_phys >= sd.autoreduct_percent_value)) ||
			(sd.autoreduct_interval_mode && ((_r_helper_unixtime64() - sd.statistic_last_reduct) >= sd.autoreduct_interval_value * 60))
		)
		{
			_Application_Reduct(NULL);
		}
	}

	// Balloon
	if((has_danger && sd.balloon_danger) || (has_warning && sd.balloon_warning))
	{
		_Reduct_ShowNotification(has_danger ? NIIF_ERROR : NIIF_WARNING, APP_NAME, _r_locale(has_danger ? IDS_BALLOON_DANGER_LEVEL : IDS_BALLOON_WARNING_LEVEL), FALSE);
	}

	if(IsWindowVisible(hwnd))
	{
		// Physical
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_format(L"%d%%", sd.ms.percent_phys), 0, 1, -1, -1, sd.ms.percent_phys);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(sd.ms.free_phys), 1, 1, -1, -1, sd.ms.percent_phys);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(sd.ms.total_phys), 2, 1, -1, -1, sd.ms.percent_phys);
		
		// Page file
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_format(L"%d%%", sd.ms.percent_page), 3, 1, -1, -1, sd.ms.percent_page);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(sd.ms.free_page), 4, 1, -1, -1, sd.ms.percent_page);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(sd.ms.total_page), 5, 1, -1, -1, sd.ms.percent_page);

		// System working set
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_format(L"%d%%", sd.ms.percent_ws), 6, 1, -1, -1, sd.ms.percent_ws);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(sd.ms.free_ws), 7, 1, -1, -1, sd.ms.percent_ws);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(sd.ms.total_ws), 8, 1, -1, -1, sd.ms.percent_ws);

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

	DeleteObject(sd.font);
	DeleteDC(sd.cdc);
	DeleteDC(sd.dc);
	DeleteObject(sd.bitmap);
	DeleteObject(sd.bitmap_mask);
}

VOID _Application_Initialize(BOOL createtrayicon)
{
	_Application_Unitialize(createtrayicon);

	sd.statistic_last_reduct = _r_cfg_read(L"StatisticLastReduct", 0);

	sd.color_indication_listview = _r_cfg_read(L"ColorIndicationListview", 1);
	sd.color_indication_tray = _r_cfg_read(L"ColorIndicationTray", 1);

	sd.level_warning = _r_cfg_read(L"LevelWarning", 60);
	sd.level_danger = _r_cfg_read(L"LevelDanger", 90);

	sd.color_warning = _r_cfg_read(L"ColorWarning", COLOR_LEVEL_WARNING);
	sd.color_danger = _r_cfg_read(L"ColorDanger", COLOR_LEVEL_DANGER);
	
	sd.autoreduct_percent_mode = _r_cfg_read(L"AutoreductPercentMode", 0);
	sd.autoreduct_percent_value = _r_cfg_read(L"AutoreductPercentValue", 90);

	sd.autoreduct_interval_mode = _r_cfg_read(L"AutoreductIntervalMode", 0);
	sd.autoreduct_interval_value = _r_cfg_read(L"AutoreductIntervalValue", 30);

	sd.balloon_warning = _r_cfg_read(L"BalloonWarning", 0);
	sd.balloon_danger = _r_cfg_read(L"BalloonDanger", 1);

	sd.rc.right = GetSystemMetrics(SM_CXSMICON);
	sd.rc.bottom = GetSystemMetrics(SM_CYSMICON);

	BITMAPINFO bmi = {0};

    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = sd.rc.right;
    bmi.bmiHeader.biHeight = sd.rc.bottom;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = 0;

	sd.dc = GetDC(NULL);

	sd.bitmap = CreateDIBSection(sd.dc, &bmi, DIB_RGB_COLORS, (LPVOID*)&sd.rgb, NULL, 0);
	sd.bitmap_mask = CreateBitmap(sd.rc.right, sd.rc.bottom, 1, 1, NULL);;

	sd.cdc = CreateCompatibleDC(sd.dc);

	ReleaseDC(NULL, sd.dc);

	// Font
	LOGFONT lf = {0};

	lf.lfQuality = ANTIALIASED_QUALITY;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfPitchAndFamily = FF_DONTCARE;
	lf.lfHeight = -MulDiv(8, GetDeviceCaps(sd.cdc, LOGPIXELSY), 72);

	StringCchCopy(lf.lfFaceName, LF_FACESIZE, L"Tahoma");

	sd.font = CreateFontIndirect(&lf);

	SelectObject(sd.cdc, sd.font);

	// Hotkey
	UINT hk = _r_cfg_read(L"Hotkey", 0);

	if(sd.is_admin && hk && _r_cfg_read(L"HotkeyEnable", 0))
	{
		RegisterHotKey(_r_hwnd, UID, (HIBYTE(hk) & 2) | ((HIBYTE(hk) & 4) >> 2) | ((HIBYTE(hk) & 1) << 2), LOBYTE(hk));
	}

	// Always on top
	_r_windowtotop(_r_hwnd, _r_cfg_read(L"AlwaysOnTop", 0));

	// Tray icon
	if(createtrayicon)
	{
		nid.cbSize = sd.is_supported_os ? sizeof(nid) : NOTIFYICONDATA_V3_SIZE;
		nid.uVersion = sd.is_supported_os ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION;
		nid.hWnd = _r_hwnd;
		nid.uID = UID;
		nid.uFlags = NIF_MESSAGE | NIF_ICON;
		nid.uCallbackMessage = WM_TRAYICON;
		nid.hIcon = _Application_GenerateIcon();

		Shell_NotifyIcon(NIM_ADD, &nid);
	}

	// Timer
	_Application_MonitorCallback(_r_hwnd, 0, NULL, 0);
	SetTimer(_r_hwnd, UID, 750, _Application_MonitorCallback);
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
				if(!sd.is_supported_os || !sd.is_admin)
				{
					EnableWindow(GetDlgItem(hwnd, IDC_SKIPUACWARNING_CHK), FALSE);
				}

				CheckDlgButton(hwnd, IDC_ALWAYSONTOP_CHK, _r_cfg_read(L"AlwaysOnTop", 0) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_STARTMINIMIZED_CHK, _r_cfg_read(L"StartMinimized", 0) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_LOADONSTARTUP_CHK, _r_autorun_is_present(APP_NAME) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_SKIPUACWARNING_CHK, _r_skipuac_is_present(FALSE) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_CHECKUPDATES_CHK, _r_cfg_read(L"CheckUpdates", 1) ? BST_CHECKED : BST_UNCHECKED);

				SendDlgItemMessage(hwnd, IDC_LANGUAGE, CB_INSERTSTRING, 0, (LPARAM)L"System default");
				SendDlgItemMessage(hwnd, IDC_LANGUAGE, CB_SETCURSEL, 0, NULL);

				EnumResourceLanguages(NULL, RT_STRING, MAKEINTRESOURCE(63), _r_locale_enum, (LONG_PTR)GetDlgItem(hwnd, IDC_LANGUAGE));
			}
			else if((INT)lparam == IDD_SETTINGS_2)
			{
				if(!sd.is_supported_os || !sd.is_admin)
				{
					EnableWindow(GetDlgItem(hwnd, IDC_WORKINGSET_CHK), FALSE);
					EnableWindow(GetDlgItem(hwnd, IDC_STANDBYLISTPRIORITY0_CHK), FALSE);
					EnableWindow(GetDlgItem(hwnd, IDC_STANDBYLIST_CHK), FALSE);
					EnableWindow(GetDlgItem(hwnd, IDC_MODIFIEDLIST_CHK), FALSE);

					if(!sd.is_admin)
					{
						EnableWindow(GetDlgItem(hwnd, IDC_SYSTEMWORKINGSET_CHK), FALSE);

						EnableWindow(GetDlgItem(hwnd, IDC_AUTOREDUCTPERCENTMODE_CHK), FALSE);
						EnableWindow(GetDlgItem(hwnd, IDC_AUTOREDUCTINTERVALMODE_CHK), FALSE);

						EnableWindow(GetDlgItem(hwnd, IDC_HOTKEYENABLE_CHK), FALSE);
						EnableWindow(GetDlgItem(hwnd, IDC_HOTKEY), FALSE);
					}
				}

				CheckDlgButton(hwnd, IDC_WORKINGSET_CHK, _r_cfg_read(L"ReductWorkingSet", 1) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_SYSTEMWORKINGSET_CHK, _r_cfg_read(L"ReductSystemWorkingSet", 1) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_STANDBYLISTPRIORITY0_CHK, _r_cfg_read(L"ReductStandbyPriority0List", 1) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_STANDBYLIST_CHK, _r_cfg_read(L"ReductStandbyList", 0) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_MODIFIEDLIST_CHK, _r_cfg_read(L"ReductModifiedList", 0) ? BST_CHECKED : BST_UNCHECKED);
				
				CheckDlgButton(hwnd, IDC_AUTOREDUCTPERCENTMODE_CHK, _r_cfg_read(L"AutoreductPercentMode", 0) ? BST_CHECKED : BST_UNCHECKED);

				SendDlgItemMessage(hwnd, IDC_AUTOREDUCTPERCENTVALUE, UDM_SETRANGE32, 5, 99);
				SendDlgItemMessage(hwnd, IDC_AUTOREDUCTPERCENTVALUE, UDM_SETPOS32, 0, _r_cfg_read(L"AutoreductPercentValue", 90));

				CheckDlgButton(hwnd, IDC_AUTOREDUCTINTERVALMODE_CHK, _r_cfg_read(L"AutoreductIntervalMode", 0) ? BST_CHECKED : BST_UNCHECKED);

				SendDlgItemMessage(hwnd, IDC_AUTOREDUCTINTERVALVALUE, UDM_SETRANGE32, 5, 1440); // 5 minutes and 1 day
				SendDlgItemMessage(hwnd, IDC_AUTOREDUCTINTERVALVALUE, UDM_SETPOS32, 0, _r_cfg_read(L"AutoreductIntervalValue", 30));

				CheckDlgButton(hwnd, IDC_HOTKEYENABLE_CHK, _r_cfg_read(L"HotkeyEnable", 0) ? BST_CHECKED : BST_UNCHECKED);
				SendDlgItemMessage(hwnd, IDC_HOTKEY, HKM_SETHOTKEY, _r_cfg_read(L"Hotkey", 0), NULL);

				SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_AUTOREDUCTPERCENTMODE_CHK, 0), NULL);
				SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_AUTOREDUCTINTERVALMODE_CHK, 0), NULL);
				SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_HOTKEYENABLE_CHK, 0), NULL);
			}
			else if((INT)lparam == IDD_SETTINGS_3)
			{
				CheckDlgButton(hwnd, IDC_COLORINDICATIONLISTVIEW, _r_cfg_read(L"ColorIndicationListview", 1) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_COLORINDICATIONTRAY, _r_cfg_read(L"ColorIndicationTray", 1) ? BST_CHECKED : BST_UNCHECKED);

				SendDlgItemMessage(hwnd, IDC_LEVELWARNING, UDM_SETRANGE32, 1, 99);
				SendDlgItemMessage(hwnd, IDC_LEVELWARNING, UDM_SETPOS32, 0, _r_cfg_read(L"LevelWarning", 60));

				SetProp(GetDlgItem(hwnd, IDC_COLORWARNING), L"color", (HANDLE)_r_cfg_read(L"ColorWarning", COLOR_LEVEL_WARNING));

				SendDlgItemMessage(hwnd, IDC_LEVELDANGER, UDM_SETRANGE32, 1, 99);
				SendDlgItemMessage(hwnd, IDC_LEVELDANGER, UDM_SETPOS32, 0, _r_cfg_read(L"LevelDanger", 90));

				SetProp(GetDlgItem(hwnd, IDC_COLORDANGER), L"color", (HANDLE)_r_cfg_read(L"ColorDanger", COLOR_LEVEL_DANGER));
			}
			else if((INT)lparam == IDD_SETTINGS_4)
			{
				CheckDlgButton(hwnd, IDC_BALLOONWARNING_CHK, _r_cfg_read(L"BalloonWarning", 0) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_BALLOONDANGER_CHK, _r_cfg_read(L"BalloonDanger", 1) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_BALLOONREDUCT_CHK, _r_cfg_read(L"BalloonReduct", 1) ? BST_CHECKED : BST_UNCHECKED);

				for(INT i = 0; i < 3; i++)
				{
					SendDlgItemMessage(hwnd, IDC_TRAYDOUBLECICKACTION, CB_INSERTSTRING, i, (LPARAM)_r_locale(IDS_DOUBLECICKACTION_1 + i).GetBuffer());
				}

				SendDlgItemMessage(hwnd, IDC_TRAYDOUBLECICKACTION, CB_SETCURSEL, _r_cfg_read(L"TrayDoubleClickAction", 0), NULL);

				for(INT i = 0; i < 3; i++)
				{
					SendDlgItemMessage(hwnd, IDC_TRAYMEMORYREGION, CB_INSERTSTRING, i, (LPARAM)_r_locale(IDS_GROUP_1 + i).GetBuffer());
				}

				SendDlgItemMessage(hwnd, IDC_TRAYMEMORYREGION, CB_SETCURSEL, _r_cfg_read(L"TrayMemoryRegion", 0), NULL);
			}

			break;
		}

		case WM_DESTROY:
		{
			if(GetProp(GetParent(hwnd), L"is_save"))
			{
				if(INT(GetProp(hwnd, L"id")) == IDD_SETTINGS_1)
				{
					// general
					_r_cfg_write(L"AlwaysOnTop", INT((IsDlgButtonChecked(hwnd, IDC_ALWAYSONTOP_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"StartMinimized", INT((IsDlgButtonChecked(hwnd, IDC_STARTMINIMIZED_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_autorun_cancer(APP_NAME, IsDlgButtonChecked(hwnd, IDC_LOADONSTARTUP_CHK) == BST_UNCHECKED);

					if(!_r_system_uacstate())
					{
						_r_skipuac_cancer(IsDlgButtonChecked(hwnd, IDC_SKIPUACWARNING_CHK) == BST_UNCHECKED);
					}

					_r_cfg_write(L"CheckUpdates", INT((IsDlgButtonChecked(hwnd, IDC_CHECKUPDATES_CHK) == BST_CHECKED) ? TRUE : FALSE));

					// language
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
					_r_cfg_write(L"ReductWorkingSet", INT((IsDlgButtonChecked(hwnd, IDC_WORKINGSET_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"ReductSystemWorkingSet", INT((IsDlgButtonChecked(hwnd, IDC_SYSTEMWORKINGSET_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"ReductStandbyPriority0List", INT((IsDlgButtonChecked(hwnd, IDC_STANDBYLISTPRIORITY0_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"ReductStandbyList", INT((IsDlgButtonChecked(hwnd, IDC_STANDBYLIST_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"ReductModifiedList", INT((IsDlgButtonChecked(hwnd, IDC_MODIFIEDLIST_CHK) == BST_CHECKED) ? TRUE : FALSE));

					_r_cfg_write(L"AutoreductPercentMode", INT((IsDlgButtonChecked(hwnd, IDC_AUTOREDUCTPERCENTMODE_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"AutoreductPercentValue", (DWORD)SendDlgItemMessage(hwnd, IDC_AUTOREDUCTPERCENTVALUE, UDM_GETPOS32, 0, NULL));

					_r_cfg_write(L"AutoreductIntervalMode", INT((IsDlgButtonChecked(hwnd, IDC_AUTOREDUCTINTERVALMODE_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"AutoreductIntervalValue", (DWORD)SendDlgItemMessage(hwnd, IDC_AUTOREDUCTINTERVALVALUE, UDM_GETPOS32, 0, NULL));

					_r_cfg_write(L"HotkeyEnable", INT((IsDlgButtonChecked(hwnd, IDC_HOTKEYENABLE_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"Hotkey", (DWORD)SendDlgItemMessage(hwnd, IDC_HOTKEY, HKM_GETHOTKEY, 0, NULL));
				}
				else if(INT(GetProp(hwnd, L"id")) == IDD_SETTINGS_3)
				{
					_r_cfg_write(L"ColorIndicationListview", INT((IsDlgButtonChecked(hwnd, IDC_COLORINDICATIONLISTVIEW) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"ColorIndicationTray", INT((IsDlgButtonChecked(hwnd, IDC_COLORINDICATIONTRAY) == BST_CHECKED) ? TRUE : FALSE));

					_r_cfg_write(L"LevelWarning", (DWORD)SendDlgItemMessage(hwnd, IDC_LEVELWARNING, UDM_GETPOS32, 0, NULL));
					_r_cfg_write(L"ColorWarning", (DWORD)GetProp(GetDlgItem(hwnd, IDC_COLORWARNING), L"color"));

					_r_cfg_write(L"LevelDanger", (DWORD)SendDlgItemMessage(hwnd, IDC_LEVELDANGER, UDM_GETPOS32, 0, NULL));
					_r_cfg_write(L"ColorDanger", (DWORD)GetProp(GetDlgItem(hwnd, IDC_COLORDANGER), L"color"));
				}
				else if(INT(GetProp(hwnd, L"id")) == IDD_SETTINGS_4)
				{
					_r_cfg_write(L"BalloonWarning", INT((IsDlgButtonChecked(hwnd, IDC_BALLOONWARNING_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"BalloonDanger", INT((IsDlgButtonChecked(hwnd, IDC_BALLOONDANGER_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"BalloonReduct", INT((IsDlgButtonChecked(hwnd, IDC_BALLOONREDUCT_CHK) == BST_CHECKED) ? TRUE : FALSE));

					_r_cfg_write(L"TrayDoubleClickAction", (INT)SendDlgItemMessage(hwnd, IDC_TRAYDOUBLECICKACTION, CB_GETCURSEL, 0, NULL));
					_r_cfg_write(L"TrayMemoryRegion", (INT)SendDlgItemMessage(hwnd, IDC_TRAYMEMORYREGION, CB_GETCURSEL, 0, NULL));
				}
			}

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			if(nmlp->idFrom == IDC_COLORWARNING || nmlp->idFrom == IDC_COLORDANGER)
			{
				switch(nmlp->code)
				{
					case NM_CUSTOMDRAW:
					{
						LPNMCUSTOMDRAW lpnmcd = (LPNMCUSTOMDRAW)lparam;

						lpnmcd->rc.left += 3;
						lpnmcd->rc.top += 3;
						lpnmcd->rc.right -= 3;
						lpnmcd->rc.bottom -= 3;

						COLORREF clr_prev = SetBkColor(lpnmcd->hdc, (COLORREF)GetProp(nmlp->hwndFrom, L"color"));
						ExtTextOut(lpnmcd->hdc, 0, 0, ETO_OPAQUE, &lpnmcd->rc, NULL, 0, NULL);
						SetBkColor(lpnmcd->hdc, clr_prev);

						return CDRF_DODEFAULT | CDRF_DOERASE;
					}
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			switch(LOWORD(wparam))
			{
				case IDC_AUTOREDUCTINTERVALMODE_CHK:
				{
					BOOL is_enabled = IsWindowEnabled(GetDlgItem(hwnd, IDC_AUTOREDUCTINTERVALMODE_CHK)) && (IsDlgButtonChecked(hwnd, IDC_AUTOREDUCTINTERVALMODE_CHK) == BST_CHECKED);

					EnableWindow(GetDlgItem(hwnd, IDC_AUTOREDUCTINTERVALVALUE), is_enabled);
					EnableWindow((HWND)SendDlgItemMessage(hwnd, IDC_AUTOREDUCTINTERVALVALUE, UDM_GETBUDDY, 0, NULL), is_enabled);

					break;
				}

				case IDC_AUTOREDUCTPERCENTMODE_CHK:
				{
					BOOL is_enabled = IsWindowEnabled(GetDlgItem(hwnd, IDC_AUTOREDUCTPERCENTMODE_CHK)) && (IsDlgButtonChecked(hwnd, IDC_AUTOREDUCTPERCENTMODE_CHK) == BST_CHECKED);

					EnableWindow(GetDlgItem(hwnd, IDC_AUTOREDUCTPERCENTVALUE), is_enabled);
					EnableWindow((HWND)SendDlgItemMessage(hwnd, IDC_AUTOREDUCTPERCENTVALUE, UDM_GETBUDDY, 0, NULL), is_enabled);

					break;
				}
					
				case IDC_HOTKEYENABLE_CHK:
				{
					BOOL is_enabled = IsWindowEnabled(GetDlgItem(hwnd, IDC_HOTKEYENABLE_CHK)) && (IsDlgButtonChecked(hwnd, IDC_HOTKEYENABLE_CHK) == BST_CHECKED);

					EnableWindow(GetDlgItem(hwnd, IDC_HOTKEY), is_enabled);

					break;
				}

				case IDC_COLORWARNING:
				case IDC_COLORDANGER:
				{
					CHOOSECOLOR cc = {0};
					COLORREF cust[16] = {COLOR_LEVEL_WARNING, COLOR_LEVEL_DANGER};

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
			// center window
			_r_windowcenter(hwnd);

			// configure treeview
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
					EndDialog(hwnd, 0);
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
		return 0;
	}

	switch(msg)
	{
		case WM_INITDIALOG:
		{
			// static initializer
			_r_hwnd = hwnd;

			sd.is_admin = _r_system_adminstate();
			sd.is_supported_os = _r_system_validversion(6, 0);

			// set privileges
			if(sd.is_admin)
			{
				_r_system_setprivilege(SE_INCREASE_QUOTA_NAME, TRUE);
				_r_system_setprivilege(SE_PROF_SINGLE_PROCESS_NAME, TRUE);
			}

			// set priority
			SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

			// dynamic initializer
			_Application_Initialize(TRUE);

			// enable taskbarcreated message bypass uipi
			if(sd.is_supported_os)
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

			if(!wcsstr(GetCommandLine(), L"/minimized") && !_r_cfg_read(L"StartMinimized", 0))
			{
				_r_windowtoggle(hwnd, TRUE);
			}

			if(!sd.is_admin)
			{
				_Reduct_ShowNotification(NIIF_ERROR, APP_NAME, _r_locale(IDS_BALLOON_NOPRIVILEGES), TRUE);
			}

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
				_Application_Reduct(NULL);
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

			if(nmlp->idFrom == IDC_LISTVIEW && sd.color_indication_listview)
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
								if((UINT)lpnmlv->nmcd.lItemlParam >= sd.level_danger)
								{
									lpnmlv->clrText = sd.color_danger;
								}
								else if((UINT)lpnmlv->nmcd.lItemlParam >= sd.level_warning)
								{
									lpnmlv->clrText = sd.color_warning;
								}

								result = (CDRF_NOTIFYPOSTPAINT | CDRF_NEWFONT);

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
					switch(_r_cfg_read(L"TrayDoubleClickAction", 0))
					{
						case 1:
						{
							ShellExecute(hwnd, NULL, L"taskmgr.exe", NULL, NULL, 0);
							break;
						}
							
						case 2:
						{
							_Application_Reduct(NULL);
							break;
						}

						default:
						{
							SendMessage(hwnd, WM_COMMAND, MAKELPARAM(IDM_TRAY_SHOW, 0), NULL);
							break;
						}
					}

					break;
				}

				case WM_RBUTTONUP:
				case WM_CONTEXTMENU:
				{
					HMENU menu = LoadMenu(NULL, MAKEINTRESOURCE(IDM_TRAY)), submenu = GetSubMenu(menu, 0);

					SetForegroundWindow(hwnd);

					POINT pt = {0};
					GetCursorPos(&pt);

					if(IsWindowVisible(hwnd))
					{
						MENUITEMINFO mii = {0};

						mii.cbSize = sizeof(mii);
						mii.fMask = MIIM_STRING;

						CString buffer = _r_locale(IDS_TRAY_HIDE);

						mii.dwTypeData = buffer.GetBuffer();
						mii.cch = buffer.GetLength();

						SetMenuItemInfo(submenu, IDM_TRAY_SHOW, FALSE, &mii);
					}

					TrackPopupMenuEx(submenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_LEFTBUTTON | TPM_NOANIMATION, pt.x, pt.y, hwnd, NULL);

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
					}
					else
					{
						if(!GetProp(hwnd, L"is_reduct_opened"))
						{
							SetProp(hwnd, L"is_reduct_opened", (HANDLE)TRUE);

							_Application_Reduct(hwnd);

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

				case IDM_STATISTIC:
				{
					SYSTEMTIME st = {0};

					UINT last = _r_cfg_read(L"StatisticLastResult", 0);
					UINT maximum = _r_cfg_read(L"StatisticMaxResult", 0);

					_r_helper_unixtime2systemtime(sd.statistic_last_reduct, &st);

					_r_msg(MB_ICONINFORMATION, _r_locale(IDS_STATISTIC_1), _r_locale(IDS_STATISTIC_2), sd.statistic_last_reduct ? _r_helper_formatdate(&st) : _r_locale(IDS_STATISTIC_3), _r_helper_formatsize64((DWORDLONG)ROUTINE_PERCENT_VAL(last, sd.ms.total_phys)), _r_cfg_read(L"StatisticTotalReduct", 0), _r_helper_formatsize64((DWORDLONG)ROUTINE_PERCENT_VAL(maximum, sd.ms.total_phys)));

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