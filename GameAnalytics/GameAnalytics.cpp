#include "stdafx.h"
#include "GameAnalytics.h"

GameAnalytics ga;
int imagecounter;
DWORD WINAPI ThreadHookFunction(LPVOID lpParam);
CRITICAL_SECTION lock;

int main(int argc, char** argv)
{
	DWORD   dwThreadIdHook;
	HANDLE  hThreadHook;

	InitializeCriticalSection(&lock);

	imagecounter = 0;

	ga.Init();

	/*hThreadHook = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		ThreadHookFunction,       // thread function name
		0,          // argument to thread function 
		0,                      // use default creation flags 
		&dwThreadIdHook);   // returns the thread identifier 
	if (hThreadHook == NULL)
	{
		std::cout << "Error thread creation GameA" << std::endl;
		exit(EXIT_FAILURE);
	}*/

	ga.Process();

	//CloseHandle(hThreadHook);

	return 0;
}

DWORD WINAPI ThreadHookFunction(LPVOID lpParam) {
	
	ClicksHooks::Instance().InstallHook();
	ClicksHooks::Instance().Messages();
	return 0;
}

GameAnalytics::GameAnalytics()
{
}

void GameAnalytics::Init() {
	SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

	hwnd = FindWindow(NULL, L"Microsoft Solitaire Collection - Firefox Developer Edition");
	if (hwnd == NULL)
	{
		std::cout << "Cant find window" << std::endl;
		exit(EXIT_FAILURE);
	}
	HMONITOR appMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO appMonitorInfo;
	appMonitorInfo.cbSize = sizeof(appMonitorInfo);
	GetMonitorInfo(appMonitor, &appMonitorInfo);
	appRect = appMonitorInfo.rcMonitor;
	addCoordinatesToBuffer(0, 0);	// take first image and add this to buffer
}

void GameAnalytics::Process()
{
	PlayingBoard playingBoard;
	ClassifyCard classifyCard;
	Mat src;
	classifiedCardsFromPlayingBoard.reserve(12);
	double width = abs(appRect.right - appRect.left);
	double height = abs(appRect.bottom - appRect.top);
	POINT pt[2];

	startOfGame = Clock::now();
	startOfMove = Clock::now();

	while (!endOfGame)
	{
		if (!buffer.empty()) {
			//std::cout << "Take image from buffer - counter = " << imagecounter << std::endl;

			EnterCriticalSection(&lock);
			src = buffer.front(); buffer.pop();
			int& x = xpos.front(); xpos.pop();
			int& y = ypos.front(); ypos.pop();
			LeaveCriticalSection(&lock);
			
			// Mapping the coordinates from the primary window to the window in which the application is playing
			pt->x = x;
			pt->y = y;
			MapWindowPoints(GetDesktopWindow(), hwnd, &pt[0], 1);
			MapWindowPoints(GetDesktopWindow(), hwnd, &pt[1], 1);
			pt->x = pt->x * standardBoardWidth / width;
			pt->y = pt->y * standardBoardHeight / height;
			std::cout << "Click registered at position (" << pt->x << "," << pt->y << ")" << std::endl;

			if ((1487 <= pt->x  && pt->x <= 1586) && (837 <= pt->y && pt->y <= 889))
			{
				std::cout << "UNDO PRESSED!" << std::endl;
			}
			if ((12 <= pt->x  && pt->x <= 111) && (837 <= pt->y && pt->y <= 889))
			{
				std::cout << "NEW GAME PRESSED!" << std::endl;
			}
		}
		src = waitForStableImage();
		playingBoard.findCardsFromBoardImage(src); // -> average 38ms
		switch (playingBoard.getState())
		{
			case outOfMoves:
				handleOutOfMoves();
				endOfGame = true;
				break;
			case playing:
				handlePlayingState(playingBoard, classifyCard);
				break;
			default:
				handlePlayingState(playingBoard, classifyCard);
				break;
		}

		if (classifiedCardsFromPlayingBoard.at(8).first == classifiedCardsFromPlayingBoard.at(9).first
			== classifiedCardsFromPlayingBoard.at(10).first == classifiedCardsFromPlayingBoard.at(11).first == KING)
		{
			handleGameWon();
			endOfGame = true;
		}

	}	
}

void GameAnalytics::handleOutOfMoves()
{
	std::cout << "GAME STATE: Out of moves" << std::endl;
	std::chrono::time_point<std::chrono::steady_clock> endOfGame = Clock::now();
	std::cout << "Game duration: " << std::chrono::duration_cast<std::chrono::seconds>(endOfGame - startOfGame).count() << " s" << std::endl;
	Sleep(8000);
}

void GameAnalytics::handleGameWon()
{
	std::cout << "GAME STATE: Game won!" << std::endl;
	std::chrono::time_point<std::chrono::steady_clock> endOfGame = Clock::now();
	std::cout << "Game duration: " << std::chrono::duration_cast<std::chrono::seconds>(endOfGame - startOfGame).count() << " s" << std::endl;
	Sleep(8000);
}

void GameAnalytics::handlePlayingState(PlayingBoard &playingBoard, ClassifyCard &classifyCard)
{
	extractedImagesFromPlayingBoard = playingBoard.getCards();
	classifyExtractedCards(classifyCard);	// -> average d133ms and 550ms
	if (init)
	{
		initializePlayingBoard(classifiedCardsFromPlayingBoard);
		init = false;
	}
	else
	{
		updateBoard(classifiedCardsFromPlayingBoard);
	}

	// determine which card has been selected
	if (indexOfSelectedCard == -1 && playingBoard.getSelectedCard() != -1)
	{
		indexOfSelectedCard = playingBoard.getSelectedCard();
	}
	else if (indexOfSelectedCard != -1 && playingBoard.getSelectedCard() != -1 && indexOfSelectedCard != playingBoard.getSelectedCard())
	{
		char prevSuit = static_cast<char>(classifiedCardsFromPlayingBoard.at(indexOfSelectedCard).second);
		char newSuit = static_cast<char>(classifiedCardsFromPlayingBoard.at(playingBoard.getSelectedCard()).second);
		char prevRank = static_cast<char>(classifiedCardsFromPlayingBoard.at(indexOfSelectedCard).first);
		char newRank = static_cast<char>(classifiedCardsFromPlayingBoard.at(playingBoard.getSelectedCard()).first);
		if (((prevSuit == 'H' || prevSuit == 'D') && (newSuit == 'H' || newSuit == 'D'))
			|| ((prevSuit == 'S' || prevSuit == 'C') && (newSuit == 'S' || newSuit == 'C')))
		{
			std::cout << "Incompatible suit! " << prevSuit << " isn't compatible with " << newSuit << std::endl;
		}
		else
		{
			std::cout << "Incompatible rank! " << prevRank << " isn't compatible with " << newRank << std::endl;
		}
		indexOfSelectedCard = playingBoard.getSelectedCard();
	}
	else if (indexOfSelectedCard != -1 && playingBoard.getSelectedCard() == -1)
	{
		indexOfSelectedCard = -1;
	}
}

void GameAnalytics::addCoordinatesToBuffer(const int x, const int y) {
	Mat src = hwnd2mat(hwnd); // mbuffer;
	imagecounter++;
	EnterCriticalSection(&lock);
	buffer.push(src);
	xpos.push(x);
	ypos.push(y);
	LeaveCriticalSection(&lock);
}

void GameAnalytics::classifyExtractedCards(ClassifyCard & cc)
{
	classifiedCardsFromPlayingBoard.clear();
	for_each(extractedImagesFromPlayingBoard.begin(), extractedImagesFromPlayingBoard.end(), [&cc, this](cv::Mat mat) {
		if (mat.empty())
		{
			cardType.first = EMPTY;
			cardType.second = EMPTY;
		}
		else
		{
			cardCharacteristics = cc.segmentRankAndSuitFromCard(mat);
			cardType = cc.classifyCard(cardCharacteristics);
		}
		classifiedCardsFromPlayingBoard.push_back(cardType);
	});
}

cv::Mat GameAnalytics::waitForStableImage()	// -> average 112ms for non-updated screen
{
	Mat src2, graySrc1, graySrc2, diff;
	do {
		graySrc1 = hwnd2mat(hwnd);
		cvtColor(graySrc1, graySrc1, COLOR_BGR2GRAY);
		Sleep(20);
		src2 = hwnd2mat(hwnd);
		cvtColor(src2, graySrc2, COLOR_BGR2GRAY);
		cv::compare(graySrc1, graySrc2, diff, cv::CMP_NE);
	} while (cv::countNonZero(diff) > 0);
	return src2;
}

void GameAnalytics::initializePlayingBoard(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard)
{
	playingBoard.resize(12);
	int i;
	cardLocation startupLocation;

	// build stack
	for (i = 0; i < 7; i++)
	{
		startupLocation.knownCards.clear();
		if (classifiedCardsFromPlayingBoard.at(i).first != EMPTY)
		{
			startupLocation.knownCards.push_back(classifiedCardsFromPlayingBoard.at(i));
		}
		startupLocation.unknownCards = i;
		playingBoard.at(i) = startupLocation;
	}

	// talon
	startupLocation.knownCards.clear();
	if (classifiedCardsFromPlayingBoard.at(7).first != EMPTY)
	{
		startupLocation.knownCards.push_back(classifiedCardsFromPlayingBoard.at(7));
	}
	startupLocation.unknownCards = 24;
	playingBoard.at(7) = startupLocation;

	// suit stack
	for (i = 8; i < playingBoard.size(); i++)
	{
		startupLocation.knownCards.clear();
		if (classifiedCardsFromPlayingBoard.at(i).first != EMPTY)
		{
			startupLocation.knownCards.push_back(classifiedCardsFromPlayingBoard.at(i));
		}
		startupLocation.unknownCards = 0;
		playingBoard.at(i) = startupLocation;
	}

	printPlayingBoardState();
}

void GameAnalytics::updateBoard(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard)
{	
	int changedIndex1 = -1, changedIndex2 = -1;
	findChangedCardLocations(classifiedCardsFromPlayingBoard, changedIndex1, changedIndex2);

	// pile pressed (only talon changed)
	if (changedIndex1 == 7 && changedIndex2 == -1)
	{
		updateDeck(changedIndex1, classifiedCardsFromPlayingBoard);
		return;
	}


	// card move from talon to board
	if (changedIndex1 == 7 || changedIndex2 == 7)
	{
		int tempIndex;
		(changedIndex1 == 7) ? (tempIndex = changedIndex2) : (tempIndex = changedIndex1);
		auto result = std::find(
			playingBoard.at(7).knownCards.begin(),
			playingBoard.at(7).knownCards.end(),
			classifiedCardsFromPlayingBoard.at(tempIndex));
		if (result != playingBoard.at(7).knownCards.end())
		{
			playingBoard.at(7).knownCards.erase(result);
			playingBoard.at(tempIndex).knownCards.push_back(classifiedCardsFromPlayingBoard.at(tempIndex));
			printPlayingBoardState();
			return;
		}
	}

	// card move between build stack and suit stack
	if (changedIndex1 != -1 && changedIndex2 != -1)
	{
		if (cardMoveBetweenBuildAndSuitStack(classifiedCardsFromPlayingBoard, changedIndex1, changedIndex2))
		{
			printPlayingBoardState();
			return;
		}
	}

	// error with previously detected card
	/*if (changedIndex1 != -1 && changedIndex1 != 7 && changedIndex2 == -1)
	{
		std::cout << "Error with previous board state! Card at index " << changedIndex1 << " should have been: " <<
			static_cast<char>(classifiedCardsFromPlayingBoard.at(changedIndex1).first) << static_cast<char>(classifiedCardsFromPlayingBoard.at(changedIndex1).second) << std::endl;
		if (!playingBoard.at(changedIndex1).knownCards.empty())
		{
			playingBoard.at(changedIndex1).knownCards.pop_back();
		}
		else if (classifiedCardsFromPlayingBoard.at(changedIndex1).first == EMPTY)
		{
			return;
		}
		else
		{
			--playingBoard.at(changedIndex1).unknownCards;
		}
		playingBoard.at(changedIndex1).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex1));
		printPlayingBoardState();
	}*/
}

bool GameAnalytics::cardMoveBetweenBuildAndSuitStack(const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard, int changedIndex1, int changedIndex2)
{
	auto inList1 = std::find(playingBoard.at(changedIndex1).knownCards.begin(), playingBoard.at(changedIndex1).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex1));
	auto inList2 = std::find(playingBoard.at(changedIndex2).knownCards.begin(), playingBoard.at(changedIndex2).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex2));
	if (inList1 != playingBoard.at(changedIndex1).knownCards.end() && inList2 == playingBoard.at(changedIndex2).knownCards.end())
	{
		inList1++;
		playingBoard.at(changedIndex2).knownCards.insert(
			playingBoard.at(changedIndex2).knownCards.end(),
			inList1,
			playingBoard.at(changedIndex1).knownCards.end());
		playingBoard.at(changedIndex1).knownCards.erase(
			inList1,
			playingBoard.at(changedIndex1).knownCards.end());
		return true;
	}
	if (inList1 == playingBoard.at(changedIndex1).knownCards.end() && inList2 != playingBoard.at(changedIndex2).knownCards.end())
	{
		inList2++;
		playingBoard.at(changedIndex1).knownCards.insert(
			playingBoard.at(changedIndex1).knownCards.end(),
			inList2,
			playingBoard.at(changedIndex2).knownCards.end());
		playingBoard.at(changedIndex2).knownCards.erase(
			inList2,
			playingBoard.at(changedIndex2).knownCards.end());
		return true;
	}
	if (inList1 == playingBoard.at(changedIndex1).knownCards.end() && inList2 == playingBoard.at(changedIndex2).knownCards.end())
	{
		auto inList1 = std::find(playingBoard.at(changedIndex1).knownCards.begin(), playingBoard.at(changedIndex1).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex2));
		auto inList2 = std::find(playingBoard.at(changedIndex2).knownCards.begin(), playingBoard.at(changedIndex2).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex1));
		if (inList1 != playingBoard.at(changedIndex1).knownCards.end() && inList2 == playingBoard.at(changedIndex2).knownCards.end())
		{
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
			return true;
		}
		if (inList1 == playingBoard.at(changedIndex1).knownCards.end() && inList2 != playingBoard.at(changedIndex2).knownCards.end())
		{
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
			return true;
		}
	}
	return false;
}

void GameAnalytics::findChangedCardLocations(const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard, int & changedIndex1, int & changedIndex2)
{
	for (int i = 0; i < playingBoard.size(); i++)
	{
		if (playingBoard.at(i).knownCards.empty())	// adding card to an empty location
		{
			if (classifiedCardsFromPlayingBoard.at(i).first != EMPTY)
			{
				changedIndex1 == -1 ? (changedIndex1 = i) : (changedIndex2 = i);
			}
		}
		else if (i == 7)
		{
			if (playingBoard.at(7).knownCards.back() != classifiedCardsFromPlayingBoard.at(7) && classifiedCardsFromPlayingBoard.at(7).first != EMPTY)
			{
				changedIndex1 == -1 ? (changedIndex1 = 7) : (changedIndex2 = 7);
			}
		}
		else
		{
			if (playingBoard.at(i).knownCards.back() != classifiedCardsFromPlayingBoard.at(i))
			{
				changedIndex1 == -1 ? (changedIndex1 = i) : (changedIndex2 = i);
			}
		}
		if (changedIndex2 != -1) { return; }
	}
}

void GameAnalytics::updateDeck(int changedIndex1, const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard)
{
	auto result = std::find(
		playingBoard.at(changedIndex1).knownCards.begin(),
		playingBoard.at(changedIndex1).knownCards.end(),
		classifiedCardsFromPlayingBoard.at(changedIndex1));
	if (result == playingBoard.at(changedIndex1).knownCards.end() && classifiedCardsFromPlayingBoard.at(changedIndex1).first != EMPTY)
	{
		playingBoard.at(changedIndex1).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex1));
		--playingBoard.at(changedIndex1).unknownCards;
		printPlayingBoardState();
	}
	else
	{
		playingBoard.at(changedIndex1).knownCards.erase(result);
		playingBoard.at(changedIndex1).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex1));
		printPlayingBoardState();
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
		std::cout << "   Pos " << i - 8 << ": ";
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
		std::cout << "   Pos " << i << ": ";
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

	auto averageThinkTime2 = Clock::now();
	averageThinkDurations.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(averageThinkTime2 - startOfMove).count());
	startOfMove = Clock::now();
	std::cout << "Last time = " << averageThinkDurations.back() << "ms" << std::endl;
	std::cout << "Avg time = " << std::accumulate(averageThinkDurations.begin(), averageThinkDurations.end(), 0) / averageThinkDurations.size() << "ms" << std::endl;
	std::cout << "Moves = " << averageThinkDurations.size() << " moves" << std::endl;
	std::cout << std::endl;
}

cv::Mat GameAnalytics::hwnd2mat(const HWND & hwnd)	//Mat = n-dimensional dense array class, HWND = handle for desktop window
{
	SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
	HDC hwindowDC, hwindowCompatibleDC;

	int height, width, srcheight, srcwidth;
	HBITMAP hbwindow;
	Mat src;
	BITMAPINFOHEADER  bi;

	hwindowDC = GetDC(hwnd);	//get device context
	hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);	//creates a compatible memory device context for the device
	SetStretchBltMode(hwindowCompatibleDC, COLORONCOLOR);	//set bitmap stretching mode, color on color deletes all eliminated lines of pixels
	RECT windowsize;    // get the height and width of the screen
	GetClientRect(hwnd, &windowsize);	//get coordinates of clients window
	srcheight = windowsize.bottom;
	srcwidth = windowsize.right;
	//srcheight = 768;
	//srcwidth = 1366;
	height = srcheight * 1;  //possibility to resize the client window screen
	width = srcwidth * 1;
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
	//imshow("test", src);

	return src;
}