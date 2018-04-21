#include "stdafx.h"
#include "PlayingBoard.h"

# define M_PIl          3.141592653589793238462643383279502884L /* pi */

PlayingBoard::PlayingBoard()
{
	cards.resize(12);
}

PlayingBoard::~PlayingBoard()
{
}

void PlayingBoard::findCardsFromBoardImage(Mat const & boardImage)
{
	cv::Mat adaptedSrc, src, hsv, mask;
	resizeBoardImage(boardImage, src);
	cv::Mat croppedSrc = src(ROI);

	// filter out the cardregions, followed by the cards
	extractCardRegions(croppedSrc);
	extractCards();
}

void PlayingBoard::determineROI(const Mat & boardImage)
{
	cv::Mat adaptedSrc, src, hsv, mask;
	resizeBoardImage(boardImage, src);
	vector<vector<Point>> contours;
	vector<Vec4i> hierarchy;

	cvtColor(src, adaptedSrc, COLOR_BGR2GRAY);	// convert the image to gray
	threshold(adaptedSrc, adaptedSrc, 120, 255, THRESH_BINARY);	// threshold the image to keep only brighter regions (cards are white)										
	findContours(adaptedSrc, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE, Point(0, 0));	// find all the contours using the thresholded image

	auto new_end = std::remove_if(contours.begin(), contours.end(), [](const std::vector<cv::Point>& c1) {
		double area = contourArea(c1, false);	// make sure the contourArea is big enough to be a card
		Rect bounding_rect = boundingRect(c1);
		float aspectRatio = (float)bounding_rect.width / (float)bounding_rect.height;	// make sure the contours have an aspect ratio of an average card (to filter out navbar)
		return ((aspectRatio < 0.1) || (aspectRatio > 10) || (area < 10000)); });
	contours.erase(new_end, contours.end());
	std::sort(contours.begin(), contours.end(), [](const vector<Point>& c1, const vector<Point>& c2) -> bool { return contourArea(c1, false) > contourArea(c2, false); });

	Rect tempRect = boundingRect(contours.at(0));
	int xmin = tempRect.x;
	int xmax = tempRect.x + tempRect.width;
	int ymin = tempRect.y;
	int ymax = tempRect.y + tempRect.height;
	topCardsHeight = ymin;
	for (int i = 1; i < contours.size(); i++)
	{
		tempRect = boundingRect(contours.at(i));
		if (xmin > tempRect.x) { xmin = tempRect.x; }
		if (xmax < tempRect.x + tempRect.width) { xmax = tempRect.x + tempRect.width; }
		if (ymin > tempRect.y)
		{ 
			ymin = tempRect.y;
			topCardsHeight = tempRect.height;
		}
	}
	ROI = Rect(xmin - 10, ymin - 10, xmax - xmin + 20, standardBoardHeight - ymin);
	topCardsHeight += 30;
}

void PlayingBoard::resizeBoardImage(Mat const & boardImage, Mat & resizedBoardImage)
{
	// certain resolutions return an image with black borders, we have to remove these for our processing
	Mat gray, thresh;
	vector<vector<Point>> contours;
	vector<Vec4i> hierarchy;
	cv::cvtColor(boardImage, gray, COLOR_BGR2GRAY);
	cv::threshold(gray, thresh, 1, 255, THRESH_BINARY);
	cv::findContours(thresh, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
	Rect board = boundingRect(contours.at(0));
	Mat crop(boardImage, board);


	int width = crop.cols,
		height = crop.rows;
	

	cv::Mat targetImage = cv::Mat::zeros(standardBoardHeight, standardBoardWidth, crop.type());

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

	cv::resize(crop, targetImage(roi), roi.size());
	resizedBoardImage = targetImage.clone();
}

void PlayingBoard::extractCardRegions(const cv::Mat &src)
{
	cardRegions.clear();
	cv::Size srcSize = src.size();
	Mat croppedtopCards(src, Rect(0, 0, (int) srcSize.width, topCardsHeight));
	Mat croppedbottomCards(src, Rect(0, topCardsHeight, (int)srcSize.width, (int) (srcSize.height - topCardsHeight - 1)));
	Size topCardsSize = croppedtopCards.size();
	Size bottomCardsSize = croppedbottomCards.size();

	for (int i = 0; i < 7; i++)	// build stack
	{
		Rect cardLocationRect = Rect((int) bottomCardsSize.width / 7.1 * i, 0, (int) (bottomCardsSize.width / 7 - 1), bottomCardsSize.height);
		Mat croppedCard(croppedbottomCards, cardLocationRect);
		cardRegions.push_back(croppedCard.clone());
	}

	Rect deckCardsRect = Rect(topCardsSize.width / 7, 0, topCardsSize.width / 5, topCardsSize.height);	// deck
	Mat croppedCard(croppedtopCards, deckCardsRect);
	cardRegions.push_back(croppedCard.clone());

	for (int i = 3; i < 7; i++)	// foundations
	{
		Rect cardLocationRect = Rect((int)(topCardsSize.width / 7.1 * i), 0, (int)(topCardsSize.width / 7 - 1), topCardsSize.height);
		Mat croppedCard(croppedtopCards, cardLocationRect);
		cardRegions.push_back(croppedCard.clone());
	}
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
		return true;
	}
	else
	{
		return false;
	}
}

void PlayingBoard::extractCards()
{
	for (int i = 0; i < cardRegions.size(); i++)
	{		
		Mat adaptedSrc;
		vector<vector<Point>> contours;
		vector<Vec4i> hierarchy;

		cv::cvtColor(cardRegions.at(i), adaptedSrc, COLOR_BGR2GRAY);
		cv::GaussianBlur(adaptedSrc, adaptedSrc, cv::Size(5, 5), 0);
		cv::threshold(adaptedSrc, adaptedSrc, 220, 255, THRESH_BINARY);
		findContours(adaptedSrc, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE, Point(0, 0));

		if (contours.size() > 1)	// remove potential noise
		{
			auto new_end = std::remove_if(contours.begin(), contours.end(), [](const std::vector<cv::Point> & c1) { return (contourArea(c1, false) < 10000); });
			contours.erase(new_end, contours.end());
			std::sort(contours.begin(), contours.end(), [](const vector<Point>& c1, const vector<Point>& c2) -> bool { return contourArea(c1, false) > contourArea(c2, false); });
		}
		
		if ( contours.size() > 0 )
		{
			Rect br = boundingRect(contours.at(0));
			if (contours.size() == 2)	// the lowest rect is the actual card
			{
				Rect br2 = boundingRect(contours.at(1));
				if (br.y < br2.y)
				{
					br = br2;
				}
			}

			//getIndexOfSelectedCard(i);
			
			Mat card = Mat(cardRegions[i], br);		
			Mat croppedRef, resizedCardImage;
			//extractTopCardUsingSobel(card, croppedRef, i);
			extractTopCardUsingAspectRatio(card, croppedRef);
			croppedTopCardToStandardSize(croppedRef, resizedCardImage);
			cards.at(i) = resizedCardImage.clone();
		}
		else
		{
			Mat empty;
			cards.at(i) = empty;
		}

	}
}

int PlayingBoard::getIndexOfSelectedCard(int i)
{
	Mat selectedCard = cardRegions.at(i).clone();
	Mat hsv, mask;
	cv::cvtColor(selectedCard, hsv, COLOR_BGR2HSV);
	blur(hsv, hsv, Size(1, 1));
	Scalar lo_int(91, 95, 214);	// light blue
	Scalar hi_int(96, 160, 255);
	inRange(hsv, lo_int, hi_int, mask);
	cv::GaussianBlur(mask, mask, cv::Size(5, 5), 0);
	vector<vector<Point>> selected_contours;
	vector<Vec4i> selected_hierarchy;
	findContours(mask, selected_contours, selected_hierarchy, RETR_CCOMP, CHAIN_APPROX_SIMPLE, Point(0, 0));
	if (selected_contours.size() > 1)	// remove potential noise
	{
		auto new_end = std::remove_if(selected_contours.begin(), selected_contours.end(), [](const std::vector<cv::Point> & c1) { return (contourArea(c1, false) < 1000); });
		selected_contours.erase(new_end, selected_contours.end());
	}
	if (selected_contours.size() > 1)
	{
		int index = selected_contours.size() - 2;
		return index;
	}
	else
	{
		return -1;
	}
}

void PlayingBoard::extractTopCardUsingSobel(const cv::Mat &src, cv::Mat& dest, int i)
{
	Mat gray, grad, abs_grad, thresh_grad;
	cv::cvtColor(src, gray, COLOR_BGR2GRAY);
	vector<Vec4i> linesP;
	Vec4i l;	
	cv::Point lowest_pt1, lowest_pt2;
	cv::Point pt1, pt2;
	float rho, theta;
	double a, b, x0, y0;
	Size cardSize = src.size();
	Rect myROI;
	//Mat cdst = src.clone();

	if (i != 7)
	{
		/// Gradient Y
		cv::Sobel(gray, grad, CV_16S, 0, 1, 3, 1, 0, BORDER_DEFAULT);
		cv::convertScaleAbs(grad, abs_grad);
		cv::threshold(abs_grad, thresh_grad, 70, 255, THRESH_BINARY);
		lowest_pt1.y = 0;
		HoughLinesP(thresh_grad, linesP, 1, CV_PI / 180, 30, 100, 10);
		for (size_t i = 0; i < linesP.size(); i++)
		{
			l = linesP[i];
			if (abs(l[1] - l[3]) < 5 && l[1] > lowest_pt1.y && l[1] < cardSize.height - 50)
			{
				lowest_pt1 = Point(l[0], l[1]);
				lowest_pt2 = Point(l[2], l[3]);
				//line(cdst, Point(l[0], l[1]), Point(l[2], l[3]), Scalar(0, 0, 255), 3, CV_AA);
			}
		}
		myROI.x = 0, myROI.width = cardSize.width;
		myROI.y = lowest_pt1.y, myROI.height = cardSize.height - lowest_pt1.y;
	}
	else
	{
		
		/// Gradient X
		cv::Sobel(gray, grad, CV_16S, 1, 0, 3, 1, 0, BORDER_DEFAULT);
		cv::convertScaleAbs(grad, abs_grad);
		cv::threshold(abs_grad, thresh_grad, 70, 255, THRESH_BINARY);
		lowest_pt1.y = 0;
		HoughLinesP(thresh_grad, linesP, 1, CV_PI / 180, 30, 100, 10);
		for (size_t i = 0; i < linesP.size(); i++)
		{
			l = linesP[i];
			if (abs(l[0] - l[2]) < 1 && l[0] > lowest_pt1.x && l[0] < cardSize.width - 50)
			{
				lowest_pt1 = Point(l[0], l[1]);
				lowest_pt2 = Point(l[2], l[3]);
				//line(cdst, Point(l[0], l[1]), Point(l[2], l[3]), Scalar(0, 0, 255), 3, CV_AA);
			}

		}
		myROI.x = lowest_pt1.x, myROI.width = cardSize.width - lowest_pt1.x;
		myROI.y = 0, myROI.height = cardSize.height;
	}
	Mat croppedRef(src, myROI);
	dest = croppedRef.clone();
}

void PlayingBoard::croppedTopCardToStandardSize(const cv::Mat &croppedRef, cv::Mat &resizedCardImage)
{
	int width = croppedRef.cols,
		height = croppedRef.rows;
	resizedCardImage = cv::Mat::zeros(standardCardHeight, standardCardWidth, croppedRef.type());
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
	cv::resize(croppedRef, resizedCardImage(roi), roi.size());
}

void PlayingBoard::extractTopCardUsingAspectRatio(const cv::Mat & src, cv::Mat & dest)
{
	Size cardSize = src.size();
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

	Mat croppedRef(src, myROI);
	dest = croppedRef.clone();
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