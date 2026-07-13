// Mem Reduct
// Copyright (c) 2011-2025 Henry++

#include "ui_mainview.h"

#include "rapp.h"
#include "resource.h"

#define MAINVIEW_CLASS_NAME L"MemReductMainView"

typedef struct _MAINVIEW_SECTION_LAYOUT
{
	RECT section_rect;
	RECT title_rect;
	RECT usage_rect;
	RECT bar_rect;
	RECT label_rects[3];
	RECT value_rects[3];
} MAINVIEW_SECTION_LAYOUT, *PMAINVIEW_SECTION_LAYOUT;

typedef struct _MAINVIEW_CONTEXT
{
	MAIN_MEMORY_STATS stats;
	MAINVIEW_SECTION_LAYOUT sections[3];
	HFONT hfont_title;
	HFONT hfont_value;
	HFONT hfont_detail;
	HDC hdc_frame;
	HBITMAP hbitmap_frame;
	HGDIOBJ hbitmap_frame_prev;
	SIZE frame_size;
	COLORREF warning_clr;
	COLORREF danger_clr;
	LONG dpi_value;
	LONG warning_level;
	LONG danger_level;
	BOOLEAN is_dark;
} MAINVIEW_CONTEXT, *PMAINVIEW_CONTEXT;

LONG _app_mainview_scale (
	_In_ LONG value,
	_In_ LONG dpi_value
)
{
	return MulDiv (value, dpi_value, USER_DEFAULT_SCREEN_DPI);
}

HFONT _app_mainview_createfont (
	_In_ LONG dpi_value,
	_In_ LONG point_size,
	_In_ LONG weight
)
{
	LOGFONT logfont = {0};

	_r_str_copy (logfont.lfFaceName, LF_FACESIZE, L"Segoe UI");

	logfont.lfHeight = _r_dc_fontsizetoheight (point_size, dpi_value);
	logfont.lfWeight = weight;
	logfont.lfQuality = CLEARTYPE_QUALITY;

	return CreateFontIndirectW (&logfont);
}

VOID _app_mainview_resetfonts (
	_Inout_ PMAINVIEW_CONTEXT context,
	_In_ LONG dpi_value
)
{
	SAFE_DELETE_OBJECT (context->hfont_title);
	SAFE_DELETE_OBJECT (context->hfont_value);
	SAFE_DELETE_OBJECT (context->hfont_detail);

	context->dpi_value = dpi_value;
	context->hfont_title = _app_mainview_createfont (dpi_value, 9, FW_SEMIBOLD);
	context->hfont_value = _app_mainview_createfont (dpi_value, 12, FW_SEMIBOLD);
	context->hfont_detail = _app_mainview_createfont (dpi_value, 8, FW_NORMAL);
}

VOID _app_mainview_drawtext (
	_In_ HDC hdc,
	_In_ LPCWSTR string,
	_In_ PRECT rect,
	_In_ HFONT hfont,
	_In_ COLORREF clr,
	_In_ UINT format
)
{
	HGDIOBJ hfont_prev;
	INT mode_prev;
	COLORREF clr_prev;

	hfont_prev = SelectObject (hdc, hfont);
	mode_prev = SetBkMode (hdc, TRANSPARENT);
	clr_prev = SetTextColor (hdc, clr);

	DrawTextW (hdc, string, -1, rect, format);

	SetTextColor (hdc, clr_prev);
	SetBkMode (hdc, mode_prev);
	SelectObject (hdc, hfont_prev);
}

VOID _app_mainview_fillroundrect (
	_In_ HDC hdc,
	_In_ LPCRECT rect,
	_In_ COLORREF fill_clr,
	_In_ COLORREF stroke_clr,
	_In_ LONG radius
)
{
	HGDIOBJ hbrush_prev;
	HGDIOBJ hpen_prev;

	hbrush_prev = SelectObject (hdc, GetStockObject (DC_BRUSH));
	hpen_prev = SelectObject (hdc, GetStockObject (DC_PEN));

	SetDCBrushColor (hdc, fill_clr);
	SetDCPenColor (hdc, stroke_clr);

	RoundRect (hdc, rect->left, rect->top, rect->right, rect->bottom, radius, radius);

	SelectObject (hdc, hpen_prev);
	SelectObject (hdc, hbrush_prev);
}

COLORREF _app_mainview_getbackground (
	_In_ PMAINVIEW_CONTEXT context
)
{
	return context->is_dark ? WND_BACKGROUND_CLR : GetSysColor (COLOR_WINDOW);
}

VOID _app_mainview_loadsettings (
	_Inout_ PMAINVIEW_CONTEXT context
)
{
	context->warning_level = _r_calc_clamp (_r_config_getlong (L"TrayLevelWarning", 70), 0, 100);
	context->danger_level = _r_calc_clamp (_r_config_getlong (L"TrayLevelDanger", 90), 0, 100);
	context->warning_clr = _r_config_getulong (L"TrayColorWarning", RGB (0xFF, 0x80, 0x40));
	context->danger_clr = _r_config_getulong (L"TrayColorDanger", RGB (0xEC, 0x1C, 0x24));
}

VOID _app_mainview_destroyframe (
	_Inout_ PMAINVIEW_CONTEXT context
)
{
	if (context->hdc_frame && context->hbitmap_frame_prev)
		SelectObject (context->hdc_frame, context->hbitmap_frame_prev);

	SAFE_DELETE_OBJECT (context->hbitmap_frame);
	SAFE_DELETE_DC (context->hdc_frame);

	context->hbitmap_frame_prev = NULL;
	context->frame_size.cx = 0;
	context->frame_size.cy = 0;
}

VOID _app_mainview_layoutsection (
	_In_ PMAINVIEW_CONTEXT context,
	_In_ LPCRECT section_rect,
	_Out_ PMAINVIEW_SECTION_LAYOUT layout
)
{
	RECT content_rect;
	RECT metric_rect;
	LONG padding_x;
	LONG padding_y;
	LONG bar_height;
	LONG header_height;
	LONG label_height;
	LONG metric_gap;
	LONG metric_width;

	padding_x = _app_mainview_scale (8, context->dpi_value);
	padding_y = max (
		_app_mainview_scale (6, context->dpi_value),
		((section_rect->bottom - section_rect->top) - _app_mainview_scale (76, context->dpi_value)) / 2
	);
	bar_height = _app_mainview_scale (5, context->dpi_value);
	header_height = _app_mainview_scale (20, context->dpi_value);
	label_height = _app_mainview_scale (13, context->dpi_value);
	metric_gap = _app_mainview_scale (8, context->dpi_value);

	RtlZeroMemory (layout, sizeof (MAINVIEW_SECTION_LAYOUT));
	layout->section_rect = *section_rect;

	content_rect = *section_rect;
	content_rect.left += padding_x;
	content_rect.right -= padding_x;
	content_rect.top += padding_y;
	content_rect.bottom -= padding_y;

	layout->title_rect = content_rect;
	layout->title_rect.bottom = min (layout->title_rect.top + header_height, content_rect.bottom);
	layout->title_rect.right -= _app_mainview_scale (78, context->dpi_value);

	layout->usage_rect = content_rect;
	layout->usage_rect.left = max (layout->usage_rect.left, layout->usage_rect.right - _app_mainview_scale (74, context->dpi_value));
	layout->usage_rect.bottom = min (layout->usage_rect.top + header_height, content_rect.bottom);

	layout->bar_rect = content_rect;
	layout->bar_rect.top = min (content_rect.top + header_height + _app_mainview_scale (4, context->dpi_value), content_rect.bottom);
	layout->bar_rect.bottom = min (layout->bar_rect.top + bar_height, content_rect.bottom);

	metric_rect = content_rect;
	metric_rect.top = min (layout->bar_rect.bottom + _app_mainview_scale (6, context->dpi_value), content_rect.bottom);
	metric_width = (metric_rect.right - metric_rect.left - (metric_gap * 2)) / 3;

	if (metric_width <= 0)
		return;

	metric_rect.right = metric_rect.left + metric_width;

	for (INT i = 0; i < 3; i++)
	{
		layout->label_rects[i] = metric_rect;
		layout->label_rects[i].bottom = min (layout->label_rects[i].top + label_height, metric_rect.bottom);

		layout->value_rects[i] = metric_rect;
		layout->value_rects[i].top = layout->label_rects[i].bottom;

		OffsetRect (&metric_rect, metric_width + metric_gap, 0);
	}
}

VOID _app_mainview_drawsectionstatic (
	_In_ PMAINVIEW_CONTEXT context,
	_In_ HDC hdc,
	_In_ PMAINVIEW_SECTION_LAYOUT layout,
	_In_ LPCWSTR title
)
{
	LPCWSTR labels[] = {
		_r_locale_getstring (IDS_ITEM_USED),
		_r_locale_getstring (IDS_ITEM_2),
		_r_locale_getstring (IDS_ITEM_3),
	};

	COLORREF text_clr;
	COLORREF muted_clr;

	text_clr = context->is_dark ? RGB (0xF4, 0xF4, 0xF4) : RGB (0x1C, 0x20, 0x26);
	muted_clr = context->is_dark ? WND_GRAYTEXT_CLR : RGB (0x5B, 0x64, 0x70);

	_app_mainview_drawtext (hdc, title, &layout->title_rect, context->hfont_title, text_clr, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

	for (INT i = 0; i < 3; i++)
		_app_mainview_drawtext (hdc, labels[i], &layout->label_rects[i], context->hfont_detail, muted_clr, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

VOID _app_mainview_drawsectiondynamic (
	_In_ PMAINVIEW_CONTEXT context,
	_In_ HDC hdc,
	_In_ PMAINVIEW_SECTION_LAYOUT layout,
	_In_ PMAIN_MEMORY_STAT stat
)
{
	LPCWSTR values[] = {
		stat->used_text,
		stat->available_text,
		stat->total_text,
	};

	RECT fill_rect;
	COLORREF bg_clr;
	COLORREF bar_clr;
	COLORREF text_clr;
	COLORREF accent_clr;
	LONG bar_height;

	bg_clr = _app_mainview_getbackground (context);
	bar_clr = context->is_dark ? WND_BACKGROUND2_CLR : RGB (0xDF, 0xE3, 0xE8);
	text_clr = context->is_dark ? RGB (0xF4, 0xF4, 0xF4) : RGB (0x1C, 0x20, 0x26);
	accent_clr = context->is_dark ? RGB (0x4A, 0xC2, 0x88) : RGB (0x0D, 0x8A, 0x5A);
	bar_height = layout->bar_rect.bottom - layout->bar_rect.top;

	if (stat->percent >= (ULONG)context->danger_level)
		accent_clr = context->danger_clr;
	else if (stat->percent >= (ULONG)context->warning_level)
		accent_clr = context->warning_clr;

	_r_dc_fillrect (hdc, &layout->usage_rect, bg_clr);
	_app_mainview_drawtext (hdc, stat->usage_text, &layout->usage_rect, context->hfont_value, text_clr, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

	if (bar_height > 0)
	{
		_app_mainview_fillroundrect (hdc, &layout->bar_rect, bar_clr, bar_clr, bar_height);

		fill_rect = layout->bar_rect;
		fill_rect.right = fill_rect.left + MulDiv (fill_rect.right - fill_rect.left, stat->percent, 100);

		if (fill_rect.right > fill_rect.left)
			_app_mainview_fillroundrect (hdc, &fill_rect, accent_clr, accent_clr, bar_height);
	}

	for (INT i = 0; i < 3; i++)
	{
		_r_dc_fillrect (hdc, &layout->value_rects[i], bg_clr);
		_app_mainview_drawtext (hdc, values[i], &layout->value_rects[i], context->hfont_detail, text_clr, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
	}
}

VOID _app_mainview_invalidatedynamic (
	_In_ HWND hwnd,
	_In_ PMAINVIEW_CONTEXT context
)
{
	for (INT i = 0; i < 3; i++)
	{
		InvalidateRect (hwnd, &context->sections[i].usage_rect, FALSE);
		InvalidateRect (hwnd, &context->sections[i].bar_rect, FALSE);

		for (INT j = 0; j < 3; j++)
			InvalidateRect (hwnd, &context->sections[i].value_rects[j], FALSE);
	}
}

VOID _app_mainview_drawdynamic (
	_In_ PMAINVIEW_CONTEXT context
)
{
	PMAIN_MEMORY_STAT stats[] = {
		&context->stats.physical_memory,
		&context->stats.page_file,
		&context->stats.system_cache,
	};

	if (!context->hdc_frame)
		return;

	for (INT i = 0; i < 3; i++)
		_app_mainview_drawsectiondynamic (context, context->hdc_frame, &context->sections[i], stats[i]);
}

BOOLEAN _app_mainview_createframe (
	_In_ HWND hwnd,
	_Inout_ PMAINVIEW_CONTEXT context
)
{
	LPCWSTR titles[] = {
		_r_locale_getstring (IDS_GROUP_1),
		_r_locale_getstring (IDS_GROUP_2),
		_r_locale_getstring (IDS_GROUP_3),
	};

	RECT client_rect;
	RECT section_rect;
	RECT separator_rect;
	RECT frame_rect;
	HDC hdc_window;
	COLORREF separator_clr;
	LONG section_height;
	LONG frame_height;
	LONG gap;

	if (!GetClientRect (hwnd, &client_rect))
		return FALSE;

	if (client_rect.right <= client_rect.left || client_rect.bottom <= client_rect.top)
		return FALSE;

	_app_mainview_destroyframe (context);

	gap = _app_mainview_scale (4, context->dpi_value);
	section_height = ((client_rect.bottom - client_rect.top) - (gap * 4)) / 3;
	section_height = max (1, min (section_height, _app_mainview_scale (104, context->dpi_value)));

	section_rect.left = client_rect.left;
	section_rect.right = client_rect.right;
	section_rect.top = client_rect.top + gap;

	for (INT i = 0; i < 3; i++)
	{
		section_rect.bottom = min (section_rect.top + section_height, client_rect.bottom - gap);
		_app_mainview_layoutsection (context, &section_rect, &context->sections[i]);
		section_rect.top = section_rect.bottom + gap;
	}

	frame_height = min (client_rect.bottom, context->sections[2].section_rect.bottom + gap);

	if (frame_height <= 0)
		return FALSE;

	hdc_window = GetDC (hwnd);

	if (!hdc_window)
		return FALSE;

	context->hdc_frame = CreateCompatibleDC (hdc_window);
	context->hbitmap_frame = CreateCompatibleBitmap (hdc_window, client_rect.right - client_rect.left, frame_height);

	ReleaseDC (hwnd, hdc_window);

	if (!context->hdc_frame || !context->hbitmap_frame)
	{
		_app_mainview_destroyframe (context);
		return FALSE;
	}

	context->hbitmap_frame_prev = SelectObject (context->hdc_frame, context->hbitmap_frame);

	if (!context->hbitmap_frame_prev || context->hbitmap_frame_prev == HGDI_ERROR)
	{
		context->hbitmap_frame_prev = NULL;
		_app_mainview_destroyframe (context);

		return FALSE;
	}

	context->frame_size.cx = client_rect.right - client_rect.left;
	context->frame_size.cy = frame_height;

	SetRect (&frame_rect, 0, 0, context->frame_size.cx, context->frame_size.cy);
	_r_dc_fillrect (context->hdc_frame, &frame_rect, _app_mainview_getbackground (context));

	separator_clr = context->is_dark ? WND_BORDER_CLR : RGB (0xD2, 0xD6, 0xDC);

	for (INT i = 0; i < 3; i++)
	{
		_app_mainview_drawsectionstatic (context, context->hdc_frame, &context->sections[i], titles[i]);

		if (i < 2)
		{
			separator_rect.left = client_rect.left + _app_mainview_scale (8, context->dpi_value);
			separator_rect.right = client_rect.right - _app_mainview_scale (8, context->dpi_value);
			separator_rect.top = context->sections[i].section_rect.bottom + (gap / 2);
			separator_rect.bottom = separator_rect.top + 1;
			_r_dc_fillrect (context->hdc_frame, &separator_rect, separator_clr);
		}
	}

	_app_mainview_drawdynamic (context);

	return TRUE;
}

VOID _app_mainview_paint (
	_In_ HWND hwnd,
	_Inout_ PMAINVIEW_CONTEXT context
)
{
	PAINTSTRUCT ps;
	RECT frame_rect;
	RECT copy_rect;
	HDC hdc;

	hdc = BeginPaint (hwnd, &ps);

	if (!hdc)
		return;

	_r_dc_fillrect (hdc, &ps.rcPaint, _app_mainview_getbackground (context));

	if (!context->hdc_frame)
		_app_mainview_createframe (hwnd, context);

	if (context->hdc_frame)
	{
		SetRect (&frame_rect, 0, 0, context->frame_size.cx, context->frame_size.cy);

		if (IntersectRect (&copy_rect, &ps.rcPaint, &frame_rect))
		{
			BitBlt (
				hdc,
				copy_rect.left,
				copy_rect.top,
				copy_rect.right - copy_rect.left,
				copy_rect.bottom - copy_rect.top,
				context->hdc_frame,
				copy_rect.left,
				copy_rect.top,
				SRCCOPY
			);
		}
	}

	EndPaint (hwnd, &ps);
}

LRESULT CALLBACK _app_mainview_wndproc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	PMAINVIEW_CONTEXT context;

	context = (PMAINVIEW_CONTEXT)GetWindowLongPtrW (hwnd, GWLP_USERDATA);

	switch (msg)
	{
		case WM_NCCREATE:
		{
			LPCREATESTRUCT create = (LPCREATESTRUCT)lparam;

			SetWindowLongPtrW (hwnd, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);

			return TRUE;
		}

		case WM_CREATE:
		{
			context = (PMAINVIEW_CONTEXT)GetWindowLongPtrW (hwnd, GWLP_USERDATA);

			if (context)
			{
				_app_mainview_resetfonts (context, _r_dc_getwindowdpi (hwnd));
				_app_mainview_loadsettings (context);
			}

			return 0;
		}

		case WM_DPICHANGED:
		{
			if (context)
			{
				_app_mainview_resetfonts (context, LOWORD (wparam));
				_app_mainview_destroyframe (context);
				InvalidateRect (hwnd, NULL, FALSE);
			}

			return 0;
		}

		case WM_ERASEBKGND:
		{
			return TRUE;
		}

		case WM_SIZE:
		{
			if (context)
			{
				_app_mainview_destroyframe (context);
				InvalidateRect (hwnd, NULL, FALSE);
			}

			return 0;
		}

		case WM_PAINT:
		{
			if (context)
			{
				_app_mainview_paint (hwnd, context);
				return 0;
			}

			break;
		}

		case WM_NCDESTROY:
		{
			if (context)
			{
				_app_mainview_destroyframe (context);

				SAFE_DELETE_OBJECT (context->hfont_title);
				SAFE_DELETE_OBJECT (context->hfont_value);
				SAFE_DELETE_OBJECT (context->hfont_detail);

				_r_mem_free (context);
				SetWindowLongPtrW (hwnd, GWLP_USERDATA, 0);
			}

			break;
		}
	}

	return DefWindowProcW (hwnd, msg, wparam, lparam);
}

BOOLEAN _app_mainview_registerclass ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	WNDCLASSEX wcex = {0};

	if (_r_initonce_begin (&init_once))
	{
		wcex.cbSize = sizeof (wcex);
		wcex.hCursor = LoadCursorW (NULL, IDC_ARROW);
		wcex.hInstance = _r_sys_getimagebase ();
		wcex.lpfnWndProc = &_app_mainview_wndproc;
		wcex.lpszClassName = MAINVIEW_CLASS_NAME;
		wcex.style = CS_HREDRAW | CS_VREDRAW;

		_r_initonce_end (&init_once);

		return RegisterClassExW (&wcex) != 0;
	}

	return TRUE;
}

HWND _app_mainview_create (
	_In_ HWND hwnd_parent
)
{
	PMAINVIEW_CONTEXT context;
	HWND hwnd;

	if (!_app_mainview_registerclass ())
		return NULL;

	context = _r_mem_allocate (sizeof (MAINVIEW_CONTEXT));
	RtlSecureZeroMemory (context, sizeof (MAINVIEW_CONTEXT));

	context->is_dark = _r_theme_isenabled ();

	hwnd = CreateWindowExW (
		0,
		MAINVIEW_CLASS_NAME,
		NULL,
		WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		0,
		0,
		0,
		0,
		hwnd_parent,
		NULL,
		_r_sys_getimagebase (),
		context
	);

	if (!hwnd)
		_r_mem_free (context);

	return hwnd;
}

VOID _app_mainview_destroy (
	_In_opt_ HWND hwnd
)
{
	if (hwnd && IsWindow (hwnd))
		DestroyWindow (hwnd);
}

VOID _app_mainview_setstats (
	_In_ HWND hwnd,
	_In_ PMAIN_MEMORY_STATS stats
)
{
	PMAINVIEW_CONTEXT context;

	if (!hwnd)
		return;

	context = (PMAINVIEW_CONTEXT)GetWindowLongPtrW (hwnd, GWLP_USERDATA);

	if (!context)
		return;

	context->stats = *stats;

	if (context->hdc_frame)
	{
		_app_mainview_drawdynamic (context);
		_app_mainview_invalidatedynamic (hwnd, context);
	}
	else
	{
		InvalidateRect (hwnd, NULL, FALSE);
	}
}

VOID _app_mainview_settheme (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_dark
)
{
	PMAINVIEW_CONTEXT context;

	if (!hwnd)
		return;

	context = (PMAINVIEW_CONTEXT)GetWindowLongPtrW (hwnd, GWLP_USERDATA);

	if (!context)
		return;

	if (context->is_dark == is_dark)
		return;

	context->is_dark = is_dark;

	_app_mainview_destroyframe (context);
	InvalidateRect (hwnd, NULL, FALSE);
}

VOID _app_mainview_refreshsettings (
	_In_ HWND hwnd
)
{
	PMAINVIEW_CONTEXT context;

	if (!hwnd)
		return;

	context = (PMAINVIEW_CONTEXT)GetWindowLongPtrW (hwnd, GWLP_USERDATA);

	if (!context)
		return;

	_app_mainview_loadsettings (context);

	if (context->hdc_frame)
	{
		_app_mainview_drawdynamic (context);
		_app_mainview_invalidatedynamic (hwnd, context);
	}
	else
	{
		InvalidateRect (hwnd, NULL, FALSE);
	}
}

VOID _app_mainview_resize (
	_In_ HWND hwnd,
	_In_ LPCRECT rect
)
{
	if (!hwnd || !rect)
		return;

	SetWindowPos (hwnd, NULL, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
}

VOID _app_mainview_invalidate (
	_In_ HWND hwnd
)
{
	PMAINVIEW_CONTEXT context;

	if (!hwnd)
		return;

	context = (PMAINVIEW_CONTEXT)GetWindowLongPtrW (hwnd, GWLP_USERDATA);

	if (context)
		_app_mainview_destroyframe (context);

	if (IsWindowVisible (hwnd))
		InvalidateRect (hwnd, NULL, FALSE);
}
