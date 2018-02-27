#include "stdafx.h"
#include "GameAnalytics.h"
#include "ClassifyCard.h"
#include "PlayingBoard.h"

int main(int argc, char** argv)
{
	GameAnalytics ga;
}


GameAnalytics::GameAnalytics()
{
	//getApplicationView();
	PlayingBoard pb;
	ClassifyCard cc;
	initializeVariables();
	String filename = "playingBoard.png";	// load testimage
	Mat src = imread("../GameAnalytics/testImages/" + filename);
	if (!src.data)	// check for invalid input
	{
		cout << "Could not open or find the image" << std::endl;
		exit(EXIT_FAILURE);
	}

	pb.extractAndSortCards(src);
	topCards = pb.getPlayingCards();
	updateBoard(cc);

	waitKey(0);
}

void GameAnalytics::initializeVariables()
{
	playingBoard.resize(12);
	for (int i = 0; i < 7; i++)
	{
		cardLocation startupLocation;
		std::pair<classifiers, classifiers> startupCard;
		startupCard.first = UNKNOWN;
		startupCard.second = UNKNOWN;
		startupLocation.topCard = startupCard;
		startupLocation.unknownCards = i;
		playingBoard[i] = startupLocation;
	}
	playingBoard[7].unknownCards = 23;	
	playingBoard[7].knownCards = 0;
}

void GameAnalytics::updateBoard(ClassifyCard &cc)
{
	std::vector<std::pair<classifiers, classifiers>> tempCards;
	for (int i = 0; i < topCards.size(); i++)
	{
		if (topCards[i].empty())
		{
			cardType.first = EMPTY;
			cardType.second = EMPTY;
		}
		else
		{
			Mat card = topCards.at(i);
			std::pair<Mat, Mat> cardCharacteristics = cc.segmentRankAndSuitFromCard(card);
			cardType = cc.classifyRankAndSuitOfCard(cardCharacteristics);
		}

		if (playingBoard[i].topCard != cardType)
		{
			tempCards.push_back(playingBoard[i].topCard);
			playingBoard[i].topCard = cardType;
		}
		else
		{
			playingBoard[i].topCard = cardType;
		}
	}
}


void getApplicationView()
{
	HWND hwndDesktop = FindWindow(0, _T("Microsoft Solitaire Collection"));
	if (hwndDesktop == NULL)
	{
		std::cout << "return" << endl;
		exit(EXIT_FAILURE);
	}

	int key = 0;

	namedWindow("My Window", 1);
	setMouseCallback("My Window", CallBackFunc, NULL);	//Function: void setMouseCallback(const string& winname, MouseCallback onMouse, void* userdata = 0)

	while (key != 27)	//key = 27 -> error
	{
		Mat src = hwnd2mat(hwndDesktop);
		imshow("My Window", src);
		key = waitKey(0);
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
	PrintWindow(hwnd, hwindowCompatibleDC, PW_CLIENTONLY);

	// copy from the window device context to the bitmap device context
	//StretchBlt(hwindowCompatibleDC, 0, 0, width, height, hwindowDC, 0, 0, srcwidth, srcheight, SRCCOPY); //change SRCCOPY to NOTSRCCOPY for wacky colors !
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
