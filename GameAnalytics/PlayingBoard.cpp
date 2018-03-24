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
	cv::Mat adaptedSrc, src, hsv, mask;
	resizeBoardImage(boardImage, src);
	cv::Rect ROI = Rect(standardBoardWidth * 0.17, standardBoardHeight * 0.07, standardBoardWidth * 0.67, standardBoardHeight * 0.90);
	cv::Mat croppedSrc = src(ROI);
	cv::cvtColor(croppedSrc, hsv, COLOR_BGR2HSV);
	blur(hsv, hsv, Size(10, 10));

	//removing low intensities
	Scalar lo_int(0, 2, 0);
	Scalar hi_int(180, 255, 125);
	inRange(hsv, lo_int, hi_int, mask);
	croppedSrc.setTo(Scalar(0, 0, 0), mask);

	//removing green
	Scalar lo(72, 184, 105); // (hsv mean, var: 75.5889 199.844 122.861 0.621726 13.5166 8.62088)
	Scalar hi(76, 215, 140);
	inRange(hsv, lo, hi, mask);
	croppedSrc.setTo(Scalar(0, 0, 0), mask);

	// filter out the cardregions, followed by the cards
	extractCards( extractCardRegions( croppedSrc ) );
}

void PlayingBoard::resizeBoardImage(Mat const & boardImage, Mat & resizedBoardImage)
{
	int width = boardImage.cols, 
		height = boardImage.rows;

	cv::Mat targetImage = cv::Mat::zeros(standardBoardHeight, standardBoardWidth, boardImage.type());

	cv::Rect roi;
	if (width >= height)
	{
		float scale = ((float)standardBoardWidth) / width;
		roi.width = standardBoardWidth;
		roi.x = 0;
		roi.height = height * scale - 1;
		roi.y = (standardBoardHeight - roi.height) / 2;
	}
	else
	{
		float scale = ((float)standardBoardHeight) / height;
		roi.y = 0;
		roi.height = standardBoardHeight;
		roi.width = width * scale - 1;
		roi.x = (standardBoardWidth - roi.width) / 2;
	}

	cv::resize(boardImage, targetImage(roi), roi.size());
	resizedBoardImage = targetImage.clone();
}

std::vector<cv::Mat> PlayingBoard::extractCardRegions(const cv::Mat &src)
{
	cv::Size srcSize = src.size();
	int topCardsHeight = srcSize.height * 0.26;
	Mat croppedtopCards(src, Rect(0, 0, (int) srcSize.width, topCardsHeight));
	Mat croppedbottomCards(src, Rect(0, topCardsHeight, (int)srcSize.width, (int) (srcSize.height - topCardsHeight - 1)));
	Size topCardsSize = croppedtopCards.size();
	Size bottomCardsSize = croppedbottomCards.size();
	std::vector<cv::Mat> playingCards;

	for (int i = 0; i < 7; i++)	// build stack
	{
		Rect cardLocationRect = Rect((int) bottomCardsSize.width / 7.1 * i, 0, (int) (bottomCardsSize.width / 7 - 1), bottomCardsSize.height);
		Mat croppedCard(croppedbottomCards, cardLocationRect);
		playingCards.push_back(croppedCard.clone());
	}

	Rect deckCardsRect = Rect(topCardsSize.width / 7, 0, topCardsSize.width / 5, topCardsSize.height);	// deck
	Mat croppedCard(croppedtopCards, deckCardsRect);
	playingCards.push_back(croppedCard.clone());

	for (int i = 3; i < 7; i++)	// foundations
	{
		Rect cardLocationRect = Rect((int)(topCardsSize.width / 7.1 * i), 0, (int)(topCardsSize.width / 7 - 1), topCardsSize.height);
		Mat croppedCard(croppedtopCards, cardLocationRect);
		playingCards.push_back(croppedCard.clone());
	}
	return playingCards;
}

boardState PlayingBoard::identifyGameState(const cv::Mat &src)
{
	// check for out of moves state
	Size imageSize = src.size();
	Rect middle = Rect(0, imageSize.height / 3, imageSize.width, imageSize.height / 3);
	Mat croppedSrc(src, middle);
	cvtColor(croppedSrc, croppedSrc, COLOR_BGR2GRAY);
	threshold(croppedSrc, croppedSrc, 240, 255, THRESH_BINARY);	// threshold the image to keep only brighter regions (cards are white)										
	if (cv::countNonZero(croppedSrc) > croppedSrc.rows * croppedSrc.cols * 0.7)
	{
		return outOfMoves;
	}
	else
	{
		return playing;
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
		cv::threshold(adaptedSrc, adaptedSrc, 220, 255, THRESH_BINARY);
		findContours(adaptedSrc, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE, Point(0, 0));

		std::sort(contours.begin(), contours.end(), [] (const vector<Point>& c1, const vector<Point>& c2) -> bool { return contourArea(c1, false) > contourArea(c2, false); });
		if ( contours.size() > 0 )
		{
			Mat card = Mat(playingCards[i], boundingRect(contours.at(0))).clone();
			Size cardSize = card.size();
			Rect myROI;

			// extract the card from the image
			if (cardSize.width * 1.35 > cardSize.height)
			{
				myROI.y = 0;
				myROI.height = cardSize.height;
				myROI.x = cardSize.width - cardSize.height / 1.34 + 1;
				myROI.width = cardSize.width - myROI.x;
			}
			else
			{
				myROI.x = 0;
				myROI.y = cardSize.height - cardSize.width * 1.32 - 1;
				myROI.height = cardSize.height - myROI.y;
				myROI.width = cardSize.width;
			}
			Mat croppedRef(card, myROI);

			// check that the image isn't 'empty' (meaning no card at that location)
			Mat checkImage;
			cv::cvtColor(croppedRef, checkImage, COLOR_BGR2GRAY);
			cv::threshold(checkImage, checkImage, 200, 255, THRESH_BINARY);
			if (cv::countNonZero(checkImage) < checkImage.rows * checkImage.cols * 0.3)
			{
				Mat empty;
				cards.at(i) = empty;
				continue;
			}

			// resize the card to a standard size
			int width = croppedRef.cols,
				height = croppedRef.rows;
			cv::Mat targetImage = cv::Mat::zeros(standardCardHeight, standardCardWidth, croppedRef.type());
			cv::Rect roi;
			if (width >= height)
			{
				float scale = ((float)standardCardWidth) / width;
				roi.width = standardCardWidth;
				roi.x = 0;
				roi.height = height * scale;
				roi.y = 0;
			}
			else
			{
				float scale = ((float)standardCardHeight) / height;
				roi.y = 0;
				roi.height = standardCardHeight;
				roi.width = width * scale;
				roi.x = 0;
			}
			cv::resize(croppedRef, targetImage(roi), roi.size());
			cards.at(i) = targetImage.clone();
		}
		else
		{
			Mat empty;
			cards.at(i) = empty;
		}
	}
}

const std::vector<cv::Mat> & PlayingBoard::getCards()
{
	return cards;
}