// Mem Reduct
// Copyright (c) 2011-2017 Henry++

#ifndef __MAIN_H__
#define __MAIN_H__

#include <windows.h>
#include <commctrl.h>
#include "resource.hpp"
#include "app.hpp"

#define WM_TRAYICON WM_APP + 1
#define FONT_NAME L"Tahoma"
#define FONT_SIZE 8
#define TIMER 1000
#define UID 1337

// libs
#pragma comment(lib, "ntdll.lib")

// memory cleaning area (mask)
#define REDUCT_WORKING_SET 0x01
#define REDUCT_SYSTEM_WORKING_SET 0x02
#define REDUCT_STANDBY_PRIORITY0_LIST 0x04
#define REDUCT_STANDBY_LIST 0x08
#define REDUCT_MODIFIED_LIST 0x10

// def. colors
#define TRAY_COLOR_MASK RGB(255, 0, 255)
#define TRAY_COLOR_TEXT RGB(255, 255, 255)
#define TRAY_COLOR_BG RGB(0, 128, 64)
#define TRAY_COLOR_WARNING RGB(255, 128, 64)
#define TRAY_COLOR_DANGER RGB(237, 28, 36)

// def. memory cleaning area
#define MASK_DEFAULT (REDUCT_WORKING_SET | REDUCT_SYSTEM_WORKING_SET | REDUCT_STANDBY_PRIORITY0_LIST)

struct MEMORYINFO
{
	// Physical
	DWORD percent_phys;
	DWORDLONG total_phys;
	DWORDLONG free_phys;

	// Page file
	DWORD percent_page;
	DWORDLONG total_page;
	DWORDLONG free_page;

	// System working set
	DWORD percent_ws;
	DWORDLONG total_ws;
	DWORDLONG free_ws;
};

struct STATIC_DATA
{
	LONG scale;

	DWORD ms_prev;

	RECT rc;

	HDC dc;
	HDC cdc1;
	HDC cdc2;

	HBITMAP bitmap;
	HBITMAP bitmap_mask;

	HFONT font;
	LOGFONT lf;

	HBRUSH bg_brush;
	HBRUSH bg_brush_warning;
	HBRUSH bg_brush_danger;

	MEMORYINFO ms;
};

// rev
// private
// source: http://www.microsoft.com/whdc/system/Sysinternals/MoreThan64proc.mspx

#define SystemFileCacheInformation 21
#define SystemMemoryListInformation 80

#define MemoryEmptyWorkingSets 2
#define MemoryFlushModifiedList 3
#define MemoryPurgeStandbyList 4
#define MemoryPurgeLowPriorityStandbyList 5

struct SYSTEM_CACHE_INFORMATION
{
	ULONG_PTR	CurrentSize;
	ULONG_PTR	PeakSize;
	ULONG_PTR	PageFaultCount;
	ULONG_PTR	MinimumWorkingSet;
	ULONG_PTR	MaximumWorkingSet;
	ULONG_PTR	TransitionSharedPages;
	ULONG_PTR	PeakTransitionSharedPages;
	DWORD		Unused[2];
};

extern "C" {
	NTSYSCALLAPI
		NTSTATUS
		NTAPI
		NtQuerySystemInformation (
			_In_ UINT SystemInformationClass,
			_Out_writes_bytes_opt_ (SystemInformationLength) PVOID SystemInformation,
			_In_ ULONG SystemInformationLength,
			_Out_opt_ PULONG ReturnLength
			);

	NTSYSCALLAPI
		NTSTATUS
		NTAPI
		NtSetSystemInformation (
			_In_ UINT SystemInformationClass,
			_In_reads_bytes_opt_ (SystemInformationLength) PVOID SystemInformation,
			_In_ ULONG SystemInformationLength
			);
}

#endif // __MAIN_H__
