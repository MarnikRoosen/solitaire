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
	while (key != 27)	//key = 27 -> error
	{
		HWND hwnd = FindWindow(NULL, L"Microsoft Solitaire Collection - Firefox Developer Edition");
		if (hwnd == NULL)
		{
			std::cout << "Cant find window" << std::endl;
			exit(EXIT_FAILURE);
		}
		namedWindow("Microsoft Solitaire Collection", 1);
		setWindowProperty("Microsoft Solitaire Collection", WND_PROP_FULLSCREEN, WINDOW_FULLSCREEN);
		setMouseCallback("Microsoft Solitaire Collection", CallBackFunc, NULL);
		Mat src = hwnd2mat(hwnd);
		imshow("Microsoft Solitaire Collection", src);
		playingBoard.extractAndSortCards(src);
		extractedCardsFromPlayingBoard = playingBoard.getPlayingCards();
		std::vector<std::pair<classifiers, classifiers>> classifiedCardsFromPlayingBoard;
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
		});
		if (init)
		{
			initializeVariables(classifiedCardsFromPlayingBoard);
			init = false;
		}
		else
		{
			updateBoard(classifiedCardsFromPlayingBoard);
		}
		key = waitKey(1000);
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
		if (playingBoard.at(i).knownCards.empty())
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
		else if (playingBoard.at(i).knownCards.back() != classifiedCardsFromPlayingBoard.at(i))
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

	if (changedIndex1 == 7 && changedIndex2 == -1)	// only possible for the deckcards
	{
		auto result = std::find(playingBoard.at(changedIndex1).knownCards.begin(),
			playingBoard.at(changedIndex1).knownCards.end(),
			classifiedCardsFromPlayingBoard.at(changedIndex1));
		if (result == playingBoard.at(changedIndex1).knownCards.end() && classifiedCardsFromPlayingBoard.at(changedIndex1).first != EMPTY)
		{
			playingBoard.at(changedIndex1).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex1));
			--playingBoard.at(changedIndex1).unknownCards;
			printPlayingBoardState();
		}
		return;
	}

	if (changedIndex1 == 7 || changedIndex2 == 7)	// move operation from deck to different location
	{
		// erase from deckcards and add to other location
		if (changedIndex1 == 7)
		{
			int topCardIndex1 = std::distance(playingBoard.at(changedIndex1).knownCards.begin(),
				std::find(playingBoard.at(changedIndex1).knownCards.begin(),
					playingBoard.at(changedIndex1).knownCards.end(),
					classifiedCardsFromPlayingBoard.at(changedIndex2)));
			playingBoard.at(changedIndex1).knownCards.erase(playingBoard.at(changedIndex1).knownCards.begin() + topCardIndex1);
			--playingBoard.at(changedIndex1).unknownCards;
			playingBoard.at(changedIndex2).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex2));
			printPlayingBoardState();
			return;
		}
		else
		{
			int topCardIndex2 = std::distance(playingBoard.at(changedIndex2).knownCards.begin(),
				std::find(playingBoard.at(changedIndex2).knownCards.begin(),
					playingBoard.at(changedIndex2).knownCards.end(),
					classifiedCardsFromPlayingBoard.at(changedIndex1)));
			playingBoard.at(changedIndex2).knownCards.erase(playingBoard.at(changedIndex2).knownCards.begin() + topCardIndex2);
			--playingBoard.at(changedIndex2).unknownCards;
			playingBoard.at(changedIndex1).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex1));
			printPlayingBoardState();
			return;
		}
	}

	if (changedIndex1 != -1 && changedIndex2 != -1)	// case: 2 cardlocations changed indicating a cardmove
	{
		// current topcard was in the list of previously known cards of changedIndex1
		//	=> all cards that were below the current topcard are moved to changedIndex2
		int topCardIndex1 = std::distance(playingBoard.at(changedIndex1).knownCards.begin(), 
			std::find(playingBoard.at(changedIndex1).knownCards.begin(),
			playingBoard.at(changedIndex1).knownCards.end(),
			classifiedCardsFromPlayingBoard.at(changedIndex1)));
		if (topCardIndex1 < playingBoard.at(changedIndex1).knownCards.size())
		{
			playingBoard.at(changedIndex2).knownCards.insert(
				playingBoard.at(changedIndex2).knownCards.begin(),
				playingBoard.at(changedIndex1).knownCards.begin() + topCardIndex1,
				playingBoard.at(changedIndex1).knownCards.end());
			playingBoard.at(changedIndex1).knownCards.erase(
				playingBoard.at(changedIndex1).knownCards.begin() + topCardIndex1,
				playingBoard.at(changedIndex1).knownCards.end());
			printPlayingBoardState();
			return;
		}

		// current topcard was in the list of previously known cards of changedIndex2
		//	=> all cards that were below the current topcard are moved to changedIndex1
		int topCardIndex2 = std::distance(playingBoard.at(changedIndex2).knownCards.begin(),
			std::find(playingBoard.at(changedIndex2).knownCards.begin(),
				playingBoard.at(changedIndex2).knownCards.end(),
				classifiedCardsFromPlayingBoard.at(changedIndex2)));
		if (topCardIndex1 < playingBoard.at(changedIndex2).knownCards.size())
		{
			playingBoard.at(changedIndex1).knownCards.insert(
				playingBoard.at(changedIndex1).knownCards.begin(),
				playingBoard.at(changedIndex2).knownCards.begin() + changedIndex2,
				playingBoard.at(changedIndex2).knownCards.end());
			playingBoard.at(changedIndex2).knownCards.erase(
				playingBoard.at(changedIndex2).knownCards.begin() + changedIndex2,
				playingBoard.at(changedIndex2).knownCards.end());
			printPlayingBoardState();
			return;
		}

		// current topcard isn't in the list of the previous known cards at that location
		//	=> 1. it is a new card: check if the card ISN'T in BOTH lists
		//	=> 2. it isn't a new card: check if the card IS in BOTH lists
		std::vector<std::pair<classifiers, classifiers>> tempList;
		tempList.reserve(playingBoard.at(changedIndex1).knownCards.size() + playingBoard.at(changedIndex2).knownCards.size());
		tempList.insert(tempList.end(), playingBoard.at(changedIndex1).knownCards.begin(), playingBoard.at(changedIndex1).knownCards.end());
		tempList.insert(tempList.end(), playingBoard.at(changedIndex2).knownCards.begin(), playingBoard.at(changedIndex2).knownCards.end());
		auto result = std::find(tempList.begin(), tempList.end(), classifiedCardsFromPlayingBoard.at(changedIndex1));
		if (result != tempList.end())	// changedIndex2 contains the new card
		{
			playingBoard.at(changedIndex1).knownCards.insert(
				playingBoard.at(changedIndex1).knownCards.end(),
				playingBoard.at(changedIndex2).knownCards.begin(),
				playingBoard.at(changedIndex2).knownCards.end());
			playingBoard.at(changedIndex2).knownCards.clear();
			playingBoard.at(changedIndex2).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex2));
			--playingBoard.at(changedIndex2).unknownCards;
			printPlayingBoardState();
			return;
		}
		else
		{
			playingBoard.at(changedIndex2).knownCards.insert(
				playingBoard.at(changedIndex2).knownCards.end(),
				playingBoard.at(changedIndex1).knownCards.begin(),
				playingBoard.at(changedIndex1).knownCards.end());
			playingBoard.at(changedIndex1).knownCards.clear();
			playingBoard.at(changedIndex1).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex1));
			--playingBoard.at(changedIndex1).unknownCards;
			printPlayingBoardState();
			return;
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
