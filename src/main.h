// Mem Reduct
// Copyright (c) 2011-2025 Henry++

#pragma once

#include "routine.h"

#include "resource.h"
#include "app.h"

DEFINE_GUID (GUID_TrayIcon, 0xAE9053F0, 0x8D59, 0x4803, 0x9A, 0xBB, 0x74, 0xAF, 0xE6, 0x6B, 0x5F, 0xD2);

#define TITLE_WORKINGSET L"Working set"
#define TITLE_SYSTEMWORKINGSET L"System working set"
#define TITLE_STANDBYLISTPRIORITY0 L"Standby list (without priority)"
#define TITLE_STANDBYLIST L"Standby list*"
#define TITLE_MODIFIEDLIST L"Modified page list*"
#define TITLE_REGISTRYCACHE L"Registry cache (win8.1+)"
#define TITLE_COMBINEMEMORYLISTS L"Combine memory lists (win10+)"

#define TIMER 1000
#define UID 1337

#define LANG_SUBMENU 1
#define LANG_MENU 4

#define TRAY_SUBMENU_1 4
#define TRAY_SUBMENU_2 5
#define TRAY_SUBMENU_3 6

#define DEFAULT_AUTOREDUCT_VAL 90
#define DEFAULT_AUTOREDUCTINTERVAL_VAL 30

#define DEFAULT_DANGER_LEVEL 90
#define DEFAULT_WARNING_LEVEL 70

// colors
#define TRAY_COLOR_BLACK RGB(0x00, 0x00, 0x00)
#define TRAY_COLOR_WHITE RGB(0xFF, 0xFF, 0xFF)
#define TRAY_COLOR_TEXT RGB(0xFF, 0xFF, 0xFF)
#define TRAY_COLOR_BG RGB(0x00, 0x80, 0x40)
#define TRAY_COLOR_WARNING RGB(0xFF, 0x80, 0x40)
#define TRAY_COLOR_DANGER RGB(0xEC, 0x1C, 0x24)

// memory cleaning mask
#define REDUCT_WORKING_SET 0x01
#define REDUCT_SYSTEM_WORKING_SET 0x02
#define REDUCT_STANDBY_PRIORITY0_LIST 0x04
#define REDUCT_STANDBY_LIST 0x08
#define REDUCT_MODIFIED_LIST 0x10
#define REDUCT_COMBINE_MEMORY_LISTS 0x20
#define REDUCT_REGISTRY_CACHE 0x40

// memory cleaning values
#define REDUCT_MASK_ALL (REDUCT_WORKING_SET | REDUCT_SYSTEM_WORKING_SET | REDUCT_STANDBY_PRIORITY0_LIST | REDUCT_STANDBY_LIST | REDUCT_REGISTRY_CACHE | REDUCT_MODIFIED_LIST)
#define REDUCT_MASK_DEFAULT (REDUCT_WORKING_SET | REDUCT_SYSTEM_WORKING_SET | REDUCT_STANDBY_PRIORITY0_LIST | REDUCT_REGISTRY_CACHE | REDUCT_COMBINE_MEMORY_LISTS)
#define REDUCT_MASK_FREEZES (REDUCT_STANDBY_LIST | REDUCT_MODIFIED_LIST)

typedef struct _STATIC_DATA
{
	HDC hdc;
	HDC hdc_mask;
	HBITMAP hbitmap;
	HBITMAP hbitmap_alpha;
	HBITMAP hbitmap_mask;
	PDWORD dwbits_icon_argb;
	PDWORD dwbits_icon_bw;
	LONG dwbits_icon_length;
	HFONT hfont;
	RECT icon_size;
	ULONG ms_prev;
} STATIC_DATA, *PSTATIC_DATA;

typedef enum _CLEANUP_SOURCE_ENUM
{
	SOURCE_AUTO,
	SOURCE_MANUAL,
	SOURCE_HOTKEY,
	SOURCE_CMDLINE
} CLEANUP_SOURCE_ENUM, *PCLEANUP_SOURCE_ENUM;
