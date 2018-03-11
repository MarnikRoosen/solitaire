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

using namespace std;
using namespace cv;

enum playingBoardState { playing, outOfMoves, inMenu, newGame, closeBrowser, undo };

class PlayingBoard
{
public:
	PlayingBoard();
	void extractAndSortCards(Mat const & boardImage);
	void extractCardsFromMatVector(std::vector<cv::Mat>& playingCards);
	Rect determineOuterRect(const std::vector<std::vector<cv::Point>>& contours);
	~PlayingBoard();
	const playingBoardState & getState();
	const std::vector<cv::Mat> & PlayingBoard::getCards();

private:
	std::vector<cv::Mat> cards;
	playingBoardState state;
};
