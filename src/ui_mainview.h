// Mem Reduct
// Copyright (c) 2011-2025 Henry++

#pragma once

#include "routine.h"

typedef struct _MAIN_MEMORY_STAT
{
	ULONG percent;
	WCHAR usage_text[64];
	WCHAR used_text[128];
	WCHAR available_text[128];
	WCHAR total_text[128];
} MAIN_MEMORY_STAT, *PMAIN_MEMORY_STAT;

typedef struct _MAIN_MEMORY_STATS
{
	MAIN_MEMORY_STAT physical_memory;
	MAIN_MEMORY_STAT page_file;
	MAIN_MEMORY_STAT system_cache;
} MAIN_MEMORY_STATS, *PMAIN_MEMORY_STATS;

HWND _app_mainview_create (
	_In_ HWND hwnd_parent
);

VOID _app_mainview_destroy (
	_In_opt_ HWND hwnd
);

VOID _app_mainview_setstats (
	_In_ HWND hwnd,
	_In_ PMAIN_MEMORY_STATS stats
);

VOID _app_mainview_settheme (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_dark
);

VOID _app_mainview_refreshsettings (
	_In_ HWND hwnd
);

VOID _app_mainview_resize (
	_In_ HWND hwnd,
	_In_ LPCRECT rect
);

VOID _app_mainview_invalidate (
	_In_ HWND hwnd
);
