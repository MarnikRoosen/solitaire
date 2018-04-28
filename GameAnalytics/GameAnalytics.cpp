#include "stdafx.h"
#include "GameAnalytics.h"
#include <thread>
#include <cwchar>
#include <fstream>
#include <iterator>

GameAnalytics ga;
DWORD WINAPI ThreadHookFunction(LPVOID lpParam);
void changeConsoleFontSize(const double & percentageIncrease);
CRITICAL_SECTION clickLock;

int main(int argc, char** argv)
{
	changeConsoleFontSize(0.25);

	DWORD   dwThreadIdHook;
	HANDLE  hThreadHook;
	InitializeCriticalSection(&clickLock);

	ga.Init();
	//ga.test();

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

	std::thread srcGrabber(&GameAnalytics::grabSrc, &ga);

	ga.Process();

	srcGrabber.join();
	TerminateThread(hThreadHook, EXIT_SUCCESS);
	CloseHandle(hThreadHook);

	return 0;
}

void changeConsoleFontSize(const double & percentageIncrease)
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_FONT_INFOEX font = { sizeof(CONSOLE_FONT_INFOEX) };

	if (!GetCurrentConsoleFontEx(hConsole, 0, &font))
	{
		exit(EXIT_FAILURE);
	}
	COORD size = font.dwFontSize;
	size.X += (SHORT)(size.X * percentageIncrease);
	size.Y += (SHORT)(size.Y * percentageIncrease);
	font.dwFontSize = size;
	if (!SetCurrentConsoleFontEx(hConsole, 0, &font))
	{
		exit(EXIT_FAILURE);
	}
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
	//hwnd = FindWindow(NULL, L"Microsoft Solitaire Collection - Google Chrome");	disable hardware acceleration in advanced settings

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
	pb.determineROI(src);
	pb.findCardsFromBoardImage(src);
	extractedImagesFromPlayingBoard = pb.getCards();
	classifyExtractedCards();
	initializePlayingBoard(classifiedCardsFromPlayingBoard);

	numberOfPresses.resize(12);
	for (int i = 0; i < numberOfPresses.size(); i++)
	{
		numberOfPresses.at(i) = 0;
	}
}

void GameAnalytics::test()
{
	// PREPARATION
	std::vector<cv::Mat> testImages;
	std::vector<std::vector<std::pair<classifiers, classifiers>>> correctClassifiedOutputVector;

	for (int i = 0; i < 10; i++)
	{
		stringstream ss;
		ss << i;
		Mat src = imread("../GameAnalytics/test/someMovesPlayed/" + ss.str() + ".png");
		if (!src.data)	// check for invalid input
		{
			cout << "Could not open or find testimage " << ss.str() << std::endl;
			exit(EXIT_FAILURE);
		}
		testImages.push_back(src.clone());
	}

	/*for (int i = 0; i < testImages.size(); i++)		// ---> used to save testdata of the classified images to a txt file
	{
		pb.findCardsFromBoardImage(testImages.at(i)); // -> average 38ms
		extractedImagesFromPlayingBoard = pb.getCards();
		classifyExtractedCards();	// -> average d133ms and 550ms
		correctClassifiedOutputVector.push_back(classifiedCardsFromPlayingBoard);
	}
	if (!writeTestData(correctClassifiedOutputVector, "../GameAnalytics/test/someMovesPlayed/correctClassifiedOutputVector.txt"))
	{
		std::cout << "Error writing testdata to txt file" << std::endl;
		exit(EXIT_FAILURE);
	}*/
	if (!readTestData(correctClassifiedOutputVector, "../GameAnalytics/test/someMovesPlayed/correctClassifiedOutputVector.txt"))
	{
		std::cout << "Error reading testdata from txt file" << std::endl;
		exit(EXIT_FAILURE);
	}


	// ACTUAL TESTING
	int wrongRankCounter = 0;
	int wrongSuitCounter = 0;
	std::chrono::time_point<std::chrono::steady_clock> test1 = Clock::now();
	for (int k = 0; k < 100; k++)
	{
		for (int i = 0; i < testImages.size(); i++)
		{
			pb.findCardsFromBoardImage(testImages.at(i));
			extractedImagesFromPlayingBoard = pb.getCards();
			classifyExtractedCards();
			for (int j = 0; j < classifiedCardsFromPlayingBoard.size(); j++)
			{
				if (correctClassifiedOutputVector.at(i).at(j).first != classifiedCardsFromPlayingBoard.at(j).first) 
				{ ++wrongRankCounter; }
				if (correctClassifiedOutputVector.at(i).at(j).second != classifiedCardsFromPlayingBoard.at(j).second) 
				{ ++wrongSuitCounter; }
			}
		}
	}
	std::chrono::time_point<std::chrono::steady_clock> test2 = Clock::now();
	std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(test2 - test1).count() << " ns" << std::endl;
	std::cout << "Rank error counter: " << wrongRankCounter << std::endl;
	std::cout << "Suit error counter: " << wrongSuitCounter << std::endl;
	int testcounter = cc.getTestCounter();
	std::cout << "Test counter: " << testcounter << std::endl;
	Sleep(10000);
}

bool GameAnalytics::writeTestData(const vector <vector <pair <classifiers, classifiers> > > &points, const string &file)
{
	if (points.empty())
		return false;

	if (file != "")
	{
		stringstream ss;
		for (int k = 0; k < points.size(); k++)
		{
			for (int i = 0; i < points.at(k).size(); i++)
			{
				ss << static_cast<char>(points.at(k).at(i).first) << " " << static_cast<char>(points.at(k).at(i).second) << "\n";
			}
			ss << "\n";
		}

		ofstream out(file.c_str());
		if (out.fail())
		{
			out.close();
			return false;
		}
		out << ss.str();
		out.close();
	}
	return true;
}

bool GameAnalytics::readTestData(vector <vector <pair <classifiers, classifiers> > > &points, const string &file)
{
	if (file != "")
	{
		stringstream ss;
		ifstream in(file.c_str());
		if (in.fail())
		{
			in.close();
			return false;
		}
		std::string str;
		// Read the next line from File untill it reaches the end.
		points.resize(10);
		for (int i = 0; i < points.size(); i++)
		{
			points.at(i).resize(12);
		}
		for (int k = 0; k < 10; k++)
		{
			for (int i = 0; i < 12; i++)
			{
				std::getline(in, str);
				istringstream iss(str);
				string subs, subs2;
				iss >> subs;
				iss >> subs2;
				points.at(k).at(i).first = static_cast<classifiers>(subs[0]);
				points.at(k).at(i).second = static_cast<classifiers>(subs2[0]);
			}
			std::getline(in, str);
		}
		in.close();
	}
	return true;
}

void GameAnalytics::Process()
{
	bool boardChanged;
	while (!endOfGameBool)
	{
		if (!srcBuffer.empty())
		{
			EnterCriticalSection(&clickLock);
			src = srcBuffer.front(); srcBuffer.pop();
			pt->x = xPosBuffer.front(); xPosBuffer.pop();
			pt->y = yPosBuffer.front(); yPosBuffer.pop();
			LeaveCriticalSection(&clickLock);
			pt->y = (pt->y - (windowHeight - distortedWindowHeight) / 2) / distortedWindowHeight * windowHeight;
			MapWindowPoints(GetDesktopWindow(), hwnd, &pt[0], 1);
			MapWindowPoints(GetDesktopWindow(), hwnd, &pt[1], 1);
			pt->x = pt->x * standardBoardWidth / windowWidth;
			pt->y = pt->y * standardBoardHeight / windowHeight;
			std::cout << "Click registered at position (" << pt->x << "," << pt->y << ")" << std::endl;

			determineNextState(pt->x, pt->y);
			switch (currentState)
			{
			case PLAYING:
				pb.findCardsFromBoardImage(src); // -> average 38ms
				boardChanged = handlePlayingState();
				break;
			case UNDO:
				if (previousPlayingBoards.size() > 1)
				{
					previousPlayingBoards.pop_back();
					currentPlayingBoard = previousPlayingBoards.back();
				}
				printPlayingBoardState();
				++numberOfUndos;			
				currentState = PLAYING;
				break;
			case QUIT:
				std::cout << "--------------------------------------------------------" << std::endl;
				std::cout << "Game over" << std::endl;
				std::cout << "--------------------------------------------------------" << std::endl;
				gameWon = false;
				endOfGameBool = true;
				break;
			case WON:
				gameWon = true;
				endOfGameBool = true;
				break;
			case AUTOCOMPLETE:
				std::cout << "--------------------------------------------------------" << std::endl;
				std::cout << "Game won! Finished using autocomplete." << std::endl;
				std::cout << "--------------------------------------------------------" << std::endl;
				gameWon = true;
				endOfGameBool = true;
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
			processCardSelection(pt->x, pt->y);

		}
		else
		{
			Sleep(10);
		}
	}
	handleEndOfGame();
}

void GameAnalytics::toggleClickDownBool()
{
	if (waitForStableImageBool)
	{
		std::cout << "CLICK DOWN BOOL" << std::endl;
		clickDownBool = true;
	}
	else
	{
		clickDownBool = false;
	}
}

void GameAnalytics::grabSrc()
{
	while (!endOfGameBool)
	{
		if (!xPosBuffer.empty())
		{
			waitForStableImageBool = true;
			cv::Mat img = waitForStableImage();
			waitForStableImageBool = false;

			//data.src = img.clone();

			EnterCriticalSection(&clickLock);
			srcBuffer.push(img.clone());
			LeaveCriticalSection(&clickLock);
			dataCounter++;

			stringstream ss;
			ss << dataCounter;
			imwrite("../GameAnalytics/screenshots/" + ss.str() + ".png", img);
		}
		else
		{
			cv::Mat img = waitForStableImage();
			if (currentState == PLAYING && pb.checkForOutOfMovesState(img))
			{
				EnterCriticalSection(&clickLock);
				srcBuffer.push(img.clone());
				xPosBuffer.push(0);	// push dummy data to inform outOfMoves
				yPosBuffer.push(0);
				LeaveCriticalSection(&clickLock);
				dataCounter++;

				stringstream ss;
				ss << dataCounter;
				imwrite("../GameAnalytics/screenshots/" + ss.str() + ".png", img);
			}
			else
			{
				Sleep(5);
			}
		}
	}
}

void GameAnalytics::processCardSelection(const int & x, const int & y)
{
	int indexOfPressedCardLocation = determineIndexOfPressedCard(x, y);
	if (indexOfPressedCardLocation != -1)
	{
		int indexOfPressedCard = pb.getIndexOfSelectedCard(indexOfPressedCardLocation);
		if (indexOfPressedCard != -1)
		{
			if (indexOfPressedCard == 0)
			{
				locationOfPresses.push_back(locationOfLastPress);
			}
			int index = currentPlayingBoard.at(indexOfPressedCardLocation).knownCards.size() - indexOfPressedCard - 1;
			std::pair<classifiers, classifiers> selectedCard = currentPlayingBoard.at(indexOfPressedCardLocation).knownCards.at(index);
			//std::cout << static_cast<char>(selectedCard.first) << static_cast<char>(selectedCard.second) << " is selected." << std::endl;

			++numberOfPresses.at(indexOfPressedCardLocation);

			// detection of wrong card placement
			if (previouslySelectedCard.first != EMPTY && previouslySelectedCard != selectedCard)
			{
				char prevSuit = static_cast<char>(previouslySelectedCard.second);
				char newSuit = static_cast<char>(selectedCard.second);
				char prevRank = static_cast<char>(previouslySelectedCard.first);
				char newRank = static_cast<char>(selectedCard.first);

				if (8 <= indexOfPressedCardLocation && indexOfPressedCardLocation < 12 && 0 <= previouslySelectedIndex && previouslySelectedIndex < 8)
				{
					if ((prevSuit == 'H' && newSuit != 'H') || (prevSuit == 'D' && newSuit != 'D')
						|| (prevSuit == 'S' && newSuit != 'S') || (prevSuit == 'C' || newSuit != 'C'))
					{
						std::cout << "Incompatible suit! " << prevSuit << " can't go on " << newSuit << " to build the suit stack" << std::endl;
						++numberOfSuitErrors;
					}
				}
				else if (((prevSuit == 'H' || prevSuit == 'D') && (newSuit == 'H' || newSuit == 'D'))
					|| ((prevSuit == 'S' || prevSuit == 'C') && (newSuit == 'S' || newSuit == 'C')))
				{
					std::cout << "Incompatible suit! " << prevSuit << " can't go on " << newSuit<< " to build the build stack" << std::endl;
					++numberOfSuitErrors;
				}

				if ((prevRank == '2' && newRank != '3') || (prevRank == '3' && newRank != '4') || (prevRank == '4' && newRank != '5')
					|| (prevRank == '5' && newRank != '6') || (prevRank == '6' && newRank != '7') || (prevRank == '7' && newRank != '8')
					|| (prevRank == '8' && newRank != '9') || (prevRank == '9' && newRank != ':') || (prevRank == ':' && newRank != 'J')
					|| (prevRank == 'J' && newRank != 'Q') || (prevRank == 'Q' && newRank != 'K') || (prevRank == 'K' && newRank != 'A'))
				{
					std::cout << "Incompatible rank! " << prevRank << " can't go on " << newRank << std::endl;
					++numberOfRankErrors;
				}
			}
			previouslySelectedCard = selectedCard;
			previouslySelectedIndex = indexOfPressedCardLocation;
		}
		else
		{
			previouslySelectedCard.first = EMPTY;
			previouslySelectedCard.second = EMPTY;
			previouslySelectedIndex = -1;
		}

		if (7 <= indexOfPressedCardLocation || indexOfPressedCardLocation < 12)
		{
			locationOfPresses.push_back(locationOfLastPress);
		}
	}
}

int GameAnalytics::determineIndexOfPressedCard(const int & x, const int & y)
{
	if (84 <= pt->y && y <= 258)
	{
		if (434 <= x && x <= 629)
		{
			locationOfLastPress.first = x - 434;
			locationOfLastPress.second = y - 84;
			return 7;
		}
		else if (734 <= x && x <= 865)
		{
			locationOfLastPress.first = x - 734;
			locationOfLastPress.second = y - 84;
			return 8;
		}
		else if (884 <= x && x <= 1015)
		{
			locationOfLastPress.first = x - 884;
			locationOfLastPress.second = y - 84;
			return 9;
		}
		else if (1034 <= x && x <= 1165)
		{
			locationOfLastPress.first = x - 1034;
			locationOfLastPress.second = y - 84;
			return 10;
		}
		else if (1184 <= x && x <= 1315)
		{
			locationOfLastPress.first = x - 1184;
			locationOfLastPress.second = y - 84;
			return 11;
		}
		else
		{
			return -1;
		}
	}
	else if (290 <= y && y <= 850)
	{
		if (284 <= x && x <= 415)
		{
			locationOfLastPress.first = x - 284;
			locationOfLastPress.second = y - 290;
			return 0;
		}
		else if (434 <= x && x <= 565)
		{
			locationOfLastPress.first = x - 434;
			locationOfLastPress.second = y - 290;
			return 1;
		}
		else if (584 <= x && x <= 715)
		{
			locationOfLastPress.first = x - 584;
			locationOfLastPress.second = y - 290;
			return 2;
		}
		else if (734 <= x && x <= 865)
		{
			locationOfLastPress.first = x - 734;
			locationOfLastPress.second = y - 290;
			return 3;
		}
		else if (884 <= x && x <= 1015)
		{
			locationOfLastPress.first = x - 884;
			locationOfLastPress.second = y - 290;
			return 4;
		}
		else if (1034 <= x && x <= 1165)
		{
			locationOfLastPress.first = x - 1034;
			locationOfLastPress.second = y - 290;
			return 5;
		}
		else if (1184 <= x && x <= 1315)
		{
			locationOfLastPress.first = x - 1184;
			locationOfLastPress.second = y - 290;
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

void GameAnalytics::determineNextState(const int & x, const int & y)
{
	switch (currentState)
	{
	case MENU:
		if ((1 <= x  && x <= 300) && (64 <= y && y <= 108))
		{
			std::cout << "HINT PRESSED!" << std::endl;
			currentState = HINT;
		}
		else if (!((1 <= x  && x <= 300) && (54 <= y && y <= 899)))
		{
			std::cout << "PLAYING!" << std::endl;
			currentState = PLAYING;
		}
		break;
	case NEWGAME:
		if ((1175 <= x  && x <= 1312) && (486 <= y && y <= 516))
		{
			std::cout << "CANCEL PRESSED!" << std::endl;
			currentState = PLAYING;
		}
		else if ((1010 <= x  && x <= 1146) && (486 <= y && y <= 516))
		{
			std::cout << "CONTINUE PRESSED!" << std::endl;
			currentState = QUIT;
		}
		break;
	case OUTOFMOVES:
		if ((1175 <= x  && x <= 1312) && (486 <= y && y <= 516))
		{
			std::cout << "UNDO PRESSED!" << std::endl;
			currentState = UNDO;
		}
		else if ((1010 <= x  && x <= 1146) && (486 <= y && y <= 516))
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
		if ((75 <= x  && x <= 360) && (149 <= y && y <= 416))
		{
			std::cout << "KLONDIKE PRESSED!" << std::endl;
			currentState = PLAYING;
		}
		break;
	case PLAYING:
		if ((12 <= x  && x <= 111) && (837 <= y && y <= 889))
		{
			std::cout << "NEWGAME PRESSED!" << std::endl;
			currentState = NEWGAME;
		}
		else if (pb.checkForOutOfMovesState(src))
		{
			std::cout << "OUT OF MOVES!" << std::endl;
			currentState = OUTOFMOVES;
		}
		else if ((1487 <= x  && x <= 1586) && (837 <= y && y <= 889))
		{
			std::cout << "UNDO PRESSED!" << std::endl;
			currentState = UNDO;
		}
		else if ((13 <= x  && x <= 82) && (1 <= y && y <= 55))
		{
			std::cout << "MENU PRESSED!" << std::endl;
			currentState = MENU;
		}
		else if ((283 <= x  && x <= 416) && (84 <= y && y <= 258))
		{
			//std::cout << "PILE PRESSED!" << std::endl;
			locationOfLastPress.first = x - 283;
			locationOfLastPress.second = y - 84;
			locationOfPresses.push_back(locationOfLastPress);
			++numberOfPilePresses;
		}
		else if ((91 <= x  && x <= 161) && (1 <= y && y <= 55))
		{
			std::cout << "BACK PRESSED!" << std::endl;
			currentState = MAINMENU;
		}
		break;
	case WON:
		std::cout << "--------------------------------------------------------" << std::endl;
		std::cout << "Game won!" << std::endl;
		std::cout << "--------------------------------------------------------" << std::endl;
		break;
	default:
		std::cerr << "Error: currentState is not defined!" << std::endl;
		break;
	}
}

void GameAnalytics::handleEndOfGame()
{
	std::chrono::time_point<std::chrono::steady_clock> endOfGame = Clock::now();
	std::cout << "Game solved: " << std::boolalpha << gameWon << std::endl;
	std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::seconds>(endOfGame - startOfGame).count() << " s" << std::endl;
	std::cout << "Points scored: " << score << std::endl;
	std::cout << "Average time per move = " << std::accumulate(averageThinkDurations.begin(), averageThinkDurations.end(), 0) / averageThinkDurations.size() << "ms" << std::endl;
	std::cout << "Number of moves = " << averageThinkDurations.size() << " moves" << std::endl;
	std::cout << "Hints requested = " << numberOfHints << std::endl;
	std::cout << "Times undo = " << numberOfUndos << std::endl;
	std::cout << "Number of rank errors = " << numberOfRankErrors << std::endl;
	std::cout << "Number of suit errors = " << numberOfSuitErrors << std::endl;
	std::cout << "--------------------------------------------------------" << std::endl;
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
	std::cout << "--------------------------------------------------------" << std::endl;
	cv::Mat pressLocations = Mat(176 * 2, 133 * 2, CV_8UC3, Scalar(255, 255, 255));
	for (int i = 0; i < locationOfPresses.size(); i++)
	{
		cv::Point point = Point(locationOfPresses.at(i).first * 2, locationOfPresses.at(i).second * 2);
		cv::circle(pressLocations, point, 2, cv::Scalar(0, 0, 255), 2);
	}
	namedWindow("clicklocations", WINDOW_NORMAL);
	resizeWindow("clicklocations", cv::Size(131 * 2, 174 * 2));
	imshow("clicklocations", pressLocations);
	waitKey(0);
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
	imageCounter++;
	EnterCriticalSection(&clickLock);
	//srcBuffer.push(src);
	xPosBuffer.push(x);
	yPosBuffer.push(y);
	LeaveCriticalSection(&clickLock);
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
	norm = DBL_MAX;
	std::chrono::time_point<std::chrono::steady_clock> duration1 = Clock::now();
	std::chrono::time_point<std::chrono::steady_clock> duration2;
	
	src1 = hwnd2mat(hwnd);
	cvtColor(src1, graySrc1, COLOR_BGR2GRAY);
	Sleep(60);
	src2 = hwnd2mat(hwnd);
	cvtColor(src2, graySrc2, COLOR_BGR2GRAY);
	norm = cv::norm(graySrc1, graySrc2, NORM_L1);
	while (norm > 0.0)
	{
		if (std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - duration1).count() > 2)
		{
			cv::Mat empty;
			return empty;
		}
		src1 = src2;
		cvtColor(src1, graySrc1, COLOR_BGR2GRAY);
		Sleep(60);
		src2 = hwnd2mat(hwnd);
		cvtColor(src2, graySrc2, COLOR_BGR2GRAY);
		norm = cv::norm(graySrc1, graySrc2, NORM_L1);
		if (clickDownBool)
		{
			clickDownBool = false;
			std::cout << "Using clickdown image" << std::endl;
			return src1;
		}
	}
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
	previousPlayingBoards.push_back(currentPlayingBoard);
	printPlayingBoardState();
}

bool GameAnalytics::updateBoard(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard)
{	
	changedIndex1 = -1, changedIndex2 = -1;
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