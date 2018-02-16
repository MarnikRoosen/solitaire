#pragma once
#include "stdafx.h"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/core/core.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <utility>
#include <Windows.h>
#include <iostream>
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "User32.lib")
using namespace cv;
using namespace std;

Mat hwnd2mat(HWND hwnd);
Size standardRankSize;
Size standardCardSize;
//Mat rank2, rank3, rank4, rank5, rank6, rank7, rank8, rank9, rank10, rankJ, rankQ, rankK, rankA;
vector<Mat> ranks;

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
int main(int argc, char ** argv);
Mat detectCard();
void getApplicationView();
std::pair<Mat, Mat> getCardCharacteristics(Mat aCard);
void classifyCard(std::pair<Mat, Mat> cardCharacteristics);
void classifyCard2(std::pair<Mat, Mat> cardCharacteristics);
void loadTemplates();