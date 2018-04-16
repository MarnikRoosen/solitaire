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

enum SolitaireState { PLAYING, UNDO, QUIT, NEWGAME, MENU, MAINMENU, OUTOFMOVES, WON, HINT, AUTOCOMPLETE };

class PlayingBoard;
class ClassifyCard;

class GameAnalytics
{
public:
	GameAnalytics();
	~GameAnalytics();
	void Init();
	void Process();

	void processCardSelection();
	int determineIndexOfPressedCard();

	void handleEndOfGame();
	bool handlePlayingState();
	void determineNextState();

	void classifyExtractedCards();
	void initializePlayingBoard(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard);
	void updateTalonStack(const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard);
	void findChangedCardLocations(const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard, int & changedIndex1, int & changedIndex2);
	bool cardMoveBetweenBuildAndSuitStack(const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard, int changedIndex1, int changedIndex2);
	bool updateBoard(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard);

	void addCoordinatesToBuffer(const int x, const int y);
	void printPlayingBoardState();
	
	cv::Mat hwnd2mat(const HWND & hwnd);
	cv::Mat waitForStableImage();

private:
	PlayingBoard pb;
	ClassifyCard cc;
	SolitaireState currentState;

	std::vector<cv::Mat> extractedImagesFromPlayingBoard;
	std::vector<std::pair<classifiers, classifiers>> classifiedCardsFromPlayingBoard;
	std::vector<cardLocation> currentPlayingBoard;
	std::vector<std::vector<cardLocation>> previousPlayingBoards;
	std::pair<classifiers, classifiers> cardType;
	std::pair<Mat, Mat> cardCharacteristics;
	std::pair<classifiers, classifiers> previouslySelectedCard;
	
	bool endOfGameBool = false;
	int numberOfUndos = 0;
	int numberOfPilePresses = 0;
	std::vector<int> numberOfPresses;
	int numberOfHints = 0;
	int numberOfSuitErrors = 0;
	int numberOfRankErrors = 0;
	int score = 0;
	std::chrono::time_point<std::chrono::steady_clock> startOfGame;
	std::chrono::time_point<std::chrono::steady_clock> startOfMove;
	std::vector<long long> averageThinkDurations;
	
	std::queue<cv::Mat> srcBuffer;
	std::queue<int> xPosBuffer;
	std::queue<int> yPosBuffer;
	DWORD   dwThreadIdHook;
	HANDLE  hThreadHook;

	HDC hwindowDC, hwindowCompatibleDC;
	HBITMAP hbwindow;
	BITMAPINFOHEADER  bi;
	RECT windowsize;    // get the height and width of the screen
	RECT appRect;	// get location of the game in respect to the primary window
	POINT pt[2];	// remap the coordinates to the correct window
	double windowWidth, windowHeight;
	int height, width;
	int distortedWindowHeight = 0;
	HWND hwnd;
	Mat src;

};
