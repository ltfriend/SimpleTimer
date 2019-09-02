#pragma once

ATOM RegisterTimerWndClass(HINSTANCE);

BOOL ShowTimerWnd(HINSTANCE);
void HideTimerWnd();

void SetStartStopTimerText(BOOL);
void SetTimerText(BOOL);
void SetAvailabilityResetMenuItem(BOOL);

void StartTimerEvent();
void StopTimerEvent();
