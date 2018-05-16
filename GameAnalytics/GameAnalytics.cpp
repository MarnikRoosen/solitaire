#include "stdafx.h"
#include "GameAnalytics.h"

CRITICAL_SECTION threadLock;
GameAnalytics ga;

int main(int argc, char** argv)
{
	// increasing the console font size by 50 %
	changeConsoleFontSize(0.5);

	// initializing lock for shared variables, screencapture and game logic
	InitializeCriticalSection(&threadLock);
	ga.initScreenCapture();
	ga.initGameLogic();
	ga.makeDBConn();
	
	
	//ga.test();	// --> used for benchmarking functions

	// initializing thread to capture mouseclicks and a thread dedicated to capturing the screen of the game 
	std::thread clickThread(&GameAnalytics::hookMouseFunction, &ga);
	std::thread srcGrabber(&GameAnalytics::grabSrc, &ga);

	// main processing function of the main thread
	ga.process();

	// terminate threads
	srcGrabber.join();
	clickThread.join();

	
	return 0;
}

void GameAnalytics::makeDBConn() {
	

	sql::Driver *driver;
	sql::Connection *con;
	sql::Statement *stmt;

	driver = get_driver_instance();
	con = driver->connect("tcp://127.0.0.1:3306", "root", "root");
	con->setSchema("game_data");

	if (con->isValid()) {

		std::cout << "Connection made with database" << std::endl;
		stmt = con->createStatement();
		stmt->execute("DROP TABLE IF EXISTS game_data");
		stmt->execute("CREATE TABLE game_data(id INT, label CHAR(1))");
		stmt->execute("INSERT INTO game_data(id, label) VALUES (1, 'a')");

		delete stmt;
		delete con;
	}
	else {
		std::cout << "No connection could be made with the database" << std::endl;
		con->reconnect();
	}
	

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

void GameAnalytics::hookMouseFunction()
{
	// installing the click hook
	ClicksHooks::Instance().InstallHook();

	// handeling the incoming mouse data
	ClicksHooks::Instance().Messages();
}

GameAnalytics::GameAnalytics() : cc(), pb()
{
	// initializing ClassifyCard.cpp and PlayingBoard.cpp
}

GameAnalytics::~GameAnalytics()
{
}

void GameAnalytics::initScreenCapture()
{
	SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);	// making sure the screensize isn't altered automatically by Windows

	hwnd = FindWindow(NULL, L"Microsoft Solitaire Collection - Firefox Developer Edition");	// find the handle of a window using the FULL name
	// hwnd = FindWindow(NULL, L"Microsoft Solitaire Collection - Google Chrome");	// disable hardware acceleration in advanced settings
	
	if (hwnd == NULL)
	{
		std::cout << "Cant find Firefox window" << std::endl;
		exit(EXIT_FAILURE);
	}

	HMONITOR appMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);	// check on which monitor Solitaire is being played
	MONITORINFO appMonitorInfo;
	appMonitorInfo.cbSize = sizeof(appMonitorInfo);
	GetMonitorInfo(appMonitor, &appMonitorInfo);	// get the location of that monitor in respect to the primary window
	appRect = appMonitorInfo.rcMonitor;	// getting the data of the monitor on which Solitaire is being played
	
	windowWidth = abs(appRect.right - appRect.left);
	windowHeight = abs(appRect.bottom - appRect.top);
	if (1.75 < windowWidth / windowHeight < 1.80)	// if the monitor doesn't have 16:9 aspect ratio, calculate the distorted coordinates
	{
		distortedWindowHeight = (windowWidth * 1080 / 1920);
	}
}

void GameAnalytics::initGameLogic()
{
	currentState = PLAYING;	// initialize the statemachine

	previouslySelectedCard.first = EMPTY;	// initialize selected card logic
	previouslySelectedCard.second = EMPTY;

	startOfGame = Clock::now();	// tracking the time between moves and total game time
	startOfMove = Clock::now();

	classifiedCardsFromPlayingBoard.reserve(12);
	src = waitForStableImage();	// get the first image of the board
	pb.determineROI(src);	// calculating the important region within this board image
	
	pb.findCardsFromBoardImage(src);	// setup the starting board
	extractedImagesFromPlayingBoard = pb.getCards();
	classifyExtractedCards();
	initializePlayingBoard(classifiedCardsFromPlayingBoard);

	numberOfPresses.resize(12);	// used to track the number of presses on each cardlocation of the playingboard
	for (int i = 0; i < numberOfPresses.size(); i++)
	{
		numberOfPresses.at(i) = 0;
	}
}

void GameAnalytics::process()
{
	while (!endOfGameBool)
	{
		if (!srcBuffer.empty())	// click registered
		{
			EnterCriticalSection(&threadLock);	// get the coordinates of the click and image of the board
			src = srcBuffer.front(); srcBuffer.pop();
			pt->x = xPosBuffer2.front(); xPosBuffer2.pop();
			pt->y = yPosBuffer2.front(); yPosBuffer2.pop();
			LeaveCriticalSection(&threadLock);

			// recalculate the coordinates to the correct window and rescale to the standardBoard
			pt->y = (pt->y - (windowHeight - distortedWindowHeight) / 2) / distortedWindowHeight * windowHeight;
			MapWindowPoints(GetDesktopWindow(), hwnd, &pt[0], 1);
			MapWindowPoints(GetDesktopWindow(), hwnd, &pt[1], 1);
			pt->x = pt->x * standardBoardWidth / windowWidth;
			pt->y = pt->y * standardBoardHeight / windowHeight;
			std::cout << "Click registered at position (" << pt->x << "," << pt->y << ")" << std::endl;

			/*stringstream ss;	// write the image to the disk for debugging or aditional testing
			ss << ++processedSrcCounter;
			imwrite("../GameAnalytics/screenshots/" + ss.str() + ".png", src);*/

			determineNextState(pt->x, pt->y);	// check if a special location was pressed (menu, new game, undo, etc.) and change to the respecting game state
			
			switch (currentState)
			{
			case PLAYING:
				handlePlayingState();
				break;
			case UNDO:
				handleUndoState();
				break;
			case QUIT:
				gameWon = false;
				endOfGameBool = true;
				break;
			case WON:
				gameWon = true;
				endOfGameBool = true;
				break;
			case AUTOCOMPLETE:
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
			processCardSelection(pt->x, pt->y);	// check which card has been pressed and whether game errors have been made
		}

		else if (currentState == WON)	// waitForStableImage took too long, game won
		{
			gameWon = true;
			endOfGameBool = true;
		}
		else if (currentState == PLAYING)	// check for out of moves using a static image
		{
			cv::Mat img = hwnd2mat(hwnd);
			if (pb.checkForOutOfMovesState(img))
			{
				std::cout << "OUT OF MOVES!" << std::endl;
				currentState = OUTOFMOVES;
			}
		}
		else
		{
			Sleep(10);
		}
	}
	handleEndOfGame();
}

void GameAnalytics::handleUndoState()
{
	if (previousPlayingBoards.size() > 1)	// at least one move has been played in the game
	{
		previousPlayingBoards.pop_back();	// take the previous board state as the current board state
		currentPlayingBoard = previousPlayingBoards.back();
	}
	printPlayingBoardState();
	++numberOfUndos;
	currentState = PLAYING;
}

void GameAnalytics::toggleClickDownBool()
{
	if (waitForStableImageBool)	// if the main process is waiting for a stable image (no animations), but a new click comes in
	{
		std::cout << "CLICK DOWN BOOL" << std::endl;	// take the image right on the new click as the image of the previous move
		clickDownBuffer.push(hwnd2mat(hwnd));
		clickDownBool = true;	// notify waitForStableImage that it can use this image
	}
	else
	{
		clickDownBool = false;
	}
}

void GameAnalytics::grabSrc()
{
	while (!endOfGameBool)	// while game not over
	{
		if (!xPosBuffer1.empty())	// click registered
		{
			waitForStableImageBool = true;
			cv::Mat img = waitForStableImage();	// grab a still image
			waitForStableImageBool = false;

			if (!img.empty())	// empty image if the animation takes longer than 3 seconds (= game won animation)
			{
				EnterCriticalSection(&threadLock);
				xPosBuffer2.push(xPosBuffer1.front()); xPosBuffer1.pop();	// push coordinates and image to a second buffer for processing
				yPosBuffer2.push(yPosBuffer1.front()); yPosBuffer1.pop();
				srcBuffer.push(img.clone());
				LeaveCriticalSection(&threadLock);
			}
			else // game won -> pop buffers
			{
				currentState = WON;
				xPosBuffer1.pop();
				yPosBuffer1.pop();
			}
		}
		else
		{
			Sleep(5);
		}
	}
}

void GameAnalytics::processCardSelection(const int & x, const int & y)
{
	int indexOfPressedCardLocation = determineIndexOfPressedCard(x, y);	// check which cardlocation has been pressed using coordinates
	if (indexOfPressedCardLocation != -1)
	{
		int indexOfPressedCard = pb.getIndexOfSelectedCard(indexOfPressedCardLocation);	// check which card(s) on that index have been selected using a blue filter
		if (indexOfPressedCard != -1)													//	-> returns an index from bottom->top of how many cards have been selected
		{
			std::pair<classifiers, classifiers> selectedCard;
			if (indexOfPressedCardLocation != 7)
			{
				int index = currentPlayingBoard.at(indexOfPressedCardLocation).knownCards.size() - indexOfPressedCard - 1;	// indexOfPressedCard is from bot->top, knownCards is from top->bot - remap index
				selectedCard = currentPlayingBoard.at(indexOfPressedCardLocation).knownCards.at(index);	// check which card has been selected
			}
			else
			{
				selectedCard = currentPlayingBoard.at(indexOfPressedCardLocation).topCard;
			}
			detectPlayerMoveErrors(selectedCard, indexOfPressedCardLocation);	// detection of wrong card placement using previous and current selected card

			previouslySelectedCard = selectedCard;	// update previously detected card
			previouslySelectedIndex = indexOfPressedCardLocation;

			// +++metrics+++
			if (indexOfPressedCard == 0)	// topcard has been pressed
			{
				locationOfPresses.push_back(locationOfLastPress);	// add the coordinate to the vector of all presses for visualisation in the end
			}
			++numberOfPresses.at(indexOfPressedCardLocation);	// increment the pressed card location
		}
		else	// no card has been selected (just a click on the screen below the card)
		{
			previouslySelectedCard.first = EMPTY;
			previouslySelectedCard.second = EMPTY;
			previouslySelectedIndex = -1;
		}

		// +++metrics+++
		if (7 <= indexOfPressedCardLocation || indexOfPressedCardLocation < 12)	// the talon and suit stack are all 'topcards' and can immediately be added to locationOfPresses
		{
			locationOfPresses.push_back(locationOfLastPress);
		}
	}
}

void GameAnalytics::detectPlayerMoveErrors(std::pair<classifiers, classifiers> &selectedCard, int indexOfPressedCardLocation)
{
	if (previouslySelectedCard.first != EMPTY && previouslySelectedCard != selectedCard)	// current selected card is different from the previous, and there was a card previously selected
	{
		char prevSuit = static_cast<char>(previouslySelectedCard.second);
		char newSuit = static_cast<char>(selectedCard.second);
		char prevRank = static_cast<char>(previouslySelectedCard.first);
		char newRank = static_cast<char>(selectedCard.first);

		if (8 <= indexOfPressedCardLocation && indexOfPressedCardLocation < 12 && 0 <= previouslySelectedIndex && previouslySelectedIndex < 8)	// suit stack
		{
			if ((prevSuit == 'H' && newSuit != 'H') || (prevSuit == 'D' && newSuit != 'D')	// check for suit error
				|| (prevSuit == 'S' && newSuit != 'S') || (prevSuit == 'C' || newSuit != 'C'))
			{
				std::cout << "Incompatible suit! " << prevSuit << " can't go on " << newSuit << " to build the suit stack" << std::endl;
				++numberOfSuitErrors;
			}
			if ((prevRank == '2' && newRank != 'A') || (prevRank == '3' && newRank != '2') || (prevRank == '4' && newRank != '3')	// check for number error 
				|| (prevRank == '5' && newRank != '4') || (prevRank == '6' && newRank != '5') || (prevRank == '7' && newRank != '6')
				|| (prevRank == '8' && newRank != '7') || (prevRank == '9' && newRank != ':') || (prevRank == ':' && newRank != 'J')
				|| (prevRank == 'J' && newRank != 'Q') || (prevRank == 'Q' && newRank != 'K') || (prevRank == 'Q' && newRank != 'K'))
			{
				std::cout << "Incompatible rank! " << prevRank << " can't go on " << newRank << std::endl;
				++numberOfRankErrors;
			}
		}
		else	// build stack
		{
			if (((prevSuit == 'H' || prevSuit == 'D') && (newSuit == 'H' || newSuit == 'D'))	// check for color error
				|| ((prevSuit == 'S' || prevSuit == 'C') && (newSuit == 'S' || newSuit == 'C')))
			{
				std::cout << "Incompatible suit! " << prevSuit << " can't go on " << newSuit << " to build the build stack" << std::endl;
				++numberOfSuitErrors;
			}
			if ((prevRank == 'A' && newRank != '2') || (prevRank == '2' && newRank != '3') || (prevRank == '3' && newRank != '4')	// check for number error 
				|| (prevRank == '4' && newRank != '5') || (prevRank == '5' && newRank != '6') || (prevRank == '6' && newRank != '7')
				|| (prevRank == '7' && newRank != '8') || (prevRank == '8' && newRank != '9') || (prevRank == '9' && newRank != ':')
				|| (prevRank == ':' && newRank != 'J') || (prevRank == 'J' && newRank != 'Q') || (prevRank == 'Q' && newRank != 'K')
				|| (prevRank == 'K' && newRank != ' '))
			{
				std::cout << "Incompatible rank! " << prevRank << " can't go on " << newRank << std::endl;
				++numberOfRankErrors;
			}
		}
	}
}

int GameAnalytics::determineIndexOfPressedCard(const int & x, const int & y)	// check if a cardlocation has been pressed and remap the coordinate to that cardlocation
{																				// -> uses hardcoded values (possible because the screen is always an identical 1600x900)
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

void GameAnalytics::determineNextState(const int & x, const int & y)	// update the statemachine by checking if a special location has been pressed (menu, hint, undo, etc.)
{																		// -> uses hardcoded values (possible because the screen is always an identical 1600x900)
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
			int cardsLeft = 0;
			for (int i = 0; i < 8; i++)
			{
				if (currentPlayingBoard.at(i).unknownCards > 0)
				{
					++cardsLeft;
				}
			}
			if (cardsLeft > 0)
			{
				std::cout << "UNDO PRESSED!" << std::endl;
				currentState = UNDO;
			}
			else
			{
				std::cout << "AUTOSOLVE PRESSED!" << std::endl;
				int remainingCards = 0;
				for (int i = 0; i < 7; ++i)
				{
					remainingCards += currentPlayingBoard.at(i).knownCards.size();
				}
				score += (remainingCards * 10);
				currentState = AUTOCOMPLETE;
			}
		}
		else if ((13 <= x  && x <= 82) && (1 <= y && y <= 55))
		{
			std::cout << "MENU PRESSED!" << std::endl;
			currentState = MENU;
		}
		else if ((283 <= x  && x <= 416) && (84 <= y && y <= 258))	// pile pressed
		{
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
		break;
	default:
		std::cerr << "Error: currentState is not defined!" << std::endl;
		break;
	}
}

void GameAnalytics::handleEndOfGame()	// print all the metrics and data captured
{
	std::cout << "--------------------------------------------------------" << std::endl;
	(gameWon) ? std::cout << "Game won!" << std::endl : std::cout << "Game over!" << std::endl;	
	std::cout << "--------------------------------------------------------" << std::endl;
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
	cv::Mat pressLocations = Mat(176 * 2, 133 * 2, CV_8UC3, Scalar(255, 255, 255));	// output an image with location of presses on a topcard
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
	pb.findCardsFromBoardImage(src); // extract the cards from the board
	extractedImagesFromPlayingBoard = pb.getCards();
	classifyExtractedCards();	// classify the extracted cards
	if (updateBoard(classifiedCardsFromPlayingBoard))	// check if the board needs to be updated
	{
		previousPlayingBoards.push_back(currentPlayingBoard);	// if so, add the new playingboard to the list
		return true;
	}
	else
	{
		return false;
	}

}

void GameAnalytics::addCoordinatesToBuffer(const int x, const int y) {
	EnterCriticalSection(&threadLock);	// function called by the clickHooksThread, pushes the coordinates of a click to the first buffer
	xPosBuffer1.push(x);
	yPosBuffer1.push(y);
	LeaveCriticalSection(&threadLock);
}

void GameAnalytics::classifyExtractedCards()
{
	classifiedCardsFromPlayingBoard.clear();	// reset the variable
	for_each(extractedImagesFromPlayingBoard.begin(), extractedImagesFromPlayingBoard.end(), [this](cv::Mat mat) {
		if (mat.empty())	// extracted card was an empty image -> no card on this location
		{
			cardType.first = EMPTY;
			cardType.second = EMPTY;
		}
		else	// segment the rank and suit + classify this rank and suit
		{
			cardCharacteristics = cc.segmentRankAndSuitFromCard(mat);
			cardType = cc.classifyCard(cardCharacteristics);
		}
		classifiedCardsFromPlayingBoard.push_back(cardType);	// push the classified card to the variable
	});
}

cv::Mat GameAnalytics::waitForStableImage()	// -> average 112ms for non-updated screen
{
	norm = DBL_MAX;
	std::chrono::time_point<std::chrono::steady_clock> duration1 = Clock::now();
	
	src2 = hwnd2mat(hwnd);
	while (norm > 0.0)
	{
		if (std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - duration1).count() > 2)	// function takes longer than 2 seconds -> end of game animation
		{
			cv::Mat empty;
			return empty;
		}
		src1 = src2;
		cvtColor(src1, graySrc1, COLOR_BGR2GRAY);
		Sleep(60);	// wait for a certain duration to check for a difference (animation)
					// -> too short? issue: first animation of cardmove, second animation of new card turning around
					//					the second animation takes a small duration to kick in, which will be missed if the Sleep duration is too short
					// -> too long? the process takes longer, which can give issues when a player plays fast (more exception cases using clickDownBuffer)
		src2 = hwnd2mat(hwnd);
		cvtColor(src2, graySrc2, COLOR_BGR2GRAY);
		norm = cv::norm(graySrc1, graySrc2, NORM_L1);	// calculates the manhattan distance (sum of absolute values) of two grayimages
		if (clickDownBool)
		{
			clickDownBool = false;	// new click registered while waitForStableImage isn't done yet
									//  -> use the image at the moment of the new click (just before the new animation) for the previous move
			src1 = clickDownBuffer.front(); clickDownBuffer.pop();
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
	for (i = 0; i < 7; i++)	// add the classified cards as the only item of known cards and set the amount of unknown cards of each location
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
	startupLocation.topCard = classifiedCardsFromPlayingBoard.at(7);
	startupLocation.unknownCards = 24;	// each time a card has been moved from the talon, this value will decrease until 0
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
	previousPlayingBoards.push_back(currentPlayingBoard);	// add the first game state to the list of all game states
	printPlayingBoardState();	// print the board
}

bool GameAnalytics::updateBoard(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard)
{	
	changedIndex1 = -1, changedIndex2 = -1;
	findChangedCardLocations(classifiedCardsFromPlayingBoard, changedIndex1, changedIndex2);	// check which card locations have changed, this is maximum 2 (move from loc1 to loc2, or click on deck)
	if (changedIndex1 == -1 && changedIndex2 == -1)
	{
		return false;	// no locations changed, no update needed
	}
	
	// pile pressed (only talon changed)
	if (changedIndex1 == 7 && changedIndex2 == -1)
	{
		currentPlayingBoard.at(7).topCard = classifiedCardsFromPlayingBoard.at(7);
		printPlayingBoardState();
		return true;
	}


	// card move from talon to board
	if (changedIndex1 == 7 || changedIndex2 == 7)
	{
		int tempIndex;	// contains the other index
		(changedIndex1 == 7) ? (tempIndex = changedIndex2) : (tempIndex = changedIndex1);
		currentPlayingBoard.at(tempIndex).knownCards.push_back(currentPlayingBoard.at(7).topCard);	// add the card to the new location
		currentPlayingBoard.at(7).topCard = classifiedCardsFromPlayingBoard.at(7);	// update the card at the talon
		--currentPlayingBoard.at(7).unknownCards;
		printPlayingBoardState();
		++numberOfPresses.at(tempIndex);	// update the number of presses on that index
		(0 <= tempIndex && tempIndex < 7) ? score += 5 : score += 10;	// update the score
		return true;
	}

	// card move between build stack and suit stack
	if (changedIndex1 != -1 && changedIndex2 != -1)
	{
		if (cardMoveBetweenBuildAndSuitStack(classifiedCardsFromPlayingBoard, changedIndex1, changedIndex2))	// true if changed board
		{
			printPlayingBoardState();
			return true;
		}
	}

	// error with previously detected card
	if (changedIndex1 != -1 && changedIndex1 != 7 && changedIndex2 == -1)	// one card location changed which wasn't a talon change
	{
		if (currentPlayingBoard.at(changedIndex1).knownCards.empty())	// animation of previous state ended too fast, a card was still turning over which was missed
																		// -> no known cards in the list, but there is a card at that location: update knownCards
		{
			if (currentPlayingBoard.at(changedIndex1).unknownCards > 0)
			{
				--currentPlayingBoard.at(changedIndex1).unknownCards;
			}
			currentPlayingBoard.at(changedIndex1).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex1));
			printPlayingBoardState();
			score += 5;
			return true;
		}
		return false;		
	}
	return false;
}

bool GameAnalytics::cardMoveBetweenBuildAndSuitStack(const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard, int changedIndex1, int changedIndex2)
{
	// 1. CURRENT VISIBLE CARD WAS ALREADY IN THE LIST OF KNOWN CARDS -> ONE OR MORE TOPCARDS WERE MOVED TO A OTHER LOCATION
	auto inList1 = std::find(currentPlayingBoard.at(changedIndex1).knownCards.begin(), currentPlayingBoard.at(changedIndex1).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex1));
	auto inList2 = std::find(currentPlayingBoard.at(changedIndex2).knownCards.begin(), currentPlayingBoard.at(changedIndex2).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex2));
	
	// 1.1 The card was in the list of the card at index "changedIndex2"
	//		-> remove all cards after this index (iterator++) from knowncards at index "changedIndex2" and add them to the knowncards at index "changedIndex1"
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
		
		// update the score
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

	// 1.2 The card was in the list of the card at index "changedIndex1"
	//		-> remove all cards after this index (iterator++) from knowncards at index "changedIndex1"  and add them to the knowncards at index "changedIndex2"
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

		// update the score
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

	// 2. CURRENT VISIBLE CARD WAS NOT IN THE LIST OF KNOWN CARDS -> ALL TOPCARDS WERE MOVED TO A OTHER LOCATION AND A NEW CARD TURNED OVER
	//		-> check if the current topcard was in the list of the other cardlocation, if so, all cards of the other cardlocation were moved to this location
	
	if (inList1 == currentPlayingBoard.at(changedIndex1).knownCards.end() && inList2 == currentPlayingBoard.at(changedIndex2).knownCards.end())
	{
		auto inList1 = std::find(currentPlayingBoard.at(changedIndex1).knownCards.begin(), currentPlayingBoard.at(changedIndex1).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex2));
		auto inList2 = std::find(currentPlayingBoard.at(changedIndex2).knownCards.begin(), currentPlayingBoard.at(changedIndex2).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex1));
		
		// 2.1 The topcard at index "changedIndex2" was in the list of the card at index "changedIndex1"
		//		-> all cards from "changedIndex1" were moved to "changedIndex2" AND "changedIndex1" got a new card
		//		-> remove all knowncards "changedIndex1" and add them to the knowncards at "changedIndex2" AND add the new card to "changedIndex1"
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

			// update the score
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

		// 2.2 The topcard at index "changedIndex1" was in the list of the card at index "changedIndex2"
		//		-> all cards from "changedIndex2" were moved to "changedIndex1" AND "changedIndex2" got a new card
		//		-> remove all knowncards "changedIndex2" and add them to the knowncards at "changedIndex1" AND add the new card to "changedIndex2"
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

			// update the score
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
		if (i == 7)	// card is different from the topcard of the talon AND the card isn't empty
		{
			if (classifiedCardsFromPlayingBoard.at(7) != currentPlayingBoard.at(7).topCard)
			{
				changedIndex1 == -1 ? (changedIndex1 = 7) : (changedIndex2 = 7);
			}
		}
		else if (currentPlayingBoard.at(i).knownCards.empty())	// adding card to an empty location
		{
			if (classifiedCardsFromPlayingBoard.at(i).first != EMPTY)	// new card isn't empty
			{
				changedIndex1 == -1 ? (changedIndex1 = i) : (changedIndex2 = i);
			}
		}
		else
		{
			if (currentPlayingBoard.at(i).knownCards.back() != classifiedCardsFromPlayingBoard.at(i))	// classified topcard is different from the previous topcard
			{
				changedIndex1 == -1 ? (changedIndex1 = i) : (changedIndex2 = i);
			}
		}
		if (changedIndex2 != -1) { return; }	// if 2 changed location were detected, return
	}
}

void GameAnalytics::test()
{
	// PREPARATION
	std::vector<cv::Mat> testImages;
	std::vector<std::vector<std::pair<classifiers, classifiers>>> correctClassifiedOutputVector;

	for (int i = 0; i < 10; i++)	// read in all testimages and push them to the vector
	{
		stringstream ss;
		ss << i;
		Mat src = imread("../GameAnalytics/test/noMovesPlayed/" + ss.str() + ".png");
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
	if (!readTestData(correctClassifiedOutputVector, "../GameAnalytics/test/noMovesPlayed/correctClassifiedOutputVector.txt"))	// read in the correct classified output
	{
		std::cout << "Error reading testdata from txt file" << std::endl;
		exit(EXIT_FAILURE);
	}


	// ACTUAL TESTING

	// 1. Test for card extraction

	/*int wrongExtraction = 0;
	std::chrono::time_point<std::chrono::steady_clock> test1 = Clock::now();
	for (int k = 0; k < 100; k++)	// repeat for k loops
	{
		for (int i = 0; i < testImages.size(); i++)	// repeat for all testimages
		{
			pb.findCardsFromBoardImage(testImages.at(i));
			extractedImagesFromPlayingBoard = pb.getCards();
			for (int j = 0; j < extractedImagesFromPlayingBoard.size(); j++)
			{
				cv::Mat test = extractedImagesFromPlayingBoard.at(j);
				Size cardSize = extractedImagesFromPlayingBoard.at(j).size();	// finally, if sobel edge doesn't extract the card correctly, try using hardcoded values (cardheight = 1.33 * cardwidth)
				if (cardSize.width * 1.3 > cardSize.height || cardSize.width * 1.4 < cardSize.height)
				{
					++wrongExtraction;
				}
			}
		}
	}
	std::chrono::time_point<std::chrono::steady_clock> test2 = Clock::now();	// test the duration of the classification of 10*100 loops
	std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(test2 - test1).count() << " ns" << std::endl;
	std::cout << "Wrong extraction counter: " << wrongExtraction << std::endl;
	Sleep(10000);*/


	// 1. Test for card classification

	int wrongRankCounter = 0;
	int wrongSuitCounter = 0;
	std::vector<std::vector<cv::Mat>> allExtractedImages;
	allExtractedImages.resize(testImages.size());
	for (int i = 0; i < testImages.size(); i++)	// first, get all cards extracted correctly
	{
		pb.findCardsFromBoardImage(testImages.at(i));

		allExtractedImages.at(i) = pb.getCards();
	}
	std::chrono::time_point<std::chrono::steady_clock> test1 = Clock::now();
	for (int k = 0; k < 100; k++)	// repeat for k loops
	{
		for (int i = 0; i < allExtractedImages.size(); i++)
		{
			classifiedCardsFromPlayingBoard.clear();	// reset the variable
			for_each(allExtractedImages.at(i).begin(), allExtractedImages.at(i).end(), [this](cv::Mat mat) {
				if (mat.empty())	// extracted card was an empty image -> no card on this location
				{
					cardType.first = EMPTY;
					cardType.second = EMPTY;
				}
				else	// segment the rank and suit + classify this rank and suit
				{
					cardCharacteristics = cc.segmentRankAndSuitFromCard(mat);
					cardType = cc.classifyCard(cardCharacteristics);
				}
				classifiedCardsFromPlayingBoard.push_back(cardType);	// push the classified card to the variable
			});	
			for (int j = 0; j < classifiedCardsFromPlayingBoard.size(); j++)
			{
				if (correctClassifiedOutputVector.at(i).at(j).first != classifiedCardsFromPlayingBoard.at(j).first)	// compare the classified output with the correct classified output
				{
					++wrongRankCounter;
				}
				if (correctClassifiedOutputVector.at(i).at(j).second != classifiedCardsFromPlayingBoard.at(j).second)
				{
					++wrongSuitCounter;
				}
			}
		}
	}

	std::chrono::time_point<std::chrono::steady_clock> test2 = Clock::now();	// test the duration of the classification of 10*100 loops
	std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::milliseconds>(test2 - test1).count() << " ms" << std::endl;
	std::cout << "Rank error counter: " << wrongRankCounter << std::endl;	// print the amount of faulty classifications
	std::cout << "Suit error counter: " << wrongSuitCounter << std::endl;
	int amountOfPerfectSegmentations = cc.getAmountOfPerfectSegmentations();
	std::cout << "Amount of perfect segmentations: " << amountOfPerfectSegmentations << std::endl;	// print amount of good segmentations
	Sleep(10000);
}

bool GameAnalytics::writeTestData(const vector <vector <pair <classifiers, classifiers> > > &classifiedBoards, const string &file)
{
	if (classifiedBoards.empty())
		return false;

	if (file != "")
	{
		stringstream ss;
		for (int k = 0; k < classifiedBoards.size(); k++)	// push each classified card as rank + space + suit, each card on seperate lines, to a stringstream
		{
			for (int i = 0; i < classifiedBoards.at(k).size(); i++)
			{
				ss << static_cast<char>(classifiedBoards.at(k).at(i).first) << " " << static_cast<char>(classifiedBoards.at(k).at(i).second) << "\n";	// cast the classified rank/suit to a char for readability
			}
			ss << "\n";	// leave a whitespace for a new classified board
		}

		ofstream out(file.c_str());
		if (out.fail())
		{
			out.close();
			return false;
		}
		out << ss.str();	// push the stringstream of all classified cards to the file
		out.close();
	}
	return true;
}

bool GameAnalytics::readTestData(vector <vector <pair <classifiers, classifiers> > > &classifiedBoards, const string &file)
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
		classifiedBoards.resize(10);
		for (int i = 0; i < classifiedBoards.size(); i++)	// prepare the classifiedBoards vector
		{
			classifiedBoards.at(i).resize(12);
		}
		for (int k = 0; k < 10; k++)
		{
			for (int i = 0; i < 12; i++)
			{
				std::getline(in, str);	// read line by line
				istringstream iss(str);	// separates each word in the string
				string subs, subs2;
				iss >> subs;	// push the rank to a temporary string
				iss >> subs2;	// push the suit to a temporary string
				classifiedBoards.at(k).at(i).first = static_cast<classifiers>(subs[0]);	// cast the chars to classifiers and push them to the classifiedBoards vector
				classifiedBoards.at(k).at(i).second = static_cast<classifiers>(subs2[0]);
			}
			std::getline(in, str);	// at the end of a board, bump one extra line for a new board
		}
		in.close();
	}
	return true;
}

void GameAnalytics::printPlayingBoardState()
{
	std::cout << "Talon: ";	// print the current topcard from deck
	if (currentPlayingBoard.at(7).topCard.first == EMPTY)
	{
		std::cout << "// ";
	}
	else
	{
		std::cout << static_cast<char>(currentPlayingBoard.at(7).topCard.first) << static_cast<char>(currentPlayingBoard.at(7).topCard.second);
	}
	std::cout << "	Remaining: " << currentPlayingBoard.at(7).unknownCards << std::endl;

	std::cout << "Suit stack: " << std::endl;		// print all cards from the suit stack
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
		std::cout << std::endl;
	}

	std::cout << "Build stack: " << std::endl;		// print all cards from the build stack
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
		std::cout << "	Hidden cards = " << currentPlayingBoard.at(i).unknownCards << std::endl;
	}

	auto averageThinkTime2 = Clock::now();	// add the average think duration to the list
	averageThinkDurations.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(averageThinkTime2 - startOfMove).count());
	startOfMove = Clock::now();
	std::cout << std::endl;
}

cv::Mat GameAnalytics::hwnd2mat(const HWND & hwnd)	//Mat = n-dimensional dense array class, HWND = handle for desktop window
{
	HDC hwindowDC, hwindowCompatibleDC;

	int height, width, srcheight, srcwidth;
	HBITMAP hbwindow;
	Mat src;
	BITMAPINFOHEADER  bi;

	hwindowDC = GetDC(hwnd);	// get the device context of the window handle 
	hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);	// get a handle to the memory of the device context
	SetStretchBltMode(hwindowCompatibleDC, COLORONCOLOR);	// set the stretching mode of the bitmap so that when the image gets resized to a smaller size, the eliminated pixels get deleted w/o preserving information

	RECT windowsize;    // get the height and width of the screen
	GetClientRect(hwnd, &windowsize);

	srcheight = windowsize.bottom;	// get the screensize of the window
	srcwidth = windowsize.right;
	height = windowsize.bottom;
	width = windowsize.right;

	src.create(height, width, CV_8UC4);	// create an a color image (R,G,B and alpha for transparency)

	// create a bitmap
	hbwindow = CreateCompatibleBitmap(hwindowDC, width, height);
	bi.biSize = sizeof(BITMAPINFOHEADER);    //http://msdn.microsoft.com/en-us/library/windows/window/dd183402%28v=vs.85%29.aspx
	bi.biWidth = width;
	bi.biHeight = -height;  //this is the line that makes it draw upside down or not
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