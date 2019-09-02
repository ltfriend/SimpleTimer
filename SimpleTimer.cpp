// SimpleTimer.cpp : Определяет точку входа для приложения.
//

#include "pch.h"
#include <shellapi.h>
#include "resource.h"
#include "TimerWnd.h"

#define TIME_TEXT_SIZE 10
#define NOTIFYICON_ID 51102
#define WM_ICON_NOTIFY (WM_USER + 5001)

HINSTANCE hInst;
HWND hAPCWindow;
HWND hAboutDlg = nullptr;
NOTIFYICONDATA* ptnd;

BOOL bTimerIsRun = FALSE;
ULONGLONG uStartTime;
ULONGLONG uTotalTime = 0;

WCHAR szStartTimerText[MAX_LOADSTRING];
WCHAR szResumeTimerText[MAX_LOADSTRING];
WCHAR szPauseTimerText[MAX_LOADSTRING];
WCHAR szTimeTooltip[MAX_LOADSTRING];
WCHAR szTimeStateRun[MAX_LOADSTRING];
WCHAR szTimeStatePause[MAX_LOADSTRING];
WCHAR szTimeStateStop[MAX_LOADSTRING];

extern HWND hTimerWnd;

LRESULT CALLBACK APCWndProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK AboutDlgProc(HWND, UINT, WPARAM, LPARAM);

BOOL InitInstance(HINSTANCE);
void LoadStrings(HINSTANCE);
void DestroyApp();

void SetTrayNotifyTooltip(BOOL);

void StartTimer();
void PauseTimer();
void ResetTimer();

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nCmdShow) {

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	LoadStrings(hInstance);

	RegisterTimerWndClass(hInstance);

	if (!InitInstance(hInstance)) {
		return FALSE;
	}

	MSG msg;

	while (GetMessageW(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	DestroyApp();

	return (int)msg.wParam;

}

BOOL InitInstance(HINSTANCE hInstance) {
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif // _DEBUG

	hInst = hInstance;

	hAPCWindow = CreateWindowExW(0L, _T("Static"), nullptr, 0, 0, 0, 0, 0,
		nullptr, nullptr, nullptr, nullptr);
	SetWindowLongW(hAPCWindow, GWL_WNDPROC, (LONG)(LONG_PTR) APCWndProc);

	ptnd = new NOTIFYICONDATA;
	ptnd->cbSize = sizeof(NOTIFYICONDATA);
	ptnd->hWnd = hAPCWindow;
	ptnd->uID = NOTIFYICON_ID;
	ptnd->hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_STOP));
	ptnd->uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	ptnd->uCallbackMessage = WM_ICON_NOTIFY;

	SetTrayNotifyTooltip(false);

	Shell_NotifyIconW(NIM_ADD, ptnd);

	return TRUE;
}

void LoadStrings(HINSTANCE hInstance) {
	LoadStringW(hInstance, IDS_START_ITEM, szStartTimerText, MAX_LOADSTRING);
	LoadStringW(hInstance, IDS_RESUME_ITEM, szResumeTimerText, MAX_LOADSTRING);
	LoadStringW(hInstance, IDS_PAUSE_ITEM, szPauseTimerText, MAX_LOADSTRING);
	LoadStringW(hInstance, IDS_TIME_TOOLTIP, szTimeTooltip, MAX_LOADSTRING);
	LoadStringW(hInstance, IDS_TIME_STATE_RUN, szTimeStateRun, MAX_LOADSTRING);
	LoadStringW(hInstance, IDS_TIME_STATE_PAUSE, szTimeStatePause, MAX_LOADSTRING);
	LoadStringW(hInstance, IDS_TIME_STATE_STOP, szTimeStateStop, MAX_LOADSTRING);
}

void DestroyApp() {
	if (bTimerIsRun)
		KillTimer(hAPCWindow, 1);

	Shell_NotifyIconW(NIM_DELETE, ptnd);
	DeleteObject(ptnd->hIcon);

	delete ptnd;
	ptnd = nullptr;

	if (hAboutDlg != nullptr)
		DestroyWindow(hAboutDlg);

	HideTimerWnd();
	DestroyWindow(hAPCWindow);
}

LRESULT OnTrayNotification(WPARAM wParam, LPARAM lParam) {
	if (wParam != ptnd->uID)
		return 0L;

	switch (LOWORD(lParam)) {
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
		if (hTimerWnd == nullptr)
			ShowTimerWnd(hInst);
		else
			HideTimerWnd();
		break;
	break;
	}

	return 1L;
}

LRESULT CALLBACK APCWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (ptnd != nullptr && message == ptnd->uCallbackMessage) {
		OnTrayNotification(wParam, lParam);
	}
	else {
		switch (message)
		{
		case WM_NULL:
			SleepEx(0, TRUE);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
			case ID_START_PAUSE:
				if (bTimerIsRun)
					PauseTimer();
				else
					StartTimer();

				break;
			case ID_RESET:
				ResetTimer();
				break;
			case ID_ABOUT:
			{
				if (hAboutDlg != nullptr) {
					ShowWindow(hAboutDlg, SW_RESTORE);
					SetForegroundWindow(hAboutDlg);
				}
				else {
					hAboutDlg = CreateDialogParamW(hInst, MAKEINTRESOURCEW(IDD_ABOUT), nullptr, AboutDlgProc, 0L);
					ShowWindow(hAboutDlg, SW_SHOW);
				}
			}
			break;
			case ID_APP_EXIT:
				PostQuitMessage(0);
				return 1L;
			default:
				return DefWindowProcW(hWnd, message, wParam, lParam);
			}
			break;
		case WM_TIMER:
			SetTrayNotifyTooltip(true);
			break;
		default:
			return DefWindowProcW(hWnd, message, wParam, lParam);
		}
	}

	return 0L;
}

void SetTrayNotifyIcon(int nIconID) {
	HICON hOldIcon = ptnd->hIcon;

	ptnd->hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(nIconID));
	Shell_NotifyIconW(NIM_MODIFY, ptnd);

	DeleteObject(hOldIcon);
}

void SetTrayNotifyTooltip(BOOL bUpdate) {
	ULONGLONG uCurrentTime;
	LPWSTR lpszState;

	if (bTimerIsRun) {
		uCurrentTime = uTotalTime + GetTickCount64() - uStartTime;
		lpszState = szTimeStateRun;
	}
	else if (uTotalTime > 0) {
		uCurrentTime = uTotalTime;
		lpszState = szTimeStatePause;
	}
	else {
		uCurrentTime = 0;
		lpszState = szTimeStateStop;
	}

	ULONGLONG hours = uCurrentTime / 3600000;
	uCurrentTime %= 3600000;
	ULONGLONG mins = uCurrentTime / 60000;

	WCHAR szTimeState[MAX_LOADSTRING];
	swprintf_s(szTimeState, MAX_LOADSTRING, szTimeTooltip, hours, mins, lpszState);

	wcscpy_s(ptnd->szTip, SIZEOF(ptnd->szTip), szTimeState);

	if (bUpdate)
		Shell_NotifyIconW(NIM_MODIFY, ptnd);
}

void StartTimer() {
	bTimerIsRun = true;
	uStartTime = GetTickCount64();

	SetTrayNotifyIcon(IDI_START);

	HideTimerWnd();

	SetTrayNotifyTooltip(true);
	SetTimer(hAPCWindow, 1, 60000, nullptr);
}

void PauseTimer() {
	bTimerIsRun = false;
	uTotalTime += GetTickCount64() - uStartTime;

	SetTrayNotifyIcon(IDI_PAUSE);

	SetStartStopTimerText(true);
	SetAvailabilityResetMenuItem(true);

	StopTimerEvent();
	KillTimer(hAPCWindow, 1);
	SetTrayNotifyTooltip(true);
}

void ResetTimer() {
	bTimerIsRun = false;
	uTotalTime = 0;

	SetTrayNotifyIcon(IDI_STOP);

	SetTimerText(true);
	SetStartStopTimerText(true);
	SetAvailabilityResetMenuItem(true);

	StopTimerEvent();
	KillTimer(hAPCWindow, 1);
	SetTrayNotifyTooltip(true);
}

BOOL CALLBACK AboutDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			DestroyWindow(hAboutDlg);
			hAboutDlg = nullptr;
			return TRUE;
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hAboutDlg);
		hAboutDlg = nullptr;
		return TRUE;
	}
	return FALSE;
}