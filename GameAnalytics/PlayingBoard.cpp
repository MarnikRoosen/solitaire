#include "stdafx.h"
#include "PlayingBoard.h"

PlayingBoard::PlayingBoard()
{
	cards.resize(12);
	state = playing;
}

PlayingBoard::~PlayingBoard()
{
}

void PlayingBoard::findCardsFromBoardImage(Mat const & boardImage)
{
	Mat adaptedSrc, src = boardImage.clone();
	if (checkForOutOfMovesState(src)) { return; }

	// convert image and find contours
	vector<vector<Point>> contours;
	vector<Vec4i> hierarchy;
	cvtColor(src, adaptedSrc, COLOR_BGR2GRAY);
	threshold(adaptedSrc, adaptedSrc, 120, 255, THRESH_BINARY);
	findContours(adaptedSrc, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE, Point(0, 0));

	// keep only large enough contours
	auto new_end = std::remove_if(contours.begin(), contours.end(), [] (const std::vector<cv::Point> & c1) {
		double area = contourArea(c1, false);
		Rect bounding_rect = boundingRect(c1);
		float aspectRatio = (float) bounding_rect.width / (float) bounding_rect.height;
		return ((aspectRatio < 0.1) || (aspectRatio > 10) || (area < 10000)); });
	contours.erase(new_end, contours.end());
	std::sort(contours.begin(), contours.end(), [] (const vector<Point>& c1, const vector<Point>& c2) -> bool { return contourArea(c1, false) > contourArea(c2, false); });

	// filter out the cardregions, followed by the cards
	extractCards( extractCardRegions(src, contours) );
}

std::vector<cv::Mat> PlayingBoard::extractCardRegions(cv::Mat &src, std::vector<std::vector<cv::Point>> &contours)
{
	Mat croppedOuterEdge(src, determineOuterRect(contours));
	Size outerEdgeSize = croppedOuterEdge.size();
	Mat croppedtopCards(croppedOuterEdge, Rect(0, 0, (int) outerEdgeSize.width, (int) outerEdgeSize.width*0.18));
	Mat croppedbottomCards(croppedOuterEdge, Rect(0, (int) outerEdgeSize.width * 0.18, (int) outerEdgeSize.width, (int) (outerEdgeSize.height - outerEdgeSize.width * 0.18 - 1)));
	Size topCardsSize = croppedtopCards.size();
	Size bottomCardsSize = croppedbottomCards.size();
	std::vector<cv::Mat> playingCards;

	for (int i = 0; i < 7; i++)	// tableau
	{
		Rect cardLocationRect = Rect((int) bottomCardsSize.width / 7 * i, 0, (int) (bottomCardsSize.width / 6.9 - 3), bottomCardsSize.height);
		Mat croppedCard(croppedbottomCards, cardLocationRect);
		playingCards.push_back(croppedCard.clone());
	}

	Rect deckCardsRect = Rect(topCardsSize.width / 7, 0, topCardsSize.width / 5, topCardsSize.height);	// deck
	Mat croppedCard(croppedtopCards, deckCardsRect);
	playingCards.push_back(croppedCard.clone());

	for (int i = 3; i < 7; i++)	// foundations
	{
		Rect cardLocationRect = Rect((int)(topCardsSize.width / 7 * i), 0, (int)(topCardsSize.width / 6.9 - 3), topCardsSize.height);
		Mat croppedCard(croppedtopCards, cardLocationRect);
		playingCards.push_back(croppedCard.clone());
	}
	return playingCards;
}

bool PlayingBoard::checkForOutOfMovesState(cv::Mat &src)
{
	Size imageSize = src.size();
	Rect middle = Rect(0, imageSize.height / 3, imageSize.width, imageSize.height / 3);
	Mat croppedSrc(src, middle);
	cvtColor(croppedSrc, croppedSrc, COLOR_BGR2GRAY);
	threshold(croppedSrc, croppedSrc, 240, 255, THRESH_BINARY);	// threshold the image to keep only brighter regions (cards are white)										
	if (cv::countNonZero(croppedSrc) > croppedSrc.rows * croppedSrc.cols * 0.7)
	{
		state = outOfMoves;
		return true;
	}
	else
	{
		state = playing;
		return false;
	}
}

void PlayingBoard::extractCards(std::vector<cv::Mat> &playingCards)
{
	for (int i = 0; i < playingCards.size(); i++)
	{		
		Mat adaptedSrc;
		vector<vector<Point>> contours, validContours;
		vector<Vec4i> hierarchy;

		cv::cvtColor(playingCards.at(i), adaptedSrc, COLOR_BGR2GRAY);
		cv::threshold(adaptedSrc, adaptedSrc, 200, 255, THRESH_BINARY);
		findContours(adaptedSrc, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE, Point(0, 0));
		auto new_end = std::remove_if(contours.begin(), contours.end(), [] (const std::vector<cv::Point> & c1) {
			double area = contourArea(c1, false);
			Rect bounding_rect = boundingRect(c1);
			float aspectRatio = (float) bounding_rect.width / (float) bounding_rect.height;
			return ((aspectRatio < 0.1) || (aspectRatio > 10) || (area < 10000)); });
		contours.erase(new_end, contours.end());
		std::sort(contours.begin(), contours.end(), [] (const vector<Point>& c1, const vector<Point>& c2) -> bool { return contourArea(c1, false) > contourArea(c2, false); });

		if ( contours.size() > 0 )
		{
			Mat card = Mat(playingCards[i], boundingRect(contours.at(0))).clone();
			Size cardSize = card.size();
			Rect myROI;

			(cardSize.width * 1.35 > cardSize.height) ?	// card height is 33% longer than card width -> extract the topcard from a stack
				(myROI = Rect((int) (cardSize.width - cardSize.height / 1.34 + 1), 0, (int) cardSize.height / 1.34 - 1, cardSize.height)) :
				(myROI = Rect(0, (int) (cardSize.height - cardSize.width * 1.33), cardSize.width, (int) cardSize.width * 1.33));

			Mat croppedRef(card, myROI);
			Mat checkImage;
			cv::cvtColor(croppedRef, checkImage, COLOR_BGR2GRAY);
			cv::threshold(checkImage, checkImage, 200, 255, THRESH_BINARY);

			if (cv::countNonZero(checkImage) > checkImage.rows * checkImage.cols * 0.3)
			{
				cards.at(i) = croppedRef.clone();
				continue;
			}
		}
		Mat empty;
		cards.at(i) = empty;
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
	return Rect(xmin * 0.99, ymin * 0.99, (xmax - xmin) * 1.02, (ymax - ymin) * 1.02);	// adding small extra margin
}

const playingBoardState & PlayingBoard::getState()
{
	return state;
}

const std::vector<cv::Mat> & PlayingBoard::getCards()
{
	return cards;
}
