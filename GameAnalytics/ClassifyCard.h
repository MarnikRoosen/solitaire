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

Mat hwnd2mat(HWND hwnd);
Size standardCardSize;

int main(int argc, char ** argv);

/*
* Function: void CallBackFunc(int event, int x, int y, int flags, void* userdata)
* Description: This function will be called on any mouse event (moving mouse over window or clicking with left/right/middle mouse button)
* Parameters: event    = EVENT_MOUSEMOVE, EVENT_LBUTTONDOWN, EVENT_RBUTTONDOWN, EVENT_MBUTTONDOWN, EVENT_LBUTTONUP,
						 EVENT_RBUTTONUP, EVENT_MBUTTONUP, EVENT_LBUTTONDBLCLK, EVENT_RBUTTONDBLCLK, EVENT_MBUTTONDBLCLK
			  x,y	   = Coordinates of the event
			  flags    = Specific event whenever the mouse event occurs
						 EVENT_FLAG_LBUTTON, EVENT_FLAG_RBUTTON, EVENT_FLAG_MBUTTON, EVENT_FLAG_CTRLKEY, EVENT_FLAG_SHIFTKEY, EVENT_FLAG_ALTKEY
			  userdata = pointer for global event tracking, storing events in a given list,...
* Return: Mouse event with coordinate (0,0 is the left top corner)
*/

void CallBackFunc(int event, int x, int y, int flags, void* userdata);

/*
* Function: Mat detectCard(String cardName);
* Description: This function will detect the top card from a given image
* Parameters: String    = [cardName].png
* Return: cloned copy of the card resized to standardCardSize
*/

Mat detectCard(String cardName);

/*
* Function: std::pair<Mat, Mat> getCardCharacteristics(Mat aCard);
* Description: This function will get the rank and suit from the topleft corner of a given card
* Parameters: Mat    = image of a card
* Return: the rank-suit pair as images from the topleft corner of the card
*/

std::pair<Mat, Mat> getCardCharacteristics(Mat aCard);

/*
* Function: void classifyCard(std::pair<Mat, Mat> cardCharacteristics);
* Description: This function will try to classify the rank and suit of the card and print it out to the terminal
* Parameters: std::pair<Mat, Mat>    = rank-suit pair as images
* Return: /
*/

void classifyCard(std::pair<Mat, Mat> cardCharacteristics);

/*
* Function: void getClassificationData(String type, cv::Mat& class_ints, cv::Mat& train_images);
* Description: This function will try to open the generated classification.xml and images.xml files so they can be used for classification,
				if opening these files fails, the function will call generateTrainingData to regenerate these files
* Parameters: String type		= "rank" or "suit" which indicates for which type of xmlfiles are being called
			  Mat& class_ints	= reference of the classification data that can be used by classifyCard 
			  Mat& train_images = reference of the images data of the classification that can be used by classifyCard
* Return: /
*/

void getClassificationData(String type, cv::Mat& class_ints, cv::Mat& train_images);

/*
* Function: void generateTrainingData(cv::Mat trainingImage, String outputPreName);
* Description: This function will generate the classification.xml and images.xml files from train images,
				which are used to classify the rank and suit using the k-nearest neighbors algorithm
* Parameters: String type		= "rank" or "suit" which indicates for which type the algorithm will train,
									it's also used for saving the xml files accordingly
			  Mat& class_ints	= reference of the classification data that can be used by classifyCard
			  Mat& train_images = reference of the images data of the classification that can be used by classifyCard
* Return: /
*/

void generateTrainingData(cv::Mat trainingImage, String outputPreName);

/*
* Function: void getApplicationView();
* Description: This function will open the desktop background in a new window
* Parameters: /
* Return: /
*/

void getApplicationView();
