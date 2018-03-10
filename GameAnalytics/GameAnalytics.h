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

struct cardLocation
{
	int unknownCards;
	std::vector<std::pair<classifiers, classifiers>> knownCards;
};

class GameAnalytics
{
public:
	GameAnalytics();
	void initializeVariables(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard);
	void updateBoard(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard);
	Mat hwnd2mat(const HWND & hwnd);
	void printPlayingBoardState();

private:
	std::vector<cv::Mat> extractedCardsFromPlayingBoard;
	std::vector<cardLocation> playingBoard;
	HWND hwnd;
	std::vector<bool> knownCards;
	std::vector<std::pair<classifiers, classifiers>> tempPlayingBoard;
};


