#include "stdafx.h"
#include "PlayingBoard.h"



PlayingBoard::PlayingBoard()
{
	String filename = "playingBoard.png";	// load testimage
	Mat src = imread("../GameAnalytics/testImages/" + filename);
	if (!src.data)	// check for invalid input
	{
		cout << "Could not open or find the image" << std::endl;
		exit(EXIT_FAILURE);
	}
	Mat resizedSrc = src.clone();
	cv::resize(src, resizedSrc, cv::Size(), 0.75, 0.75);

	Mat grayImg, blurredImg, threshImg;
	vector<vector<Point>> contours, validContours;
	vector<Vec4i> hierarchy;

	cvtColor(resizedSrc, grayImg, COLOR_BGR2GRAY);	// convert the image to gray
	//GaussianBlur(grayImg, blurredImg, Size(1, 1), 1000);	// apply gaussian blur to improve card detection
	threshold(grayImg, threshImg, 130, 255, THRESH_BINARY);	// threshold the image to keep only brighter regions (cards are white)										
	findContours(threshImg, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE, Point(0, 0));	// find all the contours using the thresholded image

	for (int i = 0; i < contours.size(); i++) // Iterate through each contour. 
	{
		int a = contourArea(contours[i], false);  // Find the area of contour
		if (a > 1000) {
			validContours.push_back(contours[i]);
		}
	}
	drawContours(resizedSrc, validContours, -1, Scalar(0, 0, 255));
	imshow("new image", resizedSrc);
	waitKey(0);

	for (int i = 0; i < validContours.size(); i++)
	{
		Rect bounding_rect = boundingRect(validContours[i]);
		Mat card = Mat(resizedSrc, bounding_rect).clone();
		Size cardSize = card.size();
		int topCardWidth = cardSize.width;
		int topCardHeight = cardSize.height;
		Rect myROI;
		if (cardSize.width * 1.35 > cardSize.height)
		{
			topCardWidth = cardSize.height / 1.34;
			myROI = Rect(cardSize.width - topCardWidth, 0, topCardWidth, cardSize.height);
		}
		else
		{
			topCardHeight = cardSize.width * 1.33;
			myROI = Rect(0, cardSize.height - topCardHeight, cardSize.width, topCardHeight);
		}
		Mat croppedRef(card, myROI);
		Mat topCard;
		croppedRef.copyTo(topCard);	// Copy the data into new matrix
		imshow("topCard", topCard);
		waitKey(0);
	}

}


PlayingBoard::~PlayingBoard()
{
}
