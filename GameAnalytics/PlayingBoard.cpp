#include "stdafx.h"
#include "PlayingBoard.h"

PlayingBoard::PlayingBoard()
{
	cards.resize(12);
}

void PlayingBoard::extractAndSortCards(Mat const & boardImage)
{
	
	Mat src = boardImage.clone();
	imshow("board", src);
	Mat grayImg, blurredImg, threshImg;
	vector<vector<Point>> contours, validContours;
	vector<Rect> validRects;
	vector<Vec4i> hierarchy;

	cvtColor(src, grayImg, COLOR_BGR2GRAY);	// convert the image to gray
	threshold(grayImg, threshImg, 120, 255, THRESH_BINARY);	// threshold the image to keep only brighter regions (cards are white)										
	findContours(threshImg, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE, Point(0, 0));	// find all the contours using the thresholded image

	for (int i = 0; i < contours.size(); i++) // Iterate through each contour. 
	{
		int a = contourArea(contours[i], false);  // Find the area of contour
		if (a > 10000) {
			Rect bounding_rect = boundingRect(contours[i]);
			float aspectRatio = (float) bounding_rect.width /  (float) bounding_rect.height;	// make sure the contours have an aspect ratio of an average card (to filter out navbar)
			if (aspectRatio > 0.1 && aspectRatio < 10)
			{
				validRects.push_back(bounding_rect);
				validContours.push_back(contours[i]);
			}
		}
	}
	int xmin = validRects[0].x;
	int xmax = validRects[0].x + validRects[0].width;
	int ymin = validRects[0].y;
	int ymax = validRects[0].y + validRects[0].height;
	for (int i = 1; i < validRects.size(); i++)
	{
		if (xmin > validRects[i].x) { xmin = validRects[i].x; }
		if (xmax < validRects[i].x + validRects[i].width) { xmax = validRects[i].x + validRects[i].width; }
		if (ymin > validRects[i].y) { ymin = validRects[i].y; }
		if (ymax < validRects[i].y + validRects[i].height) { ymax = validRects[i].y + validRects[i].height; }
	}
	Rect outerEdgeRect = Rect(xmin, ymin, xmax - xmin, ymax - ymin);
	Mat croppedOuterEdge(src, outerEdgeRect);
	Size outerEdgeSize = croppedOuterEdge.size();
	Rect topCardsRect = Rect(0, 0, outerEdgeSize.width, outerEdgeSize.width*0.2);
	Rect bottomCardsRect = Rect(0, outerEdgeSize.width*0.2, outerEdgeSize.width, outerEdgeSize.height - outerEdgeSize.width*0.2 - 1);
	Mat croppedtopCards(croppedOuterEdge, topCardsRect);
	Mat croppedbottomCards(croppedOuterEdge, bottomCardsRect);
	Size topCardsSize = croppedtopCards.size();
	Size bottomCardsSize = croppedbottomCards.size();
	//imshow("topcards", croppedtopCards);
	//imshow("bottomcards", croppedbottomCards);

	std::vector<cv::Mat> playingCards;
	for (int i = 0; i < 7; i++)
	{
		Rect cardLocationRect = Rect(bottomCardsSize.width / 7 * i, 0, bottomCardsSize.width / 6.9 - 1, bottomCardsSize.height);
		Mat croppedCard(croppedbottomCards, cardLocationRect);
		playingCards.push_back(croppedCard.clone());
	}
	Rect deckCardsRect = Rect(topCardsSize.width / 7, 0, topCardsSize.width / 5.5, topCardsSize.height);
	Mat croppedCard(croppedtopCards, deckCardsRect);
	playingCards.push_back(croppedCard.clone());
	for (int i = 3; i < 7; i++)
	{
		Rect cardLocationRect = Rect(topCardsSize.width / 7 * i, 0, topCardsSize.width / 6.9 - 1, topCardsSize.height);
		Mat croppedCard(croppedtopCards, cardLocationRect);
		playingCards.push_back(croppedCard.clone());
	}


	sort(validRects.begin(), validRects.end(), compare_rect);
	extractCardsFromMatVector(playingCards);
	//extractCardsFromMat(validRects, src);
}

void PlayingBoard::extractCardsFromMatVector(std::vector<cv::Mat> &playingCards)
{
	for (int i = 0; i < playingCards.size(); i++)
	{
		Mat src = playingCards[i].clone();
		Mat grayImg, blurredImg, threshImg;
		vector<vector<Point>> contours, validContours;
		vector<Rect> validRects;
		vector<Vec4i> hierarchy;

		cvtColor(src, grayImg, COLOR_BGR2GRAY);	// convert the image to gray
		threshold(grayImg, threshImg, 120, 255, THRESH_BINARY);	// threshold the image to keep only brighter regions (cards are white)										
		findContours(threshImg, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE, Point(0, 0));	// find all the contours using the thresholded image
		for (int i = 0; i < contours.size(); i++) // Iterate through each contour. 
		{
			int a = contourArea(contours[i], false);  // Find the area of contour
			if (a > 10000) {
				Rect bounding_rect = boundingRect(contours[i]);
				float aspectRatio = (float)bounding_rect.width / (float)bounding_rect.height;	// make sure the contours have an aspect ratio of an average card (to filter out navbar)
				if (aspectRatio > 0.1 && aspectRatio < 10)
				{
					validRects.push_back(bounding_rect);
					validContours.push_back(contours[i]);
				}
			}
		}
		if(validContours.size() > 0)
		{
			Rect bounding_rect = boundingRect(validContours[0]);
			Mat card = Mat(src, bounding_rect).clone();

			Size cardSize = card.size();
			int topCardWidth = cardSize.width;
			int topCardHeight = cardSize.height;
			Rect myROI;
			if (cardSize.width * 1.35 > cardSize.height)
			{
				topCardWidth = cardSize.height / 1.34;
				myROI = Rect(cardSize.width - topCardWidth + 1, 0, topCardWidth - 1, cardSize.height);
			}
			else
			{
				topCardHeight = cardSize.width * 1.33;
				myROI = Rect(0, cardSize.height - topCardHeight, cardSize.width, topCardHeight);
			}
			Mat croppedRef(card, myROI);
			Mat topCard;
			croppedRef.copyTo(topCard);	// Copy the data into new matrix
										// Only use images that have at least 30% white pixels (to remove the deck image)
			Mat checkImage;
			cvtColor(topCard, checkImage, COLOR_BGR2GRAY);
			threshold(checkImage, checkImage, 200, 255, THRESH_BINARY);
			int whitePixels = cv::countNonZero(checkImage);
			if (whitePixels > checkImage.rows * checkImage.cols * 0.3)
			{
				cards[i] = topCard.clone();
			}
			else
			{
				Mat empty;
				cards[i] = empty;
			}
		}	
		else
		{
			Mat empty;
			cards[i] = empty;
		}
	}
}


void PlayingBoard::extractCardsFromMat(std::vector<cv::Rect> &validRects, cv::Mat &src)
{
	for (int i = 0; i < validRects.size(); i++)
	{
		Mat card = Mat(src, validRects[i]).clone();
		Size cardSize = card.size();
		int topCardWidth = cardSize.width;
		int topCardHeight = cardSize.height;
		Rect myROI;
		if (cardSize.width * 1.35 > cardSize.height)
		{
			topCardWidth = cardSize.height / 1.34;
			myROI = Rect(cardSize.width - topCardWidth + 1, 0, topCardWidth - 1, cardSize.height);
		}
		else
		{
			topCardHeight = cardSize.width * 1.33;
			myROI = Rect(0, cardSize.height - topCardHeight, cardSize.width, topCardHeight);
		}
		Mat croppedRef(card, myROI);
		Mat topCard;
		croppedRef.copyTo(topCard);	// Copy the data into new matrix

									// Only use images that have at least 30% white pixels (to remove the deck image)
		Mat checkImage;
		cvtColor(topCard, checkImage, COLOR_BGR2GRAY);
		threshold(checkImage, checkImage, 200, 255, THRESH_BINARY);
		int whitePixels = cv::countNonZero(checkImage);
		if (whitePixels > checkImage.rows * checkImage.cols * 0.3)
		{
			cards[i] = topCard.clone();
		}
		else
		{
			Mat empty;
			cards[i] = empty;
		}
	}
}


PlayingBoard::~PlayingBoard()
{
}

std::vector<cv::Mat> & PlayingBoard::getPlayingCards()
{
	return cards;
}

bool compare_rect(const Rect & a, const Rect &b) {
	return (a.y - b.y > 10) || ((abs(a.y - b.y) < 5) && b.x - a.x > 10);
}