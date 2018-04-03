#include "stdafx.h"
#include "ClicksHooks.h"
#include "GameAnalytics.h"

//extern CONDITION_VARIABLE mouseclick;
extern GameAnalytics ga;

//sources: https://www.unknowncheats.me/wiki/C%2B%2B:WindowsHookEx_Mouse


ClicksHooks::ClicksHooks()
{
}


ClicksHooks::~ClicksHooks()
{
}

 int ClicksHooks::Messsages() {
	 printf_s("START MESSAGE\n");
	 while (msg.message != WM_QUIT) { //while we do not close our application
		 //printf_s("BEFORE PEEK MESSAGE\n");
		 
		 if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			//printf_s("PEEK MESSAGE RECEIVED\n");
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	
		Sleep(1);
	} 
	UninstallHook(); //if we close, let's uninstall our hook
	return (int)msg.wParam; //return the messages
}


void ClicksHooks::InstallHook() {
	/*
	SetWindowHookEx(
	WM_MOUSE_LL = mouse low level hook type,
	MyMouseCallback = our callback function that will deal with system messages about mouse
	NULL, 0);

	c++ note: we can check the return SetWindowsHookEx like this because:
	If it return NULL, a NULL value is 0 and 0 is false.
	*/

	/*
	HWND hwnd = FindWindow(NULL, L"Microsoft Solitaire Collection - Firefox Developer Edition");
	if (hwnd == NULL)
	{
		std::cout << "Cant find window" << std::endl;
		exit(EXIT_FAILURE);
	}
	DWORD threadId = 0; // GetWindowThreadProcessId(hwnd, NULL);
	//hinstDLL = LoadLibrary(TEXT("c:\\"))

	*/

	//HHOOK hook = ::SetWindowsHookEx(WH_CBT, HookCBTProc, hInst, threadId);

	if (!(hook = SetWindowsHookEx(WH_MOUSE_LL, MyMouseCallback, NULL, 0))) {
		//std::cout << "Error:" << GetLastError() << std::endl;
		printf_s("Error: %x \n", GetLastError());
	}
}

void ClicksHooks::UninstallHook() {
	/*
	uninstall our hook using the hook handle
	*/
	UnhookWindowsHookEx(hook);
}



LRESULT WINAPI MyMouseCallback(int nCode, WPARAM wParam, LPARAM lParam) {

	if (nCode == 0) {

		MSLLHOOKSTRUCT * pMouseStruct = (MSLLHOOKSTRUCT *)lParam;
		//std::cout << wParam << std::endl;
		//printf_s("CALLBACK");

		switch (wParam) {
			
		case WM_LBUTTONUP:
			//std::cout << "LEFT UP X:" << pMouseStruct->pt.x << " Y: " << pMouseStruct->pt.y << std::endl; 
			printf_s("LEFT CLICK UP: x = %i | y = %i \n", pMouseStruct->pt.x, pMouseStruct->pt.y);
			//WakeConditionVariable(&mouseclick);
			ga.bufferImage(pMouseStruct->pt.x, pMouseStruct->pt.y);
			break;

		case WM_LBUTTONDOWN:			
			//std::cout << "LEFT DOWN X:" << pMouseStruct->pt.x << " Y: " << pMouseStruct->pt.y << std::endl;
			//printf_s("LEFT CLICK DOWN: x = %i | y = %i \n", pMouseStruct->pt.x, pMouseStruct->pt.y);
			break;

		case WM_RBUTTONUP:
			//std::cout << "RIGHT UP X:" << pMouseStruct->pt.x << " Y: " << pMouseStruct->pt.y << std::endl;
			//printf_s("RIGHT CLICK UP: x = %i | y = %i \n", pMouseStruct->pt.x, pMouseStruct->pt.y);
			break;

		case WM_RBUTTONDOWN:
			//std::cout << "RIGHT DOWN X:" << pMouseStruct->pt.x << " Y: " << pMouseStruct->pt.y << std::endl;
			//printf_s("RIGHT CLICK DOWN: x = %i | y = %i \n", pMouseStruct->pt.x, pMouseStruct->pt.y);
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