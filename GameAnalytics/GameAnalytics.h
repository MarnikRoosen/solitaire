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
	void Init();
	void Process();

	void handleOutOfMoves();
	void handleGameWon();
	void handlePlayingState(PlayingBoard &playingBoard, ClassifyCard &classifyCard);

	void classifyExtractedCards(ClassifyCard & cc);
	void initializePlayingBoard(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard);
	void updateDeck(const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard);
	void findChangedCardLocations(const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard, int & changedIndex1, int & changedIndex2);
	bool cardMoveBetweenBuildAndSuitStack(const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard, int changedIndex1, int changedIndex2);
	void updateBoard(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard);

	void addCoordinatesToBuffer(const int x, const int y);
	void printPlayingBoardState();
	
	cv::Mat hwnd2mat(const HWND & hwnd);
	cv::Mat waitForStableImage();

private:
	std::vector<cv::Mat> extractedImagesFromPlayingBoard;
	std::vector<std::pair<classifiers, classifiers>> classifiedCardsFromPlayingBoard;
	std::vector<cardLocation> playingBoard;
	std::pair<classifiers, classifiers> cardType;
	std::pair<Mat, Mat> cardCharacteristics;
	
	bool init = true;
	bool endOfGame = false;
	std::chrono::time_point<std::chrono::steady_clock> startOfGame;
	std::chrono::time_point<std::chrono::steady_clock> startOfMove;
	std::vector<long long> averageThinkDurations;
	int indexOfSelectedCard = -1;
	RECT appRect;

	HWND hwnd;
	std::queue<cv::Mat> srcBuffer;
	std::queue<int> xPosBuffer;
	std::queue<int> yPosBuffer;
	DWORD   dwThreadIdHook;
	HANDLE  hThreadHook;

	HDC hwindowDC, hwindowCompatibleDC;
	int height, width;
	HBITMAP hbwindow;
	Mat src;
	BITMAPINFOHEADER  bi;
	RECT windowsize;    // get the height and width of the screen
};
