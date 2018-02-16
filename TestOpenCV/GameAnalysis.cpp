// Screen capture source: https://stackoverflow.com/questions/26888605/opencv-capturing-desktop-screen-live
// Mouse events source: https://opencv-srf.blogspot.be/2011/11/mouse-events.html

#include "stdafx.h"
#include "GameAnalysis.h"
#include <opencv2/opencv.hpp>
#include "opencv2/core.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/features2d.hpp"
#include <string>

const string basePath = "C:/Users/marni/source/repos/gameAnalysis/x64/Debug/";

int main(int argc, char** argv)
{
	loadTemplates();
	standardCardSize.width = 133;
	standardCardSize.height = 177;
	Mat card = detectCard();
	std::pair<Mat, Mat> cardCharacteristics = getCardCharacteristics(card);
	classifyCard2(cardCharacteristics);
	//getApplicationView();
	return 0;
}

void classifyCard2(std::pair<Mat, Mat> cardCharacteristics)
{
	String card;
	Mat rank = cardCharacteristics.first;
	cv::resize(rank, rank, cv::Size(rank.cols * 2, rank.rows * 2), cv::INTER_NEAREST);
	cvtColor(rank, rank, COLOR_BGR2GRAY);
	GaussianBlur(rank, rank, Size(1, 1), 0);
	threshold(rank, rank, 120, 255, THRESH_BINARY_INV);

	vector<vector<Point>> contours;
	vector<Vec4i> hierarchy;
	findContours(rank, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE, Point(0, 0));	// find all the contours using the thresholded image

	Mat rank_rgb(rank.size(), CV_8UC3);
	cv::cvtColor(rank, rank_rgb, CV_GRAY2RGB);
	for (int i = 0; i< contours.size(); i++)
	{
		drawContours(rank_rgb, contours, i, Scalar(0, 0, 255));
	}
	imshow("contours rank", rank_rgb);

	if (contours.size() > 1)
	{
		card += "10 of";
	}

	waitKey(0);

}

void classifyCard(std::pair<Mat, Mat> cardCharacteristics)
{
	double lowest_dist = 100;
	int index_of_closest_match = 0;
	for (int i = 0; i < ranks.size(); i++)
	{
		Mat rank_i = ranks.at(i);
		// Detect the keyPoints and compute its descriptors using ORB Detector.
		vector<KeyPoint> keyPoints1, keyPoints2;
		Mat descriptors1, descriptors2;
		Ptr<ORB> detector = ORB::create();
		detector->detectAndCompute(cardCharacteristics.first, Mat(), keyPoints1, descriptors1);
		detector->detectAndCompute(rank_i, Mat(), keyPoints2, descriptors2);

		/*
		//-- Step 3: Matching descriptor vectors with a brute force matcher
		BFMatcher bfmatcher(NORM_L2);
		std::vector< DMatch > bfmatches;
		bfmatcher.match(descriptors1, descriptors2, bfmatches);

		//-- Draw matches
		Mat bfimg_matches;
		drawMatches(cardCharacteristics.first, keyPoints1, rank_i, keyPoints2, bfmatches, bfimg_matches);

		//-- Show detected matches
		imshow("bfMatches", bfimg_matches);
		*/
		waitKey(0);


		// Match features.
		FlannBasedMatcher matcher;
		vector< DMatch > matches;
		descriptors1.convertTo(descriptors1, CV_32F);
		descriptors2.convertTo(descriptors2, CV_32F);
		matcher.match(descriptors1, descriptors2, matches);
		double max_dist = 0; double min_dist = 100;

		//-- Quick calculation of max and min distances between keypoints
		for (int i = 0; i < descriptors1.rows; i++)
		{
			double dist = matches[i].distance;
			if (dist < min_dist) min_dist = dist;
			if (dist > max_dist) max_dist = dist;
		}
		printf("-- Max dist : %f \n", max_dist);
		printf("-- Min dist : %f \n", min_dist);
		//-- Draw only "good" matches (i.e. whose distance is less than 2*min_dist,
		//-- or a small arbitary value ( 0.02 ) in the event that min_dist is very
		//-- small)
		//-- PS.- radiusMatch can also be used here.
		std::vector< DMatch > good_matches;
		for (int i = 0; i < descriptors1.rows; i++)
		{
			if (matches[i].distance <= max(2 * min_dist, 0.02))
			{
				good_matches.push_back(matches[i]);
			}
		}
		//-- Draw only "good" matches
		Mat img_matches;
		drawMatches(cardCharacteristics.first, keyPoints1, rank_i, keyPoints2,
			matches, img_matches, Scalar::all(-1), Scalar::all(-1),
			vector<char>(), DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);
		//-- Show detected matches
		imshow("Good Matches", img_matches);
		//-- Show detected matches
		for (int i = 0; i < (int)good_matches.size(); i++)
		{
			printf("-- Good Match [%d] Keypoint 1: %d  -- Keypoint 2: %d  \n", i, good_matches[i].queryIdx, good_matches[i].trainIdx);
		}
		if (lowest_dist > min_dist)
		{
			lowest_dist = min_dist;
			index_of_closest_match = i;
		}

	}
	printf("Closest match has index %d  \n", index_of_closest_match);
	waitKey(0);




	/*for (int i = 0; i < ranks.size(); i++)
	{
	String filename = "3ofspades.png";
	Mat ranki = ranks.at(0);
	imshow("ranki", ranki);
	resize(ranki, ranki, standardSize);
	Mat first = cardCharacteristics.first;
	resize(first, first, ranki.size());
	imshow("first", first);
	Mat diff_img;
	waitKey(0);
	compare(first, ranki, diff_img, cv::CMP_EQ);
	int percentage = countNonZero(diff_img);
	printf("Rankdiff %d \n", percentage);
	}*/
}

void loadTemplates()
{
	String filename = "cards/2.png";
	Mat rank2 = imread(basePath + filename);
	ranks.push_back(rank2);

	filename = "cards/3.png";
	Mat rank3 = imread(basePath + filename);
	ranks.push_back(rank3);

	filename = "cards/4.png";
	Mat rank4 = imread(basePath + filename);
	ranks.push_back(rank4);

	filename = "cards/5.png";
	Mat rank5 = imread(basePath + filename);
	ranks.push_back(rank5);

	filename = "cards/6.png";
	Mat rank6 = imread(basePath + filename);
	ranks.push_back(rank6);

	filename = "cards/7.png";
	Mat rank7 = imread(basePath + filename);
	//threshold(rank7, rank7, 120, 255, THRESH_BINARY_INV);
	ranks.push_back(rank7);

	filename = "cards/8.png";
	Mat rank8 = imread(basePath + filename);
	ranks.push_back(rank8);

	filename = "cards/9.png";
	Mat rank9 = imread(basePath + filename);
	ranks.push_back(rank9);

	filename = "cards/10.png";
	Mat rank10 = imread(basePath + filename);
	resize(rank10, rank10, Size(), 0.4, 0.4);	// Take the biggest number as standardsize
	standardRankSize = rank10.size();
	ranks.push_back(rank10);

	filename = "cards/J.png";
	Mat rankJ = imread(basePath + filename);
	ranks.push_back(rankJ);

	filename = "cards/Q.png";
	Mat rankQ = imread(basePath + filename);
	ranks.push_back(rankQ);

	filename = "cards/K.png";
	Mat rankK = imread(basePath + filename);
	ranks.push_back(rankK);

	filename = "cards/A.png";
	Mat rankA = imread(basePath + filename);
	ranks.push_back(rankA);

	for (int i = 0; i < ranks.size(); i++)
	{
		cvtColor(ranks.at(i), ranks.at(i), COLOR_BGR2GRAY);
		resize(ranks.at(i), ranks.at(i), standardRankSize);
	}
}

std::pair<Mat, Mat> getCardCharacteristics(Mat aCard)
{
	Mat card = aCard;
	if (card.size() != standardCardSize)
	{
		resize(aCard, aCard, standardCardSize);
	}

	// Get the rank and suit from the resized card
	Rect myRankROI(2, 4, 25, 23);
	Mat croppedRankRef(card, myRankROI);
	Mat rank;
	croppedRankRef.copyTo(rank);	// Copy the data into new matrix
	imshow("rank", rank);

	Rect mySuitROI(6, 27, 17, 16);
	Mat croppedSuitRef(card, mySuitROI);
	Mat suit;
	croppedSuitRef.copyTo(suit);	// Copy the data into new matrix
	imshow("suit", suit);

	std::pair<Mat, Mat> cardCharacteristics = std::make_pair(rank, suit);
	return cardCharacteristics;
}

Mat detectCard()
{
	String filename = "10ofdiamonds.png";	// load testimage
	Mat src = imread(basePath + filename);
	if (!src.data)	// check for invalid input
	{
		cout << "Could not open or find the image" << std::endl;
		exit(EXIT_FAILURE);
	}

	imshow("original image", src);
	Mat grayImg, blurredImg, threshImg;
	vector<vector<Point>> contours;
	vector<Vec4i> hierarchy;

	cvtColor(src, grayImg, COLOR_BGR2GRAY);	// convert the image to gray
	GaussianBlur(grayImg, blurredImg, Size(1, 1), 1000);	// apply gaussian blur to improve card detection
	threshold(blurredImg, threshImg, 130, 255, THRESH_BINARY);	// threshold the image to keep only brighter regions (cards are white)										
	findContours(threshImg, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE, Point(0, 0));	// find all the contours using the thresholded image

	// find the largest contour
	int largest_area = 0;
	int largest_contour_index = 0;
	Rect bounding_rect;

	for (int i = 0; i < contours.size(); i++) // Iterate through each contour. 
	{
		double a = contourArea(contours[i], false);  // Find the area of contour
		if (a > largest_area) {
			largest_area = a;
			largest_contour_index = i;                // Store the index of largest contour
			bounding_rect = boundingRect(contours[i]); // Find the bounding rectangle for biggest contour
		}
	}

	// Get only the top card
	Mat card = Mat(src, bounding_rect).clone();
	Size cardSize = card.size();
	int topCardHeight = cardSize.width * 1.33;
	Rect myROI(0, cardSize.height - topCardHeight, cardSize.width, topCardHeight);
	Mat croppedRef(card, myROI);
	Mat topCard;
	croppedRef.copyTo(topCard);	// Copy the data into new matrix
	
	imshow("topCard", topCard);
	waitKey(0);
	return topCard;
}

void getApplicationView()
{
	HWND hwndDesktop = GetDesktopWindow();	//returns a desktop window handler
											//namedWindow("output", WINDOW_NORMAL);	//creates a resizable window
	int key = 0;

	while (key != 27)	//key = 27 -> error
	{
		Mat src = hwnd2mat(hwndDesktop);
		namedWindow("My Window", WINDOW_NORMAL);
		setMouseCallback("My Window", CallBackFunc, NULL);	//Function: void setMouseCallback(const string& winname, MouseCallback onMouse, void* userdata = 0)
															//imshow("My Window", src);
		waitKey(1);
	}
}

Mat hwnd2mat(HWND hwnd)	//Mat = n-dimensional dense array class, HWND = handle for desktop window
{
	HDC hwindowDC, hwindowCompatibleDC;

	int height, width, srcheight, srcwidth;
	HBITMAP hbwindow;
	Mat src;
	BITMAPINFOHEADER  bi;

	hwindowDC = GetDC(hwnd);	//get device context
	hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);	//creates a compatible memory device context for the device
	SetStretchBltMode(hwindowCompatibleDC, COLORONCOLOR);	//set bitmap stretching mode, coloroncolor deletes all eliminated lines of pixels

	RECT windowsize;    // get the height and width of the screen
	GetClientRect(hwnd, &windowsize);	//get coordinates of clients window

	srcheight = windowsize.bottom;
	srcwidth = windowsize.right;
	height = windowsize.bottom / 1;  //possibility to resize the client window screen
	width = windowsize.right / 1;

	src.create(height, width, CV_8UC4);	//creates matrix with a given height and width, CV_ 8 unsigned 4 (color)channels

	// create a bitmap
	hbwindow = CreateCompatibleBitmap(hwindowDC, width, height);
	bi.biSize = sizeof(BITMAPINFOHEADER);    //http://msdn.microsoft.com/en-us/library/windows/window/dd183402%28v=vs.85%29.aspx
	bi.biWidth = width;
	bi.biHeight = -height;  //origin of the source is the top left corner, height is 'negative'
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	// use the previously created device context with the bitmap
	SelectObject(hwindowCompatibleDC, hbwindow);
	// copy from the window device context to the bitmap device context
	StretchBlt(hwindowCompatibleDC, 0, 0, width, height, hwindowDC, 0, 0, srcwidth, srcheight, SRCCOPY); //change SRCCOPY to NOTSRCCOPY for wacky colors !
	GetDIBits(hwindowCompatibleDC, hbwindow, 0, height, src.data, (BITMAPINFO *)&bi, DIB_RGB_COLORS);  //copy from hwindowCompatibleDC to hbwindow

	// avoid memory leak
	DeleteObject(hbwindow);
	DeleteDC(hwindowCompatibleDC);
	ReleaseDC(hwnd, hwindowDC);

	return src;
}

void CallBackFunc(int event, int x, int y, int flags, void* userdata)
{
	if (event == EVENT_LBUTTONDOWN)
	{
		cout << "Left button of the mouse is clicked - position (" << x << ", " << y << ")" << endl;
	}
	else if (event == EVENT_RBUTTONDOWN)
	{
		cout << "Right button of the mouse is clicked - position (" << x << ", " << y << ")" << endl;
	}
	else if (event == EVENT_MBUTTONDOWN)
	{
		cout << "Middle button of the mouse is clicked - position (" << x << ", " << y << ")" << endl;
	}
}
