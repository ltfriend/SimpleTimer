#include "pch.h"
#include "resource.h"
#include "TimerWnd.h"

#define TIMERWND_CLASS_NAME _T("TIMERWND")
#define TIME_TEXT_SIZE 10
#define TIMER_ID 1

typedef struct tagTIMERMENUITEM {
	UINT wID;
	UINT fType;
	UINT fState;
	WCHAR szText[MAX_LOADSTRING];
	RECT rect;
} TIMERMENUITEM, *LPTIMERMENUITEM;

typedef struct tagTIMERMENU
{
	int nCount;
	LPTIMERMENUITEM lpItems;
	HFONT hFont;
	HFONT hBoldFont;
	LPTIMERMENUITEM lpSelectedItem;
} TIMERMENU, *LPTIMERMENU;

extern WCHAR szStartTimerText[MAX_LOADSTRING];
extern WCHAR szResumeTimerText[MAX_LOADSTRING];
extern WCHAR szPauseTimerText[MAX_LOADSTRING];

extern HWND hAPCWindow;

extern BOOL bTimerIsRun;
extern ULONGLONG uStartTime;
extern ULONGLONG uTotalTime;

HWND hTimerWnd = nullptr;
TIMERMENU timerMenu;
BOOL bIsTracking = false;

LRESULT CALLBACK TimerWndProc(HWND, UINT, WPARAM, LPARAM);

void DrawTimerMenu(HDC, RECT);
void DrawMenuItem(HDC, LPTIMERMENUITEM, HFONT);

LPTIMERMENUITEM GetTimerMenuItemFromPoint(POINT);
LPTIMERMENUITEM GetTimerMenuItemFromID(UINT);

void TimeToString(ULONGLONG, LPWSTR);
void SetTimerMenuItemText(LPTIMERMENUITEM, LPWSTR, BOOL);

ATOM RegisterTimerWndClass(HINSTANCE hInstance) {
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEXW);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = TimerWndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = nullptr;
	wcex.hIconSm = nullptr;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_MENU + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = TIMERWND_CLASS_NAME;
	
	return RegisterClassExW(&wcex);
}

LRESULT CALLBACK TimerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);

		DrawTimerMenu(hdc, ps.rcPaint);

		EndPaint(hWnd, &ps);
	}
	break;
	case WM_MOUSEMOVE:
	{
		if (!bIsTracking) {
			bIsTracking = true;

			TRACKMOUSEEVENT tme;
			tme.cbSize = sizeof(tme);
			tme.dwFlags = TME_LEAVE;
			tme.hwndTrack = hTimerWnd;

			TrackMouseEvent(&tme);
		}

		POINT pt;
		GetCursorPos(&pt);
		ScreenToClient(hTimerWnd, &pt);

		LPTIMERMENUITEM lpSelectedItem = GetTimerMenuItemFromPoint(pt);
		if (lpSelectedItem != timerMenu.lpSelectedItem) {
			if (timerMenu.lpSelectedItem != nullptr) {
				timerMenu.lpSelectedItem->fState &= ~ODS_SELECTED;
				InvalidateRect(hWnd, &timerMenu.lpSelectedItem->rect, false);
			}
			if (lpSelectedItem != nullptr) {
				lpSelectedItem->fState |= ODS_SELECTED;
				InvalidateRect(hWnd, &lpSelectedItem->rect, false);
			}

			timerMenu.lpSelectedItem = lpSelectedItem;
		}
	}
	break;
	case WM_LBUTTONUP:
		if (timerMenu.lpSelectedItem != nullptr) {
			if (hAPCWindow != nullptr && !(timerMenu.lpSelectedItem->fState & ODS_DISABLED))
				SendMessageW(hAPCWindow, WM_COMMAND, timerMenu.lpSelectedItem->wID, 0);
		}
		break;
	case WM_MOUSELEAVE:
		bIsTracking = false;
		if (timerMenu.lpSelectedItem != nullptr) {
			timerMenu.lpSelectedItem->fState &= ~ODS_SELECTED;
			InvalidateRect(hWnd, &timerMenu.lpSelectedItem->rect, false);
			timerMenu.lpSelectedItem = nullptr;
		}
		break;
	case WM_KILLFOCUS:
		HideTimerWnd();
		break;
	case WM_TIMER:
		SetTimerText(true);
		break;
	case WM_DESTROY:
	{
		if (bTimerIsRun)
			StopTimerEvent();

		delete[]timerMenu.lpItems;

		DeleteObject(timerMenu.hFont);
		DeleteObject(timerMenu.hBoldFont);
	}
	break;
	default:
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}

	return 0L;
}

void LoadTimerMenu(HINSTANCE hInstance) {
	HMENU hMenu = LoadMenuW(hInstance, MAKEINTRESOURCEW(IDM_MAIN));
	HMENU hSubMenu = GetSubMenu(hMenu, 0);

	NONCLIENTMETRICSW ncm;
	memset(&ncm, 0, sizeof(ncm));
	ncm.cbSize = sizeof(ncm);
	SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, 0, (PVOID)& ncm, 0);

	timerMenu.nCount = GetMenuItemCount(hSubMenu);
	timerMenu.lpItems = new TIMERMENUITEM[timerMenu.nCount];
	timerMenu.lpSelectedItem = nullptr;

	timerMenu.hFont = CreateFontIndirectW(&ncm.lfMenuFont);

	ncm.lfMenuFont.lfWeight = 800;
	timerMenu.hBoldFont = CreateFontIndirectW(&ncm.lfMenuFont);

	LPTIMERMENUITEM lpMenuItem = timerMenu.lpItems;

	for (int itemNum = 0; itemNum < timerMenu.nCount; itemNum++, lpMenuItem++) {
		MENUITEMINFO mii;
		mii.cbSize = sizeof(MENUITEMINFO);
		mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_STRING;
		mii.dwTypeData = lpMenuItem->szText;
		mii.cch = MAX_LOADSTRING;

		GetMenuItemInfoW(hSubMenu, itemNum, TRUE, &mii);

		lpMenuItem->wID = mii.wID;
		lpMenuItem->fType = mii.fType;
		lpMenuItem->fState = mii.fState;
	}

	SetTimerText(false);
	SetStartStopTimerText(false);
	SetAvailabilityResetMenuItem(false);

	DestroyMenu(hMenu);
}

void TimeToString(ULONGLONG uTime, LPWSTR lpszTimeString) {
	ULONGLONG hours = uTime / 3600000;
	uTime %= 3600000;
	ULONGLONG mins = uTime / 60000;
	uTime %= 60000;
	ULONGLONG secs = uTime / 1000;

	swprintf_s(lpszTimeString, TIME_TEXT_SIZE, _T("%02llu:%02llu:%02llu"), hours, mins, secs);
}

SIZE MeasureMenuItem(LPTIMERMENUITEM lpMenuItem, HFONT hFont) {
	SIZE itemSize;

	if (lpMenuItem->fType & MFT_SEPARATOR) {
		itemSize.cx = 100;
		itemSize.cy = 10;
		return itemSize;
	}

	LPWSTR lpszText = lpMenuItem->szText;
	if (lpszText[0] != _T('\0')) {
		HDC hdc = GetDC(hTimerWnd);
		HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
		DWORD dwSize = GetTabbedTextExtentW(hdc, lpszText, (int)wcslen(lpszText), 0, nullptr);

		itemSize.cx = LOWORD(dwSize) + 17;
		itemSize.cy = 21;
		if (HIWORD(dwSize) > itemSize.cy)
			itemSize.cy = HIWORD(dwSize);

		SelectObject(hdc, hOldFont);
		ReleaseDC(hTimerWnd, hdc);
	}
	else {
		itemSize.cx = 100;
		itemSize.cy = 21;
	}

	return itemSize;
}

SIZE MeasureTimerMenu() {
	int left = 3;
	int y = 3;
	int width = 0;

	LPTIMERMENUITEM lpMenuItem = timerMenu.lpItems;
	for (int i = 0; i < timerMenu.nCount; i++, lpMenuItem++) {
		SIZE szItemSize = MeasureMenuItem(lpMenuItem, timerMenu.hFont);

		lpMenuItem->rect.left = left;
		lpMenuItem->rect.top = y;
		lpMenuItem->rect.bottom = lpMenuItem->rect.top + szItemSize.cy - 1;

		y += szItemSize.cy;
		if (width < szItemSize.cx)
			width = szItemSize.cx;
	}

	// Set same width for all menu items.
	lpMenuItem = timerMenu.lpItems;
	for (int i = 0; i < timerMenu.nCount; i++, lpMenuItem++) {
		lpMenuItem->rect.right = lpMenuItem->rect.left + width - 1;
	}

	SIZE szMenuSize;
	szMenuSize.cx = left + width + 2;
	szMenuSize.cy = y + 2;

	return szMenuSize;
}

void SetupTimerWnd(HINSTANCE hInstance) {
	LoadTimerMenu(hInstance);

	SIZE menuSize = MeasureTimerMenu();

	POINT pos;
	GetCursorPos(&pos);

	int left = pos.x;
	int top = pos.y - menuSize.cy;

	SetWindowPos(hTimerWnd, HWND_TOP, left, top, menuSize.cx, menuSize.cy, 0);
}

BOOL ShowTimerWnd(HINSTANCE hInstance) {
	if (hTimerWnd == nullptr) {
		hTimerWnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, TIMERWND_CLASS_NAME, nullptr, WS_POPUP,
			0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);

		if (!hTimerWnd) {
			return FALSE;
		}

		SetupTimerWnd(hInstance);
	}

	ShowWindow(hTimerWnd, SW_SHOW);
	UpdateWindow(hTimerWnd);

	SetForegroundWindow(hTimerWnd);

	if (bTimerIsRun)
		StartTimerEvent();

	return TRUE;
}

void HideTimerWnd() {
	if (hTimerWnd == nullptr)
		return;

	DestroyWindow(hTimerWnd);
	hTimerWnd = nullptr;
}

void StartTimerEvent() {
	SetTimer(hTimerWnd, TIMER_ID, 1000, nullptr);
}

void StopTimerEvent() {
	KillTimer(hTimerWnd, TIMER_ID);
}

void FillSolidRect(HDC hdc, LPCRECT lpRect, COLORREF clr) {
	SetBkColor(hdc, clr);
	ExtTextOutW(hdc, 0, 0, ETO_OPAQUE, lpRect, nullptr, 0, nullptr);
}

void DrawHorizontalLine(HDC hdc, int startX, int endX, int y, COLORREF color) {
	HPEN hPen = CreatePen(PS_SOLID, 1, color);
	HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

	MoveToEx(hdc, startX, y, nullptr);
	LineTo(hdc, endX, y);

	SelectObject(hdc, hOldPen);
	DeleteObject(hPen);
}

void DrawRect(HDC hdc, RECT rect, COLORREF color) {
	HPEN hPen = CreatePen(PS_SOLID, 1, color);
	HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

	MoveToEx(hdc, rect.left, rect.top, nullptr);
	LineTo(hdc, rect.right, rect.top);
	LineTo(hdc, rect.right, rect.bottom);
	LineTo(hdc, rect.left, rect.bottom);
	LineTo(hdc, rect.left, rect.top);

	SelectObject(hdc, hOldPen);
	DeleteObject(hPen);
}

void DrawMenuItem(HDC hdc, LPTIMERMENUITEM lpMenuItem, HFONT hFont) {
	RECT rect = lpMenuItem->rect;
	RECT rcImage = rect;
	RECT rcText = rect;

	HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

	rcImage.right = rcImage.left + rect.bottom - rect.top;
	rcText.left += rcImage.right + 1;

	SetBkMode(hdc, TRANSPARENT);

	if (lpMenuItem->fState & ODS_SELECTED) {
		FillSolidRect(hdc, &rect, GetSysColor(COLOR_MENUHILIGHT));
		SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
	}
	else {
		FillSolidRect(hdc, &rect, GetSysColor(COLOR_MENU));
		SetTextColor(hdc, GetSysColor(COLOR_MENUTEXT));
	}

	{
		SIZE sz;
		GetTextExtentPoint32W(hdc, lpMenuItem->szText, (int)wcslen(lpMenuItem->szText), &sz);
		int ay1 = (rcText.bottom - rcText.top - sz.cy) / 2;
		rcText.top += ay1;
		rcText.left += 2;
		rcText.right -= 15;
	}

	if (lpMenuItem->fState & ODS_DISABLED) {
		if (!(lpMenuItem->fState & ODS_SELECTED)) {
			RECT rcText1 = rcText;
			InflateRect(&rcText1, -1, -1);
			SetTextColor(hdc, GetSysColor(COLOR_3DHILIGHT));
			DrawTextW(hdc, lpMenuItem->szText, (int)wcslen(lpMenuItem->szText), &rcText1,
				DT_VCENTER | DT_LEFT | DT_EXPANDTABS);
			SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
			DrawTextW(hdc, lpMenuItem->szText, (int)wcslen(lpMenuItem->szText), &rcText,
				DT_VCENTER | DT_LEFT | DT_EXPANDTABS);
		}
		else {
			SetTextColor(hdc, GetSysColor(COLOR_MENU));
			DrawTextW(hdc, lpMenuItem->szText, (int)wcslen(lpMenuItem->szText), &rcText,
				DT_VCENTER | DT_LEFT | DT_EXPANDTABS);
		}
	}
	else {
		DrawTextW(hdc, lpMenuItem->szText, (int)wcslen(lpMenuItem->szText), &rcText,
			DT_VCENTER | DT_LEFT | DT_EXPANDTABS);
	}

	SelectObject(hdc, hOldFont);
}

void DrawMenuSeparator(HDC hdc, RECT rect) {
	int y = rect.top + 3;
	DrawHorizontalLine(hdc, rect.left + 3, rect.right - 3, y, GetSysColor(COLOR_3DSHADOW));
	DrawHorizontalLine(hdc, rect.left + 3, rect.right - 3, y + 1, GetSysColor(COLOR_3DHILIGHT));
}

void DrawTimerMenu(HDC hdc, RECT rcPaint) {

	LPTIMERMENUITEM lpMenuItem = timerMenu.lpItems;
	for (int i = 0; i < timerMenu.nCount; i++, lpMenuItem++) {
		RECT rect;
		if (!IntersectRect(&rect, &lpMenuItem->rect, &rcPaint))
			continue;

		if (lpMenuItem->fType & MFT_SEPARATOR)
			DrawMenuSeparator(hdc, lpMenuItem->rect);
		else
			DrawMenuItem(hdc, lpMenuItem, lpMenuItem->wID == ID_TIME ?
				timerMenu.hBoldFont : timerMenu.hFont);
	}

	RECT rect;
	GetWindowRect(hTimerWnd, &rect);

	rect.right = rect.right - rect.left - 1;
	rect.bottom = rect.bottom - rect.top - 1;
	rect.left = 0;
	rect.top = 0;
	DrawRect(hdc, rect, GetSysColor(COLOR_3DSHADOW));
}

LPTIMERMENUITEM GetTimerMenuItemFromPoint(POINT point) {
	LPTIMERMENUITEM lpMenuItem = timerMenu.lpItems;
	for (int i = 0; i < timerMenu.nCount; i++, lpMenuItem++) {
		if (PtInRect(&lpMenuItem->rect, point)) {
			return lpMenuItem->wID == ID_TIME || lpMenuItem->fState & ODS_DISABLED ?
				nullptr : lpMenuItem;
		}
	}

	return nullptr;
}

LPTIMERMENUITEM GetTimerMenuItemFromID(UINT wID) {
	LPTIMERMENUITEM lpMenuItem = timerMenu.lpItems;
	for (int i = 0; i < timerMenu.nCount; i++, lpMenuItem++) {
		if (lpMenuItem->wID == wID)
			return lpMenuItem;
	}

	return nullptr;
}

void SetTimerMenuItemText(LPTIMERMENUITEM lpMenuItem, LPWSTR lpszText, BOOL bInvalidate) {
	wcscpy_s(lpMenuItem->szText, MAX_LOADSTRING, lpszText);

	if (bInvalidate)
		InvalidateRect(hTimerWnd, &lpMenuItem->rect, false);
}

void SetStartStopTimerText(BOOL bInvalidate) {
	LPWSTR lpszStartStopTimerText;
	if (bTimerIsRun) {
		lpszStartStopTimerText = szPauseTimerText;
	}
	else {
		lpszStartStopTimerText = uTotalTime > 0 ?
			szResumeTimerText : szStartTimerText;
	}

	LPTIMERMENUITEM lpMenuItem = GetTimerMenuItemFromID(ID_START_PAUSE);
	SetTimerMenuItemText(lpMenuItem, lpszStartStopTimerText, bInvalidate);
}

void SetTimerText(BOOL bValidate) {
	WCHAR szTimeText[TIME_TEXT_SIZE] = _T("00:00:00");

	if (bTimerIsRun) {
		ULONGLONG uCurrentTime = uTotalTime + GetTickCount64() - uStartTime;
		TimeToString(uCurrentTime, szTimeText);
	}
	else if (uTotalTime > 0) {
		TimeToString(uTotalTime, szTimeText);
	}

	LPTIMERMENUITEM lpMenuItem = GetTimerMenuItemFromID(ID_TIME);
	SetTimerMenuItemText(lpMenuItem, szTimeText, bValidate);
}

void SetAvailabilityResetMenuItem(BOOL bInvalidate) {
	LPTIMERMENUITEM lpMenuItem = GetTimerMenuItemFromID(ID_RESET);

	if (bTimerIsRun || uTotalTime > 0)
		lpMenuItem->fState &= ~ODS_DISABLED;
	else
		lpMenuItem->fState |= ODS_DISABLED;

	if (bInvalidate)
		InvalidateRect(hTimerWnd, &lpMenuItem->rect, false);
}