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

class PlayingBoard
{
public:
	PlayingBoard();
	void PlayingBoard::extractAndSortCards(Mat const & boardImage);
	void extractCardsFromMatVector(std::vector<cv::Mat>& playingCards);
	void extractCardsFromMat(std::vector<cv::Rect> &validRects, cv::Mat &resizedSrc);
	~PlayingBoard();

	std::vector<cv::Mat> & getPlayingCards();

private:
	std::vector<cv::Mat> cards;

};

bool compare_rect(const Rect & a, const Rect &b);

