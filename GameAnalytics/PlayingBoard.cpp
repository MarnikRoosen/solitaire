#include "stdafx.h"
#include "PlayingBoard.h"

PlayingBoard::PlayingBoard()
{
	cards.resize(12);
}

std::vector<cv::Mat> & PlayingBoard::extractAndSortCards(Mat const & boardImage)
{
	Mat src = boardImage.clone();
	Mat adaptedImg;
	vector<vector<Point>> contours;
	vector<Vec4i> hierarchy;

	cvtColor(src, adaptedImg, COLOR_BGR2GRAY);	// convert the image to gray
	threshold(adaptedImg, adaptedImg, 120, 255, THRESH_BINARY);	// threshold the image to keep only brighter regions (cards are white)										
	findContours(adaptedImg, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE, Point(0, 0));	// find all the contours using the thresholded image

	auto new_end = std::remove_if(contours.begin(), contours.end(), [](const std::vector<cv::Point>& c1) {
		double area = contourArea(c1, false);	// make sure the contourArea is big enough to be a card
		Rect bounding_rect = boundingRect(c1);
		float aspectRatio = (float)bounding_rect.width / (float)bounding_rect.height;	// make sure the contours have an aspect ratio of an average card (to filter out navbar)
		return ((aspectRatio < 0.1) || (aspectRatio > 10) || (area < 10000)); });
	contours.erase(new_end, contours.end());
	std::sort(contours.begin(), contours.end(), [](const vector<Point>& c1, const vector<Point>& c2) -> bool { return contourArea(c1, false) > contourArea(c2, false); });

	Rect outerEdgeRect = determineOuterRect(contours);
	Mat croppedOuterEdge(src, outerEdgeRect);
	Size outerEdgeSize = croppedOuterEdge.size();
	Rect topCardsRect = Rect(0, 0, (int) outerEdgeSize.width, (int) outerEdgeSize.width*0.2);
	Rect bottomCardsRect = Rect(0, (int) outerEdgeSize.width*0.2, (int) outerEdgeSize.width, (int) (outerEdgeSize.height - outerEdgeSize.width*0.2 - 1));
	Mat croppedtopCards(croppedOuterEdge, topCardsRect);
	Mat croppedbottomCards(croppedOuterEdge, bottomCardsRect);
	Size topCardsSize = croppedtopCards.size();
	Size bottomCardsSize = croppedbottomCards.size();

	std::vector<cv::Mat> playingCards;
	for (int i = 0; i < 7; i++)
	{
		Rect cardLocationRect = Rect((int) bottomCardsSize.width / 7 * i, 0, (int) (bottomCardsSize.width / 6.9 - 3), bottomCardsSize.height);
		Mat croppedCard(croppedbottomCards, cardLocationRect);
		playingCards.push_back(croppedCard.clone());
	}
	Rect deckCardsRect = Rect(topCardsSize.width / 7, 0, topCardsSize.width / 5, topCardsSize.height);
	Mat croppedCard(croppedtopCards, deckCardsRect);
	playingCards.push_back(croppedCard.clone());
	for (int i = 3; i < 7; i++)
	{
		Rect cardLocationRect = Rect((int) (topCardsSize.width / 7 * i), 0, (int) (topCardsSize.width / 6.9 - 3), topCardsSize.height);
		Mat croppedCard(croppedtopCards, cardLocationRect);
		playingCards.push_back(croppedCard.clone());
	}

	extractCardsFromMatVector(playingCards);
	return cards;
}

void PlayingBoard::extractCardsFromMatVector(std::vector<cv::Mat> &playingCards)
{
	for (int i = 0; i < playingCards.size(); i++)
	{
		Mat src = playingCards[i].clone();
		Mat grayImg, blurredImg, threshImg;
		vector<vector<Point>> contours, validContours;
		vector<Vec4i> hierarchy;

		cv::cvtColor(src, grayImg, COLOR_BGR2GRAY);	// convert the image to gray
		cv::threshold(grayImg, threshImg, 215, 255, THRESH_BINARY);	// threshold the image to keep only brighter regions (cards are white)		
		findContours(threshImg, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE, Point(0, 0));	// find all the contours using the thresholded image

		auto new_end = std::remove_if(contours.begin(), contours.end(), [](const std::vector<cv::Point>& c1) {
			double area = contourArea(c1, false);	// make sure the contourArea is big enough to be a card
			Rect bounding_rect = boundingRect(c1);
			float aspectRatio = (float)bounding_rect.width / (float)bounding_rect.height;	// make sure the contours have an aspect ratio of an average card (to filter out navbar)
			return ((aspectRatio < 0.1) || (aspectRatio > 10) || (area < 10000)); });
		contours.erase(new_end, contours.end());
		std::sort(contours.begin(), contours.end(), [](const vector<Point>& c1, const vector<Point>& c2) -> bool { return contourArea(c1, false) > contourArea(c2, false); });

		if(contours.size() > 0)
		{
			Rect bounding_rect = boundingRect(contours.at(0));
			Mat card = Mat(src, bounding_rect).clone();

			Size cardSize = card.size();
			int topCardWidth = cardSize.width;
			int topCardHeight = cardSize.height;
			Rect myROI;
			if (cardSize.width * 1.35 > cardSize.height)
			{
				topCardWidth = cardSize.height / 1.34;
				myROI = Rect((int) (cardSize.width - topCardWidth + 1), 0, topCardWidth - 1, cardSize.height);
			}
			else
			{
				topCardHeight = cardSize.width * 1.33;
				myROI = Rect(0, (int) (cardSize.height - topCardHeight), cardSize.width, topCardHeight);
			}
			Mat croppedRef(card, myROI);
			Mat topCard;
			croppedRef.copyTo(topCard);	// Copy the data into new matrix
										// Only use images that have at least 30% white pixels (to remove the deck image)
			Mat checkImage;
			cv::cvtColor(topCard, checkImage, COLOR_BGR2GRAY);
			cv::threshold(checkImage, checkImage, 200, 255, THRESH_BINARY);
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

Rect PlayingBoard::determineOuterRect(const std::vector<std::vector<cv::Point>> & contours)
{
	Rect tempRect = boundingRect(contours.at(0));
	int xmin = tempRect.x;
	int xmax = tempRect.x + tempRect.width;
	int ymin = tempRect.y;
	int ymax = tempRect.y + tempRect.height;
	for (int i = 1; i < contours.size(); i++)
	{
		tempRect = boundingRect(contours.at(i));
		if (xmin > tempRect.x) { xmin = tempRect.x; }
		if (xmax < tempRect.x + tempRect.width) { xmax = tempRect.x + tempRect.width; }
		if (ymin > tempRect.y) { ymin = tempRect.y; }
		if (ymax < tempRect.y + tempRect.height) { ymax = tempRect.y + tempRect.height; }
	}
	return Rect(xmin, ymin, xmax - xmin, ymax - ymin);
}


PlayingBoard::~PlayingBoard()
{
}