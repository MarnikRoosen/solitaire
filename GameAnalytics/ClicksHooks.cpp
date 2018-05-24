#include "stdafx.h"
#include "ClicksHooks.h"
#include "GameAnalytics.h"

//extern CONDITION_VARIABLE mouseclick;
extern GameAnalytics ga;
std::chrono::time_point<std::chrono::steady_clock> clickUpTimer;
//std::chrono::time_point<std::chrono::steady_clock> clickDownTimer;	// TODO: implementation for detecting rank/suit errors for dragged card moves
bool clickDown = false;

//sources: https://www.unknowncheats.me/wiki/C%2B%2B:WindowsHookEx_Mouse


ClicksHooks::ClicksHooks()
{
}


ClicksHooks::~ClicksHooks()
{
}

 int ClicksHooks::Messages() {
	 while (msg.message != WM_QUIT && !ga.getEndOfGameBool())
	 {
		 // while we do not close our application
		 if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		 /*if (clickDown && std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - clickDownTimer).count() > 250)	// TODO: implementation for detecting rank/suit errors for dragged card moves
		 {
			 clickDown = false;
			 std::cout << "Moving the card!" << std::endl;
		 }*/
	
		Sleep(1);
	} 
	UninstallHook(); // if we close, let's uninstall our hook
	return (int) msg.wParam; // return the messages
}


void ClicksHooks::InstallHook()
{
	if (!(hook = SetWindowsHookEx(WH_MOUSE_LL, MyMouseCallback, NULL, 0)))
	{
		std::cout << "Error:" << GetLastError() << std::endl;
	}
}

void ClicksHooks::UninstallHook()
{
	UnhookWindowsHookEx(hook);	// Uninstall our hook using the hook handle
}



LRESULT WINAPI MyMouseCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == 0) {

		MSLLHOOKSTRUCT * pMouseStruct = (MSLLHOOKSTRUCT *)lParam;

		switch (wParam) {
		case WM_LBUTTONUP:
			/*if (std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - clickDownTimer).count() < 250)	// TODO: implementation for detecting rank/suit errors for dragged card moves
			{
				clickDown = false;
			}*/
			clickUpTimer = Clock::now();
			ga.addCoordinatesToBuffer(pMouseStruct->pt.x, pMouseStruct->pt.y);

			break;

		case WM_LBUTTONDOWN:
			//clickDownTimer = Clock::now();	// TODO: implementation for detecting rank/suit errors for dragged card moves
			//clickDown = true;
			if (std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - clickUpTimer).count() > 300)
			{
				ga.toggleClickDownBool();
 			}
			break;

		case WM_RBUTTONUP:
			break;

		case WM_RBUTTONDOWN:
			break;
		}
	}

	/*
	Every time that the nCode is less than 0 we need to CallNextHookEx:
	-> Pass to the next hook
	MSDN: Calling CallNextHookEx is optional, but it is highly recommended;
	otherwise, other applications that have installed hooks will not receive hook notifications and may behave incorrectly as a result.
	*/
	return CallNextHookEx(ClicksHooks::Instance().hook, nCode, wParam, lParam);
}