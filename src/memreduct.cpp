/************************************
*  	Mem Reduct
*	Copyright © 2013 Henry++
*
*	GNU General Public License v2
*	http://www.gnu.org/licenses/
*
*	http://www.henrypp.org/
*************************************/

// Include
#include <windows.h>
#include <commctrl.h>
#include <wininet.h>
#include <shlobj.h>
#include <atlstr.h> // cstring
#include <process.h> // _beginthreadex
#include <vssym32.h>
#include <vsstyle.h>
#include <time.h>
#include "memreduct.h"
#include "routine.h"
#include "resource.h"
#include "ini.h"

INI ini;
CONFIG cfg = {0};
CONST INT WM_MUTEX = RegisterWindowMessage(APP_NAME_SHORT);
TAB_PAGES tab_pages = {{IDS_PAGE_1, IDS_PAGE_2, IDS_PAGE_3, IDS_PAGE_4, IDS_PAGE_5}, {IDD_PAGE_1, IDD_PAGE_2, IDD_PAGE_3, IDD_PAGE_4, IDD_PAGE_5}, {0}, 0, {0}};
NOTIFYICONDATA nid = {0};

// Check for updates
UINT WINAPI CheckUpdates(LPVOID lpParam)
{
	BOOL bStatus = FALSE;
	HINTERNET hInternet = NULL, hConnect = NULL;

	// Disable Menu
	EnableMenuItem(GetMenu(cfg.hWnd), IDM_CHECK_UPDATES, MF_BYCOMMAND | MF_DISABLED);

	// Connect
	if((hInternet = InternetOpen(APP_USERAGENT, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0)) && (hConnect = InternetOpenUrl(hInternet, APP_WEBSITE L"/update.php?product=" APP_NAME_SHORT, NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0)))
	{
		// Get Status
		DWORD dwStatus = 0, dwStatusSize = sizeof(dwStatus);
		HttpQueryInfo(hConnect, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_STATUS_CODE, &dwStatus, &dwStatusSize, NULL);

		// Check Errors
		if(dwStatus == HTTP_STATUS_OK)
		{
			// Reading
			ULONG ulReaded = 0;
			CHAR szBufferA[MAX_PATH] = {0};

			if(InternetReadFile(hConnect, szBufferA, MAX_PATH, &ulReaded) && ulReaded)
			{
				// Convert to Unicode
				CA2W newver(szBufferA, CP_UTF8);

				// If CURVER < NEWVER
				if(VersionCompare(APP_VERSION, newver.m_psz) == 1)
				{
					if(MessageBox(cfg.hWnd, MB_YESNO | MB_ICONQUESTION, APP_NAME, ls(cfg.hLocale, IDS_UPDATE_YES), newver) == IDYES)
						ShellExecute(cfg.hWnd, 0, APP_WEBSITE L"/?product=" APP_NAME_SHORT, NULL, NULL, SW_SHOWDEFAULT);
				}
				else
				{
					if(!lpParam)
						MessageBox(cfg.hWnd, MB_OK | MB_ICONINFORMATION, APP_NAME, ls(cfg.hLocale, IDS_UPDATE_NO));
				}

				// Good Result
				bStatus = TRUE;
			}
		}
	}

	if(!bStatus && !lpParam)
		MessageBox(cfg.hWnd, MB_OK | MB_ICONSTOP, APP_NAME, ls(cfg.hLocale, IDS_UPDATE_ERROR));

	// Restore Menu
	EnableMenuItem(GetMenu(cfg.hWnd), IDM_CHECK_UPDATES, MF_BYCOMMAND | MF_ENABLED);

	// Clear Memory
	InternetCloseHandle(hConnect);
	InternetCloseHandle(hInternet);

	return bStatus;
}

// Get memory usage
DWORD GetMemoryUsage(LPMEMORY_USAGE lpmu)
{
	INT iBuffer = 0;

	MEMORYSTATUSEX msex = {0};
	msex.dwLength = sizeof(msex);

	SYSTEM_CACHE_INFORMATION sci = {0};

	// Physical & Pagefile
	if(GlobalMemoryStatusEx(&msex))
	{
		lpmu->dwPercentPhys = msex.dwMemoryLoad;

		lpmu->ullFreePhys = msex.ullAvailPhys / cfg.uUnitDivider;
		lpmu->ullTotalPhys = msex.ullTotalPhys / cfg.uUnitDivider;

		lpmu->dwPercentPageFile = DWORD((msex.ullTotalPageFile - msex.ullAvailPageFile) / (msex.ullTotalPageFile / 100));

		lpmu->ullFreePageFile = msex.ullAvailPageFile / cfg.uUnitDivider;
		lpmu->ullTotalPageFile = msex.ullTotalPageFile / cfg.uUnitDivider;
	}

	// System working set
	if(NT_SUCCESS(NtQuerySystemInformation(SystemFileCacheInformation, &sci, sizeof(sci), 0)))
	{
		lpmu->dwPercentSystemWorkingSet = sci.CurrentSize / (sci.PeakSize / 100);
	
		lpmu->ullFreeSystemWorkingSet = (sci.PeakSize - sci.CurrentSize) / cfg.uUnitDivider;
		lpmu->ullTotalSystemWorkingSet = sci.PeakSize / cfg.uUnitDivider;
	}

	// Return percents
	iBuffer =  ini.read(APP_NAME_SHORT, L"TrayMemoryRegion", 0);

	if(iBuffer == 1)
		return lpmu->dwPercentPageFile;

	else if(iBuffer == 2)
		return lpmu->dwPercentSystemWorkingSet;

	return lpmu->dwPercentPhys;
}

// Show tray balloon tip
BOOL ShowBalloonTip(DWORD dwInfoFlags, LPCWSTR lpcszTitle, LPCWSTR lpcszMessage, BOOL bLimit = TRUE)
{
	// Check interval
	if(bLimit && cfg.dwLastBalloon && (GetTickCount() - cfg.dwLastBalloon) < ((UINT)ini.read(APP_NAME_SHORT, L"BalloonInterval", 10)) * 1000)
		return FALSE;

	// Configure structure
	nid.uFlags = NIF_INFO;
	nid.dwInfoFlags = NIIF_RESPECT_QUIET_TIME | dwInfoFlags;

	// Set text
	StringCchCopy(nid.szInfo, _countof(nid.szInfo), lpcszMessage);
	StringCchCopy(nid.szInfoTitle, _countof(nid.szInfoTitle), lpcszTitle);

	// Show balloon
	Shell_NotifyIcon(NIM_MODIFY, &nid);

	// Clear for Prevent reshow
	nid.szInfo[0] = 0;
	nid.szInfoTitle[0] = 0;

	// Keep last show-time
	cfg.dwLastBalloon = GetTickCount();

	return TRUE;
}

// Create HICON with memory usage info
HICON CreateMemIcon(DWORD dwData)
{
	CString buffer;
	RECT rc = {0, 0, 16, 16}; // icon rect

	// Settings
	COLORREF clrTrayBackground = ini.read(APP_NAME_SHORT, L"TrayBackgroundClr", COLOR_TRAY_BG);
	COLORREF clrTrayText = ini.read(APP_NAME_SHORT, L"TrayTextClr", COLOR_TRAY_TEXT);
	COLORREF clrIndicator = NULL;

	BOOL bTrayChangeBg = ini.read(APP_NAME_SHORT, L"TrayChangeBackground", 1);
	BOOL bTrayShowBorder = ini.read(APP_NAME_SHORT, L"TrayShowBorder", 0);
	BOOL bTrayShowFree = ini.read(APP_NAME_SHORT, L"TrayShowFree", 0);

	if(ini.read(APP_NAME_SHORT, L"ColorIndicationTray", 1))
	{
		if(dwData >= cfg.uDangerLevel)
			clrIndicator = ini.read(APP_NAME_SHORT, L"LevelDangerClr", COLOR_LEVEL_DANGER);

		else if(dwData >= cfg.uWarningLevel)
			clrIndicator = ini.read(APP_NAME_SHORT, L"LevelWarningClr", COLOR_LEVEL_WARNING);
	}

	// Create bitmap
    HDC hDC = GetDC(NULL);
    HDC hCompatibleDC = CreateCompatibleDC(hDC);
	HBITMAP hBitmap = CreateCompatibleBitmap(hDC, rc.right, rc.bottom);
    HBITMAP hBitmapMask = CreateCompatibleBitmap(hDC, rc.right, rc.bottom);
	ReleaseDC(NULL, hDC);
	
    HBITMAP hOldBitMap = (HBITMAP)SelectObject(hCompatibleDC, hBitmap);

	// Fill rectangle
	COLORREF clrOld = SetBkColor(hCompatibleDC, (bTrayChangeBg && clrIndicator) ? clrIndicator : clrTrayBackground);
	ExtTextOut(hCompatibleDC, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
	SetBkColor(hCompatibleDC, clrOld);
	
	// Create font
	HFONT hTrayFont = CreateFontIndirect(&cfg.lf);
	SelectObject(hCompatibleDC, hTrayFont);

	// Draw text
	SetTextColor(hCompatibleDC, !bTrayChangeBg && clrIndicator ? clrIndicator : clrTrayText);
	SetBkMode(hCompatibleDC, TRANSPARENT);

	buffer.Format(L"%d\0", bTrayShowFree ? 100 - dwData : dwData);
	DrawTextEx(hCompatibleDC, buffer.GetBuffer(), buffer.GetLength(), &rc, DT_VCENTER | DT_CENTER | DT_SINGLELINE | DT_NOCLIP, NULL);

	// Draw border
	if(bTrayShowBorder)
	{
		HBRUSH hBrush = CreateSolidBrush(!bTrayChangeBg && clrIndicator ? clrIndicator : clrTrayText);
		HRGN hRgn = CreateRectRgnIndirect(&rc);
		FrameRgn(hCompatibleDC, hRgn, hBrush, 1, 1);

		DeleteObject(hBrush);
		DeleteObject(hRgn);
	}
	
	SelectObject(hDC, hOldBitMap);

	// Create icon
	ICONINFO ii = {TRUE, 0, 0, hBitmapMask, hBitmap};
	HICON hIcon = CreateIconIndirect(&ii);

	// Free resources
	DeleteObject(SelectObject(hCompatibleDC, hTrayFont));
	DeleteDC(hCompatibleDC);
	DeleteDC(hDC);
	DeleteObject(hBitmap);
	DeleteObject(hBitmapMask);

	return hIcon;
}

// Memory reduction wrapper
BOOL MemReduct(HWND hWnd, BOOL bSilent)
{
	CString buffer;
	INT iBuffer = IDYES;
	
	SYSTEMTIME st = {0};

	MEMORY_USAGE mu = {0};
	SYSTEM_MEMORY_LIST_COMMAND smlc;

	LPARAM lParam[3] = {0};

	// If user has no rights
	if(!cfg.bAdminPrivilege)
	{
		if(cfg.bUnderUAC)
			ShowBalloonTip(NIIF_ERROR, APP_NAME, ls(cfg.hLocale, IDS_UAC_WARNING), FALSE);

		return FALSE;
	}

	// Check settings
	if(!ini.read(APP_NAME_SHORT, L"CleanWorkingSet", 1) && !ini.read(APP_NAME_SHORT, L"CleanSystemWorkingSet", 1) && !ini.read(APP_NAME_SHORT, L"CleanModifiedPagelist", 0) && !ini.read(APP_NAME_SHORT, L"CleanStandbyPagelist", 0))
	{
		if(bSilent)
			ShowBalloonTip(NIIF_ERROR, APP_NAME, ls(cfg.hLocale, IDS_REDUCT_SELECTREGION), FALSE);

		else
			MessageBox(hWnd, ls(cfg.hLocale, IDS_REDUCT_SELECTREGION), APP_NAME, MB_OK | MB_ICONSTOP);

		return FALSE;
	}

	if(!bSilent && ini.read(APP_NAME_SHORT, L"AskBeforeCleaning", 1))
		iBuffer = MessageBox(hWnd, ls(cfg.hLocale, IDS_REDUCT_WARNING), APP_NAME, MB_YESNO | MB_ICONQUESTION);

	if(iBuffer != IDYES)
		return FALSE;

	// Show difference: BEFORE
	GetMemoryUsage(&mu);

	lParam[0] = mu.dwPercentPhys;
	lParam[1] = mu.dwPercentPageFile;
	lParam[2] = mu.dwPercentSystemWorkingSet;

	if(hWnd)
	{
		buffer.Format(L"%d%%\0", mu.dwPercentPhys);
		Lv_InsertItem(hWnd, IDC_RESULT, buffer, 0, 1);

		buffer.Format(L"%d%%\0", mu.dwPercentPageFile);
		Lv_InsertItem(hWnd, IDC_RESULT, buffer, 1, 1);

		buffer.Format(L"%d%%\0", mu.dwPercentSystemWorkingSet);
		Lv_InsertItem(hWnd, IDC_RESULT, buffer, 2, 1);
	}

	// Working set
	if(cfg.bSupportedOS && ini.read(APP_NAME_SHORT, L"CleanWorkingSet", 1))
	{
		smlc = MemoryEmptyWorkingSets;
		NtSetSystemInformation(SystemMemoryListInformation, &smlc, sizeof(smlc));
	}

	// System working set
	if(ini.read(APP_NAME_SHORT, L"CleanSystemWorkingSet", 1))
	{
		SYSTEM_CACHE_INFORMATION cache = {0};

		cache.MinimumWorkingSet = (ULONG)-1;
		cache.MaximumWorkingSet = (ULONG)-1;

		NtSetSystemInformation(SystemFileCacheInformation, &cache, sizeof(cache));
	}
						
	// Modified pagelists
	if(cfg.bSupportedOS && ini.read(APP_NAME_SHORT, L"CleanModifiedPagelist", 0))
	{
		smlc = MemoryFlushModifiedList;
		NtSetSystemInformation(SystemMemoryListInformation, &smlc, sizeof(smlc));
	}

	// Standby pagelists
	if(cfg.bSupportedOS && ini.read(APP_NAME_SHORT, L"CleanStandbyPagelist", 0))
	{
		// Standby pagelists
		smlc = MemoryPurgeStandbyList;
		NtSetSystemInformation(SystemMemoryListInformation, &smlc, sizeof(smlc));

		// Standby priority-0 pagelists
		smlc = MemoryPurgeLowPriorityStandbyList;
		NtSetSystemInformation(SystemMemoryListInformation, &smlc, sizeof(smlc));
	}

	// Set last reduction time
	GetLocalTime(&st);
	ini.write(APP_NAME_SHORT, L"LastReductionTime", (DWORD)SystemTimeToUnixTime(&st));

	// Show difference: AFTER
	GetMemoryUsage(&mu);

	lParam[0] = MAKELPARAM(lParam[0], mu.dwPercentPhys);
	lParam[1] = MAKELPARAM(lParam[1], mu.dwPercentPageFile);
	lParam[2] = MAKELPARAM(lParam[2], mu.dwPercentSystemWorkingSet);

	if(hWnd)
	{
		GetMemoryUsage(&mu);

		buffer.Format(L"%d%%\0", mu.dwPercentPhys);
		Lv_InsertItem(hWnd, IDC_RESULT, buffer, 0, 2, -1, -1, lParam[0]);

		buffer.Format(L"%d%%\0", mu.dwPercentPageFile);
		Lv_InsertItem(hWnd, IDC_RESULT, buffer, 1, 2, -1, -1, lParam[1]);

		buffer.Format(L"%d%%\0", mu.dwPercentSystemWorkingSet);
		Lv_InsertItem(hWnd, IDC_RESULT, buffer, 2, 2, -1, -1, lParam[2]);

		SetDlgItemText(hWnd, IDC_TIMESTAMP, date_format(&st, cfg.dwLanguageId, DATE_LONGDATE));
	}

	for(int i = 0; i < 3; i++)
	{
		buffer.Format(L"LastReductionItem%d", i);
		ini.write(APP_NAME_SHORT, buffer, (DWORD)lParam[i]);
	}

	return TRUE;
}

// Localize main window resources
BOOL LocalizeMainWindow()
{
	// Menu
	HMENU hMenu = LoadMenu(cfg.hLocale, MAKEINTRESOURCE(IDM_MAIN));
	SetMenu(cfg.hWnd, hMenu);
	DrawMenuBar(cfg.hWnd);

	// Clear listview
	SendDlgItemMessage(cfg.hWnd, IDC_MONITOR, LVM_DELETEALLITEMS, 0, 0);
	SendDlgItemMessage(cfg.hWnd, IDC_MONITOR, LVM_REMOVEALLGROUPS, 0, 0);

	SendDlgItemMessage(cfg.hWnd, IDC_MONITOR, LVM_ENABLEGROUPVIEW, TRUE, 0);

	// Insert listview groups
	for(int i = IDS_MEM_PHYSICAL, j = 0, k = 0; i < (IDS_MEM_SYSCACHE + 1); i++, j++)
	{
		Lv_InsertGroup(cfg.hWnd, IDC_MONITOR, ls(cfg.hLocale, i), j);

		for(int l = IDS_MEM_USAGE; l < (IDS_MEM_TOTAL + 1); l++)
			Lv_InsertItem(cfg.hWnd, IDC_MONITOR, ls(cfg.hLocale, l), k++, 0, -1, j);
	}

	// Button
	SetDlgItemText(cfg.hWnd, IDC_REDUCT, ls(cfg.hLocale, IDS_REDUCT));

	// Refresh information
	SendMessage(cfg.hWnd, WM_TIMER, UID, 0);

	return TRUE;
}

INT_PTR WINAPI PagesDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CString buffer;
	INT iBuffer = 0;

	switch(uMsg)
	{
		case WM_INITDIALOG:
		{
			// Correct position
			SetWindowPos(hwndDlg, 0, tab_pages.rc.left, tab_pages.rc.top, tab_pages.rc.right - tab_pages.rc.left, tab_pages.rc.bottom - tab_pages.rc.top, 0);

			// Enable tab texture
			EnableThemeDialogTexture(hwndDlg, ETDT_ENABLETAB);

			switch(SendDlgItemMessage(GetParent(hwndDlg), IDC_TAB, TCM_GETCURSEL, 0, 0))
			{
				// GENERAL
				case 0:
				{
					// General
					CheckDlgButton(hwndDlg, IDC_LOAD_ON_STARTUP_CHK, IsAutorunExists(APP_NAME) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton(hwndDlg, IDC_CHECK_UPDATE_AT_STARTUP_CHK, ini.read(APP_NAME_SHORT, L"CheckUpdateAtStartup", 1) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton(hwndDlg, IDC_ALWAYS_ON_TOP_CHK, ini.read(APP_NAME_SHORT, L"AlwaysOnTop", 0) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton(hwndDlg, IDC_SHOW_AS_KILOBYTE_CHK, ini.read(APP_NAME_SHORT, L"ShowAsKilobyte", 0) ? BST_CHECKED : BST_UNCHECKED);

					// Color indication
					CheckDlgButton(hwndDlg, IDC_COLOR_INDICATION_TRAY_CHK, ini.read(APP_NAME_SHORT, L"ColorIndicationTray", 1) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton(hwndDlg, IDC_COLOR_INDICATION_LISTVIEW_CHK, ini.read(APP_NAME_SHORT, L"ColorIndicationListview", 1) ? BST_CHECKED : BST_UNCHECKED);

					// Language
					SendDlgItemMessage(hwndDlg, IDC_LANGUAGE_CB, CB_ADDSTRING, 0, (LPARAM)L"English");

					WIN32_FIND_DATA wfd = {0};
					buffer.Format(L"%s\\Languages\\*.dll", cfg.szCurrentDir);
					HANDLE hFind = FindFirstFile(buffer, &wfd);
					HINSTANCE hLanguage = NULL;

					if(hFind != INVALID_HANDLE_VALUE)
					{
						do
						{
							if(!(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
							{
								PathRemoveExtension(wfd.cFileName);

								buffer.Format(L"%s\\Languages\\%s.dll", cfg.szCurrentDir, wfd.cFileName);

								if((hLanguage = LoadLanguage(buffer, APP_VERSION)))
								{
									SendDlgItemMessage(hwndDlg, IDC_LANGUAGE_CB, CB_ADDSTRING, 0, (LPARAM)wfd.cFileName);
									FreeLibrary(hLanguage);
								}
							}
						}
						while(FindNextFile(hFind, &wfd));

						FindClose(hFind);
					}

					if(SendDlgItemMessage(hwndDlg, IDC_LANGUAGE_CB, CB_GETCOUNT, 0, 0) <= 1)
						EnableWindow(GetDlgItem(hwndDlg, IDC_LANGUAGE_CB), FALSE);

					if(SendDlgItemMessage(hwndDlg, IDC_LANGUAGE_CB, CB_SELECTSTRING, 1, (LPARAM)ini.read(APP_NAME_SHORT, L"Language", MAX_PATH, 0).GetBuffer()) == CB_ERR)
						SendDlgItemMessage(hwndDlg, IDC_LANGUAGE_CB, CB_SETCURSEL, 0, 0);

					SendMessage(hwndDlg, WM_COMMAND, MAKELPARAM(IDC_LANGUAGE_CB, CBN_SELENDOK), 0);

					break;
				}

				// MEMORY REDUCTION
				case 1:
				{
					// Reduction region
					CheckDlgButton(hwndDlg, IDC_WORKING_SET_CHK, ini.read(APP_NAME_SHORT, L"CleanWorkingSet", 1) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton(hwndDlg, IDC_SYSTEM_WORKING_SET_CHK, ini.read(APP_NAME_SHORT, L"CleanSystemWorkingSet", 1) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton(hwndDlg, IDC_MODIFIED_PAGELIST_CHK, ini.read(APP_NAME_SHORT, L"CleanModifiedPagelist", 0) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton(hwndDlg, IDC_STANDBY_PAGELIST_CHK, ini.read(APP_NAME_SHORT, L"CleanStandbyPagelist", 0) ? BST_CHECKED : BST_UNCHECKED);

					// Reduction options
					CheckDlgButton(hwndDlg, IDC_ASK_BEFORE_REDUCT_CHK, ini.read(APP_NAME_SHORT, L"AskBeforeCleaning", 1) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton(hwndDlg, IDC_AUTOREDUCT_ENABLE_CHK, ini.read(APP_NAME_SHORT, L"AutoReduct", 0) ? BST_CHECKED : BST_UNCHECKED);

					// Trackbar
					SendDlgItemMessage(hwndDlg, IDC_AUTOREDUCT_TB, TBM_SETTICFREQ, 5, 0);				 
					SendDlgItemMessage(hwndDlg, IDC_AUTOREDUCT_TB, TBM_SETRANGE, 1, MAKELPARAM(1, 99));
					SendDlgItemMessage(hwndDlg, IDC_AUTOREDUCT_TB, TBM_SETPOS, 1, ini.read(APP_NAME_SHORT, L"AutoReductPercents", 90));

					SendMessage(hwndDlg, WM_HSCROLL, MAKELPARAM(SB_ENDSCROLL, 0), 1);
					SendMessage(hwndDlg, WM_COMMAND, MAKELPARAM(IDC_AUTOREDUCT_ENABLE_CHK, 0), 0);

					// Indicate unsupported features
					if(!cfg.bSupportedOS)
					{
						EnableWindow(GetDlgItem(hwndDlg, IDC_WORKING_SET_CHK), FALSE);
						EnableWindow(GetDlgItem(hwndDlg, IDC_MODIFIED_PAGELIST_CHK), FALSE);
						EnableWindow(GetDlgItem(hwndDlg, IDC_STANDBY_PAGELIST_CHK), FALSE);
					}

					break;
				}

				// TRAY
				case 2:
				{
					UDACCEL uda_rr[1] = {{0, 100}};

					// Configure Up-Down #1
					SendDlgItemMessage(hwndDlg, IDC_REFRESHRATE, UDM_SETACCEL, 1, (LPARAM)&uda_rr);
					SendDlgItemMessage(hwndDlg, IDC_REFRESHRATE, UDM_SETRANGE32, 100, 10000);
					SendDlgItemMessage(hwndDlg, IDC_REFRESHRATE, UDM_SETPOS32, 0, ini.read(APP_NAME_SHORT, L"RefreshRate", 500));

					// Configure Up-Down #2
					SendDlgItemMessage(hwndDlg, IDC_WARNING_LEVEL, UDM_SETRANGE32, 1, 99);
					SendDlgItemMessage(hwndDlg, IDC_WARNING_LEVEL, UDM_SETPOS32, 0, ini.read(APP_NAME_SHORT, L"WarningLevel", 60));

					// Configure Up-Down #3
					SendDlgItemMessage(hwndDlg, IDC_DANGER_LEVEL, UDM_SETRANGE32, 1, 99);
					SendDlgItemMessage(hwndDlg, IDC_DANGER_LEVEL, UDM_SETPOS32, 0, ini.read(APP_NAME_SHORT, L"DangerLevel", 90));
					
					// Tray double-click
					for(int i = IDS_DOUBLECLICK_1; i < (IDS_DOUBLECLICK_4 + 1); i++)
						SendDlgItemMessage(hwndDlg, IDC_DOUBLECLICK_CB, CB_ADDSTRING, 0, (LPARAM)ls(cfg.hLocale, i).GetBuffer());

					if(SendDlgItemMessage(hwndDlg, IDC_DOUBLECLICK_CB, CB_SETCURSEL, ini.read(APP_NAME_SHORT, L"OnDoubleClick", 0), 0) == CB_ERR)
						SendDlgItemMessage(hwndDlg, IDC_DOUBLECLICK_CB, CB_SETCURSEL, 0, 0);

					// Tray memory region
					for(int i = IDS_MEM_PHYSICAL; i < (IDS_MEM_SYSCACHE + 1); i++)
						SendDlgItemMessage(hwndDlg, IDC_TRAYMEMORYREGION_CB, CB_ADDSTRING, 0, (LPARAM)ls(cfg.hLocale, i).GetBuffer());

					if(SendDlgItemMessage(hwndDlg, IDC_TRAYMEMORYREGION_CB, CB_SETCURSEL, ini.read(APP_NAME_SHORT, L"TrayMemoryRegion", 0), 0) == CB_ERR)
						SendDlgItemMessage(hwndDlg, IDC_TRAYMEMORYREGION_CB, CB_SETCURSEL, 0, 0);

					// Show "Free"
					CheckDlgButton(hwndDlg, IDC_TRAYSHOWFREE_CHK, ini.read(APP_NAME_SHORT, L"TrayShowFree", 0) ? BST_CHECKED : BST_UNCHECKED);

					break;
				}

				// APPEARANCE
				case 3:
				{
					// Icon
					CheckDlgButton(hwndDlg, IDC_TRAY_SHOW_BORDER_CHK, ini.read(APP_NAME_SHORT, L"TrayShowBorder", 0) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton(hwndDlg, IDC_TRAY_CHANGE_BACKGROUND_CHK, ini.read(APP_NAME_SHORT, L"TrayChangeBackground", 1) ? BST_CHECKED : BST_UNCHECKED);

					SetDlgItemText(hwndDlg, IDC_TRAY_FONT_BTN, cfg.lf.lfFaceName);

					break;
				}

				// BALLOON
				case 4:
				{
					// General
					CheckDlgButton(hwndDlg, IDC_BALLOON_SHOW_CHK, ini.read(APP_NAME_SHORT, L"BalloonShow", 1) ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton(hwndDlg, IDC_BALLOON_AUTOREDUCT_CHK, ini.read(APP_NAME_SHORT, L"BalloonAutoReduct", 1) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton(hwndDlg, IDC_BALLOON_WARNING_CHK, ini.read(APP_NAME_SHORT, L"BalloonWarningLevel", 0) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton(hwndDlg, IDC_BALLOON_DANGER_CHK, ini.read(APP_NAME_SHORT, L"BalloonDangerLevel", 1) ? BST_CHECKED : BST_UNCHECKED);

					// Options
					SendDlgItemMessage(hwndDlg, IDC_BALLOONINTERVAL, UDM_SETRANGE32, 5, 100);
					SendDlgItemMessage(hwndDlg, IDC_BALLOONINTERVAL, UDM_SETPOS32, 0, ini.read(APP_NAME_SHORT, L"BalloonInterval", 10));

					SendMessage(hwndDlg, WM_COMMAND, MAKELPARAM(IDC_BALLOON_SHOW_CHK, 0), 0);

					break;
				}
			}

			break;
		}

		case WM_DRAWITEM:
		{
			LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;

			if(wParam >= IDC_TITLE_1)
			{
				DrawTitle(hwndDlg, wParam, lpdis->hDC, &lpdis->rcItem, cfg.hTitleFont);
			}
			else
			{
				HTHEME hTheme = OpenThemeDataEx(hwndDlg, L"BUTTON", 0);

				if(hTheme)
				{
					// Draw styled button
					iBuffer = PBS_DEFAULTED;

					if(lpdis->itemState & ODS_SELECTED)
						iBuffer |= PBS_PRESSED;

					else if(lpdis->itemState & ODS_HOTLIGHT)
						iBuffer |= PBS_HOT;

					if(IsThemeBackgroundPartiallyTransparent(hTheme, BP_PUSHBUTTON, iBuffer))
						DrawThemeParentBackground(hwndDlg, lpdis->hDC, &lpdis->rcItem);

					DrawThemeBackgroundEx(hTheme, lpdis->hDC, BP_PUSHBUTTON, iBuffer, &lpdis->rcItem, NULL);

					CloseThemeData(hTheme);
				}
				else
				{
					// Draw classic button
					iBuffer = DFCS_BUTTONPUSH;

					if(lpdis->itemState & ODS_SELECTED)
						iBuffer |= DFCS_PUSHED;

					else if(lpdis->itemState & ODS_HOTLIGHT)
						iBuffer |= DFCS_HOT;

					DrawFrameControl(lpdis->hDC, &lpdis->rcItem, DFC_BUTTON, iBuffer);
				}

				// New rect for color indicator
				lpdis->rcItem.left += 6;
				lpdis->rcItem.top += 6;
				lpdis->rcItem.right -= 6;
				lpdis->rcItem.bottom -= 6;

				// Draw background
				COLORREF clrBg = 0;

				if(wParam == IDC_TRAY_TEXT_CLR_BTN)
					clrBg = ini.read(APP_NAME_SHORT, L"TrayTextClr", COLOR_TRAY_TEXT);

				else if(wParam == IDC_TRAY_BG_CLR_BTN)
					clrBg = ini.read(APP_NAME_SHORT, L"TrayBackgroundClr", COLOR_TRAY_BG);

				else if(wParam == IDC_LISTVIEW_TEXT_CLR_BTN)
					clrBg = ini.read(APP_NAME_SHORT, L"ListViewTextClr", COLOR_LISTVIEW_TEXT);

				else if(wParam == IDC_LEVEL_NORMAL_CLR_BTN)
					clrBg = ini.read(APP_NAME_SHORT, L"LevelNormalClr", COLOR_LEVEL_NORMAL);

				else if(wParam == IDC_LEVEL_WARNING_CLR_BTN)
					clrBg = ini.read(APP_NAME_SHORT, L"LevelWarningClr", COLOR_LEVEL_WARNING);

				else if(wParam == IDC_LEVEL_DANGER_CLR_BTN)
					clrBg = ini.read(APP_NAME_SHORT, L"LevelDangerClr", COLOR_LEVEL_DANGER);

				// FillRect
				COLORREF clrOld = SetBkColor(lpdis->hDC, clrBg);
				ExtTextOut(lpdis->hDC, 0, 0, ETO_OPAQUE, &lpdis->rcItem, NULL, 0, NULL);
				SetBkColor(lpdis->hDC, clrOld);
			}

			return TRUE;
		}

		case WM_NOTIFY:
		{
			switch(((LPNMHDR)lParam)->code)
			{
				case NM_CLICK:
				case NM_RETURN:
				{
					PNMLINK nmlp = (PNMLINK)lParam;

					if(nmlp->item.szUrl[0])
						ShellExecute(hwndDlg, 0, nmlp->item.szUrl, 0, 0, SW_SHOW);

					break;
				}

				case UDN_DELTAPOS:
				{
					EnableWindow(GetDlgItem(GetParent(hwndDlg), IDC_APPLY), TRUE);
					break;
				}
			}

			break;
		}

		case WM_HSCROLL:
		case WM_VSCROLL:
		{
			if(GetDlgItem(hwndDlg, IDC_AUTOREDUCT_TB) && LOWORD(wParam) == SB_ENDSCROLL)
			{
				buffer.Format(L"%d%%", SendDlgItemMessage(hwndDlg, IDC_AUTOREDUCT_TB, TBM_GETPOS, 0, 0));
				SetDlgItemText(hwndDlg, IDC_AUTOREDUCT_PERCENT, buffer);

				if(lParam != 1)
					EnableWindow(GetDlgItem(GetParent(hwndDlg), IDC_APPLY), TRUE);
			}

			break;
		}

		case WM_COMMAND:
		{
			if(lParam && (HIWORD(wParam) == BN_CLICKED || HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == CBN_SELENDOK))
				EnableWindow(GetDlgItem(GetParent(hwndDlg), IDC_APPLY), TRUE);

			if(HIWORD(wParam) == CBN_SELENDOK && LOWORD(wParam) == IDC_LANGUAGE_CB)
			{
				HINSTANCE hModule = NULL;
				WCHAR szBuffer[MAX_PATH] = {0};

				iBuffer = (SendDlgItemMessage(hwndDlg, IDC_LANGUAGE_CB, CB_GETCURSEL, 0, 0) > 0);

				if(iBuffer)
				{
					GetDlgItemText(hwndDlg, IDC_LANGUAGE_CB, szBuffer, MAX_PATH);
					buffer.Format(L"%s\\Languages\\%s.dll", cfg.szCurrentDir, szBuffer);

					hModule = LoadLanguage(buffer, APP_VERSION);
				}

				SetDlgItemText(hwndDlg, IDC_LANGUAGE_INFO, (iBuffer && !hModule) ? L"unknown" : ls(hModule, IDS_TRANSLATION_INFO));

				if(hModule)
					FreeLibrary(hModule);
			}

			switch(LOWORD(wParam))
			{
				case IDC_TRAY_FONT_BTN:
				{
					CHOOSEFONT cf = {0};

					cf.lStructSize = sizeof(cf);
					cf.hwndOwner = hwndDlg;
					cf.Flags = CF_NOSCRIPTSEL | CF_INITTOLOGFONTSTRUCT;
					cf.lpLogFont = &cfg.lf;

					if(ChooseFont(&cf))
					{
						SetDlgItemText(hwndDlg, IDC_TRAY_FONT_BTN, cfg.lf.lfFaceName);

						ini.write(APP_NAME_SHORT, L"FontFace", cfg.lf.lfFaceName);
						ini.write(APP_NAME_SHORT, L"FontHeight", cfg.lf.lfHeight);
					}

					break;
				}

				case IDC_TRAY_TEXT_CLR_BTN:
				case IDC_TRAY_BG_CLR_BTN:
				case IDC_LISTVIEW_TEXT_CLR_BTN:
				case IDC_LEVEL_NORMAL_CLR_BTN:
				case IDC_LEVEL_WARNING_CLR_BTN:
				case IDC_LEVEL_DANGER_CLR_BTN:
				{
					COLORREF clrDefault = 0, cr[16] = {COLOR_TRAY_TEXT, COLOR_TRAY_BG, COLOR_LISTVIEW_TEXT, COLOR_LEVEL_NORMAL, COLOR_LEVEL_WARNING, COLOR_LEVEL_DANGER};
					CHOOSECOLOR cc = {0};

					if(LOWORD(wParam) == IDC_TRAY_TEXT_CLR_BTN)
					{
						buffer = L"TrayTextClr";
						clrDefault = COLOR_TRAY_TEXT;
					}
					else if(LOWORD(wParam) == IDC_TRAY_BG_CLR_BTN)
					{
						buffer = L"TrayBackgroundClr";
						clrDefault = COLOR_TRAY_BG;
					}
					else if(LOWORD(wParam) == IDC_LISTVIEW_TEXT_CLR_BTN)
					{
						buffer = L"ListViewTextClr";
						clrDefault = COLOR_LISTVIEW_TEXT;
					}
					else if(LOWORD(wParam) == IDC_LEVEL_NORMAL_CLR_BTN)
					{
						buffer = L"LevelNormalClr";
						clrDefault = COLOR_LEVEL_NORMAL;
					}
					else if(LOWORD(wParam) == IDC_LEVEL_WARNING_CLR_BTN)	
					{
						buffer = L"LevelWarningClr";
						clrDefault = COLOR_LEVEL_WARNING;
					}
					else if(LOWORD(wParam) == IDC_LEVEL_DANGER_CLR_BTN)	
					{
						buffer = L"LevelDangerClr";
						clrDefault = COLOR_LEVEL_DANGER;
					}
					else
					{
						return FALSE;
					}

					cc.lStructSize = sizeof(cc);
					cc.Flags = CC_RGBINIT | CC_FULLOPEN;
					cc.hwndOwner = hwndDlg;
					cc.lpCustColors = cr;
					cc.rgbResult = ini.read(APP_NAME_SHORT, buffer, clrDefault);

					if(ChooseColor(&cc))
					{
						ini.write(APP_NAME_SHORT, buffer, cc.rgbResult);
						InvalidateRect(hwndDlg, NULL, TRUE); // redraw color buttons
					}

					break;
				}

				case IDC_AUTOREDUCT_ENABLE_CHK:
				{
					EnableWindow(GetDlgItem(hwndDlg, IDC_AUTOREDUCT_TB), (IsDlgButtonChecked(hwndDlg, IDC_AUTOREDUCT_ENABLE_CHK) == BST_CHECKED));
					break;
				}

				case IDC_BALLOON_SHOW_CHK:
				{
					iBuffer = (IsDlgButtonChecked(hwndDlg, IDC_BALLOON_SHOW_CHK) == BST_CHECKED);

					EnableWindow(GetDlgItem(hwndDlg, IDC_BALLOON_AUTOREDUCT_CHK), iBuffer);
					EnableWindow(GetDlgItem(hwndDlg, IDC_BALLOON_WARNING_CHK), iBuffer);
					EnableWindow(GetDlgItem(hwndDlg, IDC_BALLOON_DANGER_CHK), iBuffer);
					EnableWindow(GetDlgItem(hwndDlg, IDC_BALLOONINTERVAL), iBuffer);
					EnableWindow((HWND)SendDlgItemMessage(hwndDlg, IDC_BALLOONINTERVAL, UDM_GETBUDDY, 0, 0), iBuffer);

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

INT_PTR CALLBACK SettingsDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CString buffer;
	INT iBuffer = 0;

	switch(uMsg)
	{
		case WM_INITDIALOG:
		{
			// Centering by parent
			CenterDialog(hwndDlg);

			// Insert tab pages
			TCITEM tci = {0};

			for(int i = 0; i < PAGE_COUNT; i++)
			{
				buffer = ls(cfg.hLocale, tab_pages.iTitle[i]);

				tci.mask = TCIF_TEXT;
				tci.pszText = buffer.GetBuffer();
				tci.cchTextMax = buffer.GetLength();

				SendDlgItemMessage(hwndDlg, IDC_TAB, TCM_INSERTITEM, i, (LPARAM)&tci);
			}

			// Calculate tab pages rect
			if(IsRectEmpty(&tab_pages.rc))
			{
				RECT rc = {0};
				GetWindowRect(GetDlgItem(hwndDlg, IDC_TAB), &rc);
				MapWindowPoints(NULL, hwndDlg, (LPPOINT)&rc, 2);

				GetClientRect(GetDlgItem(hwndDlg, IDC_TAB), &tab_pages.rc);
				TabCtrl_AdjustRect(GetDlgItem(hwndDlg, IDC_TAB), 0, &tab_pages.rc);

				tab_pages.rc.top += rc.top + 1;	tab_pages.rc.left += rc.left - 1; tab_pages.rc.right += rc.left - 1; tab_pages.rc.bottom += rc.top - 1;
			}

			// Reset tab pages handles
			for(int i = 0; i < PAGE_COUNT; i++)
				tab_pages.hWnd[i] = NULL;

			// Activate last used tab page
			SendDlgItemMessage(hwndDlg, IDC_TAB, TCM_SETCURSEL, ini.read(APP_NAME_SHORT, L"LastTab", 0), 0);

			// Initialize page
			NMHDR hdr = {0};
			hdr.code = TCN_SELCHANGE;
			hdr.idFrom = IDC_TAB;

			SendMessage(hwndDlg, WM_NOTIFY, 0, (LPARAM)&hdr);

			// Disable "Apply" button
			EnableWindow(GetDlgItem(hwndDlg, IDC_APPLY), FALSE);

			break;
		}

		case WM_DESTROY:
		{
			ini.write(APP_NAME_SHORT, L"LastTab", SendDlgItemMessage(hwndDlg, IDC_TAB, TCM_GETCURSEL, 0, 0));
			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lParam;

			switch(nmlp->code)
			{
				case TCN_SELCHANGE:
				{
					if(nmlp->idFrom == IDC_TAB)
					{
						iBuffer = SendDlgItemMessage(hwndDlg, nmlp->idFrom, TCM_GETCURSEL, 0, 0);
						ShowWindow(tab_pages.hCurrent, SW_HIDE);

						if(tab_pages.hWnd[iBuffer])
						{
							// Show existing...
							ShowWindow(tab_pages.hWnd[iBuffer], SW_SHOW);
							tab_pages.hCurrent = tab_pages.hWnd[iBuffer];
						}
						else
						{
							// Create new...
							tab_pages.hCurrent = CreateDialog(cfg.hLocale, MAKEINTRESOURCE(tab_pages.iDialog[iBuffer]), hwndDlg, PagesDlgProc);
							tab_pages.hWnd[iBuffer] = tab_pages.hCurrent;
						}
					}

					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case IDC_OK:
				case IDC_APPLY:
				{
					// GENERAL
					if(tab_pages.hWnd[0])
					{
						// General
						CreateAutorunEntry(APP_NAME, (IsDlgButtonChecked(tab_pages.hWnd[0], IDC_LOAD_ON_STARTUP_CHK) == BST_CHECKED));

						ini.write(APP_NAME_SHORT, L"CheckUpdateAtStartup", (IsDlgButtonChecked(tab_pages.hWnd[0], IDC_CHECK_UPDATE_AT_STARTUP_CHK) == BST_CHECKED) ? 1 : 0);

						iBuffer = (IsDlgButtonChecked(tab_pages.hWnd[0], IDC_ALWAYS_ON_TOP_CHK) == BST_CHECKED) ? 1 : 0;
						ini.write(APP_NAME_SHORT, L"AlwaysOnTop", iBuffer);
						SetWindowPos(cfg.hWnd, (iBuffer ? HWND_TOPMOST : HWND_NOTOPMOST), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

						// Color indication
						ini.write(APP_NAME_SHORT, L"ColorIndicationTray", (IsDlgButtonChecked(tab_pages.hWnd[0], IDC_COLOR_INDICATION_TRAY_CHK) == BST_CHECKED));
						ini.write(APP_NAME_SHORT, L"ColorIndicationListview", (IsDlgButtonChecked(tab_pages.hWnd[0], IDC_COLOR_INDICATION_LISTVIEW_CHK) == BST_CHECKED));

						// Language
						iBuffer = SendDlgItemMessage(tab_pages.hWnd[0], IDC_LANGUAGE_CB, CB_GETCURSEL, 0, 0);

						if(cfg.hLocale)
							FreeLibrary(cfg.hLocale);

						if(iBuffer <= 0)
						{
							ini.write(APP_NAME_SHORT, L"Language", (DWORD)0);
							cfg.hLocale = NULL;
							cfg.dwLanguageId = 0;
						}
						else
						{
							WCHAR szBuffer[MAX_PATH] = {0};

							GetDlgItemText(tab_pages.hWnd[0], IDC_LANGUAGE_CB, szBuffer, MAX_PATH);
							ini.write(APP_NAME_SHORT, L"Language", szBuffer);

							buffer.Format(L"%s\\Languages\\%s.dll", cfg.szCurrentDir, szBuffer);
							cfg.hLocale = LoadLanguage(buffer, APP_VERSION, &cfg.dwLanguageId);
						}

						// Size Unit
						iBuffer = (IsDlgButtonChecked(tab_pages.hWnd[0], IDC_SHOW_AS_KILOBYTE_CHK) == BST_CHECKED);
						ini.write(APP_NAME_SHORT, L"ShowAsKilobyte", iBuffer);
						StringCchCopy(cfg.szUnit, _countof(cfg.szUnit), ls(cfg.hLocale, iBuffer ? IDS_UNIT_KB : IDS_UNIT_MB));
						cfg.uUnitDivider = iBuffer ? 1024 : 1048576;

						// Localize main window
						LocalizeMainWindow();
					}

					// MEMORY REDUCTION
					if(tab_pages.hWnd[1])
					{
						// Reduction region
						ini.write(APP_NAME_SHORT, L"CleanWorkingSet", (IsDlgButtonChecked(tab_pages.hWnd[1], IDC_WORKING_SET_CHK) == BST_CHECKED) ? 1 : 0);
						ini.write(APP_NAME_SHORT, L"CleanSystemWorkingSet", (IsDlgButtonChecked(tab_pages.hWnd[1], IDC_SYSTEM_WORKING_SET_CHK) == BST_CHECKED) ? 1 : 0);
						ini.write(APP_NAME_SHORT, L"CleanModifiedPagelist", (IsDlgButtonChecked(tab_pages.hWnd[1], IDC_MODIFIED_PAGELIST_CHK) == BST_CHECKED) ? 1 : 0);
						ini.write(APP_NAME_SHORT, L"CleanStandbyPagelist", (IsDlgButtonChecked(tab_pages.hWnd[1], IDC_STANDBY_PAGELIST_CHK) == BST_CHECKED) ? 1 : 0);

						// Reduction options
						ini.write(APP_NAME_SHORT, L"AskBeforeCleaning", (IsDlgButtonChecked(tab_pages.hWnd[1], IDC_ASK_BEFORE_REDUCT_CHK) == BST_CHECKED) ? 1 : 0);

						cfg.bAutoReduct = (IsDlgButtonChecked(tab_pages.hWnd[1], IDC_AUTOREDUCT_ENABLE_CHK) == BST_CHECKED) ? 1 : 0;
						ini.write(APP_NAME_SHORT, L"AutoReduct", cfg.bAutoReduct);

						cfg.uAutoReductPercents = SendDlgItemMessage(tab_pages.hWnd[1], IDC_AUTOREDUCT_TB, TBM_GETPOS, 0, 0);
						ini.write(APP_NAME_SHORT, L"AutoReductPercents", cfg.uAutoReductPercents);
					}

					// TRAY
					if(tab_pages.hWnd[2])
					{
						// Refresh Rate
						iBuffer = SendDlgItemMessage(tab_pages.hWnd[2], IDC_REFRESHRATE, UDM_GETPOS32, 0, 0);

						ini.write(APP_NAME_SHORT, L"RefreshRate", iBuffer);
						KillTimer(cfg.hWnd, UID);
						SetTimer(cfg.hWnd, UID, iBuffer, 0);

						// "Warning" Level
						cfg.uWarningLevel = SendDlgItemMessage(tab_pages.hWnd[2], IDC_WARNING_LEVEL, UDM_GETPOS32, 0, 0);
						ini.write(APP_NAME_SHORT, L"WarningLevel", cfg.uWarningLevel);

						// "Danger" Level
						cfg.uDangerLevel = SendDlgItemMessage(tab_pages.hWnd[2], IDC_DANGER_LEVEL, UDM_GETPOS32, 0, 0);
						ini.write(APP_NAME_SHORT, L"DangerLevel", cfg.uDangerLevel);

						// Tray double-click
						ini.write(APP_NAME_SHORT, L"OnDoubleClick", SendDlgItemMessage(tab_pages.hWnd[2], IDC_DOUBLECLICK_CB, CB_GETCURSEL, 0, 0));

						// Tray displayed memory region
						ini.write(APP_NAME_SHORT, L"TrayMemoryRegion", SendDlgItemMessage(tab_pages.hWnd[2], IDC_TRAYMEMORYREGION_CB, CB_GETCURSEL, 0, 0));

						// Show "Free"
						ini.write(APP_NAME_SHORT, L"TrayShowFree", (IsDlgButtonChecked(tab_pages.hWnd[2], IDC_TRAYSHOWFREE_CHK) == BST_CHECKED) ? 1 : 0);
					}

					// APPEARANCE
					if(tab_pages.hWnd[3])
					{
						ini.write(APP_NAME_SHORT, L"TrayShowBorder", (IsDlgButtonChecked(tab_pages.hWnd[3], IDC_TRAY_SHOW_BORDER_CHK) == BST_CHECKED));
						ini.write(APP_NAME_SHORT, L"TrayChangeBackground", IsDlgButtonChecked(tab_pages.hWnd[3], IDC_TRAY_CHANGE_BACKGROUND_CHK) == BST_CHECKED);
					}

					// BALLOON
					if(tab_pages.hWnd[4])
					{
						// Balloon's Chk
						ini.write(APP_NAME_SHORT, L"BalloonShow", (IsDlgButtonChecked(tab_pages.hWnd[4], IDC_BALLOON_SHOW_CHK) == BST_CHECKED));

						ini.write(APP_NAME_SHORT, L"BalloonAutoReduct", (IsDlgButtonChecked(tab_pages.hWnd[4], IDC_BALLOON_AUTOREDUCT_CHK) == BST_CHECKED));
						ini.write(APP_NAME_SHORT, L"BalloonWarningLevel", (IsDlgButtonChecked(tab_pages.hWnd[4], IDC_BALLOON_WARNING_CHK) == BST_CHECKED));
						ini.write(APP_NAME_SHORT, L"BalloonDangerLevel", (IsDlgButtonChecked(tab_pages.hWnd[4], IDC_BALLOON_DANGER_CHK) == BST_CHECKED));
	
						// Balloon Options
						ini.write(APP_NAME_SHORT, L"BalloonInterval", SendDlgItemMessage(tab_pages.hWnd[4], IDC_BALLOONINTERVAL, UDM_GETPOS32, 0, 0));
					}

					if(LOWORD(wParam) == IDC_APPLY)
					{
						EnableWindow(GetDlgItem(hwndDlg, IDC_APPLY), FALSE);
						break;
					}

					// without break
				}

				case IDCANCEL: // process Esc key
				case IDC_CANCEL:
				{
					EndDialog(hwndDlg, 0);
					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

INT_PTR CALLBACK ReductDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CString buffer;
	INT iBuffer = 0;

	RECT rc = {0};
	SYSTEMTIME st = {0};

	LPARAM lLastReduct = 0;

	switch(uMsg)
	{
		case WM_INITDIALOG:
		{
			// Centering by parent
			CenterDialog(hwndDlg);

			// Set style
			Lv_SetStyleEx(hwndDlg, IDC_RESULT, LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_DOUBLEBUFFER, TRUE);

			// Insert columns
			GetClientRect(GetDlgItem(hwndDlg, IDC_RESULT), &rc);

			for (int i = 0; i < 3; i++)
				Lv_InsertColumn(hwndDlg, IDC_RESULT, L"", rc.right / (i == 0 ? 2 : 4), i, (i == 0 ? LVCFMT_LEFT : LVCFMT_RIGHT));

			// Insert items
			for(int i = IDS_MEM_PHYSICAL, j = 0; i < (IDS_MEM_SYSCACHE + 1); i++, j++)
			{
				Lv_InsertItem(hwndDlg, IDC_RESULT, ls(cfg.hLocale, i), j, 0, -1, -1, cfg.lLastResult[j]);

				for(int k = 1; k < 3; k++)
				{
					buffer.Format(L"LastReductionItem%d", j);
					lLastReduct = ini.read(APP_NAME_SHORT, buffer, 0);

					buffer.Format(L"%d%%", k % 2  ? LOWORD(lLastReduct) : HIWORD(lLastReduct));
					Lv_InsertItem(hwndDlg, IDC_RESULT, buffer, j, k, -1, -1, lLastReduct);
				}
			}

			// Add menu to button
			if(cfg.bSupportedOS)
				SetWindowLongPtr(GetDlgItem(hwndDlg, IDC_OK), GWL_STYLE, GetWindowLongPtr(GetDlgItem(hwndDlg, IDC_OK), GWL_STYLE) | BS_SPLITBUTTON);

			// Show last reduction time
			if(UnixTimeToSystemTime(ini.read(APP_NAME_SHORT, L"LastReductionTime", -1), &st))
				SetDlgItemText(hwndDlg, IDC_TIMESTAMP, date_format(&st, cfg.dwLanguageId, DATE_LONGDATE));

			break;
		}

		case WM_DRAWITEM:
		{
			LPDRAWITEMSTRUCT nmlp = (LPDRAWITEMSTRUCT)lParam;

			if(nmlp->itemAction == ODA_DRAWENTIRE && wParam >= IDC_TITLE_1)
				DrawTitle(hwndDlg, wParam, nmlp->hDC, &nmlp->rcItem, cfg.hTitleFont);

			return TRUE;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			HDC hDC = BeginPaint(hwndDlg, &ps);

			GetClientRect(hwndDlg, &rc);
			rc.top = rc.bottom - 43;

			// Instead FillRect
			COLORREF clrOld = SetBkColor(hDC, GetSysColor(COLOR_BTNFACE));
			ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
			SetBkColor(hDC, clrOld);

			// Draw Line
			for(int i = 0; i < rc.right; i++)
				SetPixel(hDC, i, rc.top, GetSysColor(COLOR_BTNSHADOW));

			EndPaint(hwndDlg, &ps);

			break;
		}

		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORDLG:
		{
			if(GetDlgCtrlID((HWND)lParam) == IDC_TIMESTAMP)
			{
				SetBkMode((HDC)wParam, TRANSPARENT);
				SetTextColor((HDC)wParam, GetSysColor(COLOR_GRAYTEXT));

				return (INT_PTR)GetSysColorBrush(COLOR_BTNFACE);
			}

			return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lParam;

			switch(nmlp->code)
			{
				case BCN_DROPDOWN:
				{
					LPNMBCDROPDOWN nmlp = (LPNMBCDROPDOWN)lParam;

					if(nmlp->hdr.idFrom == IDC_OK)
					{
						// Load menu
						HMENU hMenu = LoadMenu(cfg.hLocale, MAKEINTRESOURCE(IDM_REDUCT)), hSubMenu = GetSubMenu(hMenu, 0);

						// Check items
						CheckMenuItem(hSubMenu, IDC_WORKING_SET_CHK, (ini.read(APP_NAME_SHORT, L"CleanWorkingSet", 1) ? MF_CHECKED : MF_UNCHECKED) | MF_BYCOMMAND);
						CheckMenuItem(hSubMenu, IDC_SYSTEM_WORKING_SET_CHK, (ini.read(APP_NAME_SHORT, L"CleanSystemWorkingSet", 1) ? MF_CHECKED : MF_UNCHECKED) | MF_BYCOMMAND);
						CheckMenuItem(hSubMenu, IDC_MODIFIED_PAGELIST_CHK, (ini.read(APP_NAME_SHORT, L"CleanModifiedPagelist", 0) ? MF_CHECKED : MF_UNCHECKED) | MF_BYCOMMAND);
						CheckMenuItem(hSubMenu, IDC_STANDBY_PAGELIST_CHK, (ini.read(APP_NAME_SHORT, L"CleanStandbyPagelist", 0) ? MF_CHECKED : MF_UNCHECKED) | MF_BYCOMMAND);

						// Get cursor position
						POINT pt = {0};
						GetCursorPos(&pt);

						// Show menu
						TrackPopupMenuEx(hSubMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_LEFTBUTTON | TPM_NOANIMATION, pt.x, pt.y, hwndDlg, NULL);

						// Destroy menu
						DestroyMenu(hMenu);
						DestroyMenu(hSubMenu);
					}

					break;
				}

				case NM_CUSTOMDRAW:
				{
					LPNMLVCUSTOMDRAW nmlp = (LPNMLVCUSTOMDRAW)lParam;
					LONG lResult = CDRF_DODEFAULT;

					switch(nmlp->nmcd.dwDrawStage)
					{
						case CDDS_PREPAINT:
						{
							lResult |= CDRF_NOTIFYITEMDRAW;
							break;
						}

						case CDDS_ITEMPREPAINT:
						{
							lResult |= CDRF_NOTIFYSUBITEMDRAW;
							break;
						}

						case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
						{
							if(nmlp->iSubItem && ini.read(APP_NAME_SHORT, L"ColorIndicationListView", 1))
							{
								nmlp->clrText = ini.read(APP_NAME_SHORT, L"ListViewTextClr", COLOR_LISTVIEW_TEXT);

								if(nmlp->nmcd.lItemlParam && LOWORD(nmlp->nmcd.lItemlParam) != HIWORD(nmlp->nmcd.lItemlParam))
								{
									if(nmlp->iSubItem == 1 && LOWORD(nmlp->nmcd.lItemlParam) > HIWORD(nmlp->nmcd.lItemlParam))
										nmlp->clrText = ini.read(APP_NAME_SHORT, L"LevelDangerClr", COLOR_LEVEL_DANGER);

									else if(nmlp->iSubItem == 2 && LOWORD(nmlp->nmcd.lItemlParam) > HIWORD(nmlp->nmcd.lItemlParam))
										nmlp->clrText = ini.read(APP_NAME_SHORT, L"LevelNormalClr", COLOR_LEVEL_NORMAL);
								}

								lResult |= CDRF_NEWFONT;
							}

							break;
						}
					}

					SetWindowLongPtr(hwndDlg, DWL_MSGRESULT, lResult);
					return TRUE;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case IDC_WORKING_SET_CHK:
				case IDC_SYSTEM_WORKING_SET_CHK:
				case IDC_MODIFIED_PAGELIST_CHK:
				case IDC_STANDBY_PAGELIST_CHK:
				{
					if(LOWORD(wParam) == IDC_WORKING_SET_CHK)
						buffer = L"CleanWorkingSet";

					else if(LOWORD(wParam) == IDC_SYSTEM_WORKING_SET_CHK)
						buffer = L"CleanSystemWorkingSet";

					else if(LOWORD(wParam) == IDC_MODIFIED_PAGELIST_CHK)
						buffer = L"CleanModifiedPagelist";

					else if(LOWORD(wParam) == IDC_STANDBY_PAGELIST_CHK)
						buffer = L"CleanStandbyPagelist";

					else
						return FALSE;

					ini.write(APP_NAME_SHORT, buffer, !ini.read(APP_NAME_SHORT, buffer, 0));

					break;
				}

				case IDC_OK:
				{
					MemReduct(hwndDlg, FALSE);
					break;
				}

				case IDCANCEL: // process Esc key
				case IDC_CANCEL:
				{
					EndDialog(hwndDlg, 0);
					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

LRESULT CALLBACK DlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CString buffer;
	INT iBuffer = 0;
	RECT rc = {0};

	if(uMsg == WM_MUTEX)
		return MutexWrapper(hwndDlg, wParam, lParam);

	switch(uMsg)
	{
		case WM_INITDIALOG:
		{
			// Check mutex
			HANDLE hMutex = CreateMutex(NULL, FALSE, APP_NAME_SHORT);

			if(wcsstr(GetCommandLine(), L"/reduct"))
			{
				ReleaseMutex(hMutex);
				CloseHandle(hMutex);

				CreateMutex(NULL, FALSE, APP_NAME_SHORT);

				SendMessage(HWND_BROADCAST, WM_MUTEX, GetCurrentProcessId(), FALSE);
				ToggleVisible(hwndDlg, TRUE);
			}
			else if(GetLastError() == ERROR_ALREADY_EXISTS)
			{
				SendMessage(HWND_BROADCAST, WM_MUTEX, GetCurrentProcessId(), TRUE);
				DestroyWindow(hwndDlg);

				return FALSE;
			}

			// Set title
			SetWindowText(hwndDlg, APP_NAME L" " APP_VERSION);

			// Set icons
			SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAIN), IMAGE_ICON, 32, 32, 0));
			SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAIN), IMAGE_ICON, 16, 16, 0));

			// Modify system menu
			HMENU hMenu = GetSystemMenu(hwndDlg, 0);
			InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
			InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_ABOUT, ls(cfg.hLocale, IDS_ABOUT));

			// Load settings
			cfg.hTitleFont = GetFont();
			cfg.hWnd = hwndDlg;

			cfg.bSupportedOS = ValidWindowsVersion(6, 0); // if vista (6.0) and later
			cfg.bAdminPrivilege = IsAdmin(); // if user has admin rights
			cfg.bUnderUAC = IsUnderUAC(); // if running under UAC

			cfg.uWarningLevel = ini.read(APP_NAME_SHORT, L"WarningLevel", 60);
			cfg.uDangerLevel = ini.read(APP_NAME_SHORT, L"DangerLevel", 90);

			cfg.bAutoReduct = ini.read(APP_NAME_SHORT, L"AutoReduct", 0);
			cfg.uAutoReductPercents = ini.read(APP_NAME_SHORT, L"AutoReductPercents", 90);

			iBuffer = ini.read(APP_NAME_SHORT, L"ShowAsKilobyte", 0);
			cfg.uUnitDivider = iBuffer ? 1024 : 1048576;
			StringCchCopy(cfg.szUnit, _countof(cfg.szUnit), ls(cfg.hLocale, iBuffer ? IDS_UNIT_KB : IDS_UNIT_MB));

			StringCchCopy(cfg.lf.lfFaceName, _countof(cfg.lf.lfFaceName), ini.read(APP_NAME_SHORT, L"FontFace", MAX_PATH, L"Tahoma"));
			cfg.lf.lfHeight = ini.read(APP_NAME_SHORT, L"FontHeight", 13);

			// Always on top
			SetWindowPos(hwndDlg, (ini.read(APP_NAME_SHORT, L"AlwaysOnTop", 0) ? HWND_TOPMOST : HWND_NOTOPMOST), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

			// Privileges enabler
			if(cfg.bAdminPrivilege)
			{
				HANDLE hToken = NULL;

				if(OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
				{
					SetPrivilege(hToken, SE_INCREASE_QUOTA_NAME, TRUE);
					SetPrivilege(hToken, SE_PROF_SINGLE_PROCESS_NAME, TRUE);
				}

				if(hToken)
					CloseHandle(hToken);
			}

			// Styling
			Lv_SetStyleEx(hwndDlg, IDC_MONITOR, LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_DOUBLEBUFFER, TRUE, TRUE);

			// Insert columns
			Lv_InsertColumn(hwndDlg, IDC_MONITOR, L"", GetWindowDimension(GetDlgItem(hwndDlg, IDC_MONITOR), WIDTH, TRUE) / 2, 1, LVCFMT_RIGHT);
			Lv_InsertColumn(hwndDlg, IDC_MONITOR, L"", GetWindowDimension(GetDlgItem(hwndDlg, IDC_MONITOR), WIDTH, TRUE) / 2, 2, LVCFMT_LEFT);

			// Privilege indicator (Windows Vista and above)
			if(cfg.bUnderUAC)
			{
				 // Set UAC shield to button
				SendDlgItemMessage(hwndDlg, IDC_REDUCT, BCM_SETSHIELD, 0, TRUE);

				// Set text margins
				SendDlgItemMessage(hwndDlg, IDC_REDUCT, BCM_GETTEXTMARGIN, 0, (LPARAM)&rc);
				rc.left += 7;
				SendDlgItemMessage(hwndDlg, IDC_REDUCT, BCM_SETTEXTMARGIN, 0, (LPARAM)&rc);
			}

			// Privilege indicator (Windows XP)
			if(!cfg.bAdminPrivilege && !cfg.bSupportedOS)
				EnableWindow(GetDlgItem(hwndDlg, IDC_REDUCT), FALSE);

			// Tray icon
			MEMORY_USAGE mu = {0};

			nid.cbSize = cfg.bSupportedOS ? sizeof(nid) : NOTIFYICONDATA_V3_SIZE;
			nid.hWnd = hwndDlg;
			nid.uID = UID;
			nid.uFlags = NIF_MESSAGE | NIF_ICON;
			nid.uCallbackMessage = WM_TRAYICON;
			nid.hIcon = CreateMemIcon(GetMemoryUsage(&mu));

			Shell_NotifyIcon(NIM_ADD, &nid);

			// Timer
			SetTimer(hwndDlg, UID, ini.read(APP_NAME_SHORT, L"RefreshRate", 500), NULL);

			// Localize main window
			LocalizeMainWindow();

			// Check Updates
			if(ini.read(APP_NAME_SHORT, L"CheckUpdateAtStartup", 1))
				_beginthreadex(NULL, 0, &CheckUpdates, (LPVOID)1, 0, NULL);

			// Check command line
			if(wcsstr(GetCommandLine(), L"/reduct"))
				SendMessage(hwndDlg, WM_COMMAND, MAKELPARAM(IDM_TRAY_REDUCT, 0), 0);

			break;
		}

		case WM_DESTROY:
		{
			// Destroy timer
			KillTimer(hwndDlg, UID);

			// Destroy resources
			if(cfg.hTitleFont)
				DeleteObject(cfg.hTitleFont);

			if(nid.hIcon)
				DestroyIcon(nid.hIcon);

			if(cfg.hLocale)
				FreeLibrary(cfg.hLocale);

			// Destroy tray icon
			if(nid.uID)
				Shell_NotifyIcon(NIM_DELETE, &nid);

			PostQuitMessage(0);

			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			HDC hDC = BeginPaint(hwndDlg, &ps);

			GetClientRect(hwndDlg, &rc);
			rc.top = rc.bottom - 43;

			// Instead FillRect
			COLORREF clrOld = SetBkColor(hDC, GetSysColor(COLOR_BTNFACE));
			ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
			SetBkColor(hDC, clrOld);

			// Draw Line
			for(int i = 0; i < rc.right; i++)
				SetPixel(hDC, i, rc.top, GetSysColor(COLOR_BTNSHADOW));

			EndPaint(hwndDlg, &ps);

			break;
		}
		
		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORDLG:
		{
			return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lParam;

			switch(nmlp->code)
			{
				case NM_CUSTOMDRAW:
				{
					LONG lResult = CDRF_DODEFAULT;
					LPNMLVCUSTOMDRAW lpnmlv = (LPNMLVCUSTOMDRAW)lParam;

					switch(lpnmlv->nmcd.dwDrawStage)
					{
						case CDDS_PREPAINT:
						{
							lResult = CDRF_NOTIFYITEMDRAW;
							break;
						}

						case CDDS_ITEMPREPAINT:
						{
							lResult = CDRF_NOTIFYSUBITEMDRAW;
							break;
						}

						case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
						{
							if(lpnmlv->nmcd.hdr.idFrom == IDC_MONITOR && lpnmlv->iSubItem == 1)
							{
								iBuffer = ini.read(APP_NAME_SHORT, L"ColorIndicationListView", 1);

								if(iBuffer && (UINT)lpnmlv->nmcd.lItemlParam >= cfg.uDangerLevel)
									lpnmlv->clrText = ini.read(APP_NAME_SHORT, L"LevelDangerClr", COLOR_LEVEL_DANGER);

								else if(iBuffer && (UINT)lpnmlv->nmcd.lItemlParam >= cfg.uWarningLevel)
									lpnmlv->clrText = ini.read(APP_NAME_SHORT, L"LevelWarningClr", COLOR_LEVEL_WARNING);

								else
									lpnmlv->clrText = ini.read(APP_NAME_SHORT, L"ListViewTextClr", COLOR_LISTVIEW_TEXT);

								lResult = CDRF_NEWFONT;
							}

							break;
						}
					}

					SetWindowLongPtr(hwndDlg, DWL_MSGRESULT, lResult);
					return TRUE;
				}
			}

			break;
		}

		case WM_SIZE:
		{
			if(wParam == SIZE_MINIMIZED)
				ToggleVisible(hwndDlg);

			break;
		}

		case WM_SYSCOMMAND:
		{
			if(wParam == IDM_ABOUT)
			{
				SendMessage(hwndDlg, WM_COMMAND, MAKELPARAM(IDM_ABOUT, 0), 0);
			}
			else if(wParam == SC_CLOSE)
			{
				ToggleVisible(hwndDlg);
				return TRUE;
			}

			break;
		}

		case WM_TIMER:
		{
			if(wParam == UID)
			{		
				MEMORY_USAGE mu = {0};

				iBuffer = GetMemoryUsage(&mu);

				// Destroy tray icon
				if(nid.hIcon)
					DestroyIcon(nid.hIcon);

				// Refresh tray info
				nid.uFlags = NIF_ICON | NIF_TIP;
				StringCchPrintf(nid.szTip, _countof(nid.szTip), ls(cfg.hLocale, IDS_TRAY_TOOLTIP), mu.dwPercentPhys, mu.dwPercentPageFile, mu.dwPercentSystemWorkingSet);
				nid.hIcon = CreateMemIcon(iBuffer);

				Shell_NotifyIcon(NIM_MODIFY, &nid);

				// Auto-reduction
				if(cfg.bAdminPrivilege && cfg.bAutoReduct && mu.dwPercentPhys >= cfg.uAutoReductPercents)
				{
					MemReduct(NULL, TRUE);

					if(ini.read(APP_NAME_SHORT, L"BalloonShow", 1) && ini.read(APP_NAME_SHORT, L"BalloonAutoReduct", 1))
					{
						GetMemoryUsage(&mu);

						buffer.Format(ls(cfg.hLocale, IDS_BALLOON_AUTOREDUCT), cfg.uAutoReductPercents, mu.dwPercentPhys);
						ShowBalloonTip(NIIF_INFO, APP_NAME, buffer);
					}
				}

				// Show balloon tips
				if(ini.read(APP_NAME_SHORT, L"BalloonShow", 1))
				{
					if(mu.dwPercentPhys >= cfg.uDangerLevel && ini.read(APP_NAME_SHORT, L"BalloonDangerLevel", 1))
						ShowBalloonTip(NIIF_ERROR, APP_NAME, ls(cfg.hLocale, IDS_BALLOON_DANGER_LEVEL));

					else if(mu.dwPercentPhys >= cfg.uWarningLevel && ini.read(APP_NAME_SHORT, L"BalloonWarningLevel", 0))
						ShowBalloonTip(NIIF_WARNING, APP_NAME, ls(cfg.hLocale, IDS_BALLOON_WARNING_LEVEL));
				}

				if(IsWindowVisible(hwndDlg))
				{
					// Physical memory
					buffer.Format(L"%d%%", mu.dwPercentPhys);

					Lv_InsertItem(hwndDlg, IDC_MONITOR, buffer, 0, 1, -1, -1, mu.dwPercentPhys);
					Lv_InsertItem(hwndDlg, IDC_MONITOR, number_format(mu.ullFreePhys, cfg.szUnit), 1, 1, -1, -1, mu.dwPercentPhys);
					Lv_InsertItem(hwndDlg, IDC_MONITOR, number_format(mu.ullTotalPhys, cfg.szUnit), 2, 1, -1, -1, mu.dwPercentPhys);

					// Pagefile memory
					buffer.Format(L"%d%%", mu.dwPercentPageFile);

					Lv_InsertItem(hwndDlg, IDC_MONITOR, buffer, 3, 1, -1, -1, mu.dwPercentPageFile);
					Lv_InsertItem(hwndDlg, IDC_MONITOR, number_format(mu.ullFreePageFile, cfg.szUnit), 4, 1, -1, -1, mu.dwPercentPageFile);
					Lv_InsertItem(hwndDlg, IDC_MONITOR, number_format(mu.ullTotalPageFile, cfg.szUnit), 5, 1, -1, -1, mu.dwPercentPageFile);

					// System working set
					buffer.Format(L"%d%%", mu.dwPercentSystemWorkingSet);

					Lv_InsertItem(hwndDlg, IDC_MONITOR, buffer, 6, 1, -1, -1, mu.dwPercentSystemWorkingSet);
					Lv_InsertItem(hwndDlg, IDC_MONITOR, number_format(mu.ullFreeSystemWorkingSet, cfg.szUnit), 7, 1, -1, -1, mu.dwPercentSystemWorkingSet);
					Lv_InsertItem(hwndDlg, IDC_MONITOR, number_format(mu.ullTotalSystemWorkingSet, cfg.szUnit), 8, 1, -1, -1, mu.dwPercentSystemWorkingSet);
				}
			}

			break;
		}

		case WM_TRAYICON:
		{
			switch(LOWORD(lParam))
			{
				case WM_LBUTTONDBLCLK:
				{
					switch(ini.read(APP_NAME_SHORT, L"OnDoubleClick", 0))
					{
						case 1:
						{
							ShellExecute(hwndDlg, 0, L"taskmgr.exe", NULL, NULL, 0);
							break;
						}
							
						case 2:
						{
							SendMessage(hwndDlg, WM_COMMAND, MAKELPARAM(IDM_TRAY_REDUCT, 0), 0);
							break;
						}

						case 3:
						{
							MemReduct(hwndDlg, FALSE);
							break;
						}

						default:
						{
							SendMessage(hwndDlg, WM_COMMAND, MAKELPARAM(IDM_TRAY_SHOW, 0), 0);
							break;
						}
					}

					break;
				}

				case WM_RBUTTONUP:
				case WM_CONTEXTMENU:
				{
					// Load menu
					HMENU hMenu = LoadMenu(cfg.hLocale, MAKEINTRESOURCE(IDM_TRAY)), hSubMenu = GetSubMenu(hMenu, 0);

					// Set default menu item
					if(ini.read(APP_NAME_SHORT, L"OnDoubleClick", 0) == 0)
						SetMenuDefaultItem(hSubMenu, IDM_TRAY_SHOW, 0);

					// Indicate (SHOW/HIDE) Action
					if(IsWindowVisible(hwndDlg))
					{
						MENUITEMINFO mii = {0};

						mii.cbSize = sizeof(mii);
						mii.fMask = MIIM_STRING;

						buffer = ls(cfg.hLocale, IDS_TRAY_HIDE);

						mii.dwTypeData = buffer.GetBuffer();
						mii.cch = buffer.GetLength();

						SetMenuItemInfo(hSubMenu, IDM_TRAY_SHOW, FALSE, &mii);
					}

					if(cfg.bUnderUAC)
						SetMenuItemShield(hSubMenu, IDM_TRAY_REDUCT, FALSE);

					// Switch window to foreground
					SetForegroundWindow(hwndDlg);

					// Get cursor position
					POINT pt = {0};
					GetCursorPos(&pt);

					// Show menu
					TrackPopupMenuEx(hSubMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_LEFTBUTTON | TPM_NOANIMATION, pt.x, pt.y, hwndDlg, NULL);

					// Destroy menu
					DestroyMenu(hMenu);
					DestroyMenu(hSubMenu);

					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case IDM_TRAY_EXIT:
				case IDM_EXIT:
				{
					DestroyWindow(hwndDlg);
					break;
				}

				case IDCANCEL: // process Esc key
				case IDM_TRAY_SHOW:
				{
					ToggleVisible(hwndDlg);
					break;
				}

				case IDM_TRAY_REDUCT:
				case IDC_REDUCT:
				{
					if(cfg.bUnderUAC)
					{
						GetModuleFileName(NULL, buffer.GetBuffer(MAX_PATH), MAX_PATH);
						buffer.ReleaseBuffer();

						if(RunElevated(hwndDlg, buffer, L"/reduct"))
							DestroyWindow(hwndDlg);

						else
							ShowBalloonTip(NIIF_ERROR, APP_NAME, ls(cfg.hLocale, IDS_UAC_WARNING));

						return FALSE;
					}

					DialogBox(cfg.hLocale, MAKEINTRESOURCE(IDD_REDUCT), hwndDlg, ReductDlgProc);

					break;
				}

				case IDM_TRAY_SETTINGS:
				case IDM_SETTINGS:
				{
					DialogBox(cfg.hLocale, MAKEINTRESOURCE(IDD_SETTINGS), hwndDlg, SettingsDlgProc);
					break;
				}
				
				case IDM_TRAY_WEBSITE:
				case IDM_WEBSITE:
				{
					ShellExecute(hwndDlg, 0, APP_WEBSITE, NULL, NULL, 0);
					break;
				}

				case IDM_CHECK_UPDATES:
				{
					_beginthreadex(NULL, 0, &CheckUpdates, 0, 0, NULL);
					break;
				}

				case IDM_TRAY_ABOUT:
				case IDM_ABOUT:
				{
					buffer.Format(ls(cfg.hLocale, IDS_COPYRIGHT), APP_WEBSITE, APP_HOST);
					AboutBoxCreate(hwndDlg, MAKEINTRESOURCE(IDI_MAIN), ls(cfg.hLocale, IDS_ABOUT), APP_NAME L" " APP_VERSION, L"Copyright © 2013 Henry++\r\nAll Rights Reversed\r\n\r\n" + buffer, L"<a href=\"" APP_WEBSITE L"\">" APP_HOST L"</a>");

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

INT APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nShowCmd)
{
	CString buffer;

	MSG msg = {0};
	INITCOMMONCONTROLSEX icex = {0};

	// Set current dir
	GetModuleFileName(NULL, buffer.GetBuffer(MAX_PATH), MAX_PATH);
	buffer.ReleaseBuffer();

	PathRemoveFileSpec(buffer.GetBuffer(MAX_PATH));
	buffer.ReleaseBuffer();

	cfg.szCurrentDir = buffer;

	// Generate ini path
	if(!FileExists((buffer = cfg.szCurrentDir + L"\\" APP_NAME_SHORT + L".cfg")))
	{
		ExpandEnvironmentStrings(L"%APPDATA%\\" APP_AUTHOR L"\\" APP_NAME, buffer.GetBuffer(MAX_PATH), MAX_PATH);
		buffer.ReleaseBuffer();

		SHCreateDirectoryEx(NULL, buffer, NULL);
		buffer.Append(+ L"\\" APP_NAME_SHORT L".cfg");
	}

	// Set ini path
	ini.set_path(buffer);

	// Load language
	buffer.Format(L"%s\\Languages\\%s.dll", cfg.szCurrentDir, ini.read(APP_NAME_SHORT, L"Language", MAX_PATH, 0));

	if(FileExists(buffer))
		cfg.hLocale = LoadLanguage(buffer, APP_VERSION, &cfg.dwLanguageId);

	// Initialize and create window
	icex.dwSize = sizeof(icex);
	icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;

	if(!InitCommonControlsEx(&icex) || !CreateDialog(cfg.hLocale, MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC)DlgProc))
		return FALSE;

	while(GetMessage(&msg, NULL, 0, 0))
	{
		if(!IsDialogMessage(cfg.hWnd, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	
	return msg.wParam;
}