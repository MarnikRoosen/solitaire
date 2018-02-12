// Screen capture source: https://stackoverflow.com/questions/26888605/opencv-capturing-desktop-screen-live
// Mouse events source: https://opencv-srf.blogspot.be/2011/11/mouse-events.html

#include "stdafx.h"
#include "GameAnalysis.h"

const string basePath = "C:/Users/marni/source/repos/gameAnalysis/x64/Debug/";

int main(int argc, char** argv)
{
	Mat card = detectCard();
	std::pair<Mat, Mat> cardCharacteristics = getCardCharacteristics(card);
	classifyCard(cardCharacteristics);
	//getApplicationView();
	return 0;
}

void classifyCard(std::pair<Mat, Mat> cardCharacteristics)
{

}

std::pair<Mat, Mat> getCardCharacteristics(Mat aCard)
{
	Rect classArea;
	classArea.x = aCard.cols / 100;
	classArea.y = aCard.rows / 100;
	classArea.width = aCard.cols / 4.2;
	classArea.height = aCard.rows / 3.8;
	Mat card = Mat(aCard, classArea).clone();

	Mat grayImg, blurredImg, threshImg;
	vector<vector<Point>> contours;
	vector<Vec4i> hierarchy;

	// convert the image to gray
	cvtColor(card, grayImg, COLOR_BGR2GRAY);

	// apply gaussian blur to improve card detection
	GaussianBlur(grayImg, blurredImg, Size(1, 1), 1000);

	// threshold the image to keep only brighter regions (cards are white)
	threshold(blurredImg, threshImg, 120, 255, THRESH_BINARY);

	// find all the contours using the thresholded image
	findContours(threshImg, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE, Point(0, 0));

	Mat rank, type;
	std::pair<Mat, Mat> cardCharacteristics;
	int type_index = 0;
	for (int i = 0; i < contours.size(); i++) // iterate through each contour. 
	{
		double a = contourArea(contours[i], false);  //  Find the area of contour
		if (type_index == 1 && a < card.cols * card.rows * 0.9) {	// Don't take the outerborder (margin of 10%)
			Rect bounding_rect = boundingRect(contours[i]);
			type = Mat(card, bounding_rect).clone();
			imshow("type", type);
		}
		if (type_index == 0 && a < card.cols * card.rows * 0.9) {	// Don't take the outerborder (margin of 10%)
			Rect bounding_rect = boundingRect(contours[i]);
			rank = Mat(card, bounding_rect).clone();
			imshow("rank", rank);
			type_index = 1;
		}
	}	
	cardCharacteristics = std::make_pair(type, rank);
	waitKey(0);
	return cardCharacteristics;
}

Mat detectCard()
{
	// load testimage
	String filename = "3ofspades.png";
	Mat src = imread(basePath + filename);
	
	// check for invalid input
	if (!src.data)
	{
		cout << "Could not open or find the image" << std::endl;
		exit(EXIT_FAILURE);
	}

	imshow("original image", src);

	Mat grayImg, blurredImg, threshImg;
	vector<vector<Point>> contours;
	vector<Vec4i> hierarchy;

	// convert the image to gray
	cvtColor(src, grayImg, COLOR_BGR2GRAY);

	// apply gaussian blur to improve card detection
	GaussianBlur(grayImg, blurredImg, Size(1, 1), 1000);

	// threshold the image to keep only brighter regions (cards are white)
	threshold(blurredImg, threshImg, 120, 255, THRESH_BINARY);

	// find all the contours using the thresholded image
	findContours(threshImg, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE, Point(0, 0));

	// find the largest contour
	int largest_area = 0;
	int largest_contour_index = 0;
	Rect bounding_rect;

	for (int i = 0; i < contours.size(); i++) // iterate through each contour. 
	{
		double a = contourArea(contours[i], false);  //  Find the area of contour
		if (a > largest_area) {
			largest_area = a;
			largest_contour_index = i;                //Store the index of largest contour
			bounding_rect = boundingRect(contours[i]); // Find the bounding rectangle for biggest contour
		}
	}

	// display the largest contour
	Mat card = Mat(src, bounding_rect).clone();
	imshow("card", card);
	return card;
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
