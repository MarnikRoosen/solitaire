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
	//generateData();
	standardCardSize.width = 133;
	standardCardSize.height = 177;
	Mat card = detectCard();
	std::pair<Mat, Mat> cardCharacteristics = getCardCharacteristics(card);
	classifyCard(cardCharacteristics);
	//getApplicationView();
	return 0;
}

void classifyCard(std::pair<Mat, Mat> cardCharacteristics)
{
	Mat matClassificationInts;      // read in classification data
	FileStorage fsClassifications("classifications.xml", FileStorage::READ);
	if (fsClassifications.isOpened() == false) {
		std::cout << "error, unable to open training classifications file, exiting program\n\n";
		exit(EXIT_FAILURE);
	}
	fsClassifications["classifications"] >> matClassificationInts;
	fsClassifications.release();

	Mat matTrainingImagesAsFlattenedFloats;	// read in trained images
	FileStorage fsTrainingImages("images.xml", FileStorage::READ);
	if (fsTrainingImages.isOpened() == false) {
		std::cout << "error, unable to open training images file, exiting program\n\n";
		exit(EXIT_FAILURE);
	}
	fsTrainingImages["images"] >> matTrainingImagesAsFlattenedFloats;
	fsTrainingImages.release();

	Ptr<ml::KNearest>  kNearest(ml::KNearest::create());
	kNearest->train(matTrainingImagesAsFlattenedFloats, cv::ml::ROW_SAMPLE, matClassificationInts);

	// process the rank
	String card;
	Mat grayImg;
	Mat blurredImg;
	Mat threshImg;
	Mat threshImgCopy;
	Mat rank = cardCharacteristics.first;
	//resize(rank, rank, cv::Size(rank.cols * 2, rank.rows * 2), INTER_NEAREST);
	cvtColor(rank, grayImg, COLOR_BGR2GRAY);
	cv::GaussianBlur(grayImg, blurredImg, Size(5, 5), 0);
	adaptiveThreshold(blurredImg, threshImg, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 11, 2);
	threshImgCopy = threshImg.clone();

	vector<vector<Point> > contours;
	vector<Vec4i> hierarchy;

	cv::findContours(threshImgCopy, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

	if (contours.size() > 1)
	{
		card = "10";
	}
	else
	{
		cv::rectangle(rank, boundingRect(contours[0]), cv::Scalar(0, 255, 0),2); 
		cv::Mat matROI = threshImg(boundingRect(contours[0]));
		cv::Mat matROIResized;
		cv::resize(matROI, matROIResized, cv::Size(RESIZED_IMAGE_WIDTH, RESIZED_IMAGE_HEIGHT));

		imshow("resized", matROIResized);
		Mat ROIFloat;
		matROIResized.convertTo(ROIFloat, CV_32FC1);
		Mat ROIFlattenedFloat = ROIFloat.reshape(1, 1);
		Mat CurrentChar(0, 0, CV_32F);

		kNearest->findNearest(ROIFlattenedFloat, 1, CurrentChar);
		float fltCurrentChar = (float)CurrentChar.at<float>(0, 0);
		card += char(int(fltCurrentChar));
	}
	std::cout << "\n\n" << "card rank = " << card << "\n\n";
	cv::waitKey(0);
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
	String filename = "kingofspades.png";	// load testimage
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
