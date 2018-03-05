#include "stdafx.h"
#include "GameAnalytics.h"
#include "ClassifyCard.h"
#include "PlayingBoard.h"
#include <cstdio>
#include <windows.h>
#include <tlhelp32.h>

int main(int argc, char** argv)
{
	GameAnalytics ga;
}

GameAnalytics::GameAnalytics()
{
	PlayingBoard playingBoard;
	ClassifyCard classifyCard;
	bool init = true;
	int key = 0;
	HWND hwnd;
	namedWindow("Microsoft Solitaire Collection", 1);
	//setWindowProperty("Microsoft Solitaire Collection", WND_PROP_FULLSCREEN, WINDOW_FULLSCREEN);
	setMouseCallback("Microsoft Solitaire Collection", CallBackFunc, NULL);
	std::vector<std::pair<classifiers, classifiers>> classifiedCardsFromPlayingBoard;
	classifiedCardsFromPlayingBoard.reserve(12);
	while (key != 27)	//key = 27 -> error
	{
		hwnd = FindWindow(NULL, L"Microsoft Solitaire Collection - Firefox Developer Edition");
		if (hwnd == NULL)
		{
			std::cout << "Cant find window" << std::endl;
			exit(EXIT_FAILURE);
		}
		Mat src = hwnd2mat(hwnd);
		imshow("Microsoft Solitaire Collection", src);
		extractedCardsFromPlayingBoard = playingBoard.extractAndSortCards(src); // -> average 50ms

		classifiedCardsFromPlayingBoard.clear();
		for_each(extractedCardsFromPlayingBoard.begin(), extractedCardsFromPlayingBoard.end(), [&classifyCard, &classifiedCardsFromPlayingBoard](cv::Mat mat) {
			std::pair<classifiers, classifiers> cardType;
			if (mat.empty())
			{
				cardType.first = EMPTY;
				cardType.second = EMPTY;
			}
			else
			{
				std::pair<Mat, Mat> cardCharacteristics = classifyCard.segmentRankAndSuitFromCard(mat);
				cardType = classifyCard.classifyRankAndSuitOfCard(cardCharacteristics);
			}
			classifiedCardsFromPlayingBoard.push_back(cardType);
		});	// -> average 200ms

		if (init)
		{
			initializeVariables(classifiedCardsFromPlayingBoard);
			init = false;
		}
		else
		{
			updateBoard(classifiedCardsFromPlayingBoard);
		}
		key = waitKey(600);
	}
}

void GameAnalytics::initializeVariables(std::vector<std::pair<classifiers, classifiers>> classifiedCardsFromPlayingBoard)
{
	playingBoard.resize(12);
	for (int i = 0; i < 7; i++)
	{
		cardLocation startupLocation;
		if (classifiedCardsFromPlayingBoard.at(i).first != EMPTY || classifiedCardsFromPlayingBoard.at(i).second != EMPTY)
		{
			startupLocation.knownCards.push_back(classifiedCardsFromPlayingBoard.at(i));
		}
		startupLocation.unknownCards = i;
		playingBoard.at(i) = startupLocation;
	}
	cardLocation startupLocation;
	if (classifiedCardsFromPlayingBoard.at(7).first != EMPTY || classifiedCardsFromPlayingBoard.at(7).second != EMPTY)
	{
		startupLocation.knownCards.push_back(classifiedCardsFromPlayingBoard.at(7));
	}
	startupLocation.unknownCards = 24;
	playingBoard.at(7) = startupLocation;
	for (int i = 8; i < playingBoard.size(); i++)
	{
		cardLocation startupLocation;
		if (classifiedCardsFromPlayingBoard.at(i).first != EMPTY || classifiedCardsFromPlayingBoard.at(i).second != EMPTY)
		{
			startupLocation.knownCards.push_back(classifiedCardsFromPlayingBoard.at(i));
		}
		startupLocation.unknownCards = 0;
		playingBoard.at(i) = startupLocation;
	}
	printPlayingBoardState();
}

void GameAnalytics::updateBoard(std::vector<std::pair<classifiers, classifiers>> classifiedCardsFromPlayingBoard)
{
	int changedIndex1 = -1, changedIndex2 = -1;
	for (int i = 0; i < playingBoard.size(); i++)
	{
		if (playingBoard.at(i).knownCards.empty())	// case used for deckcards
		{
			if (classifiedCardsFromPlayingBoard.at(i).first != EMPTY || classifiedCardsFromPlayingBoard.at(i).second != EMPTY)
			{
				if (changedIndex1 == -1)
				{
					changedIndex1 = i;
				}
				else
				{
					changedIndex2 = i;
					break;
				}
			}
		}
		else
		{
			if (playingBoard.at(i).knownCards.back() != classifiedCardsFromPlayingBoard.at(i))
			{
				if (changedIndex1 == -1)
				{
					changedIndex1 = i;
				}
				else
				{
					changedIndex2 = i;
					break;
				}
			}
		}
	}

	if (changedIndex1 == 7 && changedIndex2 == -1)	// only possible for the deckcards
	{
		auto result = std::find(playingBoard.at(changedIndex1).knownCards.begin(),
			playingBoard.at(changedIndex1).knownCards.end(),
			classifiedCardsFromPlayingBoard.at(changedIndex1));
		if (result == playingBoard.at(changedIndex1).knownCards.end())
		{
			if (classifiedCardsFromPlayingBoard.at(changedIndex1).first != EMPTY)
			{
				playingBoard.at(changedIndex1).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex1));
				--playingBoard.at(changedIndex1).unknownCards;
			}
			printPlayingBoardState();
		}
		return;
	}

	if (changedIndex1 == 7 || changedIndex2 == 7)	// move operation from deck to different location
	{
		// erase from deckcards and add to other location
		if (changedIndex1 == 7)
		{
			auto result = std::find(playingBoard.at(changedIndex1).knownCards.begin(),
				playingBoard.at(changedIndex1).knownCards.end(),
				classifiedCardsFromPlayingBoard.at(changedIndex2));
			if (result != playingBoard.at(changedIndex1).knownCards.end())
			{
				playingBoard.at(changedIndex1).knownCards.erase(result);
			}
			if (classifiedCardsFromPlayingBoard.at(changedIndex2).first != EMPTY)
			{
				playingBoard.at(changedIndex2).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex2));
				--playingBoard.at(changedIndex1).unknownCards;
			}
			else
			{
				assert(playingBoard.at(changedIndex2).unknownCards == 0);
			}
			printPlayingBoardState();
			return;
		}
		else
		{
			auto result = std::find(playingBoard.at(changedIndex2).knownCards.begin(),
				playingBoard.at(changedIndex2).knownCards.end(),
				classifiedCardsFromPlayingBoard.at(changedIndex1));
			if (result != playingBoard.at(changedIndex2).knownCards.end())
			{
				playingBoard.at(changedIndex2).knownCards.erase(result);
			}
			--playingBoard.at(changedIndex2).unknownCards;
			if (classifiedCardsFromPlayingBoard.at(changedIndex1).first != EMPTY)
			{
				playingBoard.at(changedIndex1).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex1));
				--playingBoard.at(changedIndex2).unknownCards;
			}
			else
			{
				assert(playingBoard.at(changedIndex1).unknownCards == 0);
			}
			printPlayingBoardState();
			return;
		}
	}

	if (changedIndex1 != -1 && changedIndex2 != -1)	// case: 2 cardlocations changed indicating a cardmove
	{
		auto inList1 = std::find(playingBoard.at(changedIndex1).knownCards.begin(), playingBoard.at(changedIndex1).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex1));
		auto inList2 = std::find(playingBoard.at(changedIndex2).knownCards.begin(), playingBoard.at(changedIndex2).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex2));
		if (inList1 == playingBoard.at(changedIndex1).knownCards.end() && inList2 != playingBoard.at(changedIndex2).knownCards.end())
		{
			inList1++;
			assert(inList1 != playingBoard.at(changedIndex1).knownCards.end());	// else, the last card would be the topcard (so the cardlocation hasn't changed)
			playingBoard.at(changedIndex2).knownCards.insert(
				playingBoard.at(changedIndex2).knownCards.end(),
				inList1,
				playingBoard.at(changedIndex1).knownCards.end());
			playingBoard.at(changedIndex1).knownCards.erase(
				inList1,
				playingBoard.at(changedIndex1).knownCards.end());
			printPlayingBoardState();
			return;
		}
		if (inList1 != playingBoard.at(changedIndex1).knownCards.end() && inList2 == playingBoard.at(changedIndex2).knownCards.end())
		{
			inList2++;
			assert(inList2 != playingBoard.at(changedIndex2).knownCards.end());	// else, the last card would be the topcard (so the cardlocation hasn't changed)
			playingBoard.at(changedIndex1).knownCards.insert(
				playingBoard.at(changedIndex1).knownCards.end(),
				inList2,
				playingBoard.at(changedIndex2).knownCards.end());
			playingBoard.at(changedIndex2).knownCards.erase(
				inList2,
				playingBoard.at(changedIndex2).knownCards.end());
			printPlayingBoardState();
			return;
		}
		if (inList1 == playingBoard.at(changedIndex1).knownCards.end() && inList2 == playingBoard.at(changedIndex2).knownCards.end())
		{
			auto inList1 = std::find(playingBoard.at(changedIndex1).knownCards.begin(), playingBoard.at(changedIndex1).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex2));
			auto inList2 = std::find(playingBoard.at(changedIndex2).knownCards.begin(), playingBoard.at(changedIndex2).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex1));
			if (inList1 != playingBoard.at(changedIndex1).knownCards.end() && inList2 == playingBoard.at(changedIndex2).knownCards.end())
			{
				assert(!playingBoard.at(changedIndex1).knownCards.empty());
				playingBoard.at(changedIndex2).knownCards.insert(
					playingBoard.at(changedIndex2).knownCards.end(),
					playingBoard.at(changedIndex1).knownCards.begin(),
					playingBoard.at(changedIndex1).knownCards.end());
				playingBoard.at(changedIndex1).knownCards.clear();
				if (classifiedCardsFromPlayingBoard.at(changedIndex1).first != EMPTY)
				{
					playingBoard.at(changedIndex1).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex1));
					--playingBoard.at(changedIndex1).unknownCards;
				}
				else
				{
					assert(playingBoard.at(changedIndex1).unknownCards == 0);
				}
				printPlayingBoardState();
				return;
			}
			if (inList1 == playingBoard.at(changedIndex1).knownCards.end() && inList2 != playingBoard.at(changedIndex2).knownCards.end())
			{
				assert(!playingBoard.at(changedIndex2).knownCards.empty());
				playingBoard.at(changedIndex1).knownCards.insert(
					playingBoard.at(changedIndex1).knownCards.end(),
					playingBoard.at(changedIndex2).knownCards.begin(),
					playingBoard.at(changedIndex2).knownCards.end());
				playingBoard.at(changedIndex2).knownCards.clear();
				if (classifiedCardsFromPlayingBoard.at(changedIndex2).first != EMPTY)
				{
					playingBoard.at(changedIndex2).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex2));
					--playingBoard.at(changedIndex2).unknownCards;
				}
				else
				{
					assert(playingBoard.at(changedIndex2).unknownCards == 0);
				}
				printPlayingBoardState();
				return;
			}
		}
	}
}

void GameAnalytics::printPlayingBoardState()
{
	std::cout << "Deck: ";
	if (playingBoard.at(7).knownCards.empty())
	{
		std::cout << "// ";
	}
	for (int i = 0; i < playingBoard.at(7).knownCards.size(); i++)
	{
		std::cout << static_cast<char>(playingBoard.at(7).knownCards.at(i).first) << static_cast<char>(playingBoard.at(7).knownCards.at(i).second) << " ";
	}
	std::cout << "     Hidden cards = " << playingBoard.at(7).unknownCards << std::endl;

	std::cout << "Solved cards: " << std::endl;
	for (int i = 8; i < playingBoard.size(); i++)
	{
		std::cout << "   Position " << i - 8 << ": ";
		if (playingBoard.at(i).knownCards.empty())
		{
			std::cout << "// ";
		}
		for (int j = 0; j < playingBoard.at(i).knownCards.size(); j++)
		{
			std::cout << static_cast<char>(playingBoard.at(i).knownCards.at(j).first) << static_cast<char>(playingBoard.at(i).knownCards.at(j).second) << " ";
		}
		std::cout << "     Hidden cards = " << playingBoard.at(i).unknownCards << std::endl;
	}

	std::cout << "Bottom cards: " << std::endl;
	for (int i = 0; i < 7; i++)
	{
		std::cout << "   Position " << i << ": ";
		if (playingBoard.at(i).knownCards.empty())
		{
			std::cout << "// ";
		}
		for (int j = 0; j < playingBoard.at(i).knownCards.size(); j++)
		{
			std::cout << static_cast<char>(playingBoard.at(i).knownCards.at(j).first) << static_cast<char>(playingBoard.at(i).knownCards.at(j).second) << " ";
		}
		std::cout << "     Hidden cards = " << playingBoard.at(i).unknownCards << std::endl;
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
	//PrintWindow(hwnd, hwindowCompatibleDC, PW_CLIENTONLY);

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
