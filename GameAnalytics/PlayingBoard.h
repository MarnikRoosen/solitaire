#pragma once

#include "stdafx.h"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/core.hpp"
#include <opencv2/opencv.hpp>
#include "opencv2/features2d.hpp"
#include "opencv2/imgcodecs.hpp"

#include <vector>
#include <iostream>
#include <string>

#define standardWidth 1280
#define standardHeight 720

using namespace std;
using namespace cv;

enum playingBoardState { playing, outOfMoves, inMenu, newGame, closeBrowser, undo };

class PlayingBoard
{
public:
	PlayingBoard();
	~PlayingBoard();
	Rect determineOuterRect(const std::vector<std::vector<cv::Point>>& contours);
	void findCardsFromBoardImage(Mat const & boardImage);
	void resizeBoardImage(Mat const & boardImage, Mat & resizedBoardImage);
	std::vector<cv::Mat> extractCardRegions(const cv::Mat & src);
	void extractCards(std::vector<cv::Mat>& playingCards);
	bool checkForOutOfMovesState(const cv::Mat &src);
	const playingBoardState & getState();
	const std::vector<cv::Mat> & PlayingBoard::getCards();

private:
	std::vector<cv::Mat> cards;
	playingBoardState state;
};
