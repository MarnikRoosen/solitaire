#pragma once

#include "stdafx.h"
#include "ClassifyCard.h"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/core.hpp"
#include <opencv2/opencv.hpp>
#include "opencv2/features2d.hpp"
#include "opencv2/imgcodecs.hpp"

#include <vector>
#include <utility>
#include <Windows.h>
#include <iostream>
#include <string>

#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "User32.lib")

using namespace cv;
using namespace std;

Mat hwnd2mat(HWND hwnd);

struct cardLocation
{
	int unknownCards;
	int knownCards;
	std::pair<classifiers, classifiers> topCard;
};

class GameAnalytics
{
public:
	GameAnalytics();
	void initializeVariables();
	void updateBoard(ClassifyCard &cc);

private:
	std::vector<cv::Mat> topCards;
	std::vector<std::pair<classifiers, classifiers>> playingCards;
	std::pair<classifiers, classifiers> cardType;
	std::vector<cardLocation> playingBoard;
	std::set<std::pair<classifiers, classifiers>> playingBoard2;

};

/*
* Function: void CallBackFunc(int event, int x, int y, int flags, void* userdata)
* Description: This function will be called on any mouse event (moving mouse over window or clicking with left/right/middle mouse button)
* Parameters: event    = EVENT_MOUSEMOVE, EVENT_LBUTTONDOWN, EVENT_RBUTTONDOWN, EVENT_MBUTTONDOWN, EVENT_LBUTTONUP,
EVENT_RBUTTONUP, EVENT_MBUTTONUP, EVENT_LBUTTONDBLCLK, EVENT_RBUTTONDBLCLK, EVENT_MBUTTONDBLCLK
x,y	   = Coordinates of the event
flags    = Specific event whenever the mouse event occurs
EVENT_FLAG_LBUTTON, EVENT_FLAG_RBUTTON, EVENT_FLAG_MBUTTON, EVENT_FLAG_CTRLKEY, EVENT_FLAG_SHIFTKEY, EVENT_FLAG_ALTKEY
userdata = pointer for global event tracking, storing events in a given list,...
* Return: Mouse event with coordinate (0,0 is the left top corner)
*/

void CallBackFunc(int event, int x, int y, int flags, void* userdata);

/*
* Function: void getApplicationView();
* Description: This function will open the desktop background in a new window
* Parameters: /
* Return: /
*/

void getApplicationView();

