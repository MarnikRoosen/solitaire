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

	//CloseHandle(hThreadHook);
	

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

GameAnalytics::~GameAnalytics()
{
	DeleteObject(hbwindow);
	DeleteDC(hwindowCompatibleDC);
	ReleaseDC(hwnd, hwindowDC);
}

void GameAnalytics::Init() {
	currentState = PLAYING;
	SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

	hwnd = FindWindow(NULL, L"Microsoft Solitaire Collection - Firefox Developer Edition");
	//hwnd = FindWindow(_T("Chrome_WidgetWin_1"), NULL);

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
	windowWidth = abs(appRect.right - appRect.left);
	windowHeight = abs(appRect.bottom - appRect.top);
	if (1.75 < windowWidth / windowHeight < 1.80)
	{
		distortedWindowHeight = (windowWidth * 1080 / 1920);
		//std::cout << "heightOffset = " << distortedWindowHeight << std::endl;
	}
	
	// preparing for screencapture

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
	SelectObject(hwindowCompatibleDC, hbwindow);	// use the previously created device context with the bitmap

	previouslySelectedCard.first = EMPTY;
	previouslySelectedCard.second = EMPTY;
	startOfGame = Clock::now();
	startOfMove = Clock::now();

	classifiedCardsFromPlayingBoard.reserve(12);
	src = waitForStableImage();
	pb.findCardsFromBoardImage(src);
	extractedImagesFromPlayingBoard = pb.getCards();
	classifyExtractedCards();
	initializePlayingBoard(classifiedCardsFromPlayingBoard);

	numberOfPresses.resize(12);
	for (int i = 0; i < numberOfPresses.size(); i++)
	{
		numberOfPresses.at(i) = 0;
	}
	int indexOfPressedCardLocation = determineIndexOfPressedCard();
}

void GameAnalytics::Process()
{
	bool boardChanged;
	while (!endOfGameBool)
	{
		if (currentPlayingBoard.at(8).knownCards.size() == 13 && currentPlayingBoard.at(9).knownCards.size() == 13
			&& currentPlayingBoard.at(10).knownCards.size() == 13 && currentPlayingBoard.at(11).knownCards.size() == 13)
		{
			currentState = WON;
		}
		else if (!xPosBuffer.empty())
		{
			EnterCriticalSection(&lock);
			int& x = xPosBuffer.front(); xPosBuffer.pop();
			int& y = yPosBuffer.front(); yPosBuffer.pop();
			LeaveCriticalSection(&lock);

			// Mapping the coordinates from the primary window to the window in which the application is playing
			pt->x = x;
			pt->y = (y - (windowHeight - distortedWindowHeight)/2) / distortedWindowHeight * windowHeight;
			MapWindowPoints(GetDesktopWindow(), hwnd, &pt[0], 1);
			MapWindowPoints(GetDesktopWindow(), hwnd, &pt[1], 1);
			pt->x = pt->x * standardBoardWidth / windowWidth;
			pt->y = pt->y * standardBoardHeight / windowHeight;
			std::cout << "Click registered at position (" << pt->x << "," << pt->y << ")" << std::endl;

			src = waitForStableImage();
			determineNextState();
			switch (currentState)
			{
			case PLAYING:
				pb.findCardsFromBoardImage(src); // -> average 38ms
				boardChanged = handlePlayingState();
				break;
			case UNDO:
				previousPlayingBoards.pop_back();
				currentPlayingBoard = previousPlayingBoards.back();
				printPlayingBoardState();
				++numberOfUndos;			
				currentState = PLAYING;
				break;
			case QUIT:
				std::cout << "--------------------------------------------------------" << std::endl;
				std::cout << "Game over" << std::endl;
				std::cout << "--------------------------------------------------------" << std::endl;
				handleEndOfGame();
				endOfGameBool = true;
				break;
			case WON:
				std::cout << "--------------------------------------------------------" << std::endl;
				std::cout << "Game won!" << std::endl;
				std::cout << "--------------------------------------------------------" << std::endl;
				endOfGameBool = true;
				handleEndOfGame();
				break;
			case AUTOCOMPLETE:
				std::cout << "--------------------------------------------------------" << std::endl;
				std::cout << "Game won! Finished using autocomplete." << std::endl;
				std::cout << "--------------------------------------------------------" << std::endl;
				endOfGameBool = true;
				handleEndOfGame();
				break;
			case HINT:
				++numberOfHints;
				currentState = PLAYING;
				break;
			case NEWGAME:
				break;
			case MENU:
				break;
			case MAINMENU:
				break;
			case OUTOFMOVES:
				break;
			default:
				std::cerr << "Error: currentState is not defined!" << std::endl;
				break;
			}
			processCardSelection();

		}
		else
		{
			Sleep(10);
		}
	}
}

void GameAnalytics::processCardSelection()
{
	int indexOfPressedCardLocation = determineIndexOfPressedCard();
	if (indexOfPressedCardLocation != -1)
	{
		pb.findCardsFromBoardImage(src);
		int indexOfPressedCard = pb.getIndexOfSelectedCard(indexOfPressedCardLocation);
		if (indexOfPressedCard != -1)
		{
			int index = currentPlayingBoard.at(indexOfPressedCardLocation).knownCards.size() - indexOfPressedCard - 1;
			std::pair<classifiers, classifiers> selectedCard = currentPlayingBoard.at(indexOfPressedCardLocation).knownCards.at(index);
			//std::cout << static_cast<char>(selectedCard.first) << static_cast<char>(selectedCard.second) << " is selected." << std::endl;

			++numberOfPresses.at(indexOfPressedCardLocation);

			// detection of wrong card placement
			if (previouslySelectedCard.first != EMPTY)
			{
				char prevSuit = static_cast<char>(previouslySelectedCard.second);
				char newSuit = static_cast<char>(selectedCard.second);
				char prevRank = static_cast<char>(previouslySelectedCard.first);
				char newRank = static_cast<char>(selectedCard.first);

				if (((prevSuit == 'H' || prevSuit == 'D') && (newSuit == 'H' || newSuit == 'D'))
					|| ((prevSuit == 'S' || prevSuit == 'C') && (newSuit == 'S' || newSuit == 'C')))
				{
					std::cout << "Incompatible suit! " << prevSuit << " isn't compatible with " << newSuit << std::endl;
					++numberOfSuitErrors;
				}
				else
				{
					std::cout << "Incompatible rank! " << prevRank << " isn't compatible with " << newRank << std::endl;
					++numberOfRankErrors;
				}
			}
			previouslySelectedCard = selectedCard;
		}
		else
		{
			previouslySelectedCard.first = EMPTY;
			previouslySelectedCard.second = EMPTY;
		}
	}
}

int GameAnalytics::determineIndexOfPressedCard()
{
	if (84 <= pt->y && pt->y <= 258)
	{
		if (434 <= pt->x && pt->x <= 629)
		{
			return 7;
		}
		else if (734 <= pt->x && pt->x <= 865)
		{
			return 8;
		}
		else if (884 <= pt->x && pt->x <= 1015)
		{
			return 9;
		}
		else if (1034 <= pt->x && pt->x <= 1165)
		{
			return 10;
		}
		else if (1184 <= pt->x && pt->x <= 1315)
		{
			return 11;
		}
		else
		{
			return -1;
		}
	}
	else if (290 <= pt->y && pt->y <= 850)
	{
		if (284 <= pt->x && pt->x <= 415)
		{
			return 0;
		}
		else if (434 <= pt->x && pt->x <= 565)
		{
			return 1;
		}
		else if (584 <= pt->x && pt->x <= 715)
		{
			return 2;
		}
		else if (734 <= pt->x && pt->x <= 865)
		{
			return 3;
		}
		else if (884 <= pt->x && pt->x <= 1015)
		{
			return 4;
		}
		else if (1034 <= pt->x && pt->x <= 1165)
		{
			return 5;
		}
		else if (1184 <= pt->x && pt->x <= 1315)
		{
			return 6;
		}
		else
		{
			return -1;
		}
	}
	else
	{
		return -1;
	}	
	return -1;
}

void GameAnalytics::determineNextState()
{
	switch (currentState)
	{
	case MENU:
		if ((1 <= pt->x  && pt->x <= 300) && (64 <= pt->y && pt->y <= 108))
		{
			std::cout << "HINT PRESSED!" << std::endl;
			currentState = HINT;
		}
		else if (!((1 <= pt->x  && pt->x <= 300) && (54 <= pt->y && pt->y <= 899)))
		{
			std::cout << "PLAYING!" << std::endl;
			currentState = PLAYING;
		}
		break;
	case NEWGAME:
		if ((1175 <= pt->x  && pt->x <= 1312) && (486 <= pt->y && pt->y <= 516))
		{
			std::cout << "CANCEL PRESSED!" << std::endl;
			currentState = PLAYING;
		}
		else if ((1010 <= pt->x  && pt->x <= 1146) && (486 <= pt->y && pt->y <= 516))
		{
			std::cout << "CONTINUE PRESSED!" << std::endl;
			currentState = QUIT;
		}
		break;
	case OUTOFMOVES:
		if ((1175 <= pt->x  && pt->x <= 1312) && (486 <= pt->y && pt->y <= 516))
		{
			std::cout << "UNDO PRESSED!" << std::endl;
			currentState = UNDO;
		}
		else if ((1010 <= pt->x  && pt->x <= 1146) && (486 <= pt->y && pt->y <= 516))
		{
			std::cout << "CONTINUE PRESSED!" << std::endl;
			currentState = QUIT;
		}
		else
		{
			currentState = OUTOFMOVES;
		}
		break;
	case MAINMENU:
		if ((75 <= pt->x  && pt->x <= 360) && (149 <= pt->y && pt->y <= 416))
		{
			std::cout << "KLONDIKE PRESSED!" << std::endl;
			currentState = PLAYING;
		}
		break;
	case PLAYING:
		if ((12 <= pt->x  && pt->x <= 111) && (837 <= pt->y && pt->y <= 889))
		{
			std::cout << "NEWGAME PRESSED!" << std::endl;
			currentState = NEWGAME;
		}
		else if (pb.checkForOutOfMovesState(src))
		{
			std::cout << "OUT OF MOVES!" << std::endl;
			currentState = OUTOFMOVES;
		}
		else if ((1487 <= pt->x  && pt->x <= 1586) && (837 <= pt->y && pt->y <= 889))
		{
			std::cout << "UNDO PRESSED!" << std::endl;
			currentState = UNDO;
		}
		else if ((13 <= pt->x  && pt->x <= 82) && (1 <= pt->y && pt->y <= 55))
		{
			std::cout << "MENU PRESSED!" << std::endl;
			currentState = MENU;
		}
		else if ((283 <= pt->x  && pt->x <= 416) && (84 <= pt->y && pt->y <= 258))
		{
			//std::cout << "PILE PRESSED!" << std::endl;
			++numberOfPilePresses;
		}
		else if ((91 <= pt->x  && pt->x <= 161) && (1 <= pt->y && pt->y <= 55))
		{
			std::cout << "BACK PRESSED!" << std::endl;
			currentState = MAINMENU;
		}
		break;
	default:
		std::cerr << "Error: currentState is not defined!" << std::endl;
		break;
	}
}

void GameAnalytics::handleEndOfGame()
{
	std::chrono::time_point<std::chrono::steady_clock> endOfGame = Clock::now();
	std::cout << "Game solved: " << std::boolalpha << endOfGameBool << std::endl;
	std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::seconds>(endOfGame - startOfGame).count() << " s" << std::endl;
	std::cout << "Points scored: " << score << std::endl;
	std::cout << "Average time per move = " << std::accumulate(averageThinkDurations.begin(), averageThinkDurations.end(), 0) / averageThinkDurations.size() << "ms" << std::endl;
	std::cout << "Number of moves = " << averageThinkDurations.size() << " moves" << std::endl;
	std::cout << "Times undo = " << numberOfUndos << std::endl;
	std::cout << "Number of rank errors = " << numberOfRankErrors << std::endl;
	std::cout << "Number of suit errors = " << numberOfSuitErrors << std::endl;
	for (int i = 0; i < 7; i++)
	{
		std::cout << "Number of build stack " << i << " presses = " << numberOfPresses.at(i) << std::endl;
	}
	std::cout << "Number of pile presses = " << numberOfPilePresses << std::endl;
	std::cout << "Number of talon presses = " << numberOfPresses.at(7) << std::endl;
	for (int i = 8; i < 12; i++)
	{
		std::cout << "Number of suit stack " << i << " presses = " << numberOfPresses.at(i) << std::endl;
	}
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
	Mat src1, src2, graySrc1, graySrc2;
	double norm = DBL_MAX;
	do {
		src1 = hwnd2mat(hwnd);
		cvtColor(src1, graySrc1, COLOR_BGR2GRAY);
		Sleep(50);
		src2 = hwnd2mat(hwnd);
		cvtColor(src2, graySrc2, COLOR_BGR2GRAY);
		norm = cv::norm(graySrc1, graySrc2, NORM_L1);
	} while (norm > 0.0);
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
			updateTalonStack(classifiedCardsFromPlayingBoard);
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
				updateTalonStack(classifiedCardsFromPlayingBoard);
			}
			printPlayingBoardState();
			++numberOfPresses.at(tempIndex);
			(0 <= tempIndex && tempIndex < 7) ? score += 5 : score += 10;
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
			score += 5;
			--currentPlayingBoard.at(changedIndex1).unknownCards;
		}
		currentPlayingBoard.at(changedIndex1).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex1));
		printPlayingBoardState();
		return true;
	}
}

bool GameAnalytics::cardMoveBetweenBuildAndSuitStack(const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard, int changedIndex1, int changedIndex2)
{
	// current visible card was already in the list of known cards
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
		
		if (8 <= changedIndex2 && changedIndex2 < 12 && 0 <= changedIndex1 && changedIndex1 < 7)	// build to suit stack
		{
			score += 10;
		}
		else if (8 <= changedIndex1 && changedIndex1 < 12 && 0 <= changedIndex2 && changedIndex2 < 7)	// suit to build stack
		{
			score -= 10;
		}
		++numberOfPresses.at(changedIndex2);
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

		if (8 <= changedIndex1 && changedIndex1 < 12 && 0 <= changedIndex2 && changedIndex2 < 7)	// build to suit stack
		{
			score += 10;
		}
		else if (8 <= changedIndex2 && changedIndex2 < 12 && 0 <= changedIndex1 && changedIndex1 < 7)	// suit to build stack
		{
			score -= 10;
		}
		++numberOfPresses.at(changedIndex1);
		return true;
	}
	// current visible card wasn't in the list of already known cards
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
				score += 5;
				--currentPlayingBoard.at(changedIndex1).unknownCards;
			}

			if (8 <= changedIndex2 && changedIndex2 < 12 && 0 <= changedIndex1 && changedIndex1 < 7)	// build to suit stack
			{
				score += 10;
			}
			else if (8 <= changedIndex1 && changedIndex1 < 12 && 0 <= changedIndex2 && changedIndex2 < 7)	// suit to build stack
			{
				score -= 10;
			}
			++numberOfPresses.at(changedIndex2);
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
				score += 5;
				--currentPlayingBoard.at(changedIndex2).unknownCards;
			}

			if (8 <= changedIndex1 && changedIndex1 < 12 && 0 <= changedIndex2 && changedIndex2 < 7)	// build to suit stack
			{
				score += 10;
			}
			else if (8 <= changedIndex2 && changedIndex2 < 12 && 0 <= changedIndex1 && changedIndex1 < 7)	// suit to build stack
			{
				score -= 10;
			}
			++numberOfPresses.at(changedIndex1);
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

void GameAnalytics::updateTalonStack(const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard)
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
	// copy from the window device context to the bitmap device context
	StretchBlt(hwindowCompatibleDC, 0, 0, width, height, hwindowDC, 0, 0, width, height, SRCCOPY); //change SRCCOPY to NOTSRCCOPY for wacky colors !
	GetDIBits(hwindowCompatibleDC, hbwindow, 0, height, src.data, (BITMAPINFO *)&bi, DIB_RGB_COLORS);  //copy from hwindowCompatibleDC to hbwindow
	return src;
}