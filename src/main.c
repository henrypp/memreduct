// Mem Reduct
// Copyright (c) 2011-2025 Henry++

#include "routine.h"

#include "main.h"
#include "rapp.h"

#include "resource.h"

STATIC_DATA config = {0};

ULONG limits_arr[13] = {0};
ULONG intervals_arr[13] = {0};

INT WINAPIV compare_numbers (
	_In_opt_ PVOID context,
	_In_ LPCVOID ptr1,
	_In_ LPCVOID ptr2
)
{
	ULONG val1;
	ULONG val2;

	val1 = PtrToUlong (ptr1);
	val2 = PtrToUlong (ptr2);

	if (val1 < val2)
		return -1;

	if (val1 > val2)
		return 1;

	return 0;
}

VOID _app_generate_array (
	_Out_ _Writable_elements_ (count) PULONG integers,
	_In_ ULONG_PTR count,
	_In_ ULONG value
)
{
	PR_HASHTABLE hashtable;
	ULONG_PTR enum_key = 0;
	ULONG hash_code;
	ULONG index = 0;

	RtlSecureZeroMemory (integers, sizeof (ULONG) * count);

	hashtable = _r_obj_createhashtable (sizeof (BOOLEAN), 16, NULL);

	for (index = 1; index < 9; index++)
	{
		_r_obj_addhashtableitem (hashtable, index * 10, NULL);
	}

	for (index = value - 2; index <= (value + 2); index++)
	{
		if (index >= 5)
			_r_obj_addhashtableitem (hashtable, index, NULL);
	}

	while (_r_obj_enumhashtable (hashtable, NULL, &hash_code, &enum_key))
	{
		if (hash_code <= 99)
			*(PULONG_PTR)PTR_ADD_OFFSET (integers, index * sizeof (ULONG)) = hash_code;

		if (++index >= count)
			break;
	}

	qsort_s (integers, count, sizeof (ULONG), &compare_numbers, NULL);

	_r_obj_dereference (hashtable);
}

VOID _app_generate_menu (
	_In_ HMENU hsubmenu,
	_In_ ULONG menu_idx,
	_Out_ _Writable_elements_ (count) PULONG integers,
	_In_ ULONG_PTR count,
	_In_ LPCWSTR format,
	_In_ ULONG value,
	_In_ BOOLEAN is_enabled
)
{
	WCHAR buffer[64];
	LONG64 menu_value;
	ULONG menu_items = 0;
	ULONG menu_id;
	BOOLEAN is_checked = FALSE;

	_r_menu_setitemtext (hsubmenu, 0, TRUE, _r_locale_getstring (IDS_TRAY_DISABLE));

	_app_generate_array (integers, count, value);

	for (ULONG i = 0; i < count; i++)
	{
		menu_value = integers[i];

		if (!menu_value)
			continue;

		menu_id = menu_idx + i;

		_r_str_printf (buffer, RTL_NUMBER_OF (buffer), format, menu_value);

		_r_menu_additem (hsubmenu, menu_id, buffer);

		if (!_r_sys_iselevated ())
			_r_menu_enableitem (hsubmenu, menu_id, FALSE, FALSE);

		if (value == menu_value)
		{
			_r_menu_checkitem (hsubmenu, menu_id, menu_id, MF_BYCOMMAND, menu_id);

			is_checked = TRUE;
		}

		menu_items += 1;
	}

	if (!is_enabled || !is_checked)
		_r_menu_checkitem (hsubmenu, 0, menu_items + 2, MF_BYPOSITION, 0);
}

ULONG _app_getlimitvalue ()
{
	ULONG value;

	value = _r_config_getulong (L"AutoreductValue", DEFAULT_AUTOREDUCT_VAL, NULL);

	return _r_calc_clamp (value, 1, 99);
}

ULONG _app_getintervalvalue ()
{
	ULONG value;

	value = _r_config_getulong (L"AutoreductIntervalValue", DEFAULT_AUTOREDUCTINTERVAL_VAL, NULL);

	return _r_calc_clamp (value, 1, 1440);
}

ULONG _app_getdangervalue ()
{
	ULONG value;

	value = _r_config_getulong (L"TrayLevelDanger", DEFAULT_DANGER_LEVEL, NULL);

	return _r_calc_clamp (value, 1, 99);
}

ULONG _app_getwarningvalue ()
{
	ULONG value;

	value = _r_config_getulong (L"TrayLevelWarning", DEFAULT_WARNING_LEVEL, NULL);

	return _r_calc_clamp (value, 1, 99);
}

ULONG64 _app_getmemoryinfo (
	_Out_ PR_MEMORY_INFO mem_info
)
{
	_r_sys_getmemoryinfo (mem_info);

	return mem_info->physical_memory.used_bytes;
}

FORCEINLINE LPCWSTR _app_getcleanupreason (
	_In_ CLEANUP_SOURCE_ENUM src
)
{
	switch (src)
	{
		case SOURCE_AUTO:
		{
			return L"Cleanup (Auto)";
		}

		case SOURCE_MANUAL:
		{
			return L"Cleanup (Manual)";
		}

		case SOURCE_HOTKEY:
		{
			return L"Cleanup (Hotkey)";
		}

		case SOURCE_CMDLINE:
		{
			return L"Cleanup (Command-line)";
		}

		default:
		{
			return L"Unknown";
		}
	}
}

NTSTATUS _app_flushvolumecache ()
{
	PMOUNTMGR_MOUNT_POINTS object_mountpoints;
	PMOUNTMGR_MOUNT_POINT mountpoint;
	OBJECT_ATTRIBUTES oa = {0};
	IO_STATUS_BLOCK isb;
	UNICODE_STRING us;
	HANDLE hdevice;
	HANDLE hvolume;
	NTSTATUS status;

	RtlInitUnicodeString (&us, MOUNTMGR_DEVICE_NAME);

	InitializeObjectAttributes (&oa, &us, OBJ_CASE_INSENSITIVE, NULL, NULL);

	status = NtCreateFile (
		&hdevice,
		FILE_READ_ATTRIBUTES | SYNCHRONIZE,
		&oa,
		&isb,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_OPEN,
		FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
		NULL,
		0
	);

	if (!NT_SUCCESS (status))
		return status;

	status = _r_fs_getvolumemountpoints (hdevice, &object_mountpoints);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	for (ULONG i = 0; i < object_mountpoints->NumberOfMountPoints; i++)
	{
		mountpoint = &object_mountpoints->MountPoints[i];

		us.Length = mountpoint->SymbolicLinkNameLength;
		us.MaximumLength = mountpoint->SymbolicLinkNameLength + sizeof (UNICODE_NULL);
		us.Buffer = PTR_ADD_OFFSET (object_mountpoints, mountpoint->SymbolicLinkNameOffset);

		if (MOUNTMGR_IS_VOLUME_NAME (&us)) // \\??\\Volume{1111-2222}
		{
			InitializeObjectAttributes (&oa, &us, OBJ_CASE_INSENSITIVE, NULL, NULL);

			status = NtCreateFile (
				&hvolume,
				FILE_WRITE_DATA | SYNCHRONIZE,
				&oa,
				&isb,
				NULL,
				FILE_ATTRIBUTE_NORMAL,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				FILE_OPEN,
				FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
				NULL,
				0
			);

			if (NT_SUCCESS (status))
			{
				status = _r_fs_flushfile (hvolume);

				NtClose (hvolume);
			}
		}
	}

	_r_mem_free (object_mountpoints);

CleanupExit:

	NtClose (hdevice);

	return status;
}

VOID _app_memoryclean (
	_In_opt_ HWND hwnd,
	_In_ CLEANUP_SOURCE_ENUM src,
	_In_opt_ ULONG mask
)
{
	MEMORY_COMBINE_INFORMATION_EX combine_info_ex = {0};
	SYSTEM_FILECACHE_INFORMATION sfci = {0};
	SYSTEM_MEMORY_LIST_COMMAND command;
	R_MEMORY_INFO mem_info;
	WCHAR buffer1[256] = {0};
	WCHAR buffer2[256] = {0};
	LPCWSTR error_text;
	ULONG64 reduct_before;
	ULONG64 reduct_after;
	ULONG flags = NIIF_WARNING;
	NTSTATUS status;

	if (!_r_config_getboolean (L"IsNotificationsSound", TRUE, NULL))
		flags |= NIIF_NOSOUND;

	if (!_r_sys_iselevated ())
	{
		error_text = _r_locale_getstring (IDS_STATUS_NOPRIVILEGES);

		if (_r_app_runasadmin ())
		{
			if (hwnd)
				DestroyWindow (hwnd);
		}
		else
		{
			if (src == SOURCE_CMDLINE)
			{
				if (hwnd)
					_r_show_message (hwnd, MB_OK | MB_ICONSTOP, NULL, error_text);
			}
			else
			{
				if (hwnd)
					_r_tray_popup (hwnd, &GUID_TrayIcon, flags, _r_app_getname (), error_text);
			}
		}

		return;
	}

	if (!mask)
		mask = _r_config_getulong (L"ReductMask2", REDUCT_MASK_DEFAULT, NULL);

	if (src == SOURCE_AUTO)
	{
		if (!_r_config_getboolean (L"IsAllowStandbyListCleanup", FALSE, NULL))
			mask &= ~REDUCT_MASK_FREEZES; // exclude freezes from autoclean feature ;)
	}
	else if (src == SOURCE_MANUAL)
	{
		if ((mask & REDUCT_WORKING_SET) == REDUCT_WORKING_SET)
			_r_str_append (buffer1, RTL_NUMBER_OF (buffer1), L"- " TITLE_WORKINGSET L"\r\n");

		if ((mask & REDUCT_SYSTEM_FILE_CACHE) == REDUCT_SYSTEM_FILE_CACHE)
			_r_str_append (buffer1, RTL_NUMBER_OF (buffer1), L"- " TITLE_SYSTEMFILECACHE L"\r\n");

		if ((mask & REDUCT_MODIFIED_FILE_CACHE) == REDUCT_MODIFIED_FILE_CACHE)
			_r_str_append (buffer1, RTL_NUMBER_OF (buffer1), L"- " TITLE_MODIFIEDFILECACHE L"\r\n");

		if ((mask & REDUCT_MODIFIED_LIST) == REDUCT_MODIFIED_LIST)
			_r_str_append (buffer1, RTL_NUMBER_OF (buffer1), L"- " TITLE_MODIFIEDLIST L"\r\n");

		if ((mask & REDUCT_STANDBY_LIST) == REDUCT_STANDBY_LIST)
			_r_str_append (buffer1, RTL_NUMBER_OF (buffer1), L"- " TITLE_STANDBYLIST L"\r\n");

		if ((mask & REDUCT_STANDBY_PRIORITY0_LIST) == REDUCT_STANDBY_PRIORITY0_LIST)
			_r_str_append (buffer1, RTL_NUMBER_OF (buffer1), L"- " TITLE_STANDBYLISTPRIORITY0 L"\r\n");

		if ((mask & REDUCT_REGISTRY_CACHE) == REDUCT_REGISTRY_CACHE)
			_r_str_append (buffer1, RTL_NUMBER_OF (buffer1), L"- " TITLE_REGISTRYCACHE L"\r\n");

		if ((mask & REDUCT_COMBINE_MEMORY_LISTS) == REDUCT_COMBINE_MEMORY_LISTS)
			_r_str_append (buffer1, RTL_NUMBER_OF (buffer1), L"- " TITLE_COMBINEMEMORYLISTS L"\r\n");

		StrTrimW (buffer1, L"\r\n");

		if (!_r_show_confirmmessage (hwnd, _r_locale_getstring (IDS_QUESTION), buffer1, L"IsShowReductConfirmation", FALSE))
			return;
	}

	SetCursor (LoadCursorW (NULL, IDC_WAIT));

	// difference (before)
	reduct_before = _app_getmemoryinfo (&mem_info);

	// Working set (vista+)
	if ((mask & REDUCT_WORKING_SET) == REDUCT_WORKING_SET)
	{
		command = MemoryEmptyWorkingSets;

		status = NtSetSystemInformation (SystemMemoryListInformation, &command, sizeof (SYSTEM_MEMORY_LIST_COMMAND));

		if (!NT_SUCCESS (status))
			_r_log (LOG_LEVEL_ERROR, NULL, L"NtSetSystemInformation", status, L"MemoryEmptyWorkingSets");
	}

	// System file cache
	if ((mask & REDUCT_SYSTEM_FILE_CACHE) == REDUCT_SYSTEM_FILE_CACHE)
	{
		sfci.MinimumWorkingSet = MAXSIZE_T;
		sfci.MaximumWorkingSet = MAXSIZE_T;

		status = NtSetSystemInformation (SystemFileCacheInformationEx, &sfci, sizeof (SYSTEM_FILECACHE_INFORMATION));

		if (!NT_SUCCESS (status))
			_r_log (LOG_LEVEL_ERROR, NULL, L"NtSetSystemInformation", status, L"SystemFileCacheInformation");
	}

	// Modified page list (vista+)
	if ((mask & REDUCT_MODIFIED_LIST) == REDUCT_MODIFIED_LIST)
	{
		command = MemoryFlushModifiedList;

		status = NtSetSystemInformation (SystemMemoryListInformation, &command, sizeof (SYSTEM_MEMORY_LIST_COMMAND));

		if (!NT_SUCCESS (status))
			_r_log (LOG_LEVEL_ERROR, NULL, L"NtSetSystemInformation", status, L"MemoryFlushModifiedList");
	}

	// Standby list (vista+)
	if ((mask & REDUCT_STANDBY_LIST) == REDUCT_STANDBY_LIST)
	{
		command = MemoryPurgeStandbyList;

		status = NtSetSystemInformation (SystemMemoryListInformation, &command, sizeof (SYSTEM_MEMORY_LIST_COMMAND));

		if (!NT_SUCCESS (status))
			_r_log (LOG_LEVEL_ERROR, NULL, L"NtSetSystemInformation", status, L"MemoryPurgeStandbyList");
	}

	// Standby priority-0 list (vista+)
	if ((mask & REDUCT_STANDBY_PRIORITY0_LIST) == REDUCT_STANDBY_PRIORITY0_LIST)
	{
		command = MemoryPurgeLowPriorityStandbyList;

		status = NtSetSystemInformation (SystemMemoryListInformation, &command, sizeof (SYSTEM_MEMORY_LIST_COMMAND));

		if (!NT_SUCCESS (status))
			_r_log (LOG_LEVEL_ERROR, NULL, L"NtSetSystemInformation", status, L"MemoryPurgeLowPriorityStandbyList");
	}

	// Flush volume cache
	if ((mask & REDUCT_MODIFIED_FILE_CACHE) == REDUCT_MODIFIED_FILE_CACHE)
		_app_flushvolumecache ();

	// Flush registry cache (win8.1+)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_8_1))
	{
		if ((mask & REDUCT_REGISTRY_CACHE) == REDUCT_REGISTRY_CACHE)
		{
			status = NtSetSystemInformation (SystemRegistryReconciliationInformation, NULL, 0);

			if (!NT_SUCCESS (status))
				_r_log (LOG_LEVEL_ERROR, NULL, L"NtSetSystemInformation", status, L"SystemRegistryReconciliationInformation");
		}
	}

	// Combine memory lists (win10+)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_10))
	{
		if ((mask & REDUCT_COMBINE_MEMORY_LISTS) == REDUCT_COMBINE_MEMORY_LISTS)
		{
			status = NtSetSystemInformation (SystemCombinePhysicalMemoryInformation, &combine_info_ex, sizeof (MEMORY_COMBINE_INFORMATION_EX));

			if (!NT_SUCCESS (status))
				_r_log (LOG_LEVEL_ERROR, NULL, L"NtSetSystemInformation", status, L"SystemCombinePhysicalMemoryInformation");
		}
	}

	SetCursor (LoadCursorW (NULL, IDC_ARROW));

	// difference (after)
	reduct_after = _app_getmemoryinfo (&mem_info);

	if (reduct_after < reduct_before)
	{
		reduct_after = (reduct_before - reduct_after);
	}
	else
	{
		reduct_after = 0;
	}

	// time of last cleaning
	_r_config_setlong64 (L"StatisticLastReduct", _r_unixtime_now (), NULL);

	_r_format_bytesize64 (buffer1, RTL_NUMBER_OF (buffer1), reduct_after);

	_r_str_printf (buffer2, RTL_NUMBER_OF (buffer2), _r_locale_getstring (IDS_STATUS_CLEANED), buffer1);

	if (src == SOURCE_CMDLINE)
	{
		_r_show_message (hwnd, MB_OK | MB_ICONINFORMATION, NULL, buffer2);
	}
	else
	{
		if (hwnd && _r_config_getboolean (L"BalloonCleanResults", TRUE, NULL))
			_r_tray_popup (hwnd, &GUID_TrayIcon, flags, _r_app_getname (), buffer2);
	}

	if (_r_config_getboolean (L"LogCleanResults", FALSE, NULL))
		_r_log_v (LOG_LEVEL_INFO, 0, _app_getcleanupreason (src), 0, buffer1);
}

VOID _app_fontinit (
	_Out_ PLOGFONT logfont,
	_In_ LONG dpi_value
)
{
	RtlZeroMemory (logfont, sizeof (LOGFONT));

	_r_str_copy (logfont->lfFaceName, LF_FACESIZE, L"Lucida Console");

	logfont->lfHeight = _r_dc_fontsizetoheight (8, dpi_value);
	logfont->lfWeight = FW_NORMAL;

	_r_config_getfont (L"TrayFont", logfont, dpi_value, NULL);

	logfont->lfCharSet = DEFAULT_CHARSET;
	logfont->lfQuality = CLEARTYPE_QUALITY;
}

VOID _app_drawbackground (
	_In_ HDC hdc,
	_In_ COLORREF bg_clr,
	_In_ COLORREF pen_clr,
	_In_ COLORREF brush_clr,
	_In_ LPCRECT rect,
	_In_ BOOLEAN is_round
)
{
	HGDIOBJ prev_brush;
	HGDIOBJ prev_pen;
	COLORREF prev_clr;

	prev_brush = SelectObject (hdc, GetStockObject (DC_BRUSH));
	prev_pen = SelectObject (hdc, GetStockObject (DC_PEN));

	prev_clr = SetBkColor (hdc, bg_clr);

	SetDCPenColor (hdc, pen_clr);
	SetDCBrushColor (hdc, brush_clr);

	_r_dc_fillrect (hdc, rect, bg_clr);

	if (is_round)
	{
		RoundRect (hdc, rect->left, rect->top, rect->right, rect->bottom, rect->right - 2, rect->right / 2);
	}
	else
	{
		Rectangle (hdc, rect->left, rect->top, rect->right, rect->bottom);
	}

	SelectObject (hdc, prev_brush);
	SelectObject (hdc, prev_pen);

	SetBkColor (hdc, prev_clr);
}

HICON _app_iconcreate (
	_In_opt_ ULONG percent
)
{
	static HICON hicon = NULL;

	R_MEMORY_INFO mem_info;
	R_STRINGREF sr;
	ICONINFO ii = {0};
	WCHAR icon_text[8];
	HGDIOBJ prev_font;
	HGDIOBJ prev_bmp;
	HICON hicon_new;
	COLORREF text_color;
	COLORREF bg_color;
	LONG prev_mode;
	BOOLEAN is_transparent;
	BOOLEAN is_border;
	BOOLEAN is_round;
	BOOLEAN has_warning;
	BOOLEAN has_danger;

	text_color = _r_config_getulong (L"TrayColorText", TRAY_COLOR_TEXT, NULL);
	bg_color = _r_config_getulong (L"TrayColorBg", TRAY_COLOR_BG, NULL);

	is_transparent = _r_config_getboolean (L"TrayUseTransparency", FALSE, NULL);
	is_border = _r_config_getboolean (L"TrayShowBorder", FALSE, NULL);
	is_round = _r_config_getboolean (L"TrayRoundCorners", FALSE, NULL);

	if (!percent)
	{
		_app_getmemoryinfo (&mem_info);

		percent = mem_info.physical_memory.percent;
	}

	has_danger = percent >= _app_getdangervalue ();
	has_warning = !has_danger && percent >= _app_getwarningvalue ();

	if (has_danger || has_warning)
	{
		if (_r_config_getboolean (L"TrayChangeBg", TRUE, NULL))
		{
			if (has_danger)
			{
				bg_color = _r_config_getulong (L"TrayColorDanger", TRAY_COLOR_DANGER, NULL);
			}
			else
			{
				bg_color = _r_config_getulong (L"TrayColorWarning", TRAY_COLOR_WARNING, NULL);
			}

			is_transparent = FALSE;
		}
		else
		{
			if (has_danger)
			{
				text_color = _r_config_getulong (L"TrayColorDanger", TRAY_COLOR_DANGER, NULL);
			}
			else
			{
				text_color = _r_config_getulong (L"TrayColorWarning", TRAY_COLOR_WARNING, NULL);
			}
		}
	}

	// set tray text
	_r_str_fromulong (icon_text, RTL_NUMBER_OF (icon_text), percent);

	_r_obj_initializestringref (&sr, icon_text);

	// draw main device context
	prev_bmp = SelectObject (config.hdc, config.hbitmap);
	prev_font = SelectObject (config.hdc, config.hfont);
	prev_mode = SetBkMode (config.hdc, TRANSPARENT);

	_app_drawbackground (config.hdc, bg_color, is_border ? text_color : bg_color, is_transparent ? text_color : bg_color, &config.icon_size, is_round);

	_r_dc_drawtext (NULL, config.hdc, &sr, &config.icon_size, 0, 0, DT_VCENTER | DT_CENTER | DT_SINGLELINE | DT_NOCLIP | DT_NOPREFIX, text_color);

	SetBkMode (config.hdc, prev_mode);

	SelectObject (config.hdc, prev_font);
	SelectObject (config.hdc, prev_bmp);

	// draw mask device context
	prev_bmp = SelectObject (config.hdc_mask, config.hbitmap_mask);
	prev_font = SelectObject (config.hdc_mask, config.hfont);
	prev_mode = SetBkMode (config.hdc_mask, TRANSPARENT);

	_app_drawbackground (
		config.hdc_mask,
		TRAY_COLOR_WHITE,
		is_border ? TRAY_COLOR_BLACK : TRAY_COLOR_WHITE,
		is_transparent ? TRAY_COLOR_WHITE : TRAY_COLOR_BLACK,
		&config.icon_size,
		is_round
	);

	_r_dc_drawtext (NULL, config.hdc_mask, &sr, &config.icon_size, 0, 0, DT_VCENTER | DT_CENTER | DT_SINGLELINE | DT_NOCLIP | DT_NOPREFIX, TRAY_COLOR_BLACK);

	SetBkMode (config.hdc, prev_mode);

	SelectObject (config.hdc_mask, prev_bmp);
	SelectObject (config.hdc_mask, prev_font);

	// create icon
	ii.fIcon = TRUE;
	ii.hbmColor = config.hbitmap;
	ii.hbmMask = config.hbitmap_mask;

	hicon_new = CreateIconIndirect (&ii);

	if (hicon)
		DestroyIcon (hicon);

	hicon = hicon_new;

	return hicon;
}

VOID CALLBACK _app_timercallback (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ UINT_PTR id_event,
	_In_ ULONG time
)
{
	R_MEMORY_INFO mem_info;
	WCHAR buffer[128];
	HICON hicon = NULL;
	LONG64 timestamp;
	ULONG percent;
	BOOLEAN is_clean = FALSE;

	if (id_event != UID)
		return;

	_app_getmemoryinfo (&mem_info);

	// autocleanup functional
	if (_r_sys_iselevated ())
	{
		if (_r_config_getboolean (L"AutoreductEnable", FALSE, NULL))
		{
			if (mem_info.physical_memory.percent >= _app_getlimitvalue ())
				is_clean = TRUE;
		}

		if (!is_clean && _r_config_getboolean (L"AutoreductIntervalEnable", FALSE, NULL))
		{
			timestamp = _r_unixtime_now () - _r_config_getlong64 (L"StatisticLastReduct", 0, NULL);

			if (timestamp >= (_app_getintervalvalue () * 60))
				is_clean = TRUE;
		}

		if (is_clean)
			_app_memoryclean (hwnd, SOURCE_AUTO, 0);
	}

	// check previous percent to prevent icon redraw
	if (!config.ms_prev || config.ms_prev != mem_info.physical_memory.percent)
	{
		config.ms_prev = mem_info.physical_memory.percent; // store last percentage value (required!)

		hicon = _app_iconcreate (config.ms_prev);
	}

	_r_tray_setinfoformat (
		hwnd,
		&GUID_TrayIcon,
		hicon,
		L"%s: %" TEXT (PR_DOUBLE) L"%%\r\n%s: %" TEXT (PR_DOUBLE) L"%%\r\n%s: %" TEXT (PR_DOUBLE) L"%%",
		_r_locale_getstring (IDS_GROUP_1),
		mem_info.physical_memory.percent_f,
		_r_locale_getstring (IDS_GROUP_2),
		mem_info.page_file.percent_f,
		_r_locale_getstring (IDS_GROUP_3),
		mem_info.system_cache.percent_f
	);

	if (!_r_wnd_isvisible (hwnd, FALSE))
		return;

	// set item lparam information
	for (INT i = 0; i < _r_listview_getitemcount (hwnd, IDC_LISTVIEW); i++)
	{
		if (i < 3)
		{
			percent = mem_info.physical_memory.percent;
		}
		else if (i < 6)
		{
			percent = mem_info.page_file.percent;
		}
		else if (i < 9)
		{
			percent = mem_info.system_cache.percent;
		}

		_r_listview_setitem (hwnd, IDC_LISTVIEW, i, 0, NULL, I_DEFAULT, I_DEFAULT, (LPARAM)percent);
	}

	// physical memory
	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%" TEXT (PR_DOUBLE) L"%%", mem_info.physical_memory.percent_f);
	_r_listview_setitem (hwnd, IDC_LISTVIEW, 0, 1, buffer, I_DEFAULT, I_DEFAULT, I_DEFAULT);

	_r_format_bytesize64 (buffer, RTL_NUMBER_OF (buffer), mem_info.physical_memory.free_bytes);
	_r_listview_setitem (hwnd, IDC_LISTVIEW, 1, 1, buffer, I_DEFAULT, I_DEFAULT, I_DEFAULT);

	_r_format_bytesize64 (buffer, RTL_NUMBER_OF (buffer), mem_info.physical_memory.total_bytes);
	_r_listview_setitem (hwnd, IDC_LISTVIEW, 2, 1, buffer, I_DEFAULT, I_DEFAULT, I_DEFAULT);

	// virtual memory
	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%" TEXT (PR_DOUBLE) L"%%", mem_info.page_file.percent_f);
	_r_listview_setitem (hwnd, IDC_LISTVIEW, 3, 1, buffer, I_DEFAULT, I_DEFAULT, I_DEFAULT);

	_r_format_bytesize64 (buffer, RTL_NUMBER_OF (buffer), mem_info.page_file.free_bytes);
	_r_listview_setitem (hwnd, IDC_LISTVIEW, 4, 1, buffer, I_DEFAULT, I_DEFAULT, I_DEFAULT);

	_r_format_bytesize64 (buffer, RTL_NUMBER_OF (buffer), mem_info.page_file.total_bytes);
	_r_listview_setitem (hwnd, IDC_LISTVIEW, 5, 1, buffer, I_DEFAULT, I_DEFAULT, I_DEFAULT);

	// system cache
	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%" TEXT (PR_DOUBLE) L"%%", mem_info.system_cache.percent_f);
	_r_listview_setitem (hwnd, IDC_LISTVIEW, 6, 1, buffer, I_DEFAULT, I_DEFAULT, I_DEFAULT);

	_r_format_bytesize64 (buffer, RTL_NUMBER_OF (buffer), mem_info.system_cache.free_bytes);
	_r_listview_setitem (hwnd, IDC_LISTVIEW, 7, 1, buffer, I_DEFAULT, I_DEFAULT, I_DEFAULT);

	_r_format_bytesize64 (buffer, RTL_NUMBER_OF (buffer), mem_info.system_cache.total_bytes);
	_r_listview_setitem (hwnd, IDC_LISTVIEW, 8, 1, buffer, I_DEFAULT, I_DEFAULT, I_DEFAULT);

	if (_r_wnd_isvisible (hwnd, FALSE))
		_r_listview_redraw (hwnd, IDC_LISTVIEW);
}

VOID _app_iconredraw (
	_In_opt_ HWND hwnd
)
{
	config.ms_prev = 0;

	if (hwnd)
		_app_timercallback (hwnd, 0, UID, 0);
}

VOID _app_iconinit (
	_In_ LONG dpi_value
)
{
	LOGFONT logfont;
	PVOID bits;
	HDC hdc;

	SAFE_DELETE_OBJECT (config.hbitmap_mask);
	SAFE_DELETE_OBJECT (config.hbitmap);
	SAFE_DELETE_OBJECT (config.hfont);

	SAFE_DELETE_DC (config.hdc_mask);
	SAFE_DELETE_DC (config.hdc);

	// init font
	_app_fontinit (&logfont, dpi_value);

	config.hfont = CreateFontIndirectW (&logfont);

	// init rect
	SetRect (&config.icon_size, 0, 0, _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value), _r_dc_getsystemmetrics (SM_CYSMICON, dpi_value));

	// init dc
	hdc = GetDC (NULL);

	if (!hdc)
		return;

	config.hdc = CreateCompatibleDC (hdc);
	config.hdc_mask = CreateCompatibleDC (hdc);

	// init bitmap
	config.hbitmap = _r_dc_createbitmap (hdc, config.icon_size.right, config.icon_size.bottom, &bits);
	config.hbitmap_mask = CreateBitmap (config.icon_size.right, config.icon_size.bottom, 1, 1, NULL);

	ReleaseDC (NULL, hdc);
}

VOID _app_hotkeyinit (
	_In_ HWND hwnd
)
{
	LONG hk;

	UnregisterHotKey (hwnd, UID);

	if (!_r_config_getboolean (L"HotkeyCleanEnable", FALSE, NULL))
		return;

	hk = _r_config_getlong (L"HotkeyClean", MAKEWORD (VK_F1, HOTKEYF_CONTROL), NULL);

	if (!hk)
		return;

	if (!RegisterHotKey (hwnd, UID, (HIBYTE (hk) & 2) | ((HIBYTE (hk) & 4) >> 2) | ((HIBYTE (hk) & 1) << 2), LOBYTE (hk)))
		_r_show_errormessage (hwnd, NULL, NtLastError (), NULL, ET_WINDOWS);
}

VOID _app_setfontcontrol (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ PLOGFONT logfont,
	_In_ LONG dpi_value
)
{
	_r_ctrl_setstringformat (
		hwnd,
		IDC_FONT,
		L"%s, %" TEXT (PR_LONG) L"px, %" TEXT (PR_LONG),
		logfont->lfFaceName,
		_r_dc_fontheighttosize (logfont->lfHeight, dpi_value),
		logfont->lfWeight
	);
}

INT_PTR CALLBACK SettingsProc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	switch (msg)
	{
		case RM_INITIALIZE:
		{
			INT dialog_id;

			dialog_id = (INT)wparam;

			switch (dialog_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
					_r_ctrl_checkbutton (hwnd, IDC_ALWAYSONTOP_CHK, _r_config_getboolean (L"AlwaysOnTop", FALSE, NULL));
					_r_ctrl_checkbutton (hwnd, IDC_LOADONSTARTUP_CHK, _r_autorun_isenabled ());
					_r_ctrl_checkbutton (hwnd, IDC_STARTMINIMIZED_CHK, _r_config_getboolean (L"IsStartMinimized", FALSE, NULL));
					_r_ctrl_checkbutton (hwnd, IDC_REDUCTCONFIRMATION_CHK, _r_config_getboolean (L"IsShowReductConfirmation", TRUE, NULL));

					if (!_r_sys_iselevated ())
						_r_ctrl_enable (hwnd, IDC_SKIPUACWARNING_CHK, FALSE);

					_r_ctrl_checkbutton (hwnd, IDC_SKIPUACWARNING_CHK, _r_skipuac_isenabled ());
					_r_ctrl_checkbutton (hwnd, IDC_CHECKUPDATES_CHK, _r_update_isenabled (FALSE));

					_r_locale_enum (hwnd, IDC_LANGUAGE, 0);

					break;
				}

				case IDD_SETTINGS_MEMORY:
				{
					ULONG mask;

					_r_listview_setstyle (hwnd, IDC_REGIONS, LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP, FALSE);

					_r_wnd_setcontext (hwnd, IDC_REGIONS, INVALID_HANDLE_VALUE);

					// fix by @XakerTwo
					_r_listview_deleteallcolumns (hwnd, IDC_REGIONS);
					_r_listview_deleteallitems (hwnd, IDC_REGIONS);

					_r_listview_addcolumn (hwnd, IDC_REGIONS, 0, NULL, 10, LVCFMT_LEFT);

					_r_listview_additem (hwnd, IDC_REGIONS, 0, TITLE_WORKINGSET, I_DEFAULT, I_DEFAULT, REDUCT_WORKING_SET);
					_r_listview_additem (hwnd, IDC_REGIONS, 1, TITLE_SYSTEMFILECACHE, I_DEFAULT, I_DEFAULT, REDUCT_SYSTEM_FILE_CACHE);
					_r_listview_additem (hwnd, IDC_REGIONS, 2, TITLE_MODIFIEDLIST, I_DEFAULT, I_DEFAULT, REDUCT_MODIFIED_LIST);
					_r_listview_additem (hwnd, IDC_REGIONS, 3, TITLE_STANDBYLIST, I_DEFAULT, I_DEFAULT, REDUCT_STANDBY_LIST);
					_r_listview_additem (hwnd, IDC_REGIONS, 4, TITLE_STANDBYLISTPRIORITY0, I_DEFAULT, I_DEFAULT, REDUCT_STANDBY_PRIORITY0_LIST);
					_r_listview_additem (hwnd, IDC_REGIONS, 5, TITLE_MODIFIEDFILECACHE, I_DEFAULT, I_DEFAULT, REDUCT_MODIFIED_FILE_CACHE);
					_r_listview_additem (hwnd, IDC_REGIONS, 6, TITLE_REGISTRYCACHE, I_DEFAULT, I_DEFAULT, REDUCT_REGISTRY_CACHE);
					_r_listview_additem (hwnd, IDC_REGIONS, 7, TITLE_COMBINEMEMORYLISTS, I_DEFAULT, I_DEFAULT, REDUCT_COMBINE_MEMORY_LISTS);

					_r_listview_setcolumn (hwnd, IDC_REGIONS, 0, NULL, -100);

					mask = _r_config_getulong (L"ReductMask2", REDUCT_MASK_DEFAULT, NULL);

					_r_listview_setitemcheck (hwnd, IDC_REGIONS, 0, (mask & REDUCT_WORKING_SET) == REDUCT_WORKING_SET);
					_r_listview_setitemcheck (hwnd, IDC_REGIONS, 1, (mask & REDUCT_SYSTEM_FILE_CACHE) == REDUCT_SYSTEM_FILE_CACHE);
					_r_listview_setitemcheck (hwnd, IDC_REGIONS, 2, (mask & REDUCT_MODIFIED_LIST) == REDUCT_MODIFIED_LIST);
					_r_listview_setitemcheck (hwnd, IDC_REGIONS, 3, (mask & REDUCT_STANDBY_LIST) == REDUCT_STANDBY_LIST);
					_r_listview_setitemcheck (hwnd, IDC_REGIONS, 4, (mask & REDUCT_STANDBY_PRIORITY0_LIST) == REDUCT_STANDBY_PRIORITY0_LIST);
					_r_listview_setitemcheck (hwnd, IDC_REGIONS, 5, (mask & REDUCT_MODIFIED_FILE_CACHE) == REDUCT_MODIFIED_FILE_CACHE);
					_r_listview_setitemcheck (hwnd, IDC_REGIONS, 6, (mask & REDUCT_REGISTRY_CACHE) == REDUCT_REGISTRY_CACHE);
					_r_listview_setitemcheck (hwnd, IDC_REGIONS, 7, (mask & REDUCT_COMBINE_MEMORY_LISTS) == REDUCT_COMBINE_MEMORY_LISTS);

					_r_wnd_removecontext (hwnd, IDC_REGIONS);

					if (!_r_sys_iselevated ())
					{
						_r_ctrl_enable (hwnd, IDC_REGIONS, FALSE);
						_r_ctrl_enable (hwnd, IDC_AUTOREDUCTENABLE_CHK, FALSE);
						_r_ctrl_enable (hwnd, IDC_AUTOREDUCTINTERVALENABLE_CHK, FALSE);
						_r_ctrl_enable (hwnd, IDC_HOTKEY_CLEAN_CHK, FALSE);
						_r_ctrl_enable (hwnd, IDC_HOTKEY_CLEAN, FALSE);
					}

					_r_ctrl_checkbutton (hwnd, IDC_AUTOREDUCTENABLE_CHK, _r_config_getboolean (L"AutoreductEnable", FALSE, NULL));

					_r_updown_setrange (hwnd, IDC_AUTOREDUCTVALUE, 1, 99);

					_r_updown_setvalue (hwnd, IDC_AUTOREDUCTVALUE, _app_getlimitvalue ());

					_r_ctrl_checkbutton (hwnd, IDC_AUTOREDUCTINTERVALENABLE_CHK, _r_config_getboolean (L"AutoreductIntervalEnable", FALSE, NULL));

					_r_updown_setrange (hwnd, IDC_AUTOREDUCTINTERVALVALUE, 1, 1440);

					_r_updown_setvalue (hwnd, IDC_AUTOREDUCTINTERVALVALUE, _app_getintervalvalue ());

					_r_ctrl_checkbutton (hwnd, IDC_HOTKEY_CLEAN_CHK, _r_config_getboolean (L"HotkeyCleanEnable", FALSE, NULL));

					if (!_r_ctrl_isbuttonchecked (hwnd, IDC_HOTKEY_CLEAN_CHK))
						_r_ctrl_enable (hwnd, IDC_HOTKEY_CLEAN, FALSE);

					_r_hotkey_set (hwnd, IDC_HOTKEY_CLEAN, _r_config_getlong (L"HotkeyClean", MAKEWORD (VK_F1, HOTKEYF_CONTROL), NULL));

					_r_ctrl_sendcommand (hwnd, IDC_AUTOREDUCTENABLE_CHK, 0);
					_r_ctrl_sendcommand (hwnd, IDC_AUTOREDUCTINTERVALENABLE_CHK, 0);
					_r_ctrl_sendcommand (hwnd, IDC_HOTKEY_CLEAN_CHK, 0);

					break;
				}

				case IDD_SETTINGS_APPEARANCE:
				{
					LOGFONT logfont;
					LONG dpi_value;

					_r_ctrl_checkbutton (hwnd, IDC_TRAYUSETRANSPARENCY_CHK, _r_config_getboolean (L"TrayUseTransparency", FALSE, NULL));
					_r_ctrl_checkbutton (hwnd, IDC_TRAYSHOWBORDER_CHK, _r_config_getboolean (L"TrayShowBorder", FALSE, NULL));
					_r_ctrl_checkbutton (hwnd, IDC_TRAYROUNDCORNERS_CHK, _r_config_getboolean (L"TrayRoundCorners", FALSE, NULL));
					_r_ctrl_checkbutton (hwnd, IDC_TRAYCHANGEBG_CHK, _r_config_getboolean (L"TrayChangeBg", TRUE, NULL));
					_r_ctrl_checkbutton (hwnd, IDC_TRAYUSEANTIALIASING_CHK, _r_config_getboolean (L"TrayUseAntialiasing", FALSE, NULL));

					dpi_value = _r_dc_gettaskbardpi ();

					_app_fontinit (&logfont, dpi_value);
					_app_setfontcontrol (hwnd, IDC_FONT, &logfont, dpi_value);

					_r_listview_setstyle (hwnd, IDC_COLORS, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP, FALSE);

					// fix by @XakerTwo
					_r_listview_deleteallcolumns (hwnd, IDC_COLORS);
					_r_listview_deleteallitems (hwnd, IDC_COLORS);

					_r_listview_addcolumn (hwnd, IDC_COLORS, 0, NULL, -100, LVCFMT_LEFT);

					_r_listview_additem (
						hwnd,
						IDC_COLORS,
						0,
						_r_locale_getstring (IDS_COLOR_TEXT_HINT),
						I_DEFAULT,
						I_DEFAULT,
						_r_config_getulong (L"TrayColorText", TRAY_COLOR_TEXT, NULL)
					);

					_r_listview_additem (
						hwnd,
						IDC_COLORS,
						1,
						_r_locale_getstring (IDS_COLOR_BACKGROUND_HINT),
						I_DEFAULT,
						I_DEFAULT,
						_r_config_getulong (L"TrayColorBg", TRAY_COLOR_BG, NULL)
					);

					_r_listview_additem (
						hwnd,
						IDC_COLORS,
						2,
						_r_locale_getstring (IDS_COLOR_WARNING_HINT),
						I_DEFAULT,
						I_DEFAULT,
						_r_config_getulong (L"TrayColorWarning", TRAY_COLOR_WARNING, NULL)
					);

					_r_listview_additem (
						hwnd,
						IDC_COLORS,
						3,
						_r_locale_getstring (IDS_COLOR_DANGER_HINT),
						I_DEFAULT,
						I_DEFAULT,
						_r_config_getulong (L"TrayColorDanger", TRAY_COLOR_DANGER, NULL)
					);

					break;
				}

				case IDD_SETTINGS_TRAY:
				{
					_r_updown_setrange (hwnd, IDC_TRAYLEVELWARNING, 1, 99);
					_r_updown_setvalue (hwnd, IDC_TRAYLEVELWARNING, _app_getwarningvalue ());

					_r_updown_setrange (hwnd, IDC_TRAYLEVELDANGER, 1, 99);
					_r_updown_setvalue (hwnd, IDC_TRAYLEVELDANGER, _app_getdangervalue ());

					_r_combobox_setcurrentitem (hwnd, IDC_TRAYACTIONSC, _r_config_getlong (L"TrayActionDc", 0, NULL));
					_r_combobox_setcurrentitem (hwnd, IDC_TRAYACTIONMC, _r_config_getlong (L"TrayActionMc", 1, NULL));

					_r_ctrl_checkbutton (hwnd, IDC_SHOW_CLEAN_RESULT_CHK, _r_config_getboolean (L"BalloonCleanResults", TRUE, NULL));
					_r_ctrl_checkbutton (hwnd, IDC_NOTIFICATIONSOUND_CHK, _r_config_getboolean (L"IsNotificationsSound", TRUE, NULL));

					break;
				}

				case IDD_SETTINGS_ADVANCED:
				{
					_r_ctrl_checkbutton (hwnd, IDC_ALLOWSTANDBYLISTCLEANUP_CHK, _r_config_getboolean (L"IsAllowStandbyListCleanup", FALSE, NULL));
					_r_ctrl_checkbutton (hwnd, IDC_LOGRESULTS_CHK, _r_config_getboolean (L"LogCleanResults", FALSE, NULL));

					break;
				}
			}

			break;
		}

		case RM_LOCALIZE:
		{
			INT dialog_id;

			dialog_id = (INT)wparam;

			// localize titles
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_1, L"%s:", _r_locale_getstring (IDS_TITLE_1));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_2, L"%s: (Language)", _r_locale_getstring (IDS_TITLE_2));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_3, L"%s:", _r_locale_getstring (IDS_TITLE_3));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_4, L"%s:", _r_locale_getstring (IDS_TITLE_4));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_5, L"%s:", _r_locale_getstring (IDS_TITLE_5));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_6, L"%s:", _r_locale_getstring (IDS_TITLE_6));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_7, L"%s:", _r_locale_getstring (IDS_TITLE_7));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_8, L"%s:", _r_locale_getstring (IDS_TITLE_8));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_9, L"%s:", _r_locale_getstring (IDS_TITLE_9));

			switch (dialog_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
					_r_ctrl_setstring (hwnd, IDC_ALWAYSONTOP_CHK, _r_locale_getstring (IDS_ALWAYSONTOP_CHK));
					_r_ctrl_setstring (hwnd, IDC_LOADONSTARTUP_CHK, _r_locale_getstring (IDS_LOADONSTARTUP_CHK));
					_r_ctrl_setstring (hwnd, IDC_STARTMINIMIZED_CHK, _r_locale_getstring (IDS_STARTMINIMIZED_CHK));
					_r_ctrl_setstring (hwnd, IDC_REDUCTCONFIRMATION_CHK, _r_locale_getstring (IDS_REDUCTCONFIRMATION_CHK));
					_r_ctrl_setstring (hwnd, IDC_SKIPUACWARNING_CHK, _r_locale_getstring (IDS_SKIPUACWARNING_CHK));
					_r_ctrl_setstring (hwnd, IDC_CHECKUPDATES_CHK, _r_locale_getstring (IDS_CHECKUPDATES_CHK));
					_r_ctrl_setstring (hwnd, IDC_LANGUAGE_HINT, _r_locale_getstring (IDS_LANGUAGE_HINT));

					break;
				}

				case IDD_SETTINGS_MEMORY:
				{
					_r_ctrl_setstring (hwnd, IDC_AUTOREDUCTENABLE_CHK, _r_locale_getstring (IDS_AUTOREDUCTENABLE_CHK));
					_r_ctrl_setstring (hwnd, IDC_AUTOREDUCTINTERVALENABLE_CHK, _r_locale_getstring (IDS_AUTOREDUCTINTERVALENABLE_CHK));

					_r_ctrl_setstring (hwnd, IDC_HOTKEY_CLEAN_CHK, _r_locale_getstring (IDS_HOTKEY_CLEAN_CHK));

					break;
				}

				case IDD_SETTINGS_APPEARANCE:
				{
					_r_ctrl_setstring (hwnd, IDC_TRAYUSETRANSPARENCY_CHK, _r_locale_getstring (IDS_TRAYUSETRANSPARENCY_CHK));
					_r_ctrl_setstring (hwnd, IDC_TRAYSHOWBORDER_CHK, _r_locale_getstring (IDS_TRAYSHOWBORDER_CHK));
					_r_ctrl_setstring (hwnd, IDC_TRAYROUNDCORNERS_CHK, _r_locale_getstring (IDS_TRAYROUNDCORNERS_CHK));
					_r_ctrl_setstring (hwnd, IDC_TRAYCHANGEBG_CHK, _r_locale_getstring (IDS_TRAYCHANGEBG_CHK));
					_r_ctrl_setstring (hwnd, IDC_TRAYUSEANTIALIASING_CHK, _r_locale_getstring (IDS_TRAYUSEANTIALIASING_CHK));

					_r_ctrl_setstring (hwnd, IDC_FONT_HINT, _r_locale_getstring (IDS_FONT_HINT));

					_r_listview_setitem (hwnd, IDC_COLORS, 0, 0, _r_locale_getstring (IDS_COLOR_TEXT_HINT), I_DEFAULT, I_DEFAULT, I_DEFAULT);
					_r_listview_setitem (hwnd, IDC_COLORS, 1, 0, _r_locale_getstring (IDS_COLOR_BACKGROUND_HINT), I_DEFAULT, I_DEFAULT, I_DEFAULT);
					_r_listview_setitem (hwnd, IDC_COLORS, 2, 0, _r_locale_getstring (IDS_COLOR_WARNING_HINT), I_DEFAULT, I_DEFAULT, I_DEFAULT);
					_r_listview_setitem (hwnd, IDC_COLORS, 3, 0, _r_locale_getstring (IDS_COLOR_DANGER_HINT), I_DEFAULT, I_DEFAULT, I_DEFAULT);

					break;
				}

				case IDD_SETTINGS_TRAY:
				{
					LPCWSTR string;

					_r_ctrl_setstring (hwnd, IDC_TRAYLEVELWARNING_HINT, _r_locale_getstring (IDS_TRAYLEVELWARNING_HINT));
					_r_ctrl_setstring (hwnd, IDC_TRAYLEVELDANGER_HINT, _r_locale_getstring (IDS_TRAYLEVELDANGER_HINT));

					_r_ctrl_setstring (hwnd, IDC_TRAYACTIONSC_HINT, _r_locale_getstring (IDS_TRAYACTIONSC_HINT));
					_r_ctrl_setstring (hwnd, IDC_TRAYACTIONMC_HINT, _r_locale_getstring (IDS_TRAYACTIONMC_HINT));

					_r_combobox_clear (hwnd, IDC_TRAYACTIONSC);
					_r_combobox_clear (hwnd, IDC_TRAYACTIONMC);

					for (INT i = 0; i < 3; i++)
					{
						string = _r_locale_getstring (IDS_TRAY_ACTION_1 + i);

						_r_combobox_insertitem (hwnd, IDC_TRAYACTIONSC, i, string, i);
						_r_combobox_insertitem (hwnd, IDC_TRAYACTIONMC, i, string, i);
					}

					_r_combobox_setcurrentitembylparam (hwnd, IDC_TRAYACTIONSC, _r_config_getlong (L"TrayActionDc", 0, NULL));
					_r_combobox_setcurrentitembylparam (hwnd, IDC_TRAYACTIONMC, _r_config_getlong (L"TrayActionMc", 1, NULL));

					_r_ctrl_setstring (hwnd, IDC_SHOW_CLEAN_RESULT_CHK, _r_locale_getstring (IDS_SHOW_CLEAN_RESULT_CHK));
					_r_ctrl_setstring (hwnd, IDC_NOTIFICATIONSOUND_CHK, _r_locale_getstring (IDS_NOTIFICATIONSOUND_CHK));

					break;
				}

				case IDD_SETTINGS_ADVANCED:
				{
					_r_ctrl_setstring (hwnd, IDC_ALLOWSTANDBYLISTCLEANUP_CHK, L"Allow \"Standby lists\" and \"Modified page list\" cleanup on autoreduct");
					_r_ctrl_setstring (hwnd, IDC_LOGRESULTS_CHK, L"Log cleaning results into a debug log");

					break;
				}
			}

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp;

			nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case NM_CUSTOMDRAW:
				{
					LPNMLVCUSTOMDRAW lpnmlv;
					COLORREF clr;

					lpnmlv = (LPNMLVCUSTOMDRAW)lparam;

					switch (lpnmlv->nmcd.dwDrawStage)
					{
						case CDDS_PREPAINT:
						{
							SetWindowLongPtrW (hwnd, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);

							return CDRF_NOTIFYITEMDRAW;
						}

						case CDDS_ITEMPREPAINT:
						{
							if (lpnmlv->nmcd.hdr.idFrom == IDC_REGIONS)
							{
								if (_r_sys_isosversionlower (WINDOWS_8_1) && (lpnmlv->nmcd.lItemlParam & REDUCT_REGISTRY_CACHE) == REDUCT_REGISTRY_CACHE)
								{
									clr = _r_theme_isenabled () ? WND_GRAYTEXT_CLR : GetSysColor (COLOR_GRAYTEXT);

									lpnmlv->clrText = _r_dc_getcolorbrightness (clr);
									lpnmlv->clrTextBk = clr;

									return CDRF_NEWFONT;
								}

								if (_r_sys_isosversionlower (WINDOWS_10) && (lpnmlv->nmcd.lItemlParam & REDUCT_COMBINE_MEMORY_LISTS) == REDUCT_COMBINE_MEMORY_LISTS)
								{
									clr = _r_theme_isenabled () ? WND_GRAYTEXT_CLR : GetSysColor (COLOR_GRAYTEXT);

									lpnmlv->clrText = _r_dc_getcolorbrightness (clr);
									lpnmlv->clrTextBk = clr;

									return CDRF_NEWFONT;
								}
							}
							else if (lpnmlv->nmcd.hdr.idFrom == IDC_COLORS)
							{
								clr = (COLORREF)lpnmlv->nmcd.lItemlParam;

								lpnmlv->clrText = _r_dc_getcolorbrightness (clr);
								lpnmlv->clrTextBk = clr;

								_r_dc_fillrect (lpnmlv->nmcd.hdc, &lpnmlv->nmcd.rc, clr);

								SetWindowLongPtrW (hwnd, DWLP_MSGRESULT, CDRF_NEWFONT);

								return CDRF_NEWFONT;
							}
						}
					}

					break;
				}

				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE lpnmlv;
					CHOOSECOLOR cc = {0};
					COLORREF cust[16] = {TRAY_COLOR_DANGER, TRAY_COLOR_WARNING, TRAY_COLOR_BG, TRAY_COLOR_TEXT};
					COLORREF clr;
					LONG dpi_value;
					INT listview_id;

					lpnmlv = (LPNMITEMACTIVATE)lparam;
					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if (lpnmlv->iItem == INT_ERROR || listview_id != IDC_COLORS)
						break;

					clr = (COLORREF)_r_listview_getitemlparam (hwnd, listview_id, lpnmlv->iItem);

					if (!clr)
						break;

					cc.lStructSize = sizeof (CHOOSECOLOR);
					cc.Flags = CC_RGBINIT | CC_FULLOPEN;
					cc.hwndOwner = hwnd;
					cc.lpCustColors = cust;
					cc.rgbResult = clr;

					if (ChooseColorW (&cc))
					{
						if (lpnmlv->iItem == 0)
						{
							_r_config_setulong (L"TrayColorText", cc.rgbResult, NULL);
						}
						else if (lpnmlv->iItem == 1)
						{
							_r_config_setulong (L"TrayColorBg", cc.rgbResult, NULL);
						}
						else if (lpnmlv->iItem == 2)
						{
							_r_config_setulong (L"TrayColorWarning", cc.rgbResult, NULL);
						}
						else if (lpnmlv->iItem == 3)
						{
							_r_config_setulong (L"TrayColorDanger", cc.rgbResult, NULL);
						}

						_r_listview_setitem (hwnd, IDC_COLORS, lpnmlv->iItem, lpnmlv->iSubItem, NULL, I_DEFAULT, I_DEFAULT, cc.rgbResult);

						_r_listview_redraw (hwnd, IDC_COLORS);

						dpi_value = _r_dc_gettaskbardpi ();

						_app_iconinit (dpi_value);
						_app_iconredraw (_r_app_gethwnd ());
					}

					break;
				}

				case LVN_ITEMCHANGED:
				{
					LPNMLISTVIEW lpnmlv;
					INT listview_id;
					ULONG value;
					ULONG mask;

					lpnmlv = (LPNMLISTVIEW)lparam;
					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if (listview_id != IDC_REGIONS)
						break;

					if (_r_wnd_getcontext (hwnd, listview_id))
						break;

					if ((lpnmlv->uChanged & LVIF_STATE) == 0)
						break;

					if ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (1) || ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2)))
					{
						value = (ULONG)lpnmlv->lParam;

						mask = _r_config_getulong (L"ReductMask2", REDUCT_MASK_DEFAULT, NULL);

						if ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2))
						{
							if ((value & REDUCT_MASK_FREEZES) != 0)
							{
								if (!_r_show_confirmmessage (hwnd, NULL, _r_locale_getstring (IDS_QUESTION_WARNING), L"IsShowWarningConfirmation", FALSE))
								{
									_r_listview_setitemcheck (hwnd, listview_id, lpnmlv->iItem, FALSE);

									return FALSE;
								}
							}
						}

						if (_r_sys_isosversionlower (WINDOWS_8_1) && (value & REDUCT_REGISTRY_CACHE) == REDUCT_REGISTRY_CACHE)
						{
							_r_listview_setitemcheck (hwnd, listview_id, lpnmlv->iItem, (mask & REDUCT_REGISTRY_CACHE) == REDUCT_REGISTRY_CACHE);

							return FALSE;
						}

						if (_r_sys_isosversionlower (WINDOWS_10) && (value & REDUCT_COMBINE_MEMORY_LISTS) == REDUCT_COMBINE_MEMORY_LISTS)
						{
							_r_listview_setitemcheck (hwnd, listview_id, lpnmlv->iItem, (mask & REDUCT_COMBINE_MEMORY_LISTS) == REDUCT_COMBINE_MEMORY_LISTS);

							return FALSE;
						}

						if ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2))
						{
							mask |= value;
						}
						else
						{
							mask &= ~value;
						}

						_r_config_setulong (L"ReductMask2", mask, NULL);
					}

					break;
				}
			}

			break;
		}

		case WM_VSCROLL:
		case WM_HSCROLL:
		{
			LONG value;
			INT ctrl_id;
			BOOLEAN is_stylechanged = FALSE;

			ctrl_id = GetDlgCtrlID ((HWND)lparam);

			if (ctrl_id == IDC_AUTOREDUCTVALUE)
			{
				value = _r_updown_getvalue (hwnd, ctrl_id);

				_r_config_setlong (L"AutoreductValue", value, NULL);
			}
			else if (ctrl_id == IDC_AUTOREDUCTINTERVALVALUE)
			{
				value = _r_updown_getvalue (hwnd, ctrl_id);

				_r_config_setlong (L"AutoreductIntervalValue", value, NULL);
			}
			else if (ctrl_id == IDC_TRAYLEVELWARNING)
			{
				value = _r_updown_getvalue (hwnd, ctrl_id);

				_r_config_setlong (L"TrayLevelWarning", value, NULL);

				is_stylechanged = TRUE;
			}
			else if (ctrl_id == IDC_TRAYLEVELDANGER)
			{
				value = _r_updown_getvalue (hwnd, ctrl_id);

				_r_config_setlong (L"TrayLevelDanger", value, NULL);

				is_stylechanged = TRUE;
			}

			if (is_stylechanged)
			{
				_app_iconredraw (_r_app_gethwnd ());

				_r_listview_redraw (_r_app_gethwnd (), IDC_LISTVIEW);
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
					LONG value;

					if (notify_code == EN_CHANGE)
					{
						value = _r_updown_getvalue (hwnd, IDC_AUTOREDUCTVALUE);

						_r_config_setlong (L"AutoreductValue", value, NULL);
					}

					break;
				}

				case IDC_AUTOREDUCTINTERVALVALUE_CTRL:
				{
					LONG value;

					if (notify_code == EN_CHANGE)
					{
						value = _r_updown_getvalue (hwnd, IDC_AUTOREDUCTINTERVALVALUE);

						_r_config_setlong (L"AutoreductIntervalValue", value, NULL);
					}

					break;
				}

				case IDC_TRAYLEVELWARNING_CTRL:
				case IDC_TRAYLEVELDANGER_CTRL:
				{
					LONG value;

					if (notify_code == EN_CHANGE)
					{
						if (ctrl_id == IDC_TRAYLEVELWARNING_CTRL)
						{
							value = _r_updown_getvalue (hwnd, IDC_TRAYLEVELWARNING);

							_r_config_setlong (L"TrayLevelWarning", value, NULL);
						}
						else if (ctrl_id == IDC_TRAYLEVELDANGER_CTRL)
						{
							value = _r_updown_getvalue (hwnd, IDC_TRAYLEVELDANGER);

							_r_config_setlong (L"TrayLevelDanger", value, NULL);
						}

						_app_iconredraw (_r_app_gethwnd ());

						_r_listview_redraw (_r_app_gethwnd (), IDC_LISTVIEW);
					}

					break;
				}

				case IDC_ALWAYSONTOP_CHK:
				{
					BOOLEAN is_enable;

					is_enable = _r_ctrl_isbuttonchecked (hwnd, ctrl_id);

					_r_config_setboolean (L"AlwaysOnTop", is_enable, NULL);

					_r_menu_checkitem (GetMenu (_r_app_gethwnd ()), IDM_ALWAYSONTOP_CHK, 0, MF_BYCOMMAND, is_enable);

					break;
				}

				case IDC_LOADONSTARTUP_CHK:
				{
					BOOLEAN is_enable;

					is_enable = _r_ctrl_isbuttonchecked (hwnd, ctrl_id);

					_r_autorun_enable (hwnd, is_enable);

					is_enable = _r_autorun_isenabled ();

					_r_menu_checkitem (GetMenu (_r_app_gethwnd ()), IDM_LOADONSTARTUP_CHK, 0, MF_BYCOMMAND, is_enable);

					_r_ctrl_checkbutton (hwnd, ctrl_id, is_enable);

					break;
				}

				case IDC_STARTMINIMIZED_CHK:
				{
					BOOLEAN is_enable;

					is_enable = _r_ctrl_isbuttonchecked (hwnd, ctrl_id);

					_r_config_setboolean (L"IsStartMinimized", is_enable, NULL);

					_r_menu_checkitem (GetMenu (_r_app_gethwnd ()), IDM_STARTMINIMIZED_CHK, 0, MF_BYCOMMAND, is_enable);

					break;
				}

				case IDC_REDUCTCONFIRMATION_CHK:
				{
					BOOLEAN is_enable;

					is_enable = _r_ctrl_isbuttonchecked (hwnd, ctrl_id);

					_r_config_setboolean (L"IsShowReductConfirmation", is_enable, NULL);

					_r_menu_checkitem (GetMenu (_r_app_gethwnd ()), IDM_REDUCTCONFIRMATION_CHK, 0, MF_BYCOMMAND, is_enable);

					break;
				}

				case IDC_SKIPUACWARNING_CHK:
				{
					BOOLEAN is_enable;

					is_enable = _r_ctrl_isbuttonchecked (hwnd, ctrl_id);

					_r_skipuac_enable (hwnd, is_enable);

					is_enable = _r_skipuac_isenabled ();

					_r_menu_checkitem (GetMenu (_r_app_gethwnd ()), IDM_SKIPUACWARNING_CHK, 0, MF_BYCOMMAND, is_enable);

					_r_ctrl_checkbutton (hwnd, ctrl_id, is_enable);

					break;
				}

				case IDC_CHECKUPDATES_CHK:
				{
					BOOLEAN is_enable;

					is_enable = _r_ctrl_isbuttonchecked (hwnd, ctrl_id);

					_r_update_enable (is_enable);

					is_enable = _r_update_isenabled (FALSE);

					_r_menu_checkitem (GetMenu (_r_app_gethwnd ()), IDM_CHECKUPDATES_CHK, 0, MF_BYCOMMAND, is_enable);

					break;
				}

				case IDC_LANGUAGE:
				{
					if (notify_code == CBN_SELCHANGE)
						_r_locale_apply (hwnd, ctrl_id, 0);

					break;
				}

				case IDC_AUTOREDUCTENABLE_CHK:
				{
					HWND hbuddy;
					BOOLEAN is_enabled;

					is_enabled = _r_ctrl_isenabled (hwnd, ctrl_id);

					hbuddy = _r_updown_getbuddy (hwnd, IDC_AUTOREDUCTVALUE);

					if (hbuddy)
						_r_ctrl_enable (hbuddy, 0, is_enabled);

					if (is_enabled)
					{
						is_enabled = (_r_ctrl_isbuttonchecked (hwnd, ctrl_id));

						_r_config_setboolean (L"AutoreductEnable", is_enabled, NULL);
					}

					break;
				}

				case IDC_AUTOREDUCTINTERVALENABLE_CHK:
				{
					HWND hbuddy;
					BOOLEAN is_enabled;

					is_enabled = _r_ctrl_isenabled (hwnd, ctrl_id);

					hbuddy = _r_updown_getbuddy (hwnd, IDC_AUTOREDUCTINTERVALVALUE);

					if (hbuddy)
						_r_ctrl_enable (hbuddy, 0, is_enabled);

					if (is_enabled)
					{
						is_enabled = (_r_ctrl_isbuttonchecked (hwnd, ctrl_id));

						_r_config_setboolean (L"AutoreductIntervalEnable", is_enabled, NULL);
					}

					break;
				}

				case IDC_HOTKEY_CLEAN_CHK:
				{
					BOOLEAN is_checked;

					is_checked = _r_ctrl_isbuttonchecked (hwnd, ctrl_id);

					_r_ctrl_enable (hwnd, IDC_HOTKEY_CLEAN, is_checked);

					_r_config_setboolean (L"HotkeyCleanEnable", is_checked, NULL);

					_app_hotkeyinit (_r_app_gethwnd ());

					break;
				}

				case IDC_HOTKEY_CLEAN:
				{
					if (!_r_ctrl_isbuttonchecked (hwnd, IDC_HOTKEY_CLEAN_CHK))
						break;

					if (notify_code == EN_CHANGE)
					{
						_r_config_setlong (L"HotkeyClean", _r_hotkey_get (hwnd, ctrl_id), NULL);

						_app_hotkeyinit (_r_app_gethwnd ());
					}

					break;
				}

				case IDC_TRAYUSETRANSPARENCY_CHK:
				case IDC_TRAYSHOWBORDER_CHK:
				case IDC_TRAYROUNDCORNERS_CHK:
				case IDC_TRAYCHANGEBG_CHK:
				case IDC_TRAYUSEANTIALIASING_CHK:
				{
					BOOLEAN is_enabled;
					LONG dpi_value;

					is_enabled = _r_ctrl_isbuttonchecked (hwnd, ctrl_id);

					switch (ctrl_id)
					{
						case IDC_TRAYUSETRANSPARENCY_CHK:
						{
							_r_config_setboolean (L"TrayUseTransparency", is_enabled, NULL);
							break;
						}

						case IDC_TRAYSHOWBORDER_CHK:
						{
							_r_config_setboolean (L"TrayShowBorder", is_enabled, NULL);
							break;
						}

						case IDC_TRAYROUNDCORNERS_CHK:
						{
							_r_config_setboolean (L"TrayRoundCorners", is_enabled, NULL);
							break;
						}

						case IDC_TRAYCHANGEBG_CHK:
						{
							_r_config_setboolean (L"TrayChangeBg", is_enabled, NULL);
							break;
						}

						case IDC_TRAYUSEANTIALIASING_CHK:
						{
							_r_config_setboolean (L"TrayUseAntialiasing", is_enabled, NULL);
							break;
						}
					}

					dpi_value = _r_dc_gettaskbardpi ();

					_app_iconinit (dpi_value);
					_app_iconredraw (_r_app_gethwnd ());

					break;
				}

				case IDC_TRAYACTIONSC:
				{
					if (notify_code == CBN_SELCHANGE)
						_r_config_setlong (L"TrayActionDc", _r_combobox_getcurrentitem (hwnd, ctrl_id), NULL);

					break;
				}

				case IDC_TRAYACTIONMC:
				{
					if (notify_code == CBN_SELCHANGE)
						_r_config_setlong (L"TrayActionMc", _r_combobox_getcurrentitem (hwnd, ctrl_id), NULL);

					break;
				}

				case IDC_SHOW_CLEAN_RESULT_CHK:
				{
					BOOLEAN is_enabled;

					is_enabled = _r_ctrl_isbuttonchecked (hwnd, ctrl_id);

					_r_config_setboolean (L"BalloonCleanResults", is_enabled, NULL);

					break;
				}

				case IDC_NOTIFICATIONSOUND_CHK:
				{
					BOOLEAN is_enabled;

					is_enabled = _r_ctrl_isbuttonchecked (hwnd, ctrl_id);

					_r_config_setboolean (L"IsNotificationsSound", is_enabled, NULL);

					break;
				}

				case IDC_FONT:
				{
					CHOOSEFONT cf = {0};
					LOGFONT logfont;
					LONG dpi_value;

					cf.lStructSize = sizeof (CHOOSEFONT);
					cf.hwndOwner = hwnd;
					cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_FORCEFONTEXIST | CF_SCREENFONTS;

					dpi_value = _r_dc_gettaskbardpi ();

					_app_fontinit (&logfont, dpi_value);

					cf.lpLogFont = &logfont;

					if (ChooseFontW (&cf))
					{
						_r_config_setfont (L"TrayFont", &logfont, dpi_value, NULL);

						_app_setfontcontrol (hwnd, IDC_FONT, &logfont, dpi_value);

						_app_iconinit (dpi_value);
						_app_iconredraw (_r_app_gethwnd ());
					}

					break;
				}

				case IDC_ALLOWSTANDBYLISTCLEANUP_CHK:
				{
					BOOLEAN is_enabled;

					is_enabled = _r_ctrl_isbuttonchecked (hwnd, ctrl_id);

					_r_config_setboolean (L"IsAllowStandbyListCleanup", is_enabled, NULL);

					break;
				}

				case IDC_LOGRESULTS_CHK:
				{
					BOOLEAN is_enabled;

					is_enabled = _r_ctrl_isbuttonchecked (hwnd, ctrl_id);

					_r_config_setboolean (L"LogCleanResults", is_enabled, NULL);

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

VOID _app_resizecolumns (
	_In_ HWND hwnd
)
{
	_r_listview_setcolumn (hwnd, IDC_LISTVIEW, 0, NULL, -50);
	_r_listview_setcolumn (hwnd, IDC_LISTVIEW, 1, NULL, -50);
}

VOID _app_initialize (
	_In_opt_ HWND hwnd
)
{
	ULONG privileges[] = {
		SE_PROF_SINGLE_PROCESS_PRIVILEGE,
		SE_INCREASE_QUOTA_PRIVILEGE,
	};

	LONG dpi_value;

	if (_r_sys_iselevated ())
	{
		_r_sys_setprocessprivilege (NtCurrentProcess (), privileges, RTL_NUMBER_OF (privileges), TRUE);
	}
	else
	{
		if (hwnd)
			_r_ctrl_setbuttonshield (hwnd, IDC_CLEAN, TRUE);
	}

	if (!hwnd)
		return;

	dpi_value = _r_dc_getwindowdpi (hwnd);

	_r_ctrl_setbuttonmargins (hwnd, IDC_CLEAN, dpi_value);

	// configure listview
	_r_listview_setstyle (
		hwnd,
		IDC_LISTVIEW,
		LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP,
		TRUE
	);

	_r_listview_addcolumn (hwnd, IDC_LISTVIEW, 1, NULL, 10, LVCFMT_RIGHT);
	_r_listview_addcolumn (hwnd, IDC_LISTVIEW, 2, NULL, 10, LVCFMT_LEFT);

	// configure listview
	for (INT i = 0, k = 0; i < 3; i++)
	{
		_r_listview_addgroup (hwnd, IDC_LISTVIEW, i, _r_locale_getstring (IDS_GROUP_1 + i), 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);

		for (INT j = 0; j < 3; j++)
		{
			_r_listview_additem (hwnd, IDC_LISTVIEW, k++, _r_locale_getstring (IDS_ITEM_1 + j), I_DEFAULT, i, 0);
		}
	}

	// settings
	_r_settings_addpage (IDD_SETTINGS_GENERAL, IDS_SETTINGS_GENERAL);
	_r_settings_addpage (IDD_SETTINGS_MEMORY, IDS_SETTINGS_MEMORY);
	_r_settings_addpage (IDD_SETTINGS_APPEARANCE, IDS_SETTINGS_APPEARANCE);
	_r_settings_addpage (IDD_SETTINGS_TRAY, IDS_SETTINGS_TRAY);
	_r_settings_addpage (IDD_SETTINGS_ADVANCED, IDS_TITLE_ADVANCED);

	_app_resizecolumns (hwnd);
}

INT_PTR CALLBACK DlgProc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			_r_app_sethwnd (hwnd); // HACK!!!

			_app_initialize (hwnd);

			SetTimer (hwnd, UID, TIMER, &_app_timercallback);

			break;
		}

		case WM_DESTROY:
		{
			KillTimer (hwnd, UID);

			_r_tray_destroy (hwnd, &GUID_TrayIcon);

			PostQuitMessage (0);

			break;
		}

		case RM_INITIALIZE:
		{
			HMENU hmenu;
			LONG dpi_value;

			hmenu = GetMenu (hwnd);

			if (hmenu)
			{
				_r_menu_checkitem (hmenu, IDM_ALWAYSONTOP_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"AlwaysOnTop", FALSE, NULL));
				_r_menu_checkitem (hmenu, IDM_USEDARKTHEME, 0, MF_BYCOMMAND, _r_theme_isenabled ());
				_r_menu_checkitem (hmenu, IDM_LOADONSTARTUP_CHK, 0, MF_BYCOMMAND, _r_autorun_isenabled ());
				_r_menu_checkitem (hmenu, IDM_STARTMINIMIZED_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsStartMinimized", FALSE, NULL));
				_r_menu_checkitem (hmenu, IDM_REDUCTCONFIRMATION_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsShowReductConfirmation", TRUE, NULL));
				_r_menu_checkitem (hmenu, IDM_SKIPUACWARNING_CHK, 0, MF_BYCOMMAND, _r_skipuac_isenabled ());
				_r_menu_checkitem (hmenu, IDM_CHECKUPDATES_CHK, 0, MF_BYCOMMAND, _r_update_isenabled (FALSE));

				if (!_r_sys_iselevated ())
					_r_menu_enableitem (hmenu, IDM_SKIPUACWARNING_CHK, FALSE, FALSE);
			}

			dpi_value = _r_dc_gettaskbardpi ();

			_app_iconinit (dpi_value);

			_r_tray_create (hwnd, &GUID_TrayIcon, RM_TRAYICON, _app_iconcreate (0), _r_app_getname (), FALSE);

			_app_iconredraw (hwnd);

			break;
		}

		case RM_INITIALIZE_POST:
		{
			if (_r_sys_iselevated ())
				_app_hotkeyinit (hwnd);

			break;
		}

		case RM_TASKBARCREATED:
		{
			LONG dpi_value;

			dpi_value = _r_dc_gettaskbardpi ();

			_app_iconinit (dpi_value);

			_r_tray_create (hwnd, &GUID_TrayIcon, RM_TRAYICON, _app_iconcreate (0), _r_app_getname (), FALSE);

			_app_iconredraw (hwnd);

			break;
		}

		case RM_LOCALIZE:
		{
			// localize menu
			HMENU hmenu;

			hmenu = GetMenu (hwnd);

			if (hmenu)
			{
				_r_menu_setitemtext (hmenu, 0, TRUE, _r_locale_getstring (IDS_FILE));
				_r_menu_setitemtext (hmenu, 1, TRUE, _r_locale_getstring (IDS_VIEW));
				_r_menu_setitemtext (hmenu, 2, TRUE, _r_locale_getstring (IDS_SETTINGS));
				_r_menu_setitemtext (hmenu, 3, TRUE, _r_locale_getstring (IDS_HELP));

				_r_menu_setitemtextformat (hmenu, IDM_SETTINGS, FALSE, L"%s...\tF2", _r_locale_getstring (IDS_SETTINGS));
				_r_menu_setitemtext (hmenu, IDM_EXIT, FALSE, _r_locale_getstring (IDS_EXIT));
				_r_menu_setitemtext (hmenu, IDM_ALWAYSONTOP_CHK, FALSE, _r_locale_getstring (IDS_ALWAYSONTOP_CHK));
				_r_menu_setitemtext (hmenu, IDM_LOADONSTARTUP_CHK, FALSE, _r_locale_getstring (IDS_LOADONSTARTUP_CHK));
				_r_menu_setitemtext (hmenu, IDM_USEDARKTHEME, FALSE, _r_locale_getstring (IDS_USEDARKTHEME));
				_r_menu_setitemtext (hmenu, IDM_STARTMINIMIZED_CHK, FALSE, _r_locale_getstring (IDS_STARTMINIMIZED_CHK));
				_r_menu_setitemtext (hmenu, IDM_REDUCTCONFIRMATION_CHK, FALSE, _r_locale_getstring (IDS_REDUCTCONFIRMATION_CHK));
				_r_menu_setitemtext (hmenu, IDM_SKIPUACWARNING_CHK, FALSE, _r_locale_getstring (IDS_SKIPUACWARNING_CHK));
				_r_menu_setitemtext (hmenu, IDM_CHECKUPDATES_CHK, FALSE, _r_locale_getstring (IDS_CHECKUPDATES_CHK));
				_r_menu_setitemtextformat (GetSubMenu (hmenu, LANG_SUBMENU), LANG_MENU, TRUE, L"%s (Language)", _r_locale_getstring (IDS_LANGUAGE));
				_r_menu_setitemtext (hmenu, IDM_WEBSITE, FALSE, _r_locale_getstring (IDS_WEBSITE));
				_r_menu_setitemtext (hmenu, IDM_CHECKUPDATES, FALSE, _r_locale_getstring (IDS_CHECKUPDATES));
				_r_menu_setitemtextformat (hmenu, IDM_ABOUT, FALSE, L"%s\tF1", _r_locale_getstring (IDS_ABOUT));
			}

			// configure listview
			for (INT i = 0, k = 0; i < 3; i++)
			{
				_r_listview_setgroup (hwnd, IDC_LISTVIEW, i, _r_locale_getstring (IDS_GROUP_1 + i), 0, 0);

				for (INT j = 0; j < 3; j++)
				{
					_r_listview_setitem (hwnd, IDC_LISTVIEW, k++, 0, _r_locale_getstring (IDS_ITEM_1 + j), I_DEFAULT, I_DEFAULT, I_DEFAULT);
				}
			}

			// configure button
			_r_ctrl_setstring (hwnd, IDC_CLEAN, _r_locale_getstring (IDS_CLEAN));

			// enum localizations
			if (hmenu)
				_r_locale_enum (GetSubMenu (hmenu, LANG_SUBMENU), LANG_MENU, IDX_LANGUAGE);

			break;
		}

		case WM_DPICHANGED:
		{
			LONG dpi_value;

			dpi_value = _r_dc_gettaskbardpi ();

			_app_iconinit (dpi_value);
			_app_iconredraw (hwnd);

			_app_resizecolumns (hwnd);

			if (!_r_sys_iselevated ())
			{
				dpi_value = LOWORD (wparam);

				_r_ctrl_setbuttonmargins (hwnd, IDC_CLEAN, dpi_value);
			}

			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc;

			hdc = BeginPaint (hwnd, &ps);

			if (!hdc)
				break;

			_r_dc_drawwindow (hdc, hwnd, TRUE);

			EndPaint (hwnd, &ps);

			break;
		}

		case WM_HOTKEY:
		{
			if (wparam == UID)
				_app_memoryclean (hwnd, SOURCE_HOTKEY, 0);

			break;
		}

		case WM_SHOWWINDOW:
		{
			if (wparam)
				_app_iconredraw (hwnd);

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp;

			nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case BCN_DROPDOWN:
				{
					R_RECTANGLE rectangle;
					RECT rect;
					HMENU hsubmenu;

					hsubmenu = CreatePopupMenu ();

					if (!hsubmenu)
						break;

					_r_menu_additem (hsubmenu, IDM_CLEAN_WORKINGSET, TITLE_WORKINGSET);
					_r_menu_additem (hsubmenu, IDM_CLEAN_SYSTEMFILECACHE, TITLE_SYSTEMFILECACHE);
					_r_menu_additem (hsubmenu, IDM_CLEAN_MODIFIEDFILECACHE, TITLE_MODIFIEDFILECACHE);
					_r_menu_additem (hsubmenu, IDM_CLEAN_MODIFIEDLIST, TITLE_MODIFIEDLIST);
					_r_menu_additem (hsubmenu, IDM_CLEAN_STANDBYLIST, TITLE_STANDBYLIST);
					_r_menu_additem (hsubmenu, IDM_CLEAN_STANDBYLISTPRIORITY0, TITLE_STANDBYLISTPRIORITY0);
					_r_menu_additem (hsubmenu, IDM_CLEAN_REGISTRYCACHE, TITLE_REGISTRYCACHE);
					_r_menu_additem (hsubmenu, IDM_CLEAN_COMBINEMEMORYLISTS, TITLE_COMBINEMEMORYLISTS);

					if (_r_sys_isosversionlower (WINDOWS_8_1))
						_r_menu_enableitem (hsubmenu, IDM_CLEAN_REGISTRYCACHE, FALSE, FALSE);

					if (_r_sys_isosversionlower (WINDOWS_10))
						_r_menu_enableitem (hsubmenu, IDM_CLEAN_COMBINEMEMORYLISTS, FALSE, FALSE);

					if (GetClientRect (nmlp->hwndFrom, &rect))
					{
						ClientToScreen (nmlp->hwndFrom, (PPOINT)&rect);

						_r_wnd_recttorectangle (&rectangle, &rect);
						_r_wnd_adjustrectangletoworkingarea (nmlp->hwndFrom, &rectangle);
						_r_wnd_rectangletorect (&rect, &rectangle);

						_r_menu_popup (hsubmenu, hwnd, (PPOINT)&rect, TRUE);
					}

					DestroyMenu (hsubmenu);

					break;
				}

				case NM_CUSTOMDRAW:
				{
					LPNMLVCUSTOMDRAW lpnmlv;
					LONG_PTR result;
					ULONG value;

					lpnmlv = (LPNMLVCUSTOMDRAW)lparam;
					result = CDRF_DODEFAULT;

					if (nmlp->idFrom != IDC_LISTVIEW)
						break;

					switch (lpnmlv->nmcd.dwDrawStage)
					{
						case CDDS_PREPAINT:
						{
							result = (CDRF_NOTIFYPOSTPAINT | CDRF_NOTIFYITEMDRAW);
							break;
						}

						case CDDS_ITEMPREPAINT:
						{
							value = (ULONG)lpnmlv->nmcd.lItemlParam;

							if (value >= _app_getdangervalue ())
							{
								lpnmlv->clrText = _r_config_getulong (L"TrayColorDanger", TRAY_COLOR_DANGER, NULL);

								result = (CDRF_NOTIFYPOSTPAINT | CDRF_NEWFONT);
							}
							else if (value >= _app_getwarningvalue ())
							{
								lpnmlv->clrText = _r_config_getulong (L"TrayColorWarning", TRAY_COLOR_WARNING, NULL);

								result = (CDRF_NOTIFYPOSTPAINT | CDRF_NEWFONT);
							}

							break;
						}
					}

					SetWindowLongPtrW (hwnd, DWLP_MSGRESULT, result);

					return result;
				}
			}

			break;
		}

		case RM_TRAYICON:
		{
			switch (LOWORD (lparam))
			{
				case NIN_KEYSELECT:
				{
					if (GetForegroundWindow () != hwnd)
						_r_wnd_toggle (hwnd, TRUE);

					break;
				}

				case WM_LBUTTONDOWN:
				case WM_MBUTTONDOWN:
				{
					LONG action;

					if (LOWORD (lparam) == WM_MBUTTONDOWN)
					{
						action = _r_config_getlong (L"TrayActionMc", 1, NULL);
					}
					else
					{
						action = _r_config_getlong (L"TrayActionDc", 0, NULL);
					}

					switch (action)
					{
						case 1:
						{
							_app_memoryclean (hwnd, SOURCE_MANUAL, 0);
							break;
						}

						case 2:
						{
							_r_sys_createprocess (L"taskmgr.exe", NULL, NULL, FALSE);
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
					HMENU hmenu;
					HMENU hsubmenu;
					HMENU hsubmenu_region;
					HMENU hsubmenu_limit;
					HMENU hsubmenu_interval;
					ULONG mask;
					BOOLEAN is_enabled;

					SetForegroundWindow (hwnd); // don't touch

					hmenu = LoadMenuW (NULL, MAKEINTRESOURCEW (IDM_TRAY));

					if (!hmenu)
						break;

					hsubmenu = GetSubMenu (hmenu, 0);

					if (!hsubmenu)
						break;

					hsubmenu_region = GetSubMenu (hsubmenu, TRAY_SUBMENU_1);
					hsubmenu_limit = GetSubMenu (hsubmenu, TRAY_SUBMENU_2);
					hsubmenu_interval = GetSubMenu (hsubmenu, TRAY_SUBMENU_3);

					// localize
					_r_menu_setitemtext (hsubmenu, IDM_TRAY_SHOW, FALSE, _r_locale_getstring (IDS_TRAY_SHOW));
					_r_menu_setitemtextformat (hsubmenu, IDM_TRAY_CLEAN, FALSE, L"%s...", _r_locale_getstring (IDS_CLEAN));
					_r_menu_setitemtext (hsubmenu, TRAY_SUBMENU_1, TRUE, _r_locale_getstring (IDS_TRAY_POPUP_1));
					_r_menu_setitemtext (hsubmenu, TRAY_SUBMENU_2, TRUE, _r_locale_getstring (IDS_TRAY_POPUP_2));
					_r_menu_setitemtext (hsubmenu, TRAY_SUBMENU_3, TRUE, _r_locale_getstring (IDS_TRAY_POPUP_3));
					_r_menu_setitemtextformat (hsubmenu, IDM_TRAY_SETTINGS, FALSE, L"%s...", _r_locale_getstring (IDS_SETTINGS));
					_r_menu_setitemtext (hsubmenu, IDM_TRAY_WEBSITE, FALSE, _r_locale_getstring (IDS_WEBSITE));
					_r_menu_setitemtext (hsubmenu, IDM_TRAY_ABOUT, FALSE, _r_locale_getstring (IDS_ABOUT));
					_r_menu_setitemtext (hsubmenu, IDM_TRAY_EXIT, FALSE, _r_locale_getstring (IDS_EXIT));

					// configure region submenu
					if (hsubmenu_region)
					{
						mask = _r_config_getulong (L"ReductMask2", REDUCT_MASK_DEFAULT, NULL);

						_r_menu_setitemtext (hsubmenu_region, IDM_WORKINGSET_CHK, FALSE, TITLE_WORKINGSET);
						_r_menu_setitemtext (hsubmenu_region, IDM_SYSTEMFILECACHE_CHK, FALSE, TITLE_SYSTEMFILECACHE);
						_r_menu_setitemtext (hsubmenu_region, IDM_MODIFIEDFILECACHE_CHK, FALSE, TITLE_MODIFIEDFILECACHE);

						_r_menu_setitemtext (hsubmenu_region, IDM_MODIFIEDLIST_CHK, FALSE, TITLE_MODIFIEDLIST);
						_r_menu_setitemtext (hsubmenu_region, IDM_STANDBYLIST_CHK, FALSE, TITLE_STANDBYLIST);
						_r_menu_setitemtext (hsubmenu_region, IDM_STANDBYLISTPRIORITY0_CHK, FALSE, TITLE_STANDBYLISTPRIORITY0);
						_r_menu_setitemtext (hsubmenu_region, IDM_REGISTRYCACHE_CHK, FALSE, TITLE_REGISTRYCACHE);
						_r_menu_setitemtext (hsubmenu_region, IDM_COMBINEMEMORYLISTS_CHK, FALSE, TITLE_COMBINEMEMORYLISTS);

						_r_menu_checkitem (hsubmenu_region, IDM_WORKINGSET_CHK, 0, MF_BYCOMMAND, (mask & REDUCT_WORKING_SET) == REDUCT_WORKING_SET);
						_r_menu_checkitem (hsubmenu_region, IDM_SYSTEMFILECACHE_CHK, 0, MF_BYCOMMAND, (mask & REDUCT_SYSTEM_FILE_CACHE) == REDUCT_SYSTEM_FILE_CACHE);
						_r_menu_checkitem (hsubmenu_region, IDM_MODIFIEDLIST_CHK, 0, MF_BYCOMMAND, (mask & REDUCT_MODIFIED_LIST) == REDUCT_MODIFIED_LIST);
						_r_menu_checkitem (hsubmenu_region, IDM_STANDBYLIST_CHK, 0, MF_BYCOMMAND, (mask & REDUCT_STANDBY_LIST) == REDUCT_STANDBY_LIST);
						_r_menu_checkitem (hsubmenu_region, IDM_STANDBYLISTPRIORITY0_CHK, 0, MF_BYCOMMAND, (mask & REDUCT_STANDBY_PRIORITY0_LIST) == REDUCT_STANDBY_PRIORITY0_LIST);
						_r_menu_checkitem (hsubmenu_region, IDM_MODIFIEDFILECACHE_CHK, 0, MF_BYCOMMAND, (mask & REDUCT_MODIFIED_FILE_CACHE) == REDUCT_MODIFIED_FILE_CACHE);
						_r_menu_checkitem (hsubmenu_region, IDM_REGISTRYCACHE_CHK, 0, MF_BYCOMMAND, (mask & REDUCT_REGISTRY_CACHE) == REDUCT_REGISTRY_CACHE);
						_r_menu_checkitem (hsubmenu_region, IDM_COMBINEMEMORYLISTS_CHK, 0, MF_BYCOMMAND, (mask & REDUCT_COMBINE_MEMORY_LISTS) == REDUCT_COMBINE_MEMORY_LISTS);

						if (!_r_sys_iselevated ())
						{
							_r_menu_enableitem (hsubmenu_region, IDM_WORKINGSET_CHK, FALSE, FALSE);
							_r_menu_enableitem (hsubmenu_region, IDM_SYSTEMFILECACHE_CHK, FALSE, FALSE);
							_r_menu_enableitem (hsubmenu_region, IDM_MODIFIEDFILECACHE_CHK, FALSE, FALSE);

							_r_menu_enableitem (hsubmenu_region, IDM_MODIFIEDLIST_CHK, FALSE, FALSE);
							_r_menu_enableitem (hsubmenu_region, IDM_STANDBYLIST_CHK, FALSE, FALSE);

							_r_menu_enableitem (hsubmenu_region, IDM_STANDBYLISTPRIORITY0_CHK, FALSE, FALSE);

							_r_menu_enableitem (hsubmenu_region, IDM_REGISTRYCACHE_CHK, FALSE, FALSE);
							_r_menu_enableitem (hsubmenu_region, IDM_COMBINEMEMORYLISTS_CHK, FALSE, FALSE);
						}

						// Flush registry cache (win8.1+)
						if (_r_sys_isosversionlower (WINDOWS_8_1))
							_r_menu_enableitem (hsubmenu_region, IDM_REGISTRYCACHE_CHK, FALSE, FALSE);

						// Combine memory lists (win10+)
						if (_r_sys_isosversionlower (WINDOWS_10))
							_r_menu_enableitem (hsubmenu_region, IDM_COMBINEMEMORYLISTS_CHK, FALSE, FALSE);
					}

					// configure submenu #2
					if (hsubmenu_limit)
					{
						is_enabled = _r_config_getboolean (L"AutoreductEnable", FALSE, NULL);

						_app_generate_menu (
							hsubmenu_limit,
							IDX_TRAY_POPUP_1,
							limits_arr,
							RTL_NUMBER_OF (limits_arr),
							L"%" TEXT (PR_ULONG) L"%%",
							_app_getlimitvalue (),
							is_enabled
						);
					}

					// configure submenu #3
					if (hsubmenu_interval)
					{
						is_enabled = _r_config_getboolean (L"AutoreductIntervalEnable", FALSE, NULL);

						_app_generate_menu (
							hsubmenu_interval,
							IDX_TRAY_POPUP_2,
							intervals_arr,
							RTL_NUMBER_OF (intervals_arr),
							L"%" TEXT (PR_LONG64) L" min.",
							_app_getintervalvalue (),
							is_enabled
						);
					}

					_r_menu_popup (hsubmenu, hwnd, NULL, TRUE);

					DestroyMenu (hmenu);

					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			INT ctrl_id = LOWORD (wparam);
			INT notify_code = HIWORD (wparam);

			if (notify_code == 0 && ctrl_id >= IDX_LANGUAGE && ctrl_id <= IDX_LANGUAGE + (INT)(INT_PTR)_r_locale_getcount () + 1)
			{
				HMENU hmenu;
				HMENU hsubmenu;

				hmenu = GetMenu (hwnd);

				if (hmenu)
				{
					hsubmenu = GetSubMenu (GetSubMenu (hmenu, LANG_SUBMENU), LANG_MENU);

					if (hsubmenu)
						_r_locale_apply (hsubmenu, ctrl_id, IDX_LANGUAGE);
				}

				return FALSE;
			}
			else if ((ctrl_id >= IDX_TRAY_POPUP_1 && ctrl_id <= IDX_TRAY_POPUP_1 + (INT)RTL_NUMBER_OF (limits_arr) - 1))
			{
				ULONG_PTR idx;

				idx = (ULONG_PTR)ctrl_id - IDX_TRAY_POPUP_1;

				_r_config_setboolean (L"AutoreductEnable", TRUE, NULL);
				_r_config_setlong (L"AutoreductValue", limits_arr[idx], NULL);

				return FALSE;
			}
			else if ((ctrl_id >= IDX_TRAY_POPUP_2 && ctrl_id <= IDX_TRAY_POPUP_2 + (INT)RTL_NUMBER_OF (intervals_arr) - 1))
			{
				ULONG_PTR idx;

				idx = (ULONG_PTR)ctrl_id - IDX_TRAY_POPUP_2;

				_r_config_setboolean (L"AutoreductIntervalEnable", TRUE, NULL);
				_r_config_setlong (L"AutoreductIntervalValue", intervals_arr[idx], NULL);

				return FALSE;
			}

			switch (ctrl_id)
			{
				case IDM_ALWAYSONTOP_CHK:
				{
					BOOLEAN new_val;

					new_val = !_r_config_getboolean (L"AlwaysOnTop", FALSE, NULL);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					_r_config_setboolean (L"AlwaysOnTop", new_val, NULL);

					_r_wnd_top (hwnd, new_val);

					break;
				}

				case IDM_STARTMINIMIZED_CHK:
				{
					BOOLEAN new_val;

					new_val = !_r_config_getboolean (L"IsStartMinimized", FALSE, NULL);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					_r_config_setboolean (L"IsStartMinimized", new_val, NULL);

					break;
				}

				case IDM_REDUCTCONFIRMATION_CHK:
				{
					BOOLEAN new_val;

					new_val = !_r_config_getboolean (L"IsShowReductConfirmation", TRUE, NULL);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					_r_config_setboolean (L"IsShowReductConfirmation", new_val, NULL);

					break;
				}

				case IDM_LOADONSTARTUP_CHK:
				{
					BOOLEAN new_val;

					new_val = !_r_autorun_isenabled ();

					_r_autorun_enable (hwnd, new_val);
					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, _r_autorun_isenabled ());

					break;
				}

				case IDM_USEDARKTHEME:
				{
					BOOLEAN is_enabled = !_r_theme_isenabled ();

					_r_theme_enable (hwnd, is_enabled);
					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, is_enabled);

					break;
				}

				case IDM_SKIPUACWARNING_CHK:
				{
					BOOLEAN new_val;

					new_val = !_r_skipuac_isenabled ();

					_r_skipuac_enable (hwnd, new_val);
					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, _r_skipuac_isenabled ());

					break;
				}

				case IDM_CHECKUPDATES_CHK:
				{
					BOOLEAN new_val;

					new_val = !_r_update_isenabled (FALSE);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					_r_update_enable (new_val);

					break;
				}

				case IDM_WORKINGSET_CHK:
				case IDM_SYSTEMFILECACHE_CHK:
				case IDM_MODIFIEDFILECACHE_CHK:
				case IDM_MODIFIEDLIST_CHK:
				case IDM_STANDBYLIST_CHK:
				case IDM_STANDBYLISTPRIORITY0_CHK:
				case IDM_REGISTRYCACHE_CHK:
				case IDM_COMBINEMEMORYLISTS_CHK:
				{
					ULONG new_mask = 0;
					ULONG mask;

					mask = _r_config_getulong (L"ReductMask2", REDUCT_MASK_DEFAULT, NULL);

					switch (ctrl_id)
					{
						case IDM_WORKINGSET_CHK:
						{
							new_mask = REDUCT_WORKING_SET;
							break;
						}

						case IDM_SYSTEMFILECACHE_CHK:
						{
							new_mask = REDUCT_SYSTEM_FILE_CACHE;
							break;
						}

						case IDM_MODIFIEDFILECACHE_CHK:
						{
							new_mask = REDUCT_MODIFIED_FILE_CACHE;
							break;
						}

						case IDM_MODIFIEDLIST_CHK:
						{
							new_mask = REDUCT_MODIFIED_LIST;
							break;
						}

						case IDM_STANDBYLIST_CHK:
						{
							new_mask = REDUCT_STANDBY_LIST;
							break;
						}

						case IDM_STANDBYLISTPRIORITY0_CHK:
						{
							new_mask = REDUCT_STANDBY_PRIORITY0_LIST;
							break;
						}

						case IDM_REGISTRYCACHE_CHK:
						{
							new_mask = REDUCT_REGISTRY_CACHE;
							break;
						}

						case IDM_COMBINEMEMORYLISTS_CHK:
						{
							new_mask = REDUCT_COMBINE_MEMORY_LISTS;
							break;
						}

						default:
						{
							return FALSE;
						}
					}

					if ((ctrl_id == IDM_STANDBYLIST_CHK && !(mask & REDUCT_STANDBY_LIST)) || (ctrl_id == IDM_MODIFIEDLIST_CHK && !(mask & REDUCT_MODIFIED_LIST)))
					{
						if (!_r_show_confirmmessage (hwnd, _r_locale_getstring (IDS_QUESTION_WARNING), NULL, L"IsShowWarningConfirmation", FALSE))
							return FALSE;
					}

					_r_config_setulong (L"ReductMask2", (mask & new_mask) != 0 ? (mask & ~new_mask) : (mask | new_mask), NULL);

					break;
				}

				case IDM_CLEAN_WORKINGSET:
				case IDM_CLEAN_SYSTEMFILECACHE:
				case IDM_CLEAN_MODIFIEDFILECACHE:
				case IDM_CLEAN_MODIFIEDLIST:
				case IDM_CLEAN_STANDBYLIST:
				case IDM_CLEAN_STANDBYLISTPRIORITY0:
				case IDM_CLEAN_REGISTRYCACHE:
				case IDM_CLEAN_COMBINEMEMORYLISTS:
				{
					ULONG mask;

					switch (ctrl_id)
					{
						case IDM_CLEAN_WORKINGSET:
						{
							mask = REDUCT_WORKING_SET;
							break;
						}

						case IDM_CLEAN_SYSTEMFILECACHE:
						{
							mask = REDUCT_SYSTEM_FILE_CACHE;
							break;
						}

						case IDM_CLEAN_MODIFIEDFILECACHE:
						{
							mask = REDUCT_MODIFIED_FILE_CACHE;
							break;
						}

						case IDM_CLEAN_MODIFIEDLIST:
						{
							mask = REDUCT_MODIFIED_LIST;
							break;
						}

						case IDM_CLEAN_STANDBYLIST:
						{
							mask = REDUCT_STANDBY_LIST;
							break;
						}

						case IDM_CLEAN_STANDBYLISTPRIORITY0:
						{
							mask = REDUCT_STANDBY_PRIORITY0_LIST;
							break;
						}

						case IDM_CLEAN_REGISTRYCACHE:
						{
							mask = REDUCT_REGISTRY_CACHE;
							break;
						}

						case IDM_CLEAN_COMBINEMEMORYLISTS:
						{
							mask = REDUCT_COMBINE_MEMORY_LISTS;
							break;
						}

						default:
						{
							return FALSE;
						}
					}

					_app_memoryclean (hwnd, SOURCE_CMDLINE, mask);

					break;
				}

				case IDM_TRAY_DISABLE_1:
				{
					BOOLEAN new_val;

					new_val = !_r_config_getboolean (L"AutoreductEnable", FALSE, NULL);

					_r_config_setboolean (L"AutoreductEnable", new_val, NULL);

					break;
				}

				case IDM_TRAY_DISABLE_2:
				{
					BOOLEAN new_val;

					new_val = !_r_config_getboolean (L"AutoreductIntervalEnable", FALSE, NULL);

					_r_config_setboolean (L"AutoreductIntervalEnable", new_val, NULL);

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
					if (_r_sys_iselevated ())
					{
						_app_memoryclean (hwnd, SOURCE_MANUAL, 0);
					}
					else
					{
						if (_r_app_runasadmin ())
						{
							DestroyWindow (hwnd);
						}
						else
						{
							_r_show_message (hwnd, MB_OK | MB_ICONSTOP, NULL, _r_locale_getstring (IDS_STATUS_NOPRIVILEGES));
						}
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

BOOLEAN NTAPI _app_parseargs (
	_In_ R_CMDLINE_INFO_CLASS type
)
{
	PR_STRING clean_args;
	ULONG mask = 0;

	switch (type)
	{
		case CmdlineClean:
		{
			_r_sys_getopt (_r_sys_getcommandline (), L"clean", &clean_args);

			if (clean_args)
			{
				if (_r_str_isequal2 (&clean_args->sr, L"full", TRUE))
					mask = REDUCT_MASK_ALL;

				_r_obj_dereference (clean_args);
			}

			if (!mask)
				mask = REDUCT_MASK_DEFAULT;

			_app_initialize (NULL);

			_app_memoryclean (NULL, SOURCE_CMDLINE, mask);

			return TRUE;
		}

		case CmdlineHelp:
		{
			_r_show_message (
				NULL,
				MB_OK | MB_ICONINFORMATION | MB_TOPMOST,
				L"Available options for memreduct.exe:",
				L"-clean - clear default memory regions\r\n" \
				L"-clean:full - clear all memory regions"
			);

			return TRUE;
		}
	}

	return FALSE;
}

INT APIENTRY wWinMain (
	_In_ HINSTANCE hinst,
	_In_opt_ HINSTANCE prev_hinst,
	_In_ LPWSTR cmdline,
	_In_ INT show_cmd
)
{
	HWND hwnd;

	if (!_r_app_initialize (&_app_parseargs))
		return ERROR_APP_INIT_FAILURE;

	hwnd = _r_app_createwindow (hinst, MAKEINTRESOURCEW (IDD_MAIN), MAKEINTRESOURCEW (IDI_MAIN), &DlgProc);

	if (!hwnd)
		return ERROR_APP_INIT_FAILURE;

	return _r_wnd_message_callback (hwnd, MAKEINTRESOURCEW (IDA_MAIN));
}
