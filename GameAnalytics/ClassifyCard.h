// Screen capture source: https://stackoverflow.com/questions/26888605/opencv-capturing-desktop-screen-live
// Mouse events source: https://opencv-srf.blogspot.be/2011/11/mouse-events.html
// Suit-rank detection and training knn algorithm based on: https://github.com/MicrocontrollersAndMore/OpenCV_3_KNN_Character_Recognition_Cpp

#pragma once
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "User32.lib")

#include "stdafx.h"
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
#include <chrono>

typedef std::chrono::high_resolution_clock Clock;

using namespace cv;
using namespace std;

const int RESIZED_TYPE_WIDTH = 40;
const int RESIZED_TYPE_HEIGHT = 50;
const int MIN_CONTOUR_AREA = 100;

enum classifiers : char { TWO = '2', THREE = '3', FOUR = '4', FIVE = '5', SIX = '6',
	SEVEN = '7', EIGHT = '8', NINE = '9', TEN , JACK = 'J', QUEEN = 'Q', KING = 'K', ACE = 'A',
	CLUBS = 'C', SPADES = 'S', HEARTS = 'H', DIAMONDS = 'D', UNKNOWN = '?', EMPTY = '/'};

class ClassifyCard
{
public:
	ClassifyCard();
	void classifyRankAndSuitOfCard2(std::pair<Mat, Mat> cardCharacteristics);
	std::pair<Mat, Mat> segmentRankAndSuitFromCard(const Mat & aCard);
	std::pair<classifiers, classifiers> classifyRankAndSuitOfCard(std::pair<Mat, Mat> cardCharacteristics);
	void getTrainedData(String type, cv::Mat& class_ints, cv::Mat& train_images);
	void generateTrainingData(cv::Mat trainingImage, String outputPreName);

private:
	Size standardCardSize;
};
