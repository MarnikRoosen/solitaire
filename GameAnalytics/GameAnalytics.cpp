#include "stdafx.h"
#include "GameAnalytics.h"

GameAnalytics ga;
int imagecounter;
DWORD WINAPI ThreadHookFunction(LPVOID lpParam);
CRITICAL_SECTION lock;
bool debug = false;

int main(int argc, char** argv)
{
	DWORD   dwThreadIdHook;
	HANDLE  hThreadHook;

	InitializeCriticalSection(&lock);

	imagecounter = 0;

	ga.Init();

	hThreadHook = CreateThread(
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
		}

	ga.Process();

	CloseHandle(hThreadHook);
	

	return 0;
}

DWORD WINAPI ThreadHookFunction(LPVOID lpParam) {
	
	ClicksHooks::Instance().InstallHook();
	ClicksHooks::Instance().Messages();
	return 0;
}

GameAnalytics::GameAnalytics() : cc(), pb()
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
	//addCoordinatesToBuffer(0, 0);	// take first image and add this to srcBuffer
	windowWidth = abs(appRect.right - appRect.left);
	windowHeight = abs(appRect.bottom - appRect.top);

	startOfGame = Clock::now();
	startOfMove = Clock::now();

	classifiedCardsFromPlayingBoard.reserve(12);
	src = waitForStableImage();
	pb.findCardsFromBoardImage(src); // -> average 38ms
	extractedImagesFromPlayingBoard = pb.getCards();
	classifyExtractedCards();	// -> average d133ms and 550ms
	initializePlayingBoard(classifiedCardsFromPlayingBoard);
}

void GameAnalytics::Process()
{
	bool boardChanged;
	while (!endOfGameBool)
	{
		if (!xPosBuffer.empty() || debug)
		{
			if (!debug)
			{
				//std::cout << "Take image from srcBuffer - counter = " << imagecounter << std::endl;
				EnterCriticalSection(&lock);
				//src = srcBuffer.front(); srcBuffer.pop();
				int& x = xPosBuffer.front(); xPosBuffer.pop();
				int& y = yPosBuffer.front(); yPosBuffer.pop();
				LeaveCriticalSection(&lock);

				// Mapping the coordinates from the primary window to the window in which the application is playing
				pt->x = x;
				pt->y = y;
				MapWindowPoints(GetDesktopWindow(), hwnd, &pt[0], 1);
				MapWindowPoints(GetDesktopWindow(), hwnd, &pt[1], 1);
				pt->x = pt->x * standardBoardWidth / windowWidth;
				pt->y = pt->y * standardBoardHeight / windowHeight;
				std::cout << "Click registered at position (" << pt->x << "," << pt->y << ")" << std::endl;

				if ((1487 <= pt->x  && pt->x <= 1586) && (837 <= pt->y && pt->y <= 889))
				{
					std::cout << "UNDO PRESSED! Updated playingboard:" << std::endl;
					previousPlayingBoards.pop_back();
					currentPlayingBoard = previousPlayingBoards.back();
					printPlayingBoardState();
					++numberOfUndos;
					continue;
				}
				if ((12 <= pt->x  && pt->x <= 111) && (837 <= pt->y && pt->y <= 889))
				{
					std::cout << "NEW GAME PRESSED!" << std::endl;
				}
				if ((13 <= pt->x  && pt->x <= 82) && (1 <= pt->y && pt->y <= 55))
				{
					std::cout << "MENU PRESSED!" << std::endl;
				}
				if ((283 <= pt->x  && pt->x <= 416) && (84 <= pt->y && pt->y <= 258))
				{
					++numberOfTalonPresses;
				}
				if ((91 <= pt->x  && pt->x <= 161) && (1 <= pt->y && pt->y <= 55))
				{
					std::cout << "BACK PRESSED!" << std::endl;
				}
			}
			
			boardChanged = false;
			do
			{
				src = waitForStableImage();
				pb.findCardsFromBoardImage(src); // -> average 38ms
				switch (pb.getState())
				{
				case outOfMoves:
					std::cout << "--------------------------------------------------------" << std::endl;
					std::cout << "Out of moves" << std::endl;
					std::cout << "--------------------------------------------------------" << std::endl;
					handleEndOfGame();
					endOfGameBool = true;
					break;
				case playing:
					boardChanged = handlePlayingState();
					break;
				default:
					boardChanged = handlePlayingState();
					break;
				}

				if (classifiedCardsFromPlayingBoard.at(8).first == classifiedCardsFromPlayingBoard.at(9).first
					== classifiedCardsFromPlayingBoard.at(10).first == classifiedCardsFromPlayingBoard.at(11).first == KING)
				{
					std::cout << "--------------------------------------------------------" << std::endl;
					std::cout << "Game won!" << std::endl;
					std::cout << "--------------------------------------------------------" << std::endl;
					endOfGameBool = true;
					handleEndOfGame();
				}
			} while (false);
		}
		else
		{
			Sleep(10);
		}
	}
}

void GameAnalytics::handleEndOfGame()
{
	std::chrono::time_point<std::chrono::steady_clock> endOfGame = Clock::now();
	std::cout << "Game solved: " << std::boolalpha << endOfGameBool << std::endl;
	std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::seconds>(endOfGame - startOfGame).count() << " s" << std::endl;
	std::cout << "Average time per move = " << std::accumulate(averageThinkDurations.begin(), averageThinkDurations.end(), 0) / averageThinkDurations.size() << "ms" << std::endl;
	std::cout << "Number of moves = " << averageThinkDurations.size() << " moves" << std::endl;
	std::cout << "Times undo = " << numberOfUndos << std::endl;
	std::cout << "Times talon pressed = " << numberOfTalonPresses << std::endl;
	Sleep(15000);
}

bool GameAnalytics::handlePlayingState()
{
	extractedImagesFromPlayingBoard = pb.getCards();
	classifyExtractedCards();	// -> average d133ms and 550ms
	if (updateBoard(classifiedCardsFromPlayingBoard))
	{
		previousPlayingBoards.push_back(currentPlayingBoard);
		return true;
	}
	else
	{
		return false;
	}
}

void GameAnalytics::addCoordinatesToBuffer(const int x, const int y) {
	//Mat src = hwnd2mat(hwnd); // mbuffer;
	imagecounter++;
	EnterCriticalSection(&lock);
	//srcBuffer.push(src);
	xPosBuffer.push(x);
	yPosBuffer.push(y);
	LeaveCriticalSection(&lock);
}

void GameAnalytics::classifyExtractedCards()
{
	classifiedCardsFromPlayingBoard.clear();
	for_each(extractedImagesFromPlayingBoard.begin(), extractedImagesFromPlayingBoard.end(), [this](cv::Mat mat) {
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
		Sleep(50);
		src2 = hwnd2mat(hwnd);
		cvtColor(src2, graySrc2, COLOR_BGR2GRAY);
		cv::compare(graySrc1, graySrc2, diff, cv::CMP_NE);
	} while (cv::countNonZero(diff) > 0);
	return src2;
}

void GameAnalytics::initializePlayingBoard(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard)
{
	currentPlayingBoard.resize(12);
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
		currentPlayingBoard.at(i) = startupLocation;
	}

	// talon
	startupLocation.knownCards.clear();
	if (classifiedCardsFromPlayingBoard.at(7).first != EMPTY)
	{
		startupLocation.knownCards.push_back(classifiedCardsFromPlayingBoard.at(7));
	}
	startupLocation.unknownCards = 24;
	currentPlayingBoard.at(7) = startupLocation;

	// suit stack
	for (i = 8; i < currentPlayingBoard.size(); i++)
	{
		startupLocation.knownCards.clear();
		if (classifiedCardsFromPlayingBoard.at(i).first != EMPTY)
		{
			startupLocation.knownCards.push_back(classifiedCardsFromPlayingBoard.at(i));
		}
		startupLocation.unknownCards = 0;
		currentPlayingBoard.at(i) = startupLocation;
	}

	printPlayingBoardState();
}

bool GameAnalytics::updateBoard(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard)
{	
	int changedIndex1 = -1, changedIndex2 = -1;
	findChangedCardLocations(classifiedCardsFromPlayingBoard, changedIndex1, changedIndex2);
	if (changedIndex1 == -1 && changedIndex2 == -1)
	{
		return false;
	}
	// pile pressed (only talon changed)
	if (changedIndex1 == 7 && changedIndex2 == -1)
	{
		if (classifiedCardsFromPlayingBoard.at(7).first != EMPTY)
		{
			updateDeck(classifiedCardsFromPlayingBoard);
			printPlayingBoardState();
		}
		return true;
	}


	// card move from talon to board
	if (changedIndex1 == 7 || changedIndex2 == 7)
	{
		int tempIndex;	// contains the other index
		(changedIndex1 == 7) ? (tempIndex = changedIndex2) : (tempIndex = changedIndex1);
		auto result = std::find(
			currentPlayingBoard.at(7).knownCards.begin(),
			currentPlayingBoard.at(7).knownCards.end(),
			classifiedCardsFromPlayingBoard.at(tempIndex));
		if (result != currentPlayingBoard.at(7).knownCards.end())
		{
			currentPlayingBoard.at(7).knownCards.erase(result);
			currentPlayingBoard.at(tempIndex).knownCards.push_back(classifiedCardsFromPlayingBoard.at(tempIndex));
			if (classifiedCardsFromPlayingBoard.at(7).first != EMPTY)
			{
				updateDeck(classifiedCardsFromPlayingBoard);
			}
			printPlayingBoardState();
			return true;
		}
		return false;

	}

	// card move between build stack and suit stack
	if (changedIndex1 != -1 && changedIndex2 != -1)
	{
		if (cardMoveBetweenBuildAndSuitStack(classifiedCardsFromPlayingBoard, changedIndex1, changedIndex2))	// true if changed board
		{
			printPlayingBoardState();
			return true;
		}
		return false;
	}

	// error with previously detected card
	if (changedIndex1 != -1 && changedIndex1 != 7 && changedIndex2 == -1)
	{
		if (currentPlayingBoard.at(changedIndex1).knownCards.empty())
		{
			--currentPlayingBoard.at(changedIndex1).unknownCards;
		}
		currentPlayingBoard.at(changedIndex1).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex1));
		printPlayingBoardState();
		return true;
	}
}

bool GameAnalytics::cardMoveBetweenBuildAndSuitStack(const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard, int changedIndex1, int changedIndex2)
{
	auto inList1 = std::find(currentPlayingBoard.at(changedIndex1).knownCards.begin(), currentPlayingBoard.at(changedIndex1).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex1));
	auto inList2 = std::find(currentPlayingBoard.at(changedIndex2).knownCards.begin(), currentPlayingBoard.at(changedIndex2).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex2));
	if (inList1 != currentPlayingBoard.at(changedIndex1).knownCards.end() && inList2 == currentPlayingBoard.at(changedIndex2).knownCards.end())
	{
		inList1++;
		currentPlayingBoard.at(changedIndex2).knownCards.insert(
			currentPlayingBoard.at(changedIndex2).knownCards.end(),
			inList1,
			currentPlayingBoard.at(changedIndex1).knownCards.end());
		currentPlayingBoard.at(changedIndex1).knownCards.erase(
			inList1,
			currentPlayingBoard.at(changedIndex1).knownCards.end());
		return true;
	}
	if (inList1 == currentPlayingBoard.at(changedIndex1).knownCards.end() && inList2 != currentPlayingBoard.at(changedIndex2).knownCards.end())
	{
		inList2++;
		currentPlayingBoard.at(changedIndex1).knownCards.insert(
			currentPlayingBoard.at(changedIndex1).knownCards.end(),
			inList2,
			currentPlayingBoard.at(changedIndex2).knownCards.end());
		currentPlayingBoard.at(changedIndex2).knownCards.erase(
			inList2,
			currentPlayingBoard.at(changedIndex2).knownCards.end());
		return true;
	}
	if (inList1 == currentPlayingBoard.at(changedIndex1).knownCards.end() && inList2 == currentPlayingBoard.at(changedIndex2).knownCards.end())
	{
		auto inList1 = std::find(currentPlayingBoard.at(changedIndex1).knownCards.begin(), currentPlayingBoard.at(changedIndex1).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex2));
		auto inList2 = std::find(currentPlayingBoard.at(changedIndex2).knownCards.begin(), currentPlayingBoard.at(changedIndex2).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex1));
		if (inList1 != currentPlayingBoard.at(changedIndex1).knownCards.end() && inList2 == currentPlayingBoard.at(changedIndex2).knownCards.end())
		{
			currentPlayingBoard.at(changedIndex2).knownCards.insert(
				currentPlayingBoard.at(changedIndex2).knownCards.end(),
				currentPlayingBoard.at(changedIndex1).knownCards.begin(),
				currentPlayingBoard.at(changedIndex1).knownCards.end());
			currentPlayingBoard.at(changedIndex1).knownCards.clear();
			if (classifiedCardsFromPlayingBoard.at(changedIndex1).first != EMPTY)
			{
				currentPlayingBoard.at(changedIndex1).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex1));
				--currentPlayingBoard.at(changedIndex1).unknownCards;
			}
			return true;
		}
		if (inList1 == currentPlayingBoard.at(changedIndex1).knownCards.end() && inList2 != currentPlayingBoard.at(changedIndex2).knownCards.end())
		{
			currentPlayingBoard.at(changedIndex1).knownCards.insert(
				currentPlayingBoard.at(changedIndex1).knownCards.end(),
				currentPlayingBoard.at(changedIndex2).knownCards.begin(),
				currentPlayingBoard.at(changedIndex2).knownCards.end());
			currentPlayingBoard.at(changedIndex2).knownCards.clear();
			if (classifiedCardsFromPlayingBoard.at(changedIndex2).first != EMPTY)
			{
				currentPlayingBoard.at(changedIndex2).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex2));
				--currentPlayingBoard.at(changedIndex2).unknownCards;
			}
			return true;
		}
	}
	return false;
}

void GameAnalytics::findChangedCardLocations(const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard, int & changedIndex1, int & changedIndex2)
{
	for (int i = 0; i < currentPlayingBoard.size(); i++)
	{
		if (currentPlayingBoard.at(i).knownCards.empty())	// adding card to an empty location
		{
			if (classifiedCardsFromPlayingBoard.at(i).first != EMPTY)
			{
				changedIndex1 == -1 ? (changedIndex1 = i) : (changedIndex2 = i);
			}
		}
		else if (i == 7)
		{
			if (currentPlayingBoard.at(7).knownCards.back() != classifiedCardsFromPlayingBoard.at(7))
			{
				changedIndex1 == -1 ? (changedIndex1 = 7) : (changedIndex2 = 7);
			}
		}
		else
		{
			if (currentPlayingBoard.at(i).knownCards.back() != classifiedCardsFromPlayingBoard.at(i))
			{
				changedIndex1 == -1 ? (changedIndex1 = i) : (changedIndex2 = i);
			}
		}
		if (changedIndex2 != -1) { return; }
	}
}

void GameAnalytics::updateDeck(const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard)
{
	auto result = std::find(
		currentPlayingBoard.at(7).knownCards.begin(),
		currentPlayingBoard.at(7).knownCards.end(),
		classifiedCardsFromPlayingBoard.at(7));
	if (result == currentPlayingBoard.at(7).knownCards.end())
	{
		currentPlayingBoard.at(7).knownCards.push_back(classifiedCardsFromPlayingBoard.at(7));
		--currentPlayingBoard.at(7).unknownCards;
	}
	else
	{
		currentPlayingBoard.at(7).knownCards.erase(result);
		currentPlayingBoard.at(7).knownCards.push_back(classifiedCardsFromPlayingBoard.at(7));
	}
}

void GameAnalytics::printPlayingBoardState()
{
	std::cout << "Deck: ";
	if (currentPlayingBoard.at(7).knownCards.empty())
	{
		std::cout << "// ";
	}
	for (int i = 0; i < currentPlayingBoard.at(7).knownCards.size(); i++)
	{
		std::cout << static_cast<char>(currentPlayingBoard.at(7).knownCards.at(i).first) << static_cast<char>(currentPlayingBoard.at(7).knownCards.at(i).second) << " ";
	}
	std::cout << "     Hidden cards = " << currentPlayingBoard.at(7).unknownCards << std::endl;

	std::cout << "Solved cards: " << std::endl;
	for (int i = 8; i < currentPlayingBoard.size(); i++)
	{
		std::cout << "   Pos " << i - 8 << ": ";
		if (currentPlayingBoard.at(i).knownCards.empty())
		{
			std::cout << "// ";
		}
		for (int j = 0; j < currentPlayingBoard.at(i).knownCards.size(); j++)
		{
			std::cout << static_cast<char>(currentPlayingBoard.at(i).knownCards.at(j).first) << static_cast<char>(currentPlayingBoard.at(i).knownCards.at(j).second) << " ";
		}
		std::cout << "     Hidden cards = " << currentPlayingBoard.at(i).unknownCards << std::endl;
	}

	std::cout << "Bottom cards: " << std::endl;
	for (int i = 0; i < 7; i++)
	{
		std::cout << "   Pos " << i << ": ";
		if (currentPlayingBoard.at(i).knownCards.empty())
		{
			std::cout << "// ";
		}
		for (int j = 0; j < currentPlayingBoard.at(i).knownCards.size(); j++)
		{
			std::cout << static_cast<char>(currentPlayingBoard.at(i).knownCards.at(j).first) << static_cast<char>(currentPlayingBoard.at(i).knownCards.at(j).second) << " ";
		}
		std::cout << "     Hidden cards = " << currentPlayingBoard.at(i).unknownCards << std::endl;
	}

	auto averageThinkTime2 = Clock::now();
	averageThinkDurations.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(averageThinkTime2 - startOfMove).count());
	startOfMove = Clock::now();
	std::cout << std::endl;
}

cv::Mat GameAnalytics::hwnd2mat(const HWND & hwnd)	//Mat = n-dimensional dense array class, HWND = handle for desktop window
{
	SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
	hwindowDC = GetDC(hwnd);	//get device context
	hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);	//creates a compatible memory device context for the device
	SetStretchBltMode(hwindowCompatibleDC, COLORONCOLOR);	//set bitmap stretching mode, color on color deletes all eliminated lines of pixels

	GetClientRect(hwnd, &windowsize);	//get coordinates of clients window
	height = windowsize.bottom;
	width = windowsize.right;
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
	StretchBlt(hwindowCompatibleDC, 0, 0, width, height, hwindowDC, 0, 0, width, height, SRCCOPY); //change SRCCOPY to NOTSRCCOPY for wacky colors !
	GetDIBits(hwindowCompatibleDC, hbwindow, 0, height, src.data, (BITMAPINFO *)&bi, DIB_RGB_COLORS);  //copy from hwindowCompatibleDC to hbwindow

																									   // avoid memory leak
	DeleteObject(hbwindow);
	DeleteDC(hwindowCompatibleDC);
	ReleaseDC(hwnd, hwindowDC);

	return src;
}