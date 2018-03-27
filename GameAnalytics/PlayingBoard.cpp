#include "stdafx.h"
#include "PlayingBoard.h"

# define M_PIl          3.141592653589793238462643383279502884L /* pi */

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
	/*Scalar lo_int(0, 0, 0);
	Scalar hi_int(180, 255, 80);
	inRange(hsv, lo_int, hi_int, mask);
	croppedSrc.setTo(Scalar(0, 0, 0), mask);*/

	//removing green
	Scalar lo(72, 184, 105); //(hsv mean, var: 75.5889 199.844 122.861 0.621726 13.5166 8.62088)
	Scalar hi(76, 215, 140);
	inRange(hsv, lo, hi, mask);
	croppedSrc.setTo(Scalar(0, 0, 0), mask);

	if (checkForOutOfMovesState(boardImage)) { return; }

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

bool PlayingBoard::checkForOutOfMovesState(const cv::Mat &src)
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
		cv::GaussianBlur(adaptedSrc, adaptedSrc, cv::Size(5, 5), 0);
		cv::threshold(adaptedSrc, adaptedSrc, 220, 255, THRESH_BINARY);
		findContours(adaptedSrc, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE, Point(0, 0));

		if (contours.size() > 1)	// exceptional cases in which small noisefragments would be visible
		{
			auto new_end = std::remove_if(contours.begin(), contours.end(), [](const std::vector<cv::Point> & c1) { return (contourArea(c1, false) < 10000); });
			contours.erase(new_end, contours.end());
			std::sort(contours.begin(), contours.end(), [](const vector<Point>& c1, const vector<Point>& c2) -> bool { return contourArea(c1, false) > contourArea(c2, false); });
		}
		
		if ( contours.size() > 0 )
		{
			Rect br = boundingRect(contours.at(0));
			Mat card = Mat(playingCards[i], br).clone();
	
			Rect selectedRegion = br;
			if (selectedRegion.x >= 3) selectedRegion.x -= 3;
			if (selectedRegion.height + 3 <= playingCards.at(i).rows) selectedRegion.height += 3;
			if (selectedRegion.width + 6 <= playingCards.at(i).cols) selectedRegion.width += 6;
			Mat selectedCard = Mat(playingCards[i], selectedRegion);
			Mat hsv, mask;
			cv::cvtColor(selectedCard, hsv, COLOR_BGR2HSV);
			blur(hsv, hsv, Size(1, 1));
			Scalar lo_int(89, 43, 172);	// light blue
			Scalar hi_int(95, 168, 239);
			inRange(hsv, lo_int, hi_int, mask);
			if (countNonZero(mask) > 0)	// card has a blue border -> selected!
			{
				indexOfSelectedCard = i;
			}
			else if (indexOfSelectedCard == i)
			{
				indexOfSelectedCard = -1;
			}

			Mat gray, edges, lines, img, thresh;
			cv::cvtColor(card, gray, COLOR_BGR2GRAY);
			cv::threshold(gray, thresh, 200, 255, THRESH_BINARY);
			cv::Canny(thresh, edges, 0, 255);
			int minLineLength = 100;
			int maxLineGap = 5;
			HoughLinesP(edges, lines, 1, M_PIl / 180, 100, minLineLength, maxLineGap);




			// Extracting only the card from the segment
			Size cardSize = card.size();
			Rect myROI;
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
			Mat checkImage, resizedCardImage;

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
			resizedCardImage = targetImage.clone();

			cv::cvtColor(resizedCardImage, checkImage, COLOR_BGR2GRAY);
			cv::threshold(checkImage, checkImage, 200, 255, THRESH_BINARY);

			if (cv::countNonZero(checkImage) > checkImage.rows * checkImage.cols * 0.3)
			{
				cards.at(i) = resizedCardImage.clone();
				continue;
			}
		}
		Mat empty;
		cards.at(i) = empty;
	}
}

const playingBoardState & PlayingBoard::getState()
{
	return state;
}

const std::vector<cv::Mat> & PlayingBoard::getCards()
{
	return cards;
}

int PlayingBoard::getSelectedCard()
{
	return indexOfSelectedCard;
}

/*
Mat im_hsv, dist;
void pick_color(int e, int x, int y, int s, void *)
{
	if (e == 1)
	{
		int r = 3;
		int off[9 * 2] = { 0,0, -r,-r, -r,0, -r,r, 0,r, r,r, r,0, r,-r, 0,-r };
		for (int i = 0; i<9; i++)
		{
			Vec3b p = im_hsv.at<Vec3b>(y + off[2 * i], x + off[2 * i + 1]);
			cerr << int(p[0]) << " " << int(p[1]) << " " << int(p[2]) << endl;
			dist.push_back(p);
		}
	}
}

int main(int argc, char** argv)
{
	namedWindow("blue");
	setMouseCallback("blue", pick_color);

	String c_in = "."; // check a whole folder.
	if (argc>1) c_in = argv[1]; // or an image
	vector<String> fn;
	glob(c_in, fn, true);
	for (size_t i = 0; i<fn.size(); i++)
	{
		Mat im_bgr = imread(fn[i]);
		if (im_bgr.empty()) continue;
		cvtColor(im_bgr, im_hsv, COLOR_BGR2HSV);
		imshow("blue", im_bgr);
		int k = waitKey() & 0xff;
		if (k == 27) break; // esc.
	}
	Scalar m, v;
	meanStdDev(dist, m, v);
	cerr << "mean, var: " << endl;
	cerr << m[0] << " " << m[1] << " " << m[2] << " " << v[0] << " " << v[1] << " " << v[2] << endl;
	waitKey(0);
	return 0;
}*/