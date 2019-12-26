// Mem Reduct
// Copyright (c) 2011-2019 Henry++

#pragma once

#include <windows.h>
#include <commctrl.h>

#include "routine.hpp"
#include "resource.hpp"
#include "app.hpp"

#define FONT_DEFAULT L"Tahoma;8;400"
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

struct MEMORYINFO
{
	DWORD percent_phys = 0;
	DWORD percent_page = 0;
	DWORD percent_ws = 0;

	DWORDLONG total_phys = 0;
	DWORDLONG free_phys = 0;

	DWORDLONG total_page = 0;
	DWORDLONG free_page = 0;

	DWORDLONG total_ws = 0;
	DWORDLONG free_ws = 0;
};

struct STATIC_DATA
{
	INT scale = 0;

	DWORD ms_prev = 0;

	HDC hdc = nullptr;
	HDC hdc_buffer = nullptr;

	HBITMAP hbitmap = nullptr;
	HBITMAP hbitmap_mask = nullptr;

	HFONT hfont = nullptr;

	HBRUSH bg_brush = nullptr;
	HBRUSH bg_brush_warning = nullptr;
	HBRUSH bg_brush_danger = nullptr;

	HICON htrayicon = nullptr;
};
