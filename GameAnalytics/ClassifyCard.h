#pragma once

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

#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "User32.lib")

using namespace cv;
using namespace std;

const int RESIZED_TYPE_WIDTH = 20;
const int RESIZED_TYPE_HEIGHT = 30;
const int MIN_CONTOUR_AREA = 100;

enum classifiers : char { ONE = 1, TWO = '2', THREE = '3', FOUR = '4', FIVE = '5', SIX = '6',
	SEVEN = '7', EIGHT = '8', NINE = '9', TEN , JACK = 'J', QUEEN = 'Q', KING = 'K', ACE = 'A',
	CLUBS = 'C', SPADES = 'S', HEARTS = 'H', DIAMONDS = 'D', UNKNOWN = '?', EMPTY = '/'};


class ClassifyCard
{
public:
	ClassifyCard();

	/*
	* Function: Mat detectCardFromImageFile(String cardName);
	* Description: This function will detect the top card from a given image
	* Parameters: String    = [cardName].png
	* Return: cloned copy of the card resized to standardCardSize
	*/

	Mat detectCardFromImageFile(String cardName);

	/*
	* Function: std::pair<Mat, Mat> segmentRankAndSuitFromCard(Mat aCard);
	* Description: This function will get the rank and suit from the topleft corner of a given card
	* Parameters: Mat    = image of a card
	* Return: the rank-suit pair as images from the topleft corner of the card
	*/

	std::pair<Mat, Mat> segmentRankAndSuitFromCard(Mat aCard);

	/*
	* Function: void classifyRankAndSuitOfCard(std::pair<Mat, Mat> cardCharacteristics);
	* Description: This function will try to classify the rank and suit of the card and print it out to the terminal
	* Parameters: std::pair<Mat, Mat>    = rank-suit pair as images
	* Return: /
	*/

	std::pair<classifiers, classifiers> classifyRankAndSuitOfCard(std::pair<Mat, Mat> cardCharacteristics);

	/*
	* Function: std::pair<classifiers, classifiers> getTrainedData(String type, cv::Mat& class_ints, cv::Mat& train_images);
	* Description: This function will try to open the generated classification.xml and images.xml files so they can be used for classification,
	if opening these files fails, the function will call generateTrainingData to regenerate these files
	* Parameters: String type		= "rank" or "suit" which indicates for which type of xmlfiles are being called
	Mat& class_ints	= reference of the classification data that can be used by classifyRankAndSuitOfCard
	Mat& train_images = reference of the images data of the classification that can be used by classifyRankAndSuitOfCard
	* Return: pair of rank and suit
	*/

	void getTrainedData(String type, cv::Mat& class_ints, cv::Mat& train_images);

	/*
	* Function: void generateTrainingData(cv::Mat trainingImage, String outputPreName);
	* Description: This function will generate the classification.xml and images.xml files from train images,
	which are used to classify the rank and suit using the k-nearest neighbors algorithm
	* Parameters: String type		= "rank" or "suit" which indicates for which type the algorithm will train,
	it's also used for saving the xml files accordingly
	Mat& class_ints	= reference of the classification data that can be used by classifyRankAndSuitOfCard
	Mat& train_images = reference of the images data of the classification that can be used by classifyRankAndSuitOfCard
	* Return: /
	*/

	void generateTrainingData(cv::Mat trainingImage, String outputPreName);

	/*
	* Function: String convertCharToCardName(char aName);
	* Description: This function will convert the chars from the classification to a full cardname
	* Parameters: char aName	= the char from the classification function
	* Return: The name of the card in String
	*/

	Mat ClassifyCard::detectCardFromMat(cv::Mat anImage);


private:
	Size standardCardSize;
};
