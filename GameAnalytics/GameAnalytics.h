#pragma once
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "shcore.lib")

#include "stdafx.h"
#include "shcore.h"

#include "ClassifyCard.h"
#include "PlayingBoard.h"
#include "ClicksHooks.h"

// opencv includes
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/core.hpp"
#include "opencv2/opencv.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/imgcodecs.hpp"

// mysql includes
#include "mysql_connection.h"
#include "cppconn/driver.h"
#include "cppconn/exception.h"
#include "cppconn/resultset.h"
#include "cppconn/statement.h"

// other includes
#include <vector>
#include <utility>
#include <Windows.h>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <cstdio>
#include <chrono>
#include <numeric>
#include <thread>
#include <cwchar>
#include <fstream>
#include <iterator>

typedef std::chrono::high_resolution_clock Clock;	// clock used for testing
using namespace cv;
using namespace std;

struct cardLocation	// struct used to keep track of the playing board
{
	int remainingCards;
	std::vector<std::pair<classifiers, classifiers>> knownCards;	// used for the build- and suit stack
	std::pair<classifiers, classifiers> topCard;	// only used for the talon
};

struct srcData	// struct for buffers after a click was registered
{
	int x, y;
	cv::Mat src;
};

enum SolitaireState { PLAYING, UNDO, QUIT, NEWGAME, MENU, MAINMENU, OUTOFMOVES, WON, HINT, AUTOCOMPLETE };	// possible game states

class PlayingBoard;
class ClassifyCard;

class GameAnalytics
{
public:
	GameAnalytics();
	~GameAnalytics();

	// INITIALIZATION

	void initScreenCapture();	// initialize the screen capture relative to the correct monitor
	void initGameLogic();	// intialize tracking variables and the state of the game 
	void initDBConn();	// initialize the database connection
	void initPlayingBoard(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard);	// initialize the first playing board


	// MAIN FUNCTIONS

	void process();	// main function of the application
	void determineNextState(const int & x, const int & y);	// check what the main function should do next
	void handleUndoState();	// take the previous state of the playing board if undo was pressed
	void handleEndOfGame();	// print all tracked data
	bool handlePlayingState();	// extract cards, classify them and check for changes in the playing board
	void classifyExtractedCards();	// classify the cards if the cardImages aren't empty (no card at that location)


	// MULTITHREADED FUNCTIONS + SCREEN CAPTURE

	void hookMouseClicks();	// multithreaded function for capturing mouse clicks
	void calculateBetaErrors();	// multithreaded function for calculating the beta errors
	void grabSrc();	// multithreaded function that only captures screens after mouse clicks
	void toggleClickDownBool();	// on click before previous screen is captured, notify for immediate screengrab
	void addCoordinatesToBuffer(const int x, const int y);	// function called by the callback of the hookMouse that adds the coordinates of the mouseclick to the buffer
	cv::Mat waitForStableImage();	// wait for the end of a cardanimation before grabbing the new screenshot
	cv::Mat hwnd2mat(const HWND & hwnd);	// makes an image from the window handle


	// PROCESSING OF SELECTED CARDS BY THE PLAYER

	void processCardSelection(const int & x, const int & y);	// process if a card was selected and handle player error checking
	void detectPlayerMoveErrors(std::pair<classifiers, classifiers> &selectedCard, int indexOfPressedCardLocation);	// check for player errors if a card was selected
	int determineIndexOfPressedCard(const int & x, const int & y);	// check if a cardlocation was pressed using coordinates


	// PROCESSING THE GAME STATE OF THE PLAYING BOARD

	bool updateBoard(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard);	// general function to update the playing board
	void findChangedCardLocations(const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard, int & changedIndex1, int & changedIndex2);	// check which cards have been moved
	bool cardMoveBetweenBuildAndSuitStack(const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard, int changedIndex1, int changedIndex2);	// handle card move between build and suit stacks
	void printPlayingBoardState();	// print the new state of the playing board if cards have been moved
	

	// TEST FUNCTIONS

	void test();	// test function for card extraction and classification
	bool writeTestData(const vector <vector <pair <classifiers, classifiers> > > &classifiedBoards, const string & file);	// easily write new testdata to a textfile for later use
	bool readTestData(vector <vector <pair <classifiers, classifiers> > > &classifiedBoards, const string &file);	// read in previously saved testdata from a textfile

private:
	PlayingBoard pb;
	ClassifyCard cc;
	SolitaireState currentState;


	// SCREENSHOT AND PLAYINGBOARD PROCESSING VARIABLES

	std::vector<cv::Mat> extractedImagesFromPlayingBoard;	// extracted cards from a screenshot
	std::vector<std::pair<classifiers, classifiers>> classifiedCardsFromPlayingBoard;	// classified cards resulting from the extracted cards
	std::vector<cardLocation> currentPlayingBoard;	// current playing board identical to the state of the game
	std::vector<std::vector<cardLocation>> previousPlayingBoards;	// all playing board states that came before the current playing board


	// KEEPING TRACK OF THE SELECTED CARDS BY THE PLAYER

	std::pair<classifiers, classifiers> previouslySelectedCard;	// keeping track of the card that was selected previously to detect player move errors
	int previouslySelectedIndex = -1;	// keeping track of the index of card that was selected previously to detect player move errors
	

	// GAME METRICS

	bool endOfGameBool = false;
	bool gameWon = false;
	int numberOfUndos = 0;
	int numberOfPilePresses = 0;
	std::vector<int> numberOfPresses;
	std::vector<std::pair<int, int>> locationOfPresses;
	std::pair<int, int> locationOfLastPress;
	int numberOfHints = 0;
	int numberOfSuitErrors = 0;
	int numberOfRankErrors = 0;
	int score = 0;
	std::chrono::time_point<std::chrono::steady_clock> startOfGame;
	std::chrono::time_point<std::chrono::steady_clock> startOfMove;
	std::vector<long long> averageThinkDurations;
	

	// SCREENSHOT BUFFERS AND DATA

	std::queue<cv::Mat> srcBuffer;
	std::queue<cv::Mat> clickDownBuffer;
	std::queue<int> xPosBuffer1;
	std::queue<int> yPosBuffer1;
	std::queue<int> xPosBuffer2;
	std::queue<int> yPosBuffer2;
	int processedSrcCounter = 0;
	bool clickDownBool = false;
	bool waitForStableImageBool = false;
	

	// FREQUENTLY USED VARIABLES

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
	Mat src, src1, src2, graySrc1, graySrc2;
	double norm;
	std::pair<classifiers, classifiers> cardType;
	std::pair<Mat, Mat> cardCharacteristics;
	int changedIndex1, changedIndex2;
	srcData data;
};

void changeConsoleFontSize(const double & percentageIncrease);
