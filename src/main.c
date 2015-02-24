// Mem Reduct
// Copyright © 2011-2013, 2015 Henry++

#include <windows.h>

#include "main.h"

#include "resource.h"
#include "routine.h"

NOTIFYICONDATA _r_nid = {0};

HDC _r_dc = NULL, _r_cdc = NULL;
HBITMAP _r_bitmap = NULL, _r_bitmap_mask = NULL;
RECT _r_rc = {0};
HFONT _r_font = NULL;
LPVOID _r_rgb = NULL;
BOOL _r_supported_os = FALSE;

DWORD dwLastBalloon = 0;

CONST UINT WM_TASKBARCREATED = RegisterWindowMessage(L"TaskbarCreated");

BOOL _Reduct_ShowNotification(DWORD icon, LPCWSTR title, LPCWSTR text, BOOL important)
{
	if(!important && dwLastBalloon && (GetTickCount() - dwLastBalloon) < 5000)
	{
		return FALSE;
	}

	_r_nid.uFlags = NIF_INFO;
	_r_nid.dwInfoFlags = NIIF_RESPECT_QUIET_TIME | icon;

	StringCchCopy(_r_nid.szInfoTitle, _countof(_r_nid.szInfoTitle), title);
	StringCchCopy(_r_nid.szInfo, _countof(_r_nid.szInfo), text);

	Shell_NotifyIcon(NIM_MODIFY, &_r_nid);

	_r_nid.szInfo[0] = _r_nid.szInfoTitle[0] = L'\0'; // clean

	dwLastBalloon = GetTickCount();

	return TRUE;
}

DWORD _Application_GetMemoryStatus(_R_MEMORYSTATUS* m)
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

VOID _Application_PrintResult(HWND hwnd, DWORD new_percent, _R_MEMORYSTATUS* m)
{
	SetDlgItemText(hwnd, IDC_RESULT_1, L"~" + _r_helper_formatsize64(m ? (DWORDLONG)ROUTINE_PERCENT_VAL(m->percent_phys - new_percent, m->total_phys) : 0));

	INT max_result = max(_r_cfg_read(L"MaxResult", 0), m ? (m->percent_phys - new_percent) : 0);

	_R_MEMORYSTATUS ms = {0};
	_Application_GetMemoryStatus(&ms);

	SetDlgItemText(hwnd, IDC_RESULT_2, L"~" + _r_helper_formatsize64((DWORDLONG)ROUTINE_PERCENT_VAL(max_result, ms.total_phys)));

	_r_cfg_write(L"MaxResult", max_result);
}

BOOL _Application_Reduct(HWND hwnd)
{
	// if user has no rights
	if(!_r_system_adminstate())
	{
		_Reduct_ShowNotification(NIIF_ERROR, APP_NAME, _r_locale(IDS_BALLOON_WARNING), FALSE);
		return FALSE;
	}

	_R_MEMORYSTATUS m = {0};

	_Application_GetMemoryStatus(&m);

	SYSTEM_MEMORY_LIST_COMMAND smlc;

	// Working set
	if(_r_supported_os && _r_cfg_read(L"ReductWorkingSet", 1))
	{
		smlc = MemoryEmptyWorkingSets;
		NtSetSystemInformation(SystemMemoryListInformation, &smlc, sizeof(smlc));
	}

	// System working set
	if(_r_cfg_read(L"ReductSystemWorkingSet", 1))
	{
		SYSTEM_CACHE_INFORMATION cache = {0};

		cache.MinimumWorkingSet = (ULONG)-1;
		cache.MaximumWorkingSet = (ULONG)-1;

		NtSetSystemInformation(SystemFileCacheInformation, &cache, sizeof(cache));
	}

	// Standby priority-0 list
	if(_r_supported_os && _r_cfg_read(L"ReductStandbyPriority0List", 1))
	{
		smlc = MemoryPurgeLowPriorityStandbyList;
		NtSetSystemInformation(SystemMemoryListInformation, &smlc, sizeof(smlc));
	}

	// Standby list
	if(_r_supported_os && _r_cfg_read(L"ReductStandbyList", 0))
	{
		smlc = MemoryPurgeStandbyList;
		NtSetSystemInformation(SystemMemoryListInformation, &smlc, sizeof(smlc));
	}

	// Modified list
	if(_r_supported_os && _r_cfg_read(L"ReductModifiedList", 0))
	{
		smlc = MemoryFlushModifiedList;
		NtSetSystemInformation(SystemMemoryListInformation, &smlc, sizeof(smlc));
	}

	DWORD new_percent = _Application_GetMemoryStatus(NULL);

	if(hwnd)
	{
		_Application_PrintResult(hwnd, new_percent, &m);
	}

	_Reduct_ShowNotification(NIIF_INFO, APP_NAME, _r_helper_format(_r_locale(IDS_BALLOON_REDUCT), _r_helper_formatsize64((DWORDLONG)ROUTINE_PERCENT_VAL(m.percent_phys - new_percent, m.total_phys))), TRUE);

	return TRUE;
}

HICON _Application_GenerateIcon(DWORD percent)
{
	COLORREF clrText = 0, clrTextBk= 0;

	BOOL is_transparent = _r_cfg_read(L"UseTransparentBg", 1);

	if(!percent)
	{
		percent = _Application_GetMemoryStatus(NULL);
	}

	if(_r_cfg_read(L"ColorIndicationTray", 1))
	{
		if(percent >= _r_cfg_read(L"DangerLevel", 90))
		{
			is_transparent = FALSE;

			clrText = COLOR_TRAY_TEXT;
			clrTextBk = COLOR_LEVEL_DANGER;
		}
		else if(percent >= _r_cfg_read(L"WarningLevel", 60))
		{
			is_transparent = FALSE;

			clrText = COLOR_TRAY_TEXT;
			clrTextBk = COLOR_LEVEL_WARNING;
		}
	}

	if(!clrText) clrText = is_transparent ? 0 : COLOR_TRAY_TEXT;
	if(!clrTextBk) clrTextBk = is_transparent ? COLOR_TRAY_MASK : COLOR_TRAY_BG;

	HBITMAP old_bitmap = (HBITMAP)SelectObject(_r_cdc, _r_bitmap);

	// draw background
	COLORREF clr_prev = SetBkColor(_r_cdc, clrTextBk);
	ExtTextOut(_r_cdc, 0, 0, ETO_OPAQUE, &_r_rc, NULL, 0, NULL);
	SetBkColor(_r_cdc, clr_prev);

	// draw text
	SetTextColor(_r_cdc, clrText);
	SetBkMode(_r_cdc, TRANSPARENT);

	CString buffer = _r_helper_format(L"%d\0", percent);
	DrawTextEx(_r_cdc, buffer.GetBuffer(), buffer.GetLength(), &_r_rc, DT_VCENTER | DT_CENTER | DT_SINGLELINE | DT_NOCLIP, NULL);

	SelectObject(_r_dc, old_bitmap);

	// convert bits to transparency
	if(is_transparent)
	{
		DWORD *pixel = (DWORD*)_r_rgb;

		for(INT i = ((_r_rc.right * _r_rc.right) - 1); i >= 0; i--)
		{
			*pixel &= COLOR_TRAY_MASK;
			*pixel |= ((*pixel == COLOR_TRAY_MASK) ? 0x00000000 : 0xffffffff);

			pixel++;
		}
	}

	ICONINFO ii = {TRUE, 0, 0, _r_bitmap_mask, _r_bitmap};

	return CreateIconIndirect(&ii);
}

VOID CALLBACK _Application_MonitorCallback(HWND hwnd, UINT, UINT_PTR, DWORD)
{
	_R_MEMORYSTATUS m = {0};

	_Application_GetMemoryStatus(&m);

	if(_r_nid.hIcon)
	{
		DestroyIcon(_r_nid.hIcon);
	}

	_r_nid.uFlags = NIF_ICON | NIF_TIP;

	StringCchPrintf(_r_nid.szTip, _countof(_r_nid.szTip), _r_locale(IDS_TRAY_TOOLTIP), m.percent_phys, m.percent_page, m.percent_ws);
	_r_nid.hIcon = _Application_GenerateIcon(m.percent_phys);

	Shell_NotifyIcon(NIM_MODIFY, &_r_nid);
	
	BOOL has_danger = (m.percent_phys >= _r_cfg_read(L"DangerLevel", 90));
	BOOL has_warning = (m.percent_phys >= _r_cfg_read(L"WarningLevel", 60));

	// autoclean
	if((has_danger && _r_cfg_read(L"AutoreductWhenDanger", 1)) || (has_warning && _r_cfg_read(L"AutoreductWhenWarning", 0)))
	{
		_Application_Reduct(NULL);
	}

	// balloon
	if((has_danger && _r_cfg_read(L"BalloonWhenDanger", 1)) || (has_warning &&_r_cfg_read(L"BalloonWhenWarning", 0)))
	{
		_Reduct_ShowNotification(has_danger ? NIIF_ERROR : NIIF_WARNING, APP_NAME, _r_locale(has_danger ? IDS_BALLOON_DANGER_LEVEL : IDS_BALLOON_WARNING_LEVEL), FALSE);
	}

	if(IsWindowVisible(hwnd))
	{
		// physical
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_format(L"%d%%", m.percent_phys), 0, 1, -1, -1, m.percent_phys);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(m.free_phys), 1, 1, -1, -1, m.percent_phys);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(m.total_phys), 2, 1, -1, -1, m.percent_phys);
		
		// pagefile
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_format(L"%d%%", m.percent_page), 3, 1, -1, -1, m.percent_page);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(m.free_page), 4, 1, -1, -1, m.percent_page);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(m.total_page), 5, 1, -1, -1, m.percent_page);

		// system working set
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_format(L"%d%%", m.percent_ws), 6, 1, -1, -1, m.percent_ws);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(m.free_ws), 7, 1, -1, -1, m.percent_ws);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(m.total_ws), 8, 1, -1, -1, m.percent_ws);

		// redraw listview
		SendDlgItemMessage(hwnd, IDC_LISTVIEW, LVM_REDRAWITEMS, 0, 8);
	}
}

VOID _Application_Unitialize(BOOL deletetrayicon)
{
	if(_r_nid.hIcon)
	{
		DestroyIcon(_r_nid.hIcon);
	}

	if(deletetrayicon)
	{
		Shell_NotifyIcon(NIM_DELETE, &_r_nid);
	}

	UnregisterHotKey(_r_hwnd, UID);
	KillTimer(_r_hwnd, UID);

	DeleteObject(_r_font);
	DeleteDC(_r_cdc);
	DeleteDC(_r_dc);
	DeleteObject(_r_bitmap);
	DeleteObject(_r_bitmap_mask);
}

VOID _Application_Initialize(BOOL createtrayicon)
{
	_Application_Unitialize(createtrayicon);

	_r_rc.right = GetSystemMetrics(SM_CXSMICON);
	_r_rc.bottom = GetSystemMetrics(SM_CYSMICON);

	BITMAPINFO bmi = {0};

    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = _r_rc.right;
    bmi.bmiHeader.biHeight = _r_rc.bottom;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = 0;

	_r_dc = GetDC(NULL);

	_r_bitmap = CreateDIBSection(_r_dc, &bmi, DIB_RGB_COLORS, (LPVOID*)&_r_rgb, NULL, 0);
	_r_bitmap_mask = CreateBitmap(_r_rc.right, _r_rc.bottom, 1, 1, NULL);;

	_r_cdc = CreateCompatibleDC(_r_dc);

	ReleaseDC(NULL, _r_dc);

	// Font
	LOGFONT lf = {0};

	lf.lfQuality = ANTIALIASED_QUALITY;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfPitchAndFamily = FF_DONTCARE;
	lf.lfHeight = -MulDiv(8, GetDeviceCaps(_r_cdc, LOGPIXELSY), 72);

	StringCchCopy(lf.lfFaceName, LF_FACESIZE, L"Tahoma");

	_r_font = CreateFontIndirect(&lf);

	SelectObject(_r_cdc, _r_font);

	// Hotkey
	UINT hk = _r_cfg_read(L"Hotkey", 0);

	if(hk)
	{
		RegisterHotKey(_r_hwnd, UID, (HIBYTE(hk) & 2) | ((HIBYTE(hk) & 4) >> 2) | ((HIBYTE(hk) & 1) << 2), LOBYTE(hk));
	}

	// Always on top
	_r_windowtotop(_r_hwnd, _r_cfg_read(L"AlwaysOnTop", 0));

	// Tray icon
	if(createtrayicon)
	{
		_r_nid.cbSize = _r_supported_os ? sizeof(_r_nid) : NOTIFYICONDATA_V3_SIZE;
		_r_nid.uVersion = _r_supported_os ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION;
		_r_nid.hWnd = _r_hwnd;
		_r_nid.uID = UID;
		_r_nid.uFlags = NIF_MESSAGE | NIF_ICON;
		_r_nid.uCallbackMessage = WM_TRAYICON;
		_r_nid.hIcon = _Application_GenerateIcon(NULL);

		Shell_NotifyIcon(NIM_ADD, &_r_nid);
	}

	// Timer
	SetTimer(_r_hwnd, UID, 500, _Application_MonitorCallback);
}

INT_PTR WINAPI PagesDlgProc(HWND hwnd, UINT msg, WPARAM, LPARAM lparam)
{
	switch(msg)
	{
		case WM_INITDIALOG:
		{
			SetProp(hwnd, L"id", (HANDLE)lparam);

			if((INT)lparam == IDD_SETTINGS_1)
			{
				CheckDlgButton(hwnd, IDC_ALWAYSONTOP_CHK, _r_cfg_read(L"AlwaysOnTop", 0) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_STARTMINIMIZED_CHK, _r_cfg_read(L"StartMinimized", 0) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_LOADONSTARTUP_CHK, _r_autorun_is_present(APP_NAME) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_SKIPUACWARNING_CHK, _r_skipuac_is_present() ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_CHECKUPDATES_CHK, _r_cfg_read(L"CheckUpdates", 1) ? BST_CHECKED : BST_UNCHECKED);

				if(!_r_supported_os || !_r_system_adminstate())
				{
					EnableWindow(GetDlgItem(hwnd, IDC_SKIPUACWARNING_CHK), FALSE);
				}

				SendDlgItemMessage(hwnd, IDC_LANGUAGE, CB_INSERTSTRING, 0, (LPARAM)L"System default");
				SendDlgItemMessage(hwnd, IDC_LANGUAGE, CB_SETCURSEL, 0, NULL);

				EnumResourceLanguages(NULL, RT_STRING, MAKEINTRESOURCE(63), _r_locale_enum, (LONG_PTR)GetDlgItem(hwnd, IDC_LANGUAGE));
			}
			else if((INT)lparam == IDD_SETTINGS_2)
			{
				CheckDlgButton(hwnd, IDC_WORKINGSET_CHK, _r_cfg_read(L"ReductWorkingSet", 1) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_SYSTEMWORKINGSET_CHK, _r_cfg_read(L"ReductSystemWorkingSet", 1) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_STANDBYLISTPRIORITY0_CHK, _r_cfg_read(L"ReductStandbyPriority0List", 1) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_STANDBYLIST_CHK, _r_cfg_read(L"ReductStandbyList", 0) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_MODIFIEDLIST_CHK, _r_cfg_read(L"ReductModifiedList", 0) ? BST_CHECKED : BST_UNCHECKED);

				if(!_r_supported_os)
				{
					EnableWindow(GetDlgItem(hwnd, IDC_WORKINGSET_CHK), FALSE);
					EnableWindow(GetDlgItem(hwnd, IDC_STANDBYLISTPRIORITY0_CHK), FALSE);
					EnableWindow(GetDlgItem(hwnd, IDC_STANDBYLIST_CHK), FALSE);
					EnableWindow(GetDlgItem(hwnd, IDC_MODIFIEDLIST_CHK), FALSE);
				}

				SendDlgItemMessage(hwnd, IDC_HOTKEY, HKM_SETHOTKEY, _r_cfg_read(L"Hotkey", 0), NULL);
			}
			else if((INT)lparam == IDD_SETTINGS_3)
			{
				CheckDlgButton(hwnd, IDC_COLORINDICATIONLISTVIEW, _r_cfg_read(L"ColorIndicationListview", 1) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_COLORINDICATIONTRAY, _r_cfg_read(L"ColorIndicationTray", 1) ? BST_CHECKED : BST_UNCHECKED);

				SendDlgItemMessage(hwnd, IDC_WARNING_LEVEL, UDM_SETRANGE32, 1, 99);
				SendDlgItemMessage(hwnd, IDC_WARNING_LEVEL, UDM_SETPOS32, 0, _r_cfg_read(L"WarningLevel", 60));

				SendDlgItemMessage(hwnd, IDC_DANGER_LEVEL, UDM_SETRANGE32, 1, 99);
				SendDlgItemMessage(hwnd, IDC_DANGER_LEVEL, UDM_SETPOS32, 0, _r_cfg_read(L"DangerLevel", 90));
			}
			else if((INT)lparam == IDD_SETTINGS_4)
			{
				CheckDlgButton(hwnd, IDC_AUTOREDUCTWHENWARNING_CHK, _r_cfg_read(L"AutoreductWhenWarning", 0) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_AUTOREDUCTWHENDANGER_CHK, _r_cfg_read(L"AutoreductWhenDanger", 1) ? BST_CHECKED : BST_UNCHECKED);
			}
			else if((INT)lparam == IDD_SETTINGS_5)
			{
				CheckDlgButton(hwnd, IDC_BALLOONWHENWARNING_CHK, _r_cfg_read(L"BalloonWhenWarning", 0) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_BALLOONWHENDANGER_CHK, _r_cfg_read(L"BalloonWhenDanger", 1) ? BST_CHECKED : BST_UNCHECKED);

				CheckDlgButton(hwnd, IDC_USETRANSPARENTBG_CHK, _r_cfg_read(L"UseTransparentBg", 1) ? BST_CHECKED : BST_UNCHECKED);
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

					_r_cfg_write(L"Hotkey", (DWORD)SendDlgItemMessage(hwnd, IDC_HOTKEY, HKM_GETHOTKEY, 0, NULL));
				}
				else if(INT(GetProp(hwnd, L"id")) == IDD_SETTINGS_3)
				{
					_r_cfg_write(L"ColorIndicationListview", INT((IsDlgButtonChecked(hwnd, IDC_COLORINDICATIONLISTVIEW) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"ColorIndicationTray", INT((IsDlgButtonChecked(hwnd, IDC_COLORINDICATIONTRAY) == BST_CHECKED) ? TRUE : FALSE));

					_r_cfg_write(L"WarningLevel", (DWORD)SendDlgItemMessage(hwnd, IDC_WARNING_LEVEL, UDM_GETPOS32, 0, NULL));
					_r_cfg_write(L"DangerLevel", (DWORD)SendDlgItemMessage(hwnd, IDC_DANGER_LEVEL, UDM_GETPOS32, 0, NULL));
				}
				else if(INT(GetProp(hwnd, L"id")) == IDD_SETTINGS_4)
				{
					_r_cfg_write(L"AutoreductWhenWarning", INT((IsDlgButtonChecked(hwnd, IDC_AUTOREDUCTWHENWARNING_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"AutoreductWhenDanger", INT((IsDlgButtonChecked(hwnd, IDC_AUTOREDUCTWHENDANGER_CHK) == BST_CHECKED) ? TRUE : FALSE));
				}
				else if(INT(GetProp(hwnd, L"id")) == IDD_SETTINGS_5)
				{
					_r_cfg_write(L"BalloonWhenWarning", INT((IsDlgButtonChecked(hwnd, IDC_BALLOONWHENWARNING_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"BalloonWhenDanger", INT((IsDlgButtonChecked(hwnd, IDC_BALLOONWHENDANGER_CHK) == BST_CHECKED) ? TRUE : FALSE));

					_r_cfg_write(L"UseTransparentBg", INT((IsDlgButtonChecked(hwnd, IDC_USETRANSPARENTBG_CHK) == BST_CHECKED) ? TRUE : FALSE));
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

			SendDlgItemMessage(hwnd, IDC_NAV, TVM_SELECTITEM, TVGN_CARET, SendDlgItemMessage(hwnd, IDC_NAV, TVM_GETNEXTITEM, TVGN_FIRSTVISIBLE, NULL));

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR lphdr = (LPNMHDR)lparam;

			switch(lphdr->code)
			{
				case TVN_SELCHANGED:
				{
					if(wparam == IDC_NAV)
					{
						LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lparam;

						ShowWindow((HWND)GetProp(hwnd, L"hwnd"), SW_HIDE);

						SetProp(hwnd, L"hwnd", (HANDLE)pnmtv->itemNew.lParam);

						ShowWindow((HWND)pnmtv->itemNew.lParam, SW_SHOW);
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

INT_PTR CALLBACK ReductDlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM)
{
	switch(msg)
	{
		case WM_INITDIALOG:
		{
			_r_windowcenter(hwnd);

			LOGFONT lf ={0};

			lf.lfQuality = ANTIALIASED_QUALITY;
			//lf.lfWeight = FW_LIGHT;
			lf.lfHeight = -MulDiv(24, GetDeviceCaps(_r_dc, LOGPIXELSY), 72);

			StringCchCopy(lf.lfFaceName, _countof(lf.lfFaceName), L"Tahoma");

			HFONT font = CreateFontIndirect(&lf);

			SendDlgItemMessage(hwnd, IDC_RESULT_1, WM_SETFONT, (WPARAM)font, NULL);
			SendDlgItemMessage(hwnd, IDC_RESULT_2, WM_SETFONT, (WPARAM)font, NULL);

			_Application_PrintResult(hwnd, 0, NULL);

			//SetProp(_r_hwnd, L"progress", GetDlgItem(hwnd, IDC_RESULT));

			break;
		}

		case WM_DESTROY:
		{
			//SetProp(_r_hwnd, L"progress", NULL);

			break;
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
		
		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORDLG:
		{
			return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);
		}

		case WM_COMMAND:
		{
			switch(LOWORD(wparam))
			{
				case IDC_OK:
				{
					_Application_Reduct(hwnd);
					break;
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
			_r_windowcenter(hwnd);

			// static initializer
			_r_hwnd = hwnd;
			_r_supported_os = _r_system_validversion(6, 0);

			// dynamic initializer
			_Application_Initialize(TRUE);

			// set priority
			SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

			// set privileges
			if(_r_system_adminstate())
			{
				HANDLE token = NULL;

				if(OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
				{
					_r_system_setprivilege(token, SE_INCREASE_QUOTA_NAME, TRUE);
					_r_system_setprivilege(token, SE_PROF_SINGLE_PROCESS_NAME, TRUE);
				}

				if(token)
				{
					CloseHandle(token);
				}
			}

			// enable taskbarcreated message bypass uipi
			if(_r_supported_os)
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

			// listview
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

			if(_r_cfg_read(L"ColorIndicationListview", 1))
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
								if(lpnmlv->nmcd.hdr.idFrom == IDC_LISTVIEW)
								{
									if((UINT)lpnmlv->nmcd.lItemlParam >= _r_cfg_read(L"DangerLevel", 90))
									{
										lpnmlv->clrText = COLOR_LEVEL_DANGER;
									}
									else if((UINT)lpnmlv->nmcd.lItemlParam >= _r_cfg_read(L"WarningLevel", 60))
									{
										lpnmlv->clrText = COLOR_LEVEL_WARNING;
									}

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
				case NIN_BALLOONSHOW:
				case NIN_BALLOONHIDE:
				case NIN_BALLOONTIMEOUT:
				case NIN_BALLOONUSERCLICK:
				{
					//_r_msg(0, L"0x%08x", lparam);
					break;
				}

				case WM_LBUTTONDBLCLK:
				{
					SendMessage(hwnd, WM_COMMAND, MAKELPARAM(IDM_TRAY_SHOW, 0), NULL);
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
					DialogBox(NULL, MAKEINTRESOURCE(IDD_SETTINGS), hwnd, SettingsDlgProc);

					if(GetProp(hwnd, L"is_restart"))
					{
						_r_uninitialize(TRUE);
					}

					_Application_Initialize(FALSE);

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
					DialogBox(NULL, MAKEINTRESOURCE(IDD_REDUCT), hwnd, ReductDlgProc);
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
					_r_aboutbox(hwnd);
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