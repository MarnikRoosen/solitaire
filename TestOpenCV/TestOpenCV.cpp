// Screen capture source: https://stackoverflow.com/questions/26888605/opencv-capturing-desktop-screen-live
// Mouse events source: https://opencv-srf.blogspot.be/2011/11/mouse-events.html

#include "stdafx.h"
#include "TestOpenCV.h"

//test gitlab commit

int main(int argc, char** argv)
{
	HWND hwndDesktop = GetDesktopWindow();
	namedWindow("output", WINDOW_NORMAL);
	int key = 0;

	while (key != 27)	//key = 27 -> error
	{
		Mat src = hwnd2mat(hwndDesktop);
		namedWindow("My Window", 1);	
		setMouseCallback("My Window", CallBackFunc, NULL);	//Function: void setMouseCallback(const string& winname, MouseCallback onMouse, void* userdata = 0)
		imshow("My Window", src);
		waitKey(1);
	}
	return 0;
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
