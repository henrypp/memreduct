// Mem Reduct
// Copyright © 2011-2013, 2015 Henry++

#include <windows.h>

#include "main.h"

#include "resource.h"
#include "routine.h"

NOTIFYICONDATA _r_nid = {0};

HDC _r_dc = NULL;
HBITMAP _r_bitmap_opaque = NULL, _r_bitmap_transparent = NULL, _r_bitmap_mask = NULL;
RECT _r_rc = {0};
HFONT _r_font = NULL, _r_font2 = NULL;
RGBQUAD *_r_rgb = NULL;
BOOL _r_supported_os = FALSE;

DWORD dwLastBalloon = 0;

BOOL _Reduct_ShowNotification(DWORD icon, LPCWSTR title, LPCWSTR text, BOOL important)
{
	if(!important && dwLastBalloon && (GetTickCount() - dwLastBalloon) < 7000)
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

	SYSTEM_CACHE_INFORMATION sci = {0};

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
	SetDlgItemText(hwnd, IDC_VALUE_1, _r_helper_format(L"%d%%", m ? m->percent_phys : 0));
	SetDlgItemText(hwnd, IDC_VALUE_2, _r_helper_format(L"%d%%", m ? m->percent_phys - new_percent : 0));
	SetDlgItemText(hwnd, IDC_VALUE_3, _r_helper_format(L"%d%%", m ? new_percent : 0));

	INT max_result = max(_r_cfg_read(L"MaxResult", 0), (m ? m->percent_phys : _Application_GetMemoryStatus(NULL)) - new_percent);

	SetDlgItemText(hwnd, IDC_SAVED_1, _r_helper_format(_r_locale(IDS_SAVED_1), _r_helper_formatsize64(m ? (DWORDLONG)ROUTINE_PERCENT_VAL(m->percent_phys - new_percent, m->total_phys) : 0)));
	SetDlgItemText(hwnd, IDC_SAVED_2, _r_helper_format(_r_locale(IDS_SAVED_2), max_result, _r_helper_formatsize64(m ? (DWORDLONG)ROUTINE_PERCENT_VAL(max_result, m->total_phys) : 0)));

	_r_cfg_write(L"MaxResult", max_result);
}

BOOL _Application_Reduct(HWND hwnd)
{
	// if user has no rights
	if(!_r_system_adminstate())
	{
		_Reduct_ShowNotification(NIIF_ERROR, APP_NAME, _r_locale(IDS_BALLOON_WARNING), TRUE);
		return FALSE;
	}

	_R_MEMORYSTATUS m = {0};

	_Application_GetMemoryStatus(&m);

	SYSTEM_MEMORY_LIST_COMMAND smlc;

	// System working set
	if(_r_cfg_read(L"ReductSystemWorkingSet", 1))
	{
		SYSTEM_CACHE_INFORMATION cache = {0};

		cache.MinimumWorkingSet = (ULONG)-1;
		cache.MaximumWorkingSet = (ULONG)-1;

		NtSetSystemInformation(SystemFileCacheInformation, &cache, sizeof(cache));
	}

	// Working set
	if(_r_supported_os && _r_cfg_read(L"ReductWorkingSet", 1))
	{
		smlc = MemoryEmptyWorkingSets;
		NtSetSystemInformation(SystemMemoryListInformation, &smlc, sizeof(smlc));
	}
						
	// Modified pagelists
	if(_r_supported_os && _r_cfg_read(L"ReductModifiedList", 0))
	{
		smlc = MemoryFlushModifiedList;
		NtSetSystemInformation(SystemMemoryListInformation, &smlc, sizeof(smlc));
	}
	
	// Standby pagelists
	if(_r_supported_os && _r_cfg_read(L"ReductStandbyList", 0))
	{
		smlc = MemoryPurgeStandbyList;
		NtSetSystemInformation(SystemMemoryListInformation, &smlc, sizeof(smlc));
	}

	// Standby priority-0 pagelists
	if(_r_supported_os && _r_cfg_read(L"ReductStandbyPriority0List", 1))
	{
		smlc = MemoryPurgeLowPriorityStandbyList;
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
	bool is_trans = _r_cfg_read(L"UseTransparentBg", 1);
	bool is_text_chg = _r_cfg_read(L"IDC_CHANGETEXTCOLOR_CHK", 0);

	COLORREF bg_color = is_trans ? COLOR_TRAY_MASK : COLOR_TRAY_BG;
	COLORREF text_color = is_trans ? 0 : COLOR_TRAY_TEXT;

	if(!percent)
	{
		percent = _Application_GetMemoryStatus(NULL);
	}

	if(percent >= _r_cfg_read(L"DangerLevel", 90))
	{
		bg_color = COLOR_LEVEL_DANGER;
	}
	else if(percent >= _r_cfg_read(L"WarningLevel", 60))
	{
		bg_color = COLOR_LEVEL_WARNING;
	}

	HDC dc = CreateCompatibleDC(_r_dc);

	HBITMAP old_bitmap = (HBITMAP)SelectObject(dc, is_trans ? _r_bitmap_transparent : _r_bitmap_opaque);

	if(!is_trans)
	{
		COLORREF clrOld = SetBkColor(dc, bg_color);
		ExtTextOut(dc, 0, 0, ETO_OPAQUE, &_r_rc, NULL, 0, NULL);
		SetBkColor(dc, clrOld);
	}

//	BitBlt(_r_dc, 0, 0, _r_rc.right, _r_rc.bottom, _r_cdc, 0, 0, SRCCOPY);

	SelectObject(dc, _r_font);

	// Draw
	SetTextColor(dc, text_color);
	SetBkMode(dc, TRANSPARENT);

	CString buffer = _r_helper_format(L"%d\0", percent);

	DrawTextEx(dc, buffer.GetBuffer(), buffer.GetLength(), &_r_rc, DT_VCENTER | DT_CENTER | DT_SINGLELINE | DT_NOCLIP, NULL);

	//TransparentBlt(_r_dc, 0, 0, _r_rc.right, _r_rc.bottom, dc, 0, 0, _r_rc.right, _r_rc.bottom, COLOR_TRAY_MASK);

	SelectObject(_r_dc, old_bitmap);
	
	DeleteObject(dc);

//	KillTimer(_r_hwnd, UID);

	ICONINFO ii = {TRUE, 0, 0, _r_bitmap_mask, is_trans ? _r_bitmap_transparent : _r_bitmap_opaque};

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

	// Refresh tray info
	_r_nid.uFlags = NIF_ICON | NIF_TIP;

	StringCchPrintf(_r_nid.szTip, _countof(_r_nid.szTip), _r_locale(IDS_TRAY_TOOLTIP), m.percent_phys, m.percent_page, m.percent_ws);
	_r_nid.hIcon = _Application_GenerateIcon(m.percent_page);

	Shell_NotifyIcon(NIM_MODIFY, &_r_nid);
	
	bool has_danger = (m.percent_phys >= _r_cfg_read(L"DangerLevel", 90));
	bool has_warning = (m.percent_phys >= _r_cfg_read(L"WarningLevel", 60));

	// Autoreduct
	if((has_danger && _r_cfg_read(L"AutoreductWhenDanger", 1)) || (has_warning && _r_cfg_read(L"AutoreductWhenWarning", 0)))
	{
		_Application_Reduct(NULL);
	}

	// Balloon
	if((has_danger && _r_cfg_read(L"BalloonWhenDanger", 1)) || (has_warning &&_r_cfg_read(L"BalloonWhenWarning", 0)))
	{
		_Reduct_ShowNotification(has_danger ? NIIF_ERROR : NIIF_WARNING, APP_NAME, _r_locale(has_danger ? IDS_BALLOON_DANGER_LEVEL : IDS_BALLOON_WARNING_LEVEL), FALSE);
	}

	if(IsWindowVisible(hwnd))
	{
		// Physical
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_format(L"%d%%", m.percent_phys), 0, 1, -1, -1, m.percent_phys);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(m.free_phys), 1, 1, -1, -1, m.percent_phys);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(m.total_phys), 2, 1, -1, -1, m.percent_phys);
		
		// Pagefile
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_format(L"%d%%", m.percent_page), 3, 1, -1, -1, m.percent_page);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(m.free_page), 4, 1, -1, -1, m.percent_page);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(m.total_page), 5, 1, -1, -1, m.percent_page);

		// System working set
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_format(L"%d%%", m.percent_ws), 6, 1, -1, -1, m.percent_ws);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(m.free_ws), 7, 1, -1, -1, m.percent_ws);
		_r_listview_additem(hwnd, IDC_LISTVIEW, _r_helper_formatsize64(m.total_ws), 8, 1, -1, -1, m.percent_ws);
	}

	HWND progress = (HWND)GetProp(_r_hwnd, L"progress");

	if(progress)
	{
		if(_r_supported_os)
		{
			WPARAM state = PBST_NORMAL;

			if(m.percent_phys >= _r_cfg_read(L"DangerLevel", 90))
			{
				state = PBST_ERROR;
			}
			else if(m.percent_phys >= _r_cfg_read(L"WarningLevel", 60))
			{
				state = PBST_PAUSED;
			}

			SendMessage(progress, PBM_SETSTATE, state, NULL);
		}

		SendMessage(progress, PBM_SETPOS, m.percent_phys, NULL);
	}
}

VOID _Application_Unitialize()
{
	if(_r_nid.hIcon)
	{
		DestroyIcon(_r_nid.hIcon);
	}

	UnregisterHotKey(_r_hwnd, UID);
	KillTimer(_r_hwnd, UID);

	DeleteObject(_r_font);
	DeleteDC(_r_dc);
	DeleteObject(_r_bitmap_opaque);
	DeleteObject(_r_bitmap_transparent);
	DeleteObject(_r_bitmap_mask);

	if(_r_font2)
	{
		RemoveFontMemResourceEx(_r_font2);
	}
}

VOID _Application_Initialize()
{
	_Application_Unitialize();

	_r_rc.right = GetSystemMetrics(SM_CXSMICON);
	_r_rc.bottom = GetSystemMetrics(SM_CYSMICON);
/*
	BITMAPV5HEADER bi = {0};

	bi.bV5Size           = sizeof(bi);
    bi.bV5Width           = _r_rc.right;
    bi.bV5Height          = _r_rc.bottom;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    // The following mask specification specifies a supported 32 BPP
    // alpha format for Windows XP.
    bi.bV5RedMask   =  0x00FF0000;
    bi.bV5GreenMask =  0x0000FF00;
    bi.bV5BlueMask  =  0x000000FF;
    bi.bV5AlphaMask =  0xFF000000; 
*/
	BITMAPINFO bmi = {0};

    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = _r_rc.right;
    bmi.bmiHeader.biHeight = _r_rc.bottom;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = 0;

	_r_dc = GetDC(NULL);

	_r_bitmap_opaque = CreateCompatibleBitmap(_r_dc, _r_rc.right, _r_rc.bottom);
	_r_bitmap_transparent = CreateDIBSection(_r_dc, &bmi, DIB_RGB_COLORS, (void**)&_r_rgb, NULL, 0);

	_r_bitmap_mask = CreateBitmap(_r_rc.right, _r_rc.bottom, 1, 1, NULL);;

	ReleaseDC(NULL, _r_dc);

	if(1)
	{
		HDC buffdc = CreateCompatibleDC(NULL);

		HBITMAP old_bitmap = (HBITMAP)SelectObject(buffdc, _r_bitmap_transparent);


		COLORREF clrOld = SetBkColor(buffdc, COLOR_TRAY_MASK);
		ExtTextOut(buffdc, 0, 0, ETO_OPAQUE, &_r_rc, NULL, 0, NULL);
		SetBkColor(buffdc, clrOld);

		SelectObject(buffdc, old_bitmap);

		DeleteDC(buffdc);
/*
		DWORD *lpdwPixel = (DWORD*)_r_rgb;

		for(INT i = ((_r_rc.right * _r_rc.right) - 1); i >= 0; i--)
		{
           // Clear the alpha bits
           *lpdwPixel &= 0x00FFFFFF;

           // Set the alpha bits to 0x9F (semi-transparent)
           *lpdwPixel |= 0x01000000;

			lpdwPixel++;
		}
*/

	}

	LOGFONT lf = {0};

	lf.lfQuality = ANTIALIASED_QUALITY;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfPitchAndFamily = FF_DONTCARE;
	lf.lfHeight = -MulDiv(8, GetDeviceCaps(_r_dc, LOGPIXELSY), 72);
	//lf.lfWeight = FW_LIGHT;

	StringCchCopy(lf.lfFaceName, LF_FACESIZE, L"Tahoma");
//	StringCchCopy(lf.lfFaceName, LF_FACESIZE, L"RTFont");

	_r_font = CreateFontIndirect(&lf);

	UINT hk = _r_cfg_read(L"Hotkey", 0);

	if(hk)
	{
		RegisterHotKey(_r_hwnd, UID, (HIBYTE(hk) & 2) | ((HIBYTE(hk) & 4) >> 2) | ((HIBYTE(hk) & 1) << 2), LOBYTE(hk));
	}

	_r_windowtotop(_r_hwnd, _r_cfg_read(L"AlwaysOnTop", 0));

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
				CheckDlgButton(hwnd, IDC_SYSTEMWORKINGSET_CHK, _r_cfg_read(L"ReductSystemWorkingSet", 1) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_WORKINGSET_CHK, _r_cfg_read(L"ReductWorkingSet", 1) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_MODIFIEDLIST_CHK, _r_cfg_read(L"ReductModifiedList", 0) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_STANDBYLIST_CHK, _r_cfg_read(L"ReductStandbyList", 0) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_STANDBYLISTPRIORITY0_CHK, _r_cfg_read(L"ReductStandbyPriority0List", 1) ? BST_CHECKED : BST_UNCHECKED);

				if(!_r_supported_os)
				{
					EnableWindow(GetDlgItem(hwnd, IDC_WORKINGSET_CHK), FALSE);
					EnableWindow(GetDlgItem(hwnd, IDC_MODIFIEDLIST_CHK), FALSE);
					EnableWindow(GetDlgItem(hwnd, IDC_STANDBYLIST_CHK), FALSE);
					EnableWindow(GetDlgItem(hwnd, IDC_STANDBYLISTPRIORITY0_CHK), FALSE);
				}

				SendDlgItemMessage(hwnd, IDC_WARNING_LEVEL, UDM_SETRANGE32, 1, 99);
				SendDlgItemMessage(hwnd, IDC_WARNING_LEVEL, UDM_SETPOS32, 0, _r_cfg_read(L"WarningLevel", 60));

				SendDlgItemMessage(hwnd, IDC_DANGER_LEVEL, UDM_SETRANGE32, 1, 99);
				SendDlgItemMessage(hwnd, IDC_DANGER_LEVEL, UDM_SETPOS32, 0, _r_cfg_read(L"DangerLevel", 90));

				SendDlgItemMessage(hwnd, IDC_HOTKEY, HKM_SETHOTKEY, _r_cfg_read(L"Hotkey", 0), NULL);
			}
			else if((INT)lparam == IDD_SETTINGS_3)
			{
				CheckDlgButton(hwnd, IDC_AUTOREDUCTWHENWARNING_CHK, _r_cfg_read(L"AutoreductWhenWarning", 0) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_AUTOREDUCTWHENDANGER_CHK, _r_cfg_read(L"AutoreductWhenDanger", 1) ? BST_CHECKED : BST_UNCHECKED);
			}
			else if((INT)lparam == IDD_SETTINGS_4)
			{
				CheckDlgButton(hwnd, IDC_BALLOONWHENWARNING_CHK, _r_cfg_read(L"BalloonWhenWarning", 0) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_BALLOONWHENDANGER_CHK, _r_cfg_read(L"BalloonWhenDanger", 1) ? BST_CHECKED : BST_UNCHECKED);

				CheckDlgButton(hwnd, IDC_USETRANSPARENTBG_CHK, _r_cfg_read(L"UseTransparentBg", 1) ? BST_CHECKED : BST_UNCHECKED);
				CheckDlgButton(hwnd, IDC_CHANGETEXTCOLOR_CHK, _r_cfg_read(L"IDC_CHANGETEXTCOLOR_CHK", 0) ? BST_CHECKED : BST_UNCHECKED);
			}

			break;
		}

		case WM_DESTROY:
		{
			if(GetProp(GetParent(hwnd), L"is_save"))
			{
				if(INT(GetProp(hwnd, L"id")) == IDD_SETTINGS_1)
				{
					INT as_admin = _r_cfg_read(L"RunAsAdmin", 1);

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
					_r_cfg_write(L"ReductSystemWorkingSet", INT((IsDlgButtonChecked(hwnd, IDC_SYSTEMWORKINGSET_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"ReductWorkingSet", INT((IsDlgButtonChecked(hwnd, IDC_WORKINGSET_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"ReductModifiedList", INT((IsDlgButtonChecked(hwnd, IDC_MODIFIEDLIST_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"ReductStandbyList", INT((IsDlgButtonChecked(hwnd, IDC_STANDBYLIST_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"ReductStandbyPriority0List", INT((IsDlgButtonChecked(hwnd, IDC_STANDBYLISTPRIORITY0_CHK) == BST_CHECKED) ? TRUE : FALSE));

					_r_cfg_write(L"WarningLevel", (DWORD)SendDlgItemMessage(hwnd, IDC_WARNING_LEVEL, UDM_GETPOS32, 0, NULL));
					_r_cfg_write(L"DangerLevel", (DWORD)SendDlgItemMessage(hwnd, IDC_DANGER_LEVEL, UDM_GETPOS32, 0, NULL));

					_r_cfg_write(L"Hotkey", (DWORD)SendDlgItemMessage(hwnd, IDC_HOTKEY, HKM_GETHOTKEY, 0, NULL));
				}
				else if(INT(GetProp(hwnd, L"id")) == IDD_SETTINGS_3)
				{
					_r_cfg_write(L"AutoreductWhenWarning", INT((IsDlgButtonChecked(hwnd, IDC_AUTOREDUCTWHENWARNING_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"AutoreductWhenDanger", INT((IsDlgButtonChecked(hwnd, IDC_AUTOREDUCTWHENDANGER_CHK) == BST_CHECKED) ? TRUE : FALSE));
				}
				else if(INT(GetProp(hwnd, L"id")) == IDD_SETTINGS_4)
				{
					_r_cfg_write(L"BalloonWhenWarning", INT((IsDlgButtonChecked(hwnd, IDC_BALLOONWHENWARNING_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"BalloonWhenDanger", INT((IsDlgButtonChecked(hwnd, IDC_BALLOONWHENDANGER_CHK) == BST_CHECKED) ? TRUE : FALSE));

					_r_cfg_write(L"UseTransparentBg", INT((IsDlgButtonChecked(hwnd, IDC_USETRANSPARENTBG_CHK) == BST_CHECKED) ? TRUE : FALSE));
					_r_cfg_write(L"IDC_CHANGETEXTCOLOR_CHK", INT((IsDlgButtonChecked(hwnd, IDC_CHANGETEXTCOLOR_CHK) == BST_CHECKED) ? TRUE : FALSE));
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
			LOGFONT lf ={0};

			lf.lfQuality = ANTIALIASED_QUALITY;
			lf.lfWeight = FW_SEMIBOLD;
			lf.lfHeight = -MulDiv(12, GetDeviceCaps(_r_dc, LOGPIXELSY), 72);

			StringCchCopy(lf.lfFaceName, _countof(lf.lfFaceName), L"Tahoma");

			HFONT font = CreateFontIndirect(&lf);

			SendDlgItemMessage(hwnd, IDC_RESULT + 1, WM_SETFONT, (WPARAM)font, NULL);
			SendDlgItemMessage(hwnd, IDC_RESULT + 2, WM_SETFONT, (WPARAM)font, NULL);
			SendDlgItemMessage(hwnd, IDC_RESULT + 3, WM_SETFONT, (WPARAM)font, NULL);

			_Application_PrintResult(hwnd, 0, NULL);

			SetFocus(GetDlgItem(hwnd, IDC_OK));

			SetProp(_r_hwnd, L"progress", GetDlgItem(hwnd, IDC_RESULT));

			break;
		}

		case WM_DESTROY:
		{
			SetProp(_r_hwnd, L"progress", NULL);

			break;
		}
		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			HDC dc = BeginPaint(hwnd, &ps);
			RECT rc = {0};

			GetClientRect(hwnd, &rc);
			rc.top = rc.bottom - GetSystemMetrics(SM_CYSIZE) * 2;

			COLORREF clrOld = SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
			ExtTextOut(dc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
			SetBkColor(dc, clrOld);

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
	switch(msg)
	{
		case WM_INITDIALOG:
		{
			// static initializer
			_r_hwnd = hwnd;
			_r_supported_os = _r_system_validversion(6, 0);

			// dynamic initializer
			_Application_Initialize();

			if(_r_system_uacstate())
			{
				if(_r_skipuac_run())
				{
					DestroyWindow(hwnd);
					return FALSE;
				}
				else
				{
					WCHAR buffer[MAX_PATH] = {0};

					SHELLEXECUTEINFO shex = {0};

					shex.cbSize = sizeof(shex);
					shex.fMask = SEE_MASK_UNICODE | SEE_MASK_FLAG_NO_UI;
					shex.lpVerb = L"runas";
					shex.nShow = SW_NORMAL;
		
					GetModuleFileName(NULL, buffer, MAX_PATH);
					shex.lpFile = buffer;

					CloseHandle(_r_hmutex);

					if(ShellExecuteEx(&shex))
					{
						DestroyWindow(hwnd);
						return FALSE;
					}

					_r_hmutex = CreateMutex(NULL, FALSE, APP_NAME_SHORT);
				}
			}

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

			// Tray icon
			_r_nid.cbSize = _r_supported_os ? sizeof(_r_nid) : NOTIFYICONDATA_V3_SIZE;
			_r_nid.uVersion = _r_supported_os ? NOTIFYICON_VERSION_4 : NOTIFYICON_VERSION;
			_r_nid.hWnd = hwnd;
			_r_nid.uID = UID;
			_r_nid.uFlags = NIF_MESSAGE | NIF_ICON;
			_r_nid.uCallbackMessage = WM_TRAYICON;
			_r_nid.hIcon = _Application_GenerateIcon(NULL);

			Shell_NotifyIcon(NIM_ADD, &_r_nid);

			if(!_r_cfg_read(L"StartMinimized", 0))
			{
				ShowWindow(hwnd, SW_SHOW);
			}

			break;
		}

		case WM_DESTROY:
		{
			_Application_Unitialize();

			if(_r_nid.uID)
			{
				Shell_NotifyIcon(NIM_DELETE, &_r_nid);
			}

			PostQuitMessage(0);

			break;
		}

		case WM_QUERYENDSESSION:
		{
			if(lparam == ENDSESSION_CLOSEAPP)
			{
				return TRUE;
			}

			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			HDC dc = BeginPaint(hwnd, &ps);
			RECT rc = {0};

			GetClientRect(hwnd, &rc);
			rc.top = rc.bottom - GetSystemMetrics(SM_CYSIZE) * 2;

			COLORREF clrOld = SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
			ExtTextOut(dc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
			SetBkColor(dc, clrOld);

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
									//lpnmlv->clrTextBk = COLOR_LEVEL_DANGER;
								}
								else if((UINT)lpnmlv->nmcd.lItemlParam >= _r_cfg_read(L"WarningLevel", 60))
								{
									lpnmlv->clrText = COLOR_LEVEL_WARNING;
									//lpnmlv->clrTextBk = COLOR_LEVEL_DANGER;
								}

								result = (CDRF_NOTIFYPOSTPAINT | CDRF_NEWFONT);
								
							}

							break;
						}
					}

					SetWindowLongPtr(hwnd, 0 /*DWL_MSGRESULT*/, result);
					return TRUE;
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

					_Application_Initialize();

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

				case IDC_REDUCT:
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