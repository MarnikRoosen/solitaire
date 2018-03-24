#pragma once
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "shcore.lib")

#include "stdafx.h"
#include "shcore.h"
#include "ClassifyCard.h"
#include "PlayingBoard.h"
#include "ClicksHooks.h"

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
#include <cstdio>
#include <chrono>
#include <numeric>

typedef std::chrono::high_resolution_clock Clock;
using namespace cv;
using namespace std;

struct cardLocation
{
	int unknownCards;
	std::vector<std::pair<classifiers, classifiers>> knownCards;
};

class PlayingBoard;
class ClassifyCard;

class GameAnalytics
{
public:
	GameAnalytics();
	void handlePlayingState(PlayingBoard &playingBoard, ClassifyCard &classifyCard);
	void convertImagesToClassifiedCards(ClassifyCard & cc);
	void waitForStableImage(HWND & hwnd);
	void initializePlayingBoard(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard);
	void updateBoard(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard);
	void updateDeck(int changedIndex1, const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard);
	void findChangedCardLocations(const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard, int &changedIndex1, int &changedIndex2);
	void printPlayingBoardState();
	bool cardMoveBetweenTableauAndFoundations(int changedIndex1, const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard, int changedIndex2);
	Mat hwnd2mat(const HWND & hwnd);

private:
	std::vector<cv::Mat> extractedCardImages;
	std::vector<std::pair<classifiers, classifiers>> classifiedCardsFromImages;
	std::vector<cardLocation> playingBoard;
	std::chrono::time_point<std::chrono::steady_clock> startOfGameTimer;
	std::vector<long long> averageThinkTimings;
	bool init = true;
	Mat boardImage;

};


