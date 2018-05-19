#pragma once
#include <Windows.h>
#include <iostream>
#include <ctime>
#include <chrono>

class ClicksHooks
{
public:
	ClicksHooks();
	~ClicksHooks();

	//single ton
	static ClicksHooks& Instance() {
		static ClicksHooks myHook;
		return myHook;
	}
	void InstallHook(); // function to install our hook
	void UninstallHook(); // function to uninstall our hook
						  // struct with information about all messages in our queue
	int Messages(); // function to "deal" with our messages
	MSG msg;
	HHOOK hook; // handle to the hook	
};

LRESULT WINAPI MyMouseCallback(int nCode, WPARAM wParam, LPARAM lParam); // callback declaration
