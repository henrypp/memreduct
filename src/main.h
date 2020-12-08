// Mem Reduct
// Copyright (c) 2011-2021 Henry++

#pragma once

#include "routine.h"

#include "resource.h"
#include "app.h"

// libs
#pragma comment(lib, "msimg32.lib")

#define TIMER 1000
#define UID 1337
#define LANG_MENU 6

#define DEFAULT_AUTOREDUCT_VAL 90U
#define DEFAULT_AUTOREDUCTINTERVAL_VAL 30U

#define DEFAULT_DANGER_LEVEL 90U
#define DEFAULT_WARNING_LEVEL 60U

// memory cleaning area (mask)
#define REDUCT_WORKING_SET 0x01
#define REDUCT_SYSTEM_WORKING_SET 0x02
#define REDUCT_STANDBY_PRIORITY0_LIST 0x04
#define REDUCT_STANDBY_LIST 0x08
#define REDUCT_MODIFIED_LIST 0x10
#define REDUCT_COMBINE_MEMORY_LISTS 0x20

// def. colors
#define TRAY_COLOR_MASK RGB(255, 0, 255)
#define TRAY_COLOR_TEXT RGB(255, 255, 255)
#define TRAY_COLOR_BG RGB(0, 128, 64)
#define TRAY_COLOR_WARNING RGB(255, 128, 64)
#define TRAY_COLOR_DANGER RGB(237, 28, 36)

// def. memory cleaning area
#define REDUCT_MASK_DEFAULT (REDUCT_WORKING_SET | REDUCT_SYSTEM_WORKING_SET | REDUCT_STANDBY_PRIORITY0_LIST)
#define REDUCT_MASK_FREEZES (REDUCT_STANDBY_LIST | REDUCT_MODIFIED_LIST)

typedef struct _STATIC_DATA
{
	HDC hdc;
	HDC hdc_mask;
	HBITMAP hbitmap;
	HBITMAP hbitmap_mask;
	HFONT hfont;
	HICON htrayicon;
	DWORD ms_prev;
	INT scale;
} STATIC_DATA, *PSTATIC_DATA;

typedef struct _MEMORYINFO
{
	DWORD percent_phys;
	DWORD percent_page;
	DWORD percent_ws;
	DWORD64 total_phys;
	DWORD64 free_phys;
	DWORD64 total_page;
	DWORD64 free_page;
	DWORD64 total_ws;
	DWORD64 free_ws;
} MEMORYINFO, *PMEMORYINFO;
