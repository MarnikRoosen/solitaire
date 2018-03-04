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
	std::vector<cv::Mat> & PlayingBoard::extractAndSortCards(Mat const & boardImage);
	void extractCardsFromMatVector(std::vector<cv::Mat>& playingCards);
	~PlayingBoard();

private:
	std::vector<cv::Mat> cards;

};
