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
	cv::Rect ROI = Rect(standardWidth * 0.17, standardHeight * 0.07, standardWidth * 0.67, standardHeight * 0.90);
	cv::Mat croppedSrc = src(ROI);
	cv::cvtColor(croppedSrc, hsv, COLOR_BGR2HSV);
	blur(hsv, hsv, Size(10, 10));

	Scalar lo(72, 184, 105); // mean - var for low	(hsv mean, var: 75.5889 199.844 122.861 0.621726 13.5166 8.62088)
	Scalar hi(76, 215, 140); // mean + var for high

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

	cv::Mat targetImage = cv::Mat::zeros(standardHeight, standardWidth, boardImage.type());

	int max_dim = (width >= height) ? width : height;
	float scale = ((float)standardWidth) / max_dim;
	cv::Rect roi;
	if (width >= height)
	{
		roi.width = standardWidth;
		roi.x = 0;
		roi.height = height * scale;
		roi.y = (standardHeight - roi.height) / 2;
	}
	else
	{
		roi.y = 0;
		roi.height = standardWidth;
		roi.width = width * scale;
		roi.x = (standardWidth - roi.width) / 2;
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