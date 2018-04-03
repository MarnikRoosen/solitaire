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

/*
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>


#define MAX_THREADS 3
#define BUF_SIZE 255

DWORD WINAPI MyThreadFunction(LPVOID lpParam);
void ErrorHandler(LPTSTR lpszFunction);

// Sample custom data structure for threads to use.
// This is passed by void pointer so it can be any data type
// that can be passed using a single void pointer (LPVOID).
typedef struct MyData {
	int val1;
	int val2;
} MYDATA, *PMYDATA;
*/

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

	void handlePlayingState(PlayingBoard &playingBoard, ClassifyCard &classifyCard);
	void convertImagesToClassifiedCards(ClassifyCard & cc);
	cv::Mat waitForStableImage();
	void initializePlayingBoard(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard);
	void updateBoard(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard);
	void updateDeck(int changedIndex1, const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard);
	void findChangedCardLocations(const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard, int &changedIndex1, int &changedIndex2);
	void printPlayingBoardState();
	bool cardMoveBetweenTableauAndFoundations(int changedIndex1, const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard, int changedIndex2);
	Mat hwnd2mat(const HWND & hwnd);

	void bufferImage(const int x, const int y);

private:
	std::vector<cv::Mat> extractedImagesFromPlayingBoard;
	std::vector<std::pair<classifiers, classifiers>> classifiedCardsFromPlayingBoard;
	std::vector<cardLocation> playingBoard;
	std::vector<bool> knownCards;
	std::vector<std::pair<classifiers, classifiers>> tempPlayingBoard;
	std::pair<classifiers, classifiers> cardType;
	std::pair<Mat, Mat> cardCharacteristics;
	bool init = true;
	int key = 0;

	std::chrono::time_point<std::chrono::steady_clock> averageThinkTime1;
	std::vector<long long> averageThinkDurations;
	int indexOfSelectedCard = -1;

	HWND hwnd;
	std::queue<cv::Mat> buffer;
	std::queue<int> xpos;

};


