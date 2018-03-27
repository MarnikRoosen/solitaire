#pragma once
#include <Windows.h>
#include <iostream>

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
	HHOOK hook; // handle to the hook	
	void InstallHook(); // function to install our hook
	void UninstallHook(); // function to uninstall our hook

	MSG msg;

	// struct with information about all messages in our queue
	int Messsages(); // function to "deal" with our messages 
};

LRESULT WINAPI MyMouseCallback(int nCode, WPARAM wParam, LPARAM lParam); //callback declaration
