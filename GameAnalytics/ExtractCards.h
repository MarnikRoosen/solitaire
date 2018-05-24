#pragma once

#include "stdafx.h"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/core.hpp"
#include "opencv2/opencv.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/imgcodecs.hpp"

#include <vector>
#include <iostream>
#include <string>

#define standardBoardWidth 1600
#define standardBoardHeight 900
#define standardCardWidth 150
#define standardCardHeight 200

using namespace std;
using namespace cv;

class ExtractCards
{
public:
	ExtractCards();
	~ExtractCards();

	// INITIALIZATION
	void determineROI(const Mat & boardImage);	// find the general location (roi) that contains all cards during the initialization
	void calculateOuterRect(std::vector<std::vector<cv::Point>> &contours);	// calculate the outer rect (roi) using the contours of all cards


	// MAIN FUNCTIONS
	const std::vector<cv::Mat> & findCardsFromBoardImage(Mat const & boardImage);	// main extraction function
	void resizeBoardImage(Mat const & boardImage, Mat & resizedBoardImage);	// resize the playing board to standardBoardWidth/Height
	void extractCardRegions(const cv::Mat & src);	// extract each region that contains a card
	void extractCards();	// extract the top card from a card region
	

	// PROCESSING OF STACK CARDS WITHIN extractCards()
	void extractTopCardUsingSobel(const cv::Mat & src, cv::Mat & dest, int i);	// extract the top card from a stack of cards using Sobel edge detection
	void croppedTopCardToStandardSize(const cv::Mat &croppedRef, cv::Mat &resizedCardImage);	// resize the extracted top card to standardCardWidth/Height 
	void extractTopCardUsingAspectRatio(const cv::Mat & src, cv::Mat & dest);	// extract the top card from a stack of cards using the standard size of a card


	// PROCESSING OF SELECTED CARDS BY THE PLAYER
	int getIndexOfSelectedCard(int i);	// find the index of the deepest card that was selected by the player (topcard = 0, below top card = 1, etc.)
	

	// END OF GAME CHECK
	bool checkForOutOfMovesState(const cv::Mat &src);	// check if the middle of the screen is mostly white == out of moves screen


private:
	std::vector<cv::Mat> cards;	// extracted cards from card regions
	std::vector<cv::Mat> cardRegions;	// extracted card regions
	Rect ROI;	// roi calculated by determineROI during the initialization that contains the general region of all cards
	int topCardsHeight;	// separation of the talon and suit stack from the build stack within the ROI
};
